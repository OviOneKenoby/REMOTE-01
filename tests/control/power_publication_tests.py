"""Exercise the actual extracted POWER publication helper with bounded host seams."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--soak", action="store_true")
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
source = (root / "components/power_management/runtime.cpp").read_text()
helper = source.split("__attribute__((noinline)) void publish_power(", 1)[1].split("__attribute__((noinline)) void sample_external", 1)[0]
helper = "__attribute__((noinline)) void publish_power(" + helper
task = source.split("void task(void *)", 1)[1].split("} // namespace", 1)[0]
assert "diagnostics::Power d" not in task and "reset_history_copy()" not in task
assert task.count("publish_power(") == 1
prefix = r'''
#include "power_management/reset_diagnostic.hpp"
#include "diagnostics/model.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
static unsigned allocations=0, assertions=0;
void* operator new(std::size_t n) {++allocations; if(auto*p=std::malloc(n))return p;std::abort();}
void operator delete(void*p) noexcept {std::free(p);}
void operator delete(void*p,std::size_t) noexcept {std::free(p);}
static unsigned trace[32]{}, used=0, watermarks=0;
static diagnostics::Power captured{};
static power_management::ResetSummary history{};
static void mark(unsigned v) {if(used>=32)std::abort();trace[used++]=v;}
static unsigned uxTaskGetStackHighWaterMark(void*) {++watermarks;return 1234;}
namespace diagnostics {
void publish(const Power &v) {mark(33);captured=v;}
void event(U64 time,Event e,unsigned detail) {
  if(time!=999 || (e==Event::POWER && detail!=1) || (e==Event::POWER_ERROR && detail!=29))std::abort();
  mark(e==Event::POWER?40:41);
}
}
namespace power_management {
struct Board {bool imu_ready=true,adc_ready=true;};
ResetSummary reset_history_copy() {mark(23);return history;}
enum class Probe {SNAPSHOT=1,RESET_COPY=2,PUBLISH=3};
#define STACK_BEGIN(p) mark(unsigned(Probe::p)*10+1)
#define STACK_END(p) mark(unsigned(Probe::p)*10+2)
'''
suffix = r'''
}
#define CHECK(x) do {++assertions;if(!(x)){std::printf("FAIL line=%u\n",__LINE__);return 1;}}while(0)
int main(int argc,char**) {
  using namespace power_management;
  const unsigned cycles=argc>1?10000:1000;
  Machine m; m.state=State::DISPLAY_IDLE;m.reason=Wake::MOTION;m.wakes=7;m.sleep_requests=8;m.last_activity=900;
  Board board; Battery real,sim; ExternalView external;
  external.state=External::PRESENT;external.raw=2000;external.millivolts=2420;external.valid=external.calibrated=true;external.read_errors=4;external.error=0;
  real.raw=21;real.voltage_mv=22;real.filtered_mv=23;real.valid=true;real.condition=Condition::LOW;
  sim.raw=31;sim.voltage_mv=32;sim.filtered_mv=33;sim.valid=false;sim.condition=Condition::CRITICAL;
  unsigned long long rounds=9,errors=10,motions=11,bus=12,measurement=7,latency=16,previous_errors=0;
  Time previous_time=0;unsigned previous_state=0;
  for(unsigned age=0;age<4;++age){history.rows[age].available=age!=2;
    for(unsigned j=0;j<15;++j)history.rows[age].values[j]=age==2?0:age*100+j;}
  const unsigned allocated_before=allocations;
  for(unsigned iteration=0;iteration<cycles;++iteration) {
    used=watermarks=0;const bool injected=iteration%2;
    previous_state=iteration%3?1:0;previous_errors=iteration%3?29:0;
    publish_power(999,m,board,real,sim,external,true,false,injected,rounds,errors,motions,bus,measurement,latency,17,
                  previous_time,previous_state,previous_errors);
    const auto &b=injected?sim:real;
    const std::array<diagnostics::U64,23> expected={1,1,0,2,7,8,9,10,99,1,11,12,1234,b.raw,b.voltage_mv,b.filtered_mv,b.valid,unsigned(b.condition),injected,7,16,17,1};
    CHECK(captured.time==999);CHECK(captured.values==expected);CHECK(watermarks==1);
    CHECK(captured.external.raw==2000 && captured.external.mv==2420 && captured.external.state==1);
    CHECK(captured.external.valid && captured.external.calibrated && captured.external.error==0 && captured.external.read_errors==4);
    for(unsigned age=0;age<4;++age){CHECK(captured.resets[age][0]==history.rows[age].available);
      for(unsigned j=0;j<15;++j)CHECK(captured.resets[age][j+1]==history.rows[age].values[j]);}
    CHECK(previous_time==999 && previous_state==1 && previous_errors==29);
    const unsigned expected_trace[]={11,12,21,23,22,11,12,31,33,40,41,32};
    const bool changed=iteration%3==0;CHECK(used==(changed?12u:10u));
    for(unsigned j=0;j<used;++j)CHECK(trace[j]==expected_trace[!changed && j==9?11:j]);
    CHECK(allocations==allocated_before);
  }
  std::printf("POWER_PUBLICATION: PASS=1 FAIL=0 assertions=%u cycles=%u allocations=%u\n",assertions,cycles,allocations);
}
'''
compiler = Path(r"C:/msys64/ucrt64/bin/g++.exe")
env = dict(os.environ)
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="remote01-publication-") as work:
    cpp = Path(work) / "test.cpp"
    binary = Path(work) / "test.exe"
    cpp.write_text(prefix + helper + suffix)
    subprocess.run([str(compiler), "-std=c++23", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fno-exceptions", "-fno-rtti", "-I", str(root / "components/power_management/include"),
                    "-I", str(root / "components/diagnostics/include"), str(cpp), "-o", str(binary)], env=env, check=True)
    subprocess.run([str(binary)] + (["--soak"] if a.soak else []), env=env, check=True)
