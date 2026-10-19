#include "power_management/external_power.hpp"
#include "power_management/model.hpp"
#include "diagnostics/model.hpp"
#include <cstdio>
#include <cstring>
using namespace power_management;
static unsigned assertions=0;
#define CHECK(x) do{++assertions;if(!(x)){std::printf("FAIL line=%u\n",__LINE__);return 1;}}while(0)
int main(int argc,char**) {
 unsigned cases=0;const unsigned cycles=argc>1?10000:1000;
 ExternalPower d;CHECK(d.view.state==External::UNKNOWN && !external_plugged_in(d.view));
 auto sample=[&](unsigned mv,unsigned t){d.sample(0,mv?3000:0,mv,true,t);};
 sample(2420,0);sample(2420,2000);CHECK(d.view.valid && d.view.state==External::UNKNOWN);
 sample(2420,4000);CHECK(d.view.state==External::PRESENT && external_plugged_in(d.view));++cases;
 sample(1799,6000);sample(601,8000);CHECK(d.view.state==External::PRESENT);
 sample(600,10000);sample(1000,12000);sample(600,14000);sample(600,16000);CHECK(d.view.state==External::PRESENT);
 sample(600,18000);CHECK(d.view.state==External::ABSENT && !external_plugged_in(d.view));++cases;
 sample(1799,20000);CHECK(d.view.state==External::ABSENT);
 sample(1800,22000);sample(1800,24000);CHECK(d.view.state==External::ABSENT);
 sample(1800,26000);CHECK(d.view.state==External::PRESENT);++cases;
 d.sample(-7,0,0,true,28000);CHECK(!d.view.valid && d.view.error==-7 && d.view.read_errors==1 && d.view.state==External::UNKNOWN);++cases;
 d.sample(0,0,0,false,30000);CHECK(!d.view.valid && !external_plugged_in(d.view));
 d.sample(0,4095,2420,true,32000);CHECK(!d.view.valid && d.view.state==External::UNKNOWN);
 d.sample(0,2000,3101,true,34000);CHECK(!d.view.valid && d.view.read_errors==4);++cases;
 sample(1200,36000);sample(1200,38000);sample(1200,40000);CHECK(d.view.valid && d.view.state==External::UNKNOWN);++cases;
 sample(0,42000);sample(0,44000);sample(0,46000);CHECK(d.view.valid && d.view.state==External::ABSENT);++cases;
 sample(2420,60000);CHECK(d.view.state==External::UNKNOWN);sample(2420,62000);sample(2420,64000);CHECK(external_plugged_in(d.view));
 sample(2420,1);CHECK(d.view.state==External::UNKNOWN);++cases;
 for(auto state:{External::PRESENT,External::ABSENT,External::UNKNOWN}) {
  ExternalView v;v.state=state;v.valid=v.calibrated=true;CHECK(external_plugged_in(v)==(state==External::PRESENT));
  v.valid=false;CHECK(!external_plugged_in(v));v.valid=true;v.calibrated=false;CHECK(!external_plugged_in(v));
  Machine m;m.tick(300000,true,state);CHECK(m.state==State::SYSTEM_SLEEP);
 }++cases;
 diagnostics::Snapshots snap;char out[4096];snap.power.time=1;
 for(unsigned state=0;state<4;++state) {
  snap.power.external={2000,2420,state,1,1,0,7};
  CHECK(diagnostics::status_json(snap,100,"REMOTE","1","hash",3,out,sizeof out));
  const char *expected=state==1?"\"external\":\"PRESENT\"":state==2?"\"external\":\"ABSENT\"":"\"external\":\"UNKNOWN\"";
  CHECK(std::strstr(out,expected));CHECK(std::strstr(out,"\"charging\":\"UNKNOWN\""));
  CHECK(std::strstr(out,"\"external_raw\":2000") && std::strstr(out,"\"external_mv\":2420") && std::strstr(out,"\"external_error\":0"));
  snap.power.external.error=-3;CHECK(diagnostics::status_json(snap,100,"REMOTE","1","hash",3,out,sizeof out));CHECK(std::strstr(out,"\"external\":\"UNKNOWN\""));
  snap.power.external.valid=0;CHECK(diagnostics::status_json(snap,100,"REMOTE","1","hash",3,out,sizeof out));CHECK(std::strstr(out,"\"external\":\"UNKNOWN\""));
 }++cases;
 for(unsigned i=0;i<cycles;++i) {
  ExternalPower detector;for(unsigned j=0;j<3;++j)detector.sample(0,3000,2420,true,j*2000);
  CHECK(detector.view.state==External::PRESENT);
  for(unsigned j=3;j<6;++j)detector.sample(0,0,0,true,j*2000);
  CHECK(detector.view.state==External::ABSENT);
  detector.sample(5,0,0,true,12000);CHECK(detector.view.state==External::UNKNOWN && detector.view.read_errors==1);
  detector.view.read_errors=~std::uint64_t{0};detector.sample(5,0,0,true,14000);CHECK(detector.view.read_errors==~std::uint64_t{0});
 }++cases;
 std::printf("EXTERNAL_POWER: PASS=%u FAIL=0 assertions=%u cycles=%u\n",cases,assertions,cycles);
}
