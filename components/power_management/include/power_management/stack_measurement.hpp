#pragma once
#include <array>
#include <cstdint>
#include <limits>
// Temporary DEVELOPMENT_ONLY experiment; compile with =0 to remove all probes,
// storage and S/T commands. This is independent of existing development commands.
#ifndef POWER_STACK_MEASUREMENT
#define POWER_STACK_MEASUREMENT 1
#endif
namespace power_management::stack_measurement {
enum class Path : unsigned { PROBE, STARTUP, NORMAL, ADC, INPUT, SNAPSHOT,
  RESET_COPY, PUBLISH, B, DISPLAY, SLEEP, RELEASE, COMMAND, REPORT, ERROR, COUNT };
constexpr bool key(unsigned char c) { return POWER_STACK_MEASUREMENT && (c == 'S' || c == 'T'); }
struct Pair { std::uint32_t begin=0, end=0, before=0, after=0; };
struct Record { Pair last{}, lowest{}; std::uint32_t count=0, drops=0; };
struct Drop { Pair pair{}; Path path=Path::PROBE; };
// POWER is the sole caller. No lock, allocation, RTC persistence or ISR use.
struct State {
  std::array<Record, unsigned(Path::COUNT)> records{};
  std::array<Drop, 16> history{};
  std::uint32_t sequence=0, epoch=0, history_count=0, overwritten=0, invalid=0;
  std::uint32_t baseline=0;
  bool saturated=false;
  static void increment(std::uint32_t &n) { if(n!=UINT32_MAX) ++n; }
  std::uint32_t next() {
    if(sequence==UINT32_MAX) { saturated=true; return sequence; }
    return ++sequence;
  }
  void clear(std::uint32_t watermark) {
    auto next_epoch=epoch; increment(next_epoch);
    records={}; history={}; sequence=history_count=overwritten=invalid=0;
    epoch=next_epoch; baseline=watermark; saturated=false;
  }
  void begin(Path p, std::uint32_t watermark) {
    auto &r=records[unsigned(p)];
    if(!epoch) { epoch=1; baseline=watermark; }
    if(r.last.begin && !r.last.end) increment(invalid);
    r.last={next(),0,watermark,0};
  }
  void end(Path p, std::uint32_t watermark) {
    auto &r=records[unsigned(p)];
    if(!r.last.begin || r.last.end) { increment(invalid); return; }
    r.last.end=next(); r.last.after=watermark; increment(r.count);
    if(watermark>r.last.before) increment(invalid); // historical HWM cannot rise
    if(!r.lowest.end || watermark<r.lowest.after) r.lowest=r.last;
    if(watermark<r.last.before) {
      increment(r.drops);
      history[history_count%history.size()]={r.last,p};
      if(history_count>=history.size()) increment(overwritten);
      increment(history_count);
    }
  }
};
static_assert(sizeof(State)<=1024);
} // namespace power_management::stack_measurement
