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
struct Machine {
  static constexpr Time display_idle_ms = 30000, system_idle_ms = 300000;
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
