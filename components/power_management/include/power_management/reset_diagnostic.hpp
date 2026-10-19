#pragma once
#include "reset_record.hpp"
#include "model.hpp"
namespace power_management {
// app_main owns until start(); POWER is the only writer/reporter thereafter.
void reset_boot();
void reset_event(reset_diagnostic::Event, unsigned detail = 0);
void reset_snapshot(const Machine &, const Battery &, bool hold, unsigned long long rounds);
void reset_report(); // boot and existing B command only; bounded four-boot history
// Copied public fields only. POWER calls the accessor after worker startup.
struct ResetRow { bool available = false; std::uint32_t values[15]{}; };
struct ResetSummary { ResetRow rows[4]{}; };
inline ResetRow sanitize_reset_record(reset_diagnostic::Record r) {
  ResetRow out{};
  if (!reset_diagnostic::valid(r)) return out;
  constexpr unsigned fields[]{reset_diagnostic::BOOT, reset_diagnostic::RESET,
    reset_diagnostic::WAKE, reset_diagnostic::RUNTIME, reset_diagnostic::STATE,
    reset_diagnostic::CONDITION, reset_diagnostic::VALID, reset_diagnostic::MV,
    reset_diagnostic::FILTERED_MV, reset_diagnostic::HOLD, reset_diagnostic::EVENT,
    reset_diagnostic::DETAIL, reset_diagnostic::UPTIME_MS,
    reset_diagnostic::SLEEP_REQUESTS, reset_diagnostic::SLEEP_ROUNDS};
  out.available = true;
  for (unsigned i = 0; i < 15; ++i) out.values[i] = r.w[fields[i]];
  return out;
}
ResetSummary reset_history_copy(); // No pointers/references to internal RTC storage.
}
