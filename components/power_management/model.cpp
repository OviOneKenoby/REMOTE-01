#include "power_management/model.hpp"
#include <algorithm>

namespace power_management {
void Battery::sample(bool ok, unsigned raw_value, unsigned mv, Time now) {
  raw = raw_value;
  if (!ok || mv < 2500 || mv > 4500) {
    valid = qualifying = false; count = index = 0;
    condition = Condition::UNKNOWN; return;
  }
  voltage_mv = mv;
  window[index] = mv; index = (index + 1) % window.size();
  if (count < window.size()) ++count;
  auto sorted = window;
  std::sort(sorted.begin(), sorted.begin() + count);
  const auto median = sorted[count / 2];
  auto next = valid ? (3 * filtered_mv + median + 2) / 4 : median;
  // Integer EMA must reach a steady input, including exact safety thresholds.
  if (valid && next == filtered_mv && median != filtered_mv)
    next = median > filtered_mv ? next + 1 : next - 1;
  filtered_mv = next;
  valid = true;
  if (condition == Condition::CRITICAL && filtered_mv < critical_recover_mv) return;
  if (count == window.size() && median <= critical_mv && filtered_mv <= critical_mv) {
    if (!qualifying || now < critical_since) { critical_since = now; qualifying = true; }
    if (now - critical_since >= qualification_ms) { condition = Condition::CRITICAL; return; }
  } else qualifying = false;
  if (condition == Condition::UNKNOWN || condition == Condition::CRITICAL)
    condition = filtered_mv <= low_mv ? Condition::LOW : Condition::NORMAL;
  else if (condition == Condition::NORMAL && filtered_mv <= low_mv) condition = Condition::LOW;
  else if (condition == Condition::LOW && filtered_mv >= low_recover_mv) condition = Condition::NORMAL;
}
void Machine::wake(Time now, Wake source) {
  if (state != State::ACTIVE) ++wakes;
  state = State::ACTIVE; reason = source; last_activity = now;
}
void Machine::tick(Time now, bool safe, External source) {
  (void)source; // Owner policy: external state is not sleep eligibility.
  if (state == State::CRITICAL_SHUTDOWN || now < last_activity) return;
  if (state == State::ACTIVE && now - last_activity >= display_idle_ms) state = State::DISPLAY_IDLE;
  if (state == State::DISPLAY_IDLE && now - last_activity >= system_idle_ms && now >= retry_after) {
    if (safe) { state = State::SYSTEM_SLEEP; ++sleep_requests; }
    else { ++refusals; retry_after = now + 60000; }
  }
}
void Machine::refuse_sleep(Time now) {
  state = State::DISPLAY_IDLE; retry_after = now + 60000; ++refusals;
}
} // namespace power_management
