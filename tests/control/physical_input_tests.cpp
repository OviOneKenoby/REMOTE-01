#include "physical_input/model.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
using namespace physical_input;
static unsigned assertions = 0, allocations = 0, cycles = 1000;
void *operator new(std::size_t n) { ++allocations; if (void *p=std::malloc(n)) return p; std::abort(); }
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p,std::size_t) noexcept { std::free(p); }
void operator delete[](void *p,std::size_t) noexcept { std::free(p); }
#define CHECK(x) do { ++assertions; if(!(x)){std::printf("FAIL input line=%u %s\n",unsigned(__LINE__),#x);return false;} }while(0)
static bool event(Model &m,Kind kind,unsigned id,int delta,unsigned sequence) {
  Event e{};bool present=false;CHECK(m.pop(1,e,present)==Result::OK && present);
  CHECK(e.kind==kind && e.button==id && e.delta==delta && e.sequence==sequence && e.epoch==1);return true;
}
static bool empty(Model &m) { Event e{};bool present=true;CHECK(m.pop(1,e,present)==Result::OK && !present);return true; }
static bool clean() { Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  CHECK(m.feed(1,{1,1,0})==Result::OK);CHECK(empty(m));CHECK(m.feed(1,{11,1,0})==Result::OK);
  CHECK(event(m,Kind::PRESS,0,0,1));CHECK(m.feed(1,{12,0,0})==Result::OK);CHECK(m.feed(1,{22,0,0})==Result::OK);
  CHECK(event(m,Kind::RELEASE,0,0,2));CHECK(empty(m));return true; }
static bool bounce() { Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  for(unsigned t=1;t<20;++t)CHECK(m.feed(1,{t,std::uint8_t(t%2),0})==Result::OK);
  CHECK(empty(m));CHECK(m.feed(1,{20,1,0})==Result::OK);CHECK(m.feed(1,{29,1,0})==Result::OK);
  CHECK(event(m,Kind::PRESS,0,0,1));CHECK(empty(m));return true; }
static bool release_bounce() {Model m(1,10);CHECK(m.baseline(1,{0,15,0})==Result::OK);
  for(unsigned t=1;t<20;++t)CHECK(m.feed(1,{t,std::uint8_t(t%2?15:0),0})==Result::OK);
  CHECK(empty(m));CHECK(m.feed(1,{20,0,0})==Result::OK);CHECK(m.feed(1,{30,0,0})==Result::OK);
  for(unsigned id=0;id<4;++id) { CHECK(event(m,Kind::RELEASE,id,0,id+1)); }return true; }
static bool simultaneous() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  CHECK(m.feed(1,{1,15,0})==Result::OK);CHECK(m.feed(1,{11,15,0})==Result::OK);
  for(unsigned id=0;id<4;++id) { CHECK(event(m,Kind::PRESS,id,0,id+1)); }CHECK(empty(m));return true; }
static bool duplicates() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  for(unsigned t=0;t<100;++t)CHECK(m.feed(1,{t,0,0})==Result::OK);
  CHECK(empty(m));CHECK(m.feed(1,{100,1,0})==Result::OK);CHECK(m.feed(1,{110,1,0})==Result::OK);
  for(unsigned t=110;t<200;++t)CHECK(m.feed(1,{t,1,0})==Result::OK);
  CHECK(event(m,Kind::PRESS,0,0,1));CHECK(empty(m));return true; }
static bool malformed() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  CHECK(m.feed(1,{1,16,0})==Result::BAD_SAMPLE);CHECK(m.feed(1,{1,0,4})==Result::BAD_SAMPLE);
  CHECK(m.feed(1,{10,0,0})==Result::OK);CHECK(m.feed(1,{9,1,0})==Result::BAD_SAMPLE);
  View v;CHECK(m.view(1,v)==Result::OK && v.stats.malformed==3 && v.pressed==0);CHECK(empty(m));return true; }
