#pragma once
#include <cstdint>
#include <limits>
namespace power_management {
enum class External { UNKNOWN, PRESENT, ABSENT };
struct ExternalView {
  External state=External::UNKNOWN;
  unsigned raw=0, millivolts=0;
  std::int32_t error=0;
  std::uint64_t read_errors=0;
  bool valid=false, calibrated=false;
};
inline const char *external_name(External state) {
  switch(state) {case External::PRESENT:return "PRESENT";case External::ABSENT:return "ABSENT";default:return "UNKNOWN";}
}
inline bool external_plugged_in(const ExternalView &v) {
  return v.valid && v.calibrated && v.error==0 && v.state==External::PRESENT;
}
// Provisional GPIO1-node thresholds from measured 0mV vs2420mV, not nominal VBUS.
struct ExternalPower {
  static constexpr unsigned absent_mv=600, present_mv=1800, confirmations=3;
  static constexpr std::uint64_t sample_ms=2000;
  ExternalView view;
  External candidate=External::UNKNOWN;
  unsigned count=0;
  std::uint64_t last_sample=0;
  bool sampled=false;
  void sample(std::int32_t error,unsigned raw,unsigned mv,bool calibrated,std::uint64_t now) {
    if(sampled && (now<last_sample || now-last_sample>2*sample_ms)) {
      view.state=External::UNKNOWN;candidate=External::UNKNOWN;count=0;
    }
    sampled=true;last_sample=now;
    view.raw=raw;view.millivolts=mv;view.calibrated=calibrated;view.error=error;
    view.valid=error==0 && calibrated && raw<4095 && mv<=3100;
    if(!view.valid) {
      view.state=External::UNKNOWN;candidate=External::UNKNOWN;count=0;
      if(view.read_errors!=std::numeric_limits<std::uint64_t>::max())++view.read_errors;
      return;
    }
    const auto next=mv<=absent_mv?External::ABSENT:mv>=present_mv?External::PRESENT:view.state;
    if(next==view.state || next==External::UNKNOWN) {candidate=External::UNKNOWN;count=0;return;}
    if(next!=candidate) {candidate=next;count=0;}
    if(++count==confirmations) {view.state=next;candidate=External::UNKNOWN;count=0;}
  }
};
} // namespace power_management
