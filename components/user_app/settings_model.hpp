#pragma once
// Pure helpers for the settings/info page: no LVGL, no hardware, host-testable.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "power_management/model.hpp"

namespace ui_settings {

// Backlight: the brightness slider covers 0..255 and the LEDC duty written is
// 0xff - slider (components/user_app/user_app.cpp, lvgl_obj_event_callback).
constexpr std::uint8_t brightness_max = 255;
// 50 % of the slider scale, used at every boot (not stored).
constexpr std::uint8_t brightness_boot = 128;
constexpr std::uint16_t brightness_pwm(std::uint8_t slider) {
  return static_cast<std::uint16_t>(0xff - slider);
}
constexpr unsigned brightness_percent(std::uint8_t slider) {
  return (static_cast<unsigned>(slider) * 100U + 127U) / 255U;
}

// The two delay rows share the value lists of the power model (single source).
enum class Row { DISPLAY, SYSTEM };

inline std::size_t value_count(Row row) {
  return row == Row::DISPLAY ? power_management::display_idle_choices.size()
                             : power_management::system_idle_choices.size();
}
inline unsigned value_at(Row row, std::size_t index) {
  return static_cast<unsigned>(row == Row::DISPLAY ? power_management::display_idle_choices[index]
                                                   : power_management::system_idle_choices[index]);
}
// Position of ms in the row's list, or -1.
inline int value_index(Row row, unsigned ms) {
  for (std::size_t i = 0; i < value_count(row); ++i)
    if (value_at(row, i) == ms) return static_cast<int>(i);
  return -1;
}
// Value after one step (dir = +1 / -1) from the current pair, or 0 when the step
// is not available: the end of the list, or the display timeout / sleep delay
// invariant (sleep strictly greater) would be broken. The other row is never
// adjusted; an unavailable step is shown disabled instead.
inline unsigned step_value(Row row, unsigned display_ms, unsigned system_ms, int dir) {
  const int index = value_index(row, row == Row::DISPLAY ? display_ms : system_ms);
  const int next = index + (dir > 0 ? 1 : -1);
  if (index < 0 || next < 0 || next >= static_cast<int>(value_count(row))) return 0;
  const unsigned value = value_at(row, static_cast<std::size_t>(next));
  const bool ok = row == Row::DISPLAY ? power_management::sleep_settings_valid(value, system_ms)
                                      : power_management::sleep_settings_valid(display_ms, value);
  return ok ? value : 0;
}
// "15 s", "30 s", "1 min", ... "30 min".
inline void format_duration(char *out, std::size_t n, unsigned ms) {
  if (!out || !n) return;
  if (ms < 60000) std::snprintf(out, n, "%u s", ms / 1000);
  else std::snprintf(out, n, "%u min", ms / 60000);
}

// Text for rtcp_client::CloseReason (numeric value as carried by
// control_home::FullControlSnapshot::rtcp_close_reason).
inline const char *close_reason_text(std::uint8_t reason) {
  static constexpr const char *names[] = {
      "none",           "suspended",     "Wi-Fi lost",      "connect failed",
      "connect timeout", "no response",  "closed by TUNER", "short read",
      "invalid frame",  "auth failed",   "send failed",     "receive failed",
      "crypto failed",  "unexpected data", "queue failed"};
  return reason < sizeof names / sizeof names[0] ? names[reason] : "unknown";
}

// "HH:MM:SS", or "Dd HH:MM:SS" from one day on. n >= 16 always fits.
inline void format_uptime(char *out, std::size_t n, std::uint64_t ms) {
  if (!out || !n) return;
  const std::uint64_t total = ms / 1000;
  const unsigned days = static_cast<unsigned>(total / 86400 > 9999 ? 9999 : total / 86400);
  const unsigned h = static_cast<unsigned>(total % 86400 / 3600);
  const unsigned m = static_cast<unsigned>(total % 3600 / 60);
  const unsigned s = static_cast<unsigned>(total % 60);
  if (days) std::snprintf(out, n, "%ud %02u:%02u:%02u", days, h, m, s);
  else std::snprintf(out, n, "%02u:%02u:%02u", h, m, s);
}

}  // namespace ui_settings