static bool overflow() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  for(unsigned i=0;i<9;++i){auto mask=std::uint8_t(i%2?0:1);CHECK(m.feed(1,{i*20+1,mask,0})==Result::OK);
    CHECK(m.feed(1,{i*20+11,mask,0})==(i==8?Result::LOSS:Result::OK));}
  View v;CHECK(m.view(1,v)==Result::OK && v.loss && v.queued==0 && v.stats.discarded==9);
  Event e;bool p;CHECK(m.pop(1,e,p)==Result::LOSS && !p);CHECK(m.feed(1,{1000,0,0})==Result::LOSS);
  CHECK(m.baseline(1,{1001,1,0})==Result::OK);CHECK(m.view(1,v)==Result::OK && v.epoch==2 && !v.loss);
  CHECK(empty(m));return true; }
static bool encoder_positive() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  unsigned t=0;for(auto ab:{1,3,2,0})CHECK(m.feed(1,{++t,0,std::uint8_t(ab)})==Result::OK);
  CHECK(event(m,Kind::ENCODER_CYCLE,0,1,1));CHECK(empty(m));return true; }
static bool encoder_negative() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  unsigned t=0;for(auto ab:{2,3,1,0})CHECK(m.feed(1,{++t,0,std::uint8_t(ab)})==Result::OK);
  CHECK(event(m,Kind::ENCODER_CYCLE,0,-1,1));CHECK(empty(m));return true; }
static bool encoder_bounce() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  unsigned t=0;for(auto ab:{1,0,1,3,1,3,2,3,2,0})CHECK(m.feed(1,{++t,0,std::uint8_t(ab)})==Result::OK);
  CHECK(event(m,Kind::ENCODER_CYCLE,0,1,1));CHECK(empty(m));return true; }
static bool encoder_invalid() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  CHECK(m.feed(1,{1,0,3})==Result::OK);CHECK(empty(m));View v;CHECK(m.view(1,v)==Result::OK && v.stats.encoder_invalid==1);
  unsigned t=1;for(auto ab:{2,0,1,3})CHECK(m.feed(1,{++t,0,std::uint8_t(ab)})==Result::OK);
  CHECK(event(m,Kind::ENCODER_CYCLE,0,1,1));return true; }
static bool rapid() {Model m(1,1);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  unsigned t=0;for(unsigned i=0;i<100;++i){for(auto ab:{1,3,2,0})CHECK(m.feed(1,{++t,0,std::uint8_t(ab)})==Result::OK);
    CHECK(event(m,Kind::ENCODER_CYCLE,0,1,i+1));}return true; }
static bool mixed_order() {Model m(1,3);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  CHECK(m.feed(1,{1,15,1})==Result::OK);CHECK(m.feed(1,{2,15,3})==Result::OK);
  CHECK(m.feed(1,{3,15,2})==Result::OK);CHECK(m.feed(1,{4,15,0})==Result::OK);
  for(unsigned id=0;id<4;++id) { CHECK(event(m,Kind::PRESS,id,0,id+1)); }
  CHECK(event(m,Kind::ENCODER_CYCLE,0,1,5));return true; }
static bool initialization() {Model m(1,10);Event e;bool p;CHECK(m.feed(1,{0,0,0})==Result::NOT_READY);
  CHECK(m.pop(1,e,p)==Result::NOT_READY);CHECK(m.baseline(1,{0,15,3})==Result::OK);CHECK(empty(m));
  CHECK(m.feed(1,{100,15,3})==Result::OK);CHECK(empty(m));Model bad(1,0);
  CHECK(bad.baseline(1,{0,0,0})==Result::CONFIG);Model owner_zero(0,10);CHECK(owner_zero.baseline(0,{0,0,0})==Result::CONFIG);return true; }
