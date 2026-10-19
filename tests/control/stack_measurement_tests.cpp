#include "power_management/stack_measurement.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
using namespace power_management::stack_measurement;
unsigned assertions=0,cycles=1000;
#define CHECK(x) do { ++assertions; if(!(x)){std::printf("FAIL line=%d %s\n",__LINE__,#x);return false;} }while(0)
bool attribution() {
 State s; s.begin(Path::ADC,1400);s.end(Path::ADC,1200);
 const auto &r=s.records[unsigned(Path::ADC)];
 CHECK(s.epoch==1&&s.baseline==1400);CHECK(r.count==1&&r.drops==1);
 CHECK(r.last.begin==1&&r.last.end==2&&r.last.before==1400&&r.last.after==1200);
 s.begin(Path::B,1200);s.end(Path::B,608);
 CHECK(s.history_count==2&&s.history[1].path==Path::B);
 s.begin(Path::ADC,608);s.end(Path::ADC,608);
 CHECK(r.count==2&&r.drops==1&&r.lowest.after==608);
 CHECK(r.last.before==608&&r.last.after==608); // warmed floor is not an ADC drop
 CHECK(!s.invalid);return true;
}
bool clear_records() {
 State s;s.begin(Path::B,1200);s.end(Path::B,608);s.clear(608);
 CHECK(s.epoch==2&&s.baseline==608&&s.sequence==0&&!s.history_count);
 CHECK(s.records[unsigned(Path::B)].count==0);
 s.begin(Path::ADC,608);s.end(Path::ADC,608);
 CHECK(!s.history_count&&s.records[unsigned(Path::ADC)].drops==0);
 CHECK(s.records[unsigned(Path::ADC)].lowest.after==608);return true;
}
bool nested_logging() {
 State s;s.begin(Path::ADC,1200);s.begin(Path::ERROR,1200);s.end(Path::ERROR,900);s.end(Path::ADC,900);
 CHECK(s.history_count==2);CHECK(s.history[0].path==Path::ERROR&&s.history[1].path==Path::ADC);
 CHECK(s.history[0].pair.begin>s.history[1].pair.begin&&s.history[0].pair.end<s.history[1].pair.end);
 CHECK(!s.invalid);return true; // nested inclusive evidence, not independent peaks
}
bool bounded_history() {
 State s;for(unsigned i=0;i<cycles;++i){s.begin(Path::INPUT,cycles+100-i);s.end(Path::INPUT,cycles+99-i);
 CHECK(s.history_count==i+1);CHECK(s.overwritten==(i<16?0:i-15));
 CHECK(s.history[i%16].pair.after==cycles+99-i);CHECK(s.records[unsigned(Path::INPUT)].count==i+1);
 CHECK(s.sequence==(i+1)*2);CHECK(!s.invalid);}
 CHECK(sizeof(State)<=1024);return true;
}
bool invalid_brackets() {
 State s;s.end(Path::B,800);CHECK(s.invalid==1);
 s.begin(Path::B,800);s.begin(Path::B,800);CHECK(s.invalid==2);
 s.end(Path::B,900);CHECK(s.invalid==3);s.end(Path::B,900);CHECK(s.invalid==4);return true;
}
bool saturation() {
 State s;s.sequence=UINT32_MAX-1;s.begin(Path::B,800);s.end(Path::B,800);
 CHECK(s.saturated&&s.sequence==UINT32_MAX);CHECK(!s.history_count);
 s.clear(800);CHECK(!s.saturated&&s.sequence==0);return true;
}
bool keys() {
 CHECK(key('S')==bool(POWER_STACK_MEASUREMENT));CHECK(key('T')==bool(POWER_STACK_MEASUREMENT));
 for(auto c:"BIZLCHNK!WGYRpas") { CHECK(!key(c)); }
 return true;
}
int main(int argc,char **argv){if(argc==2&&!std::strcmp(argv[1],"--soak"))cycles=10000;
 unsigned pass=0;for(auto f:{attribution,clear_records,nested_logging,bounded_history,invalid_brackets,saturation,keys}){if(!f())return 1;++pass;}
 std::printf("POWER_STACK: enabled=%u PASS=%u FAIL=0 assertions=%u cycles=%u state_bytes=%zu\n",unsigned(POWER_STACK_MEASUREMENT),pass,assertions,cycles,sizeof(State));}
