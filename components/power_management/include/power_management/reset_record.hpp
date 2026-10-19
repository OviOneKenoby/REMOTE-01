#pragma once
#include <cstdint>
#include <atomic>

namespace power_management::reset_diagnostic {
// Plain words only: no pointers, constructors, heap, flash writes or ISR access.
enum Field : unsigned { MAGIC, VERSION, SEQUENCE, BOOT, RESET, WAKE, RUNTIME,
  STATE, CONDITION, MV, FILTERED_MV, VALID, EVENT, UPTIME_MS, HOLD, DETAIL,
  SLEEP_REQUESTS, SLEEP_ROUNDS, RESERVED, CHECKSUM, WORDS };
enum class Event : unsigned { BOOT, HOLD_BEGIN, HOLD_ASSERTED, STARTUP,
  WORKER_INIT, RUNTIME_READY, INPUT_BEGIN, INPUT_OK, INPUT_FAULT,
  IMU_BEGIN, IMU_OK, IMU_FAULT, ADC_BEGIN, ADC_OK, ADC_FAULT,
  COMMAND, TRANSITION, DISPLAY_BEGIN, DISPLAY_OK, DISPLAY_FAULT,
  RELEASE_BEGIN, RELEASE_OK, RELEASE_REFUSED, SLEEP_BEGIN, SLEEP_END,
  WORKER_FAILED, COUNT };
struct Record { std::uint32_t w[WORDS]; };
struct History { Record copies[4][2]; };
static_assert(sizeof(Record) == 80 && sizeof(History) == 640);
inline std::uint32_t checksum(const Record &r) {
  std::uint32_t h = 2166136261u;
  for (unsigned i = 0; i < CHECKSUM; ++i) h = (h ^ r.w[i]) * 16777619u;
  return h;
}
inline bool valid(const Record &r) {
  return r.w[MAGIC] == 0x52303144u && r.w[VERSION] == 1 &&
    r.w[RUNTIME] <= 1 && r.w[VALID] <= 1 && r.w[HOLD] <= 2 &&
    r.w[STATE] <= 4 && r.w[CONDITION] <= 3 &&
    r.w[EVENT] < unsigned(Event::COUNT) && r.w[CHECKSUM] == checksum(r);
}
inline bool newer(std::uint32_t a, std::uint32_t b) {
  return a != b && std::uint32_t(a - b) < 0x80000000u;
}
inline int latest(const History &h) {
  int best = -1;
  for (unsigned i = 0; i < 8; ++i) {
    const auto &r = h.copies[i / 2][i % 2];
    if (valid(r) && (best < 0 || newer(r.w[SEQUENCE], h.copies[best / 2][best % 2].w[SEQUENCE]))) best = int(i);
  }
  return best;
}
inline int boot_record(const History &h, std::uint32_t boot) {
  int best = -1;
  for (unsigned i = 0; i < 8; ++i) {
    const auto &r = h.copies[i / 2][i % 2];
    if (valid(r) && r.w[BOOT] == boot && (best < 0 || newer(r.w[SEQUENCE], h.copies[best / 2][best % 2].w[SEQUENCE]))) best = int(i);
  }
  return best;
}
// Alternate copies preserve the preceding checkpoint if reset interrupts commit.
inline void save(History &h, Record r) {
  const int previous = boot_record(h, r.w[BOOT]);
  const int last = latest(h);
  r.w[MAGIC] = 0x52303144u; r.w[VERSION] = 1;
  r.w[SEQUENCE] = last < 0 ? 1 : h.copies[last / 2][last % 2].w[SEQUENCE] + 1;
  r.w[CHECKSUM] = checksum(r);
  volatile auto &out = h.copies[r.w[BOOT] % 4][previous < 0 ? 0 : 1 - previous % 2];
  out.w[MAGIC] = 0;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  for (unsigned i = 1; i < WORDS; ++i) out.w[i] = r.w[i];
  std::atomic_thread_fence(std::memory_order_seq_cst);
  out.w[MAGIC] = r.w[MAGIC];
}
inline Record begin(History &h, unsigned reason, unsigned wake) {
  Record r{};
  const int last = latest(h);
  r.w[BOOT] = last < 0 ? 1 : h.copies[last / 2][last % 2].w[BOOT] + 1;
  r.w[RESET] = reason; r.w[WAKE] = wake; r.w[STATE] = 4; r.w[HOLD] = 2;
  r.w[EVENT] = unsigned(Event::BOOT);
  save(h, r); return r;
}
} // namespace power_management::reset_diagnostic
