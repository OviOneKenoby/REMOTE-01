#pragma once
// Pure helpers for the settings/info page: no LVGL, no hardware, host-testable.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

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

struct TimeoutChoice {
  unsigned ms;
  const char *label;
};
// Same values as power_management::display_idle_choices.
inline constexpr std::array<TimeoutChoice, 5> timeout_choices{{
    {15000, "15 s"}, {30000, "30 s"}, {60000, "1 min"}, {120000, "2 min"}, {300000, "5 min"}}};

constexpr int timeout_index(unsigned ms) {
  for (std::size_t i = 0; i < timeout_choices.size(); ++i)
    if (timeout_choices[i].ms == ms) return static_cast<int>(i);
  return -1;
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
