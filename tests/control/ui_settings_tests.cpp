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

bool timeout_choices_match_power_model() {
  CHECK(timeout_choices.size() == power_management::display_idle_choices.size());
  for (std::size_t i = 0; i < timeout_choices.size(); ++i) {
    CHECK(timeout_choices[i].ms == power_management::display_idle_choices[i]);
    CHECK(power_management::display_idle_choice(timeout_choices[i].ms));
    CHECK(timeout_index(timeout_choices[i].ms) == static_cast<int>(i));
    CHECK(timeout_choices[i].label && timeout_choices[i].label[0]);
  }
  CHECK(timeout_choices[0].ms == 15000 && !std::strcmp(timeout_choices[0].label, "15 s"));
  CHECK(timeout_choices[1].ms == 30000 && !std::strcmp(timeout_choices[1].label, "30 s"));
  CHECK(timeout_choices[2].ms == 60000 && !std::strcmp(timeout_choices[2].label, "1 min"));
  CHECK(timeout_choices[3].ms == 120000 && !std::strcmp(timeout_choices[3].label, "2 min"));
  CHECK(timeout_choices[4].ms == 300000 && !std::strcmp(timeout_choices[4].label, "5 min"));
  CHECK(timeout_index(power_management::Machine::default_display_idle_ms) == 1); // 30 s default
  CHECK(timeout_index(0) == -1 && timeout_index(45000) == -1);
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
                        {"timeout_choices_match_power_model", timeout_choices_match_power_model},
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