static bool ownership() {Model m(1,10);CHECK(m.baseline(1,{0,0,0})==Result::OK);View v;Event e;bool p;
  CHECK(m.feed(2,{1,7,3})==Result::NOT_OWNER);CHECK(m.baseline(2,{1,7,3})==Result::NOT_OWNER);
  CHECK(m.pop(2,e,p)==Result::NOT_OWNER && !p);CHECK(m.view(2,v)==Result::NOT_OWNER);
  CHECK(m.view(1,v)==Result::OK && v.pressed==0 && v.epoch==1 && v.queued==0);return true; }
static bool bounds() {Model m(1,10);CHECK(m.baseline(1,{UINT64_MAX-11,0,0})==Result::OK);
  CHECK(m.feed(1,{UINT64_MAX-10,1,0})==Result::OK);CHECK(m.feed(1,{UINT64_MAX,1,0})==Result::OK);
  CHECK(event(m,Kind::PRESS,0,0,1));CHECK(m.feed(1,{0,0,0})==Result::BAD_SAMPLE);
  std::uint64_t count=UINT64_MAX-1;CHECK(increment(count) && count==UINT64_MAX);CHECK(!increment(count) && count==UINT64_MAX);
  CHECK(m.baseline(1,{0,0,0})==Result::OK);CHECK(empty(m));return true; }
static bool ring_wrap() {Model m(1,1);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  for(unsigned i=0;i<100;++i){auto mask=std::uint8_t(i%2?0:1);CHECK(m.feed(1,{i*3+1,mask,0})==Result::OK);
    CHECK(m.feed(1,{i*3+2,mask,0})==Result::OK);CHECK(event(m,i%2?Kind::RELEASE:Kind::PRESS,0,0,i+1));}return true; }
static bool resync() {Model m(1,1);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  CHECK(m.feed(1,{1,1,0})==Result::OK);CHECK(m.feed(1,{2,1,0})==Result::OK);
  CHECK(m.baseline(1,{3,1,0})==Result::OK);View v;CHECK(m.view(1,v)==Result::OK && v.epoch==2 && v.stats.discarded==1);
  CHECK(empty(m));return true; }
static bool endurance() {const auto before=allocations;Model m(1,2);CHECK(m.baseline(1,{0,0,0})==Result::OK);
  std::uint64_t tick=0;unsigned sequence=0;
  for(unsigned i=0;i<cycles;++i){auto mask=std::uint8_t(i%2?15:0);CHECK(m.feed(1,{++tick,mask,0})==Result::OK);
    tick+=2;CHECK(m.feed(1,{tick,mask,0})==Result::OK);
    if(i) { for(unsigned id=0;id<4;++id) { CHECK(event(m,i%2?Kind::PRESS:Kind::RELEASE,id,0,++sequence)); } }
    for(auto ab:{1,3,2,0})CHECK(m.feed(1,{++tick,mask,std::uint8_t(ab)})==Result::OK);
    CHECK(event(m,Kind::ENCODER_CYCLE,0,1,++sequence));CHECK(empty(m));}
  CHECK(allocations==before);View v;CHECK(m.view(1,v)==Result::OK && !v.loss && !v.stats.saturated && !v.stats.malformed && !v.stats.encoder_invalid);return true; }
int main(int argc,char **argv) {
  if(argc>1 && !std::strcmp(argv[1],"--soak"))cycles=10000;
  bool (*tests[])()={clean,bounce,release_bounce,simultaneous,duplicates,malformed,overflow,encoder_positive,encoder_negative,encoder_bounce,encoder_invalid,rapid,mixed_order,initialization,ownership,bounds,ring_wrap,resync,endurance};
  unsigned pass=0,fail=0;for(auto test:tests){if(test())++pass;else ++fail;}
  std::printf("PHYSICAL_INPUT: PASS=%u FAIL=%u assertions=%u cycles=%u model_bytes=%u allocations=%u\n",pass,fail,assertions,cycles,unsigned(sizeof(Model)),allocations);
  return fail?1:0;
}
