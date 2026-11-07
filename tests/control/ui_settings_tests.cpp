#include "settings_model.hpp"
#include "power_management/model.hpp"
#include "rtcp_client/diagnostic.hpp"
#include <cstdio>
#include <cstring>

namespace {
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)
using namespace ui_settings;

bool brightness_scale() {
  static_assert(brightness_boot == 128 && brightness_max == 255);
  CHECK(brightness_percent(brightness_boot) == 50);
  CHECK(brightness_percent(0) == 0 && brightness_percent(255) == 100);
  CHECK(brightness_percent(127) == 50 && brightness_percent(129) == 51);
  // The duty is the slider complement, as in the existing slider callback.
  CHECK(brightness_pwm(255) == 0 && brightness_pwm(0) == 255);
  CHECK(brightness_pwm(brightness_boot) == 127);
  unsigned last = 0;
  for (unsigned v = 0; v <= 255; ++v) {
    const auto p = brightness_percent(static_cast<std::uint8_t>(v));
    CHECK(p >= last && p <= 100);
    last = p;
  }
  return true;
}

bool value_lists() {
  CHECK(value_count(Row::DISPLAY) == 5 && value_count(Row::SYSTEM) == 5);
  const unsigned display[] = {15000, 30000, 60000, 120000, 300000};
  const unsigned system[] = {60000, 120000, 300000, 600000, 1800000};
  for (unsigned i = 0; i < 5; ++i) {
    CHECK(value_at(Row::DISPLAY, i) == display[i] && value_index(Row::DISPLAY, display[i]) == int(i));
    CHECK(value_at(Row::SYSTEM, i) == system[i] && value_index(Row::SYSTEM, system[i]) == int(i));
    CHECK(power_management::display_idle_choice(display[i]) && power_management::system_idle_choice(system[i]));
  }
  CHECK(value_index(Row::DISPLAY, 0) == -1 && value_index(Row::DISPLAY, 45000) == -1);
  CHECK(value_index(Row::SYSTEM, 30000) == -1 && value_index(Row::SYSTEM, 3600000) == -1);
  // Defaults sit on both lists: 30 s / 5 min.
  CHECK(value_index(Row::DISPLAY, power_management::Machine::default_display_idle_ms) == 1);
  CHECK(value_index(Row::SYSTEM, power_management::Machine::default_system_idle_ms) == 2);
  char b[16];
  const struct { unsigned ms; const char *text; } labels[] = {
      {15000, "15 s"}, {30000, "30 s"}, {60000, "1 min"}, {120000, "2 min"}, {300000, "5 min"},
      {600000, "10 min"}, {1800000, "30 min"}};
  for (const auto &l : labels) { format_duration(b, sizeof b, l.ms); CHECK(!std::strcmp(b, l.text)); CHECK(std::strlen(b) <= 6); }
  format_duration(b, 3, 1800000); CHECK(std::strlen(b) == 2);
  format_duration(nullptr, 0, 1); format_duration(b, 0, 1);
  return true;
}

bool steps_and_disabled_logic() {
  using power_management::sleep_settings_valid;
  // Default pair 30 s / 5 min.
  CHECK(step_value(Row::DISPLAY, 30000, 300000, -1) == 15000);
  CHECK(step_value(Row::DISPLAY, 30000, 300000, +1) == 60000);
  CHECK(step_value(Row::DISPLAY, 120000, 300000, +1) == 0);   // 5 min screen-off: not below 5 min sleep
  CHECK(step_value(Row::SYSTEM, 30000, 300000, +1) == 600000);
  CHECK(step_value(Row::SYSTEM, 30000, 300000, -1) == 120000);
  // List ends clamp.
  CHECK(step_value(Row::DISPLAY, 15000, 300000, -1) == 0);
  CHECK(step_value(Row::SYSTEM, 30000, 1800000, +1) == 0);
  // Needing a longer sleep first: with the default 5 min sleep a 5 min screen-off is unreachable...
  CHECK(step_value(Row::DISPLAY, 120000, 300000, +1) == 0);
  // ...and reachable after raising the sleep delay.
  CHECK(step_value(Row::SYSTEM, 120000, 300000, +1) == 600000);
  CHECK(step_value(Row::DISPLAY, 120000, 600000, +1) == 300000);
  // Lowering the sleep delay is blocked at the display timeout.
  CHECK(step_value(Row::SYSTEM, 120000, 300000, -1) == 0);    // 2 min sleep == 2 min screen-off
  CHECK(step_value(Row::SYSTEM, 60000, 300000, -1) == 120000);
  CHECK(step_value(Row::SYSTEM, 60000, 120000, -1) == 0);     // 1 min sleep == 1 min screen-off
  // Exhaustive: every enabled step gives a valid pair from the lists; every disabled
  // step is disabled only by a list end or the invariant; a valid pair never leaves.
  for (unsigned di = 0; di < 5; ++di)
    for (unsigned si = 0; si < 5; ++si) {
      const unsigned d = value_at(Row::DISPLAY, di), s = value_at(Row::SYSTEM, si);
      if (!sleep_settings_valid(d, s)) continue;
      for (int dir : {-1, +1}) {
        const unsigned nd = step_value(Row::DISPLAY, d, s, dir), ns = step_value(Row::SYSTEM, d, s, dir);
        const int ndi = int(di) + dir, nsi = int(si) + dir;
        const bool d_in_list = ndi >= 0 && ndi < 5, s_in_list = nsi >= 0 && nsi < 5;
        CHECK((nd != 0) == (d_in_list && sleep_settings_valid(value_at(Row::DISPLAY, ndi < 0 ? 0 : ndi > 4 ? 4 : ndi), s)));
        CHECK((ns != 0) == (s_in_list && sleep_settings_valid(d, value_at(Row::SYSTEM, nsi < 0 ? 0 : nsi > 4 ? 4 : nsi))));
        if (nd) CHECK(sleep_settings_valid(nd, s));
        if (ns) CHECK(sleep_settings_valid(d, ns));
      }
    }
  // Off-list or invalid current values give no step.
  CHECK(step_value(Row::DISPLAY, 45000, 300000, +1) == 0 && step_value(Row::SYSTEM, 30000, 45000, +1) == 0);
  return true;
}

