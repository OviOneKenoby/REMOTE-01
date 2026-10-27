#include "control_home/source_confirmation.hpp"
#include <cassert>
#include <cstdio>

using namespace control_home;

static FullControlSnapshot state(TunerSource source, std::uint64_t generation,
                                 bool result, bool accepted,
                                 TunerSource requested=TunerSource::BLUETOOTH) {
  FullControlSnapshot s{};
  s.connected=true;s.supported=true;s.source=source;s.source_generation=generation;
  s.result_present=result;s.result_accepted=accepted;s.requested_source=requested;
  s.command_id=42;s.session=7;
  return s;
}

int main() {
  SourceConfirmation c;
  c.start(TunerSource::BLUETOOTH,10,7,100);
  assert(c.observe(state(TunerSource::RADIO,10,true,true),7,101)==SourceConfirmationDecision::WAIT);
  assert(c.command_id==42);
  assert(c.observe(state(TunerSource::BLUETOOTH,11,false,false),7,102)==SourceConfirmationDecision::SUCCESS);

  c.start(TunerSource::BLUETOOTH,10,7,100);
  assert(c.observe(state(TunerSource::BLUETOOTH,11,false,false),7,101)==SourceConfirmationDecision::WAIT);
  assert(c.observe(state(TunerSource::BLUETOOTH,11,true,true),7,102)==SourceConfirmationDecision::SUCCESS);

  c.start(TunerSource::RADIO,11,7,100);
  auto rejected=state(TunerSource::BLUETOOTH,11,true,false,TunerSource::RADIO);rejected.last_error=4;
  assert(c.observe(rejected,7,101)==SourceConfirmationDecision::REJECTED&&c.error==4);

  c.start(TunerSource::RADIO,11,7,100);
  auto disconnected=state(TunerSource::BLUETOOTH,11,false,false,TunerSource::RADIO);disconnected.connected=false;
  assert(c.observe(disconnected,7,101)==SourceConfirmationDecision::WAIT);
  assert(c.observe(state(TunerSource::RADIO,12,false,false,TunerSource::RADIO),8,102)==SourceConfirmationDecision::SUCCESS);

  c.start(TunerSource::RADIO,12,8,100,10);
  assert(c.observe(state(TunerSource::BLUETOOTH,12,false,false,TunerSource::RADIO),8,110)==SourceConfirmationDecision::EXPIRED);

  c.start(TunerSource::RADIO,12,8,100);
  assert(c.observe(state(TunerSource::RADIO,12,true,true,TunerSource::RADIO),8,101)==SourceConfirmationDecision::WAIT);
  assert(c.active); // stale generation cannot confirm success
  c.clear();assert(!c.active);
  std::puts("source confirmation: PASS");
}
