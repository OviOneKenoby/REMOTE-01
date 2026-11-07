#pragma once
#include <array>
#include <cstdint>
#include "power_management/external_power.hpp"

namespace power_management {
using Time = std::uint64_t;
enum class State { ACTIVE, DISPLAY_IDLE, SYSTEM_SLEEP, CRITICAL_SHUTDOWN };
enum class Condition { UNKNOWN, NORMAL, LOW, CRITICAL };
enum class Wake { BOOT, TOUCH, MOTION, TIMER, FAULT, DEVELOPMENT };
struct Battery {
  static constexpr unsigned low_mv = 3600, low_recover_mv = 3700;
  static constexpr unsigned critical_mv = 3400, critical_recover_mv = 3550;
  static constexpr Time qualification_ms = 30000;
  std::array<unsigned, 5> window{};
  unsigned index = 0, count = 0, raw = 0, voltage_mv = 0, filtered_mv = 0;
  bool valid = false, qualifying = false;
  Time critical_since = 0;
  Condition condition = Condition::UNKNOWN;
  External external = External::UNKNOWN;
  void sample(bool calibrated_valid, unsigned raw_value, unsigned mv, Time now);
  bool shutdown_required() const { return valid && condition == Condition::CRITICAL && external == External::ABSENT; }
};
// Display timeouts the user may select (ms): 15 s, 30 s, 1 min, 2 min, 5 min.
inline constexpr std::array<Time, 5> display_idle_choices{15000, 30000, 60000, 120000, 300000};
constexpr bool display_idle_choice(Time ms) {
  for (const Time choice : display_idle_choices) if (choice == ms) return true;
  return false;
}
// System sleep delays the user may select (ms): 1, 2, 5, 10 and 30 min.
// There is deliberately no "never" choice.
inline constexpr std::array<Time, 5> system_idle_choices{60000, 120000, 300000, 600000, 1800000};
constexpr bool system_idle_choice(Time ms) {
  for (const Time choice : system_idle_choices) if (choice == ms) return true;
  return false;
}
// Invariant: both values are allowed choices and the sleep delay is STRICTLY
// greater than the display timeout (the display always idles first).
constexpr bool sleep_settings_valid(Time display_ms, Time system_ms) {
  return display_idle_choice(display_ms) && system_idle_choice(system_ms) && system_ms > display_ms;
}
struct Machine {
  static constexpr Time default_display_idle_ms = 30000, default_system_idle_ms = 300000;
  // Selectable at runtime (see SleepSettings); defaults apply at every boot.
  Time display_idle_ms = default_display_idle_ms, system_idle_ms = default_system_idle_ms;
  State state = State::ACTIVE;
  Wake reason = Wake::BOOT;
  Time last_activity = 0, retry_after = 0;
  std::uint64_t wakes = 0, sleep_requests = 0, refusals = 0;
  explicit Machine(Time now = 0) : last_activity(now) {}
  void activity(Time now) { last_activity = now; }
  void wake(Time now, Wake source);
  // External source does not gate sleep; hardware and owner coordination still do.
  void tick(Time now, bool sleep_safe, External source = External::UNKNOWN);
  void refuse_sleep(Time now);
};
// The two user-selectable delays. A setter that would break the invariant
// (see sleep_settings_valid) is rejected and changes nothing.
struct SleepSettings {
  Time display_ms = Machine::default_display_idle_ms, system_ms = Machine::default_system_idle_ms;
  bool set_display(Time ms) {
    if (!sleep_settings_valid(ms, system_ms)) return false;
    display_ms = ms; return true;
  }
  bool set_system(Time ms) {
    if (!sleep_settings_valid(display_ms, ms)) return false;
    system_ms = ms; return true;
  }
};
static_assert(sleep_settings_valid(Machine::default_display_idle_ms, Machine::default_system_idle_ms));
// LVGL-owner-only gesture latch. First wake contact never becomes a command.
struct TouchGate {
  bool consume_until_release = false;
  bool accept(bool pressed, bool active, bool wake_contact, bool settling) {
    if (wake_contact) consume_until_release = true;
    if (!pressed) { consume_until_release = false; return false; }
    if (!active || settling) consume_until_release = true;
    return active && !consume_until_release;
  }
};
} // namespace power_management