bool close_reason_names_cover_the_enum() {
  using rtcp_client::CloseReason;
  CHECK(static_cast<unsigned>(CloseReason::queue_failed) == 14);
  CHECK(!std::strcmp(close_reason_text(static_cast<std::uint8_t>(CloseReason::none)), "none"));
  for (unsigned r = 0; r <= static_cast<unsigned>(CloseReason::queue_failed); ++r) {
    const char *text = close_reason_text(static_cast<std::uint8_t>(r));
    CHECK(text && text[0] && std::strcmp(text, "unknown") != 0);
    CHECK(std::strlen(text) <= 16); // fits one 148 px line at 14 px
  }
  CHECK(!std::strcmp(close_reason_text(15), "unknown") && !std::strcmp(close_reason_text(255), "unknown"));
  CHECK(!std::strcmp(close_reason_text(static_cast<std::uint8_t>(CloseReason::peer_eof)), "closed by TUNER"));
  return true;
}

bool uptime_format() {
  char b[24];
  format_uptime(b, sizeof b, 0); CHECK(!std::strcmp(b, "00:00:00"));
  format_uptime(b, sizeof b, 999); CHECK(!std::strcmp(b, "00:00:00"));
  format_uptime(b, sizeof b, 1000); CHECK(!std::strcmp(b, "00:00:01"));
  format_uptime(b, sizeof b, 249324); CHECK(!std::strcmp(b, "00:04:09"));
  format_uptime(b, sizeof b, 86399000ULL); CHECK(!std::strcmp(b, "23:59:59"));
  format_uptime(b, sizeof b, 86400000ULL); CHECK(!std::strcmp(b, "1d 00:00:00"));
  format_uptime(b, sizeof b, 90061000ULL); CHECK(!std::strcmp(b, "1d 01:01:01"));
  format_uptime(b, sizeof b, UINT64_MAX); CHECK(std::strlen(b) <= 15 && !std::strncmp(b, "9999d", 5));
  format_uptime(b, 4, 90061000ULL); CHECK(std::strlen(b) == 3); // truncates, always terminated
  format_uptime(nullptr, 0, 1); format_uptime(b, 0, 1);
  return true;
}
}  // namespace

int main() {
  struct Test { const char *name; bool (*run)(); };
  const Test tests[] = {{"brightness_scale", brightness_scale},
                        {"value_lists", value_lists},
                        {"steps_and_disabled_logic", steps_and_disabled_logic},
                        {"close_reason_names_cover_the_enum", close_reason_names_cover_the_enum},
                        {"uptime_format", uptime_format}};
  unsigned pass = 0, fail = 0;
  for (const auto &t : tests) {
    const bool ok = t.run();
    std::printf("%s UI_SETTINGS %s\n", ok ? "PASS" : "FAIL", t.name);
    ok ? ++pass : ++fail;
  }
  std::printf("UI SETTINGS SUMMARY PASS=%u FAIL=%u assertions=%u\n", pass, fail, assertions);
  return fail ? 1 : 0;
}
