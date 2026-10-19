#include "power_management/qmi_wom.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
using namespace power_management;
static unsigned assertions=0,allocations=0,cycles=1000;
void* operator new(std::size_t n){++allocations;if(auto*p=std::malloc(n))return p;std::abort();}
void operator delete(void*p) noexcept {std::free(p);}
void operator delete(void*p,std::size_t) noexcept {std::free(p);}
#define CHECK(x) do {++assertions;if(!(x)){std::printf("FAIL line=%d %s\n",__LINE__,#x);return false;}}while(0)
// Frozen pre-instrumentation configure/wait oracle: preserves transaction/delay trace.
struct LegacyWom : QMI8658Constants {
  static constexpr unsigned threshold_mg = 200, blanking_samples = 4;
  template<class Bus> static bool wait(Bus &b, unsigned reg, unsigned mask, unsigned expected, unsigned timeout) {
    const auto start = b.now(); unsigned char v = 0;
    do {
      if (!b.read(reg, v)) return false;
      if ((v & mask) == expected) return true;
      b.delay(5);
    } while (b.now() - start < timeout);
    return false;
  }
  template<class Bus> static bool configure(Bus &b) {
    unsigned char id = 0;
    if (!b.read(QMI8658_REG_WHOAMI, id) || id != QMI8658_REG_WHOAMI_DEFAULT ||
        !b.write(QMI8658_REG_RESET, QMI8658_REG_RESET_DEFAULT) ||
        !wait(b, QMI8658_REG_RST_RESULT, 0xff, QMI8658_REG_RST_RESULT_VAL, 500)) return false;
    // CTRL1: address auto-increment + INT1 enable, INT2 disabled.
    // CTRL2: +/-8g, low-power21Hz (ODR0xD). CAL1_H: INT1 initially1,
    // active0, four-sample startup blanking. No gyro/data-stream polling.
    if (!b.write(QMI8658_REG_CTRL1, 0x48) || !b.write(QMI8658_REG_CTRL7, 0) ||
        !b.write(QMI8658_REG_CTRL2, 0x2d) ||
        !b.write(QMI8658_REG_CAL1_L, threshold_mg) ||
        !b.write(QMI8658_REG_CAL1_H, 0x80 | blanking_samples) ||
        !b.write(QMI8658_REG_CTRL9, 0x08) ||
        !wait(b, QMI8658_REG_STATUS_INT, 0x80, 0x80, 1000) ||
        !b.write(QMI8658_REG_CTRL9, 0) ||
        !wait(b, QMI8658_REG_STATUS_INT, 0x80, 0, 1000) ||
        !b.write(QMI8658_REG_CTRL7, 1)) return false;
    unsigned char v = 0;
    return b.read(QMI8658_REG_CTRL7, v) && v == 1 &&
           b.read(QMI8658_REG_CTRL1, v) && v == 0x48 &&
           b.read(QMI8658_REG_STATUS1, v); // clear startup event
  }
};

struct Event {unsigned op=0,reg=0,value=0;bool operator==(const Event&)const=default;};
struct Bus {
  std::array<unsigned char,256> reg{};
  std::array<Event,1024> events{};unsigned used=0,operations=0,clock=0,fail_at=0;
  bool reset_ack=true,apply_ack=true,clear_ack=true;
  unsigned wrong=0,reset_reads=0,delay_reset_reads=0;
#if POWER_WOM_DIAGNOSTIC
  WomDiagnostic diagnostic{};
  WomDiagnostic &wom_diagnostic(){return diagnostic;}
#endif
  Bus(){reg[0]=5;}
  void mark(unsigned op,unsigned r,unsigned v){if(used==events.size())std::abort();events[used++]={op,r,v};}
  unsigned now(){return clock;}
  void delay(unsigned ms){mark(3,0,ms);clock+=ms;}
  bool transport(){const bool ok=++operations!=fail_at;
#if POWER_WOM_DIAGNOSTIC
    if(diagnostic.result==1)diagnostic.error=ok?0:263;
#endif
    return ok;}
  bool read(unsigned r,unsigned char &v){mark(1,r,0);if(!transport())return false;
    if(r==0x4d && reset_ack && reset_reads++>=delay_reset_reads)reg[r]=0x80;
    v=reg[r];if(wrong==r && r!=0)v^=1;return true;}
  bool write(unsigned r,unsigned char v){mark(2,r,v);if(!transport())return false;reg[r]=v;
    if(r==0x60)reg[0x4d]=0;
    if(r==0x0a){if(v && apply_ack)reg[0x2d]=0x80;if(!v && clear_ack)reg[0x2d]=0;}
    return true;}
};
static bool same(Bus &a,Bus &b,bool expected){auto x=LegacyWom::configure(a),y=Wom::configure(b);
  CHECK(x==expected && y==expected);CHECK(a.used==b.used && a.operations==b.operations && a.clock==b.clock);
  CHECK(a.reg==b.reg);for(unsigned i=0;i<a.used;++i)CHECK(a.events[i]==b.events[i]);return true;}
