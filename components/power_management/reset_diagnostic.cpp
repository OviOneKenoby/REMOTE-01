#include "power_management/reset_diagnostic.hpp"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_log.h"

namespace power_management {
namespace {
using namespace reset_diagnostic;
RTC_NOINIT_ATTR History history;
Record current{};
const char *reason_name(unsigned reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    case ESP_RST_PWR_GLITCH: return "POWER_GLITCH";
    case ESP_RST_CPU_LOCKUP: return "CPU_LOCKUP";
    default: return "OTHER_OR_UNKNOWN";
  }
}
const char *event_name(unsigned e) {
  static const char *const names[]{"BOOT", "HOLD_BEGIN", "HOLD_ASSERTED", "STARTUP",
    "WORKER_INIT", "RUNTIME_READY", "INPUT_BEGIN", "INPUT_OK", "INPUT_FAULT",
    "IMU_BEGIN", "IMU_OK", "IMU_FAULT", "ADC_BEGIN", "ADC_OK", "ADC_FAULT",
    "COMMAND", "TRANSITION", "DISPLAY_BEGIN", "DISPLAY_OK", "DISPLAY_FAULT",
    "RELEASE_BEGIN", "RELEASE_OK", "RELEASE_REFUSED", "SLEEP_BEGIN", "SLEEP_END", "WORKER_FAILED"};
  static_assert(sizeof(names) / sizeof(names[0]) == unsigned(Event::COUNT));
  return e < unsigned(Event::COUNT) ? names[e] : "INVALID";
}
}
void reset_boot() {
  current = reset_diagnostic::begin(history, unsigned(esp_reset_reason()), unsigned(esp_sleep_get_wakeup_cause()));
}
void reset_event(reset_diagnostic::Event e, unsigned detail) {
  using namespace reset_diagnostic;
  current.w[EVENT] = unsigned(e); current.w[DETAIL] = detail;
  current.w[UPTIME_MS] = unsigned(esp_timer_get_time() / 1000);
  if (e == Event::HOLD_ASSERTED) current.w[HOLD] = 1;
  if (e == Event::RUNTIME_READY) current.w[RUNTIME] = 1;
  if (e == Event::SLEEP_END && detail == 0) current.w[WAKE] = unsigned(esp_sleep_get_wakeup_cause());
  save(history, current);
}
void reset_snapshot(const Machine &m, const Battery &b, bool hold, unsigned long long rounds) {
  using namespace reset_diagnostic;
  current.w[STATE] = unsigned(m.state); current.w[CONDITION] = unsigned(b.condition);
  current.w[MV] = b.voltage_mv; current.w[FILTERED_MV] = b.filtered_mv;
  current.w[VALID] = b.valid; current.w[HOLD] = hold;
  current.w[SLEEP_REQUESTS] = unsigned(m.sleep_requests); current.w[SLEEP_ROUNDS] = unsigned(rounds);
}
void reset_report() {
  using namespace reset_diagnostic;
  ESP_LOGI("POWER_RESET", "DEVELOPMENT_ONLY rtc_history=4 retention=BEST_EFFORT boot_count_epoch=RTC wake=SDK_NUMERIC state=0_ACTIVE_1_IDLE_2_SLEEP_3_SHUTDOWN_4_NOT_READY condition=0_UNKNOWN_1_NORMAL_2_LOW_3_CRITICAL");
  for (unsigned age = 0; age < 4; ++age) {
    const auto boot = current.w[BOOT] - age;
    const int i = boot_record(history, boot);
    if (i < 0) { ESP_LOGI("POWER_RESET", "age=%u retained=UNAVAILABLE", age); continue; }
    const auto &r = history.copies[i / 2][i % 2];
    ESP_LOGI("POWER_RESET", "age=%u boot=%lu reset=%lu/%s brownout=%u wake=%lu runtime=%lu state=%lu battery=%lu valid=%lu mv=%lu filtered_mv=%lu hold=%lu event=%s detail=%lu uptime_ms=%lu sleep_requests=%lu rounds=%lu", age,
      static_cast<unsigned long>(r.w[BOOT]), static_cast<unsigned long>(r.w[RESET]), reason_name(r.w[RESET]), r.w[RESET] == ESP_RST_BROWNOUT,
      static_cast<unsigned long>(r.w[WAKE]), static_cast<unsigned long>(r.w[RUNTIME]), static_cast<unsigned long>(r.w[STATE]), static_cast<unsigned long>(r.w[CONDITION]),
      static_cast<unsigned long>(r.w[VALID]), static_cast<unsigned long>(r.w[MV]), static_cast<unsigned long>(r.w[FILTERED_MV]), static_cast<unsigned long>(r.w[HOLD]), event_name(r.w[EVENT]),
      static_cast<unsigned long>(r.w[DETAIL]), static_cast<unsigned long>(r.w[UPTIME_MS]), static_cast<unsigned long>(r.w[SLEEP_REQUESTS]), static_cast<unsigned long>(r.w[SLEEP_ROUNDS]));
  }
}
ResetSummary reset_history_copy() {
  ResetSummary out{};
  for (unsigned age = 0; age < 4; ++age) {
    const int i = reset_diagnostic::boot_record(history, current.w[reset_diagnostic::BOOT] - age);
    if (i >= 0) out.rows[age] = sanitize_reset_record(history.copies[i / 2][i % 2]);
  }
  return out;
}
} // namespace power_management