static bool success(){const auto before=allocations;
  for(unsigned i=0;i<cycles;++i){Bus a,b;CHECK(same(a,b,true));
#if POWER_WOM_DIAGNOSTIC
    CHECK(sizeof(WomDiagnostic)==44);CHECK(b.diagnostic.result==2 && b.diagnostic.failed==0 && b.diagnostic.cause==0);
    CHECK(b.diagnostic.stage==16 && b.diagnostic.op==1 && b.diagnostic.reg==0x2f && b.diagnostic.actual_valid);
    CHECK(b.diagnostic.polls==3 && b.diagnostic.stage_polls==0 && b.diagnostic.total_ms==0);
    const auto saved=b.diagnostic;bool motion=false;CHECK(Wom::status(b,motion));
    CHECK(std::memcmp(&saved,&b.diagnostic,sizeof saved)==0);
#endif
  }CHECK(allocations==before);return true;}
static bool transports(){for(unsigned n=1;n<=16;++n){Bus a,b;a.fail_at=b.fail_at=n;CHECK(same(a,b,false));
#if POWER_WOM_DIAGNOSTIC
    CHECK(b.diagnostic.result==3 && b.diagnostic.failed==n && b.diagnostic.cause==1);
    CHECK(b.diagnostic.error==263 && !b.diagnostic.actual_valid);
#endif
  }return true;}
static bool identity(){Bus a,b;a.reg[0]=b.reg[0]=0;CHECK(same(a,b,false));
#if POWER_WOM_DIAGNOSTIC
  CHECK(b.diagnostic.failed==1 && b.diagnostic.cause==3 && b.diagnostic.error==0);
  CHECK(b.diagnostic.expected==5 && b.diagnostic.mask==255 && b.diagnostic.actual==0 && b.diagnostic.actual_valid);
#endif
  return true;}
static bool timeouts(){for(unsigned type=0;type<3;++type){Bus a,b;
  if(type==0){a.reset_ack=b.reset_ack=false;}
  if(type==1){a.apply_ack=b.apply_ack=false;}
  if(type==2){a.clear_ack=b.clear_ack=false;}
  CHECK(same(a,b,false));
#if POWER_WOM_DIAGNOSTIC
  const auto &d=b.diagnostic;CHECK(d.failed==(type==0?3:type==1?10:12) && d.cause==2 && d.error==0 && d.actual_valid);
  CHECK(d.elapsed_ms==(type==0?500u:1000u) && d.stage_polls==(type==0?100u:200u));
#endif
  }return true;}
static bool readbacks(){for(unsigned r:{8u,2u}){Bus a,b;a.wrong=b.wrong=r;CHECK(same(a,b,false));
#if POWER_WOM_DIAGNOSTIC
  CHECK(b.diagnostic.failed==(r==8?14:15) && b.diagnostic.cause==3 && b.diagnostic.error==0);
  CHECK(b.diagnostic.actual_valid && b.diagnostic.actual!=b.diagnostic.expected);
#endif
  }return true;}
static bool delayed(){Bus a,b;a.delay_reset_reads=b.delay_reset_reads=3;CHECK(same(a,b,true));
#if POWER_WOM_DIAGNOSTIC
  CHECK(b.diagnostic.total_ms==15 && b.diagnostic.polls==6);
#endif
  return true;}
static bool clock_wrap(){Bus a,b;a.clock=b.clock=UINT32_MAX-100;a.reset_ack=b.reset_ack=false;CHECK(same(a,b,false));
#if POWER_WOM_DIAGNOSTIC
  CHECK(b.diagnostic.elapsed_ms==500 && b.diagnostic.total_ms==500 && b.diagnostic.failed==3);
#endif
  return true;}
int main(int argc,char**){if(argc>1)cycles=10000;
  bool(*tests[])()={success,transports,identity,timeouts,readbacks,delayed,clock_wrap};unsigned pass=0,fail=0;
  for(auto test:tests){if(test())++pass;else ++fail;}
  std::printf("WOM_DIAGNOSTIC: PASS=%u FAIL=%u assertions=%u cycles=%u enabled=%u allocations=%u\n",pass,fail,assertions,cycles,POWER_WOM_DIAGNOSTIC,allocations);
  return fail?1:0;
}
