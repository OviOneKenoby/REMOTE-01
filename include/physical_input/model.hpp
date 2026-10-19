#pragma once
#include <array>
#include <cstdint>
#include <limits>

// One serialized task owner supplies normalized samples. Never call this
// model from an ISR.
namespace physical_input {
enum class Kind : std::uint8_t { PRESS, RELEASE, ENCODER_CYCLE };
enum class Result : std::uint8_t { OK, NOT_OWNER, NOT_READY, BAD_SAMPLE, LOSS, LIMIT, CONFIG };
struct Sample { std::uint64_t time_ms = 0; std::uint8_t pressed = 0, ab = 0; };
struct Event {
  std::uint64_t epoch = 0, sequence = 0, time_ms = 0;
  Kind kind = Kind::PRESS;
  std::uint8_t button = 0;
  std::int8_t delta = 0; // Complete logical AB cycle, NOT physical direction/detent.
};
constexpr bool increment(std::uint64_t &value) {
  if (value == UINT64_MAX) return false;
  ++value; return true;
}
struct Stats {
  std::uint64_t emitted = 0, discarded = 0, malformed = 0, encoder_invalid = 0;
  bool saturated = false;
};
struct View { Stats stats{}; std::uint64_t epoch = 0; std::uint8_t pressed = 0; bool ready = false, loss = false; unsigned queued = 0; };
class Model {
  struct Button { std::uint64_t since = 0; bool stable = false, candidate = false; };
  const std::uintptr_t owner_;
  const std::uint32_t debounce_ms_;
  std::array<Button, 4> buttons_{};
  std::array<Event, 8> queue_{};
  Stats stats_{};
  std::uint64_t time_ = 0, epoch_ = 0, sequence_ = 0;
  unsigned head_ = 0, count_ = 0;
  std::uint8_t ab_ = 0;
  std::int8_t quarter_ = 0;
  bool ready_ = false, loss_ = false;
  void count(std::uint64_t &value) { if (!increment(value)) stats_.saturated = true; }
  void discard() { while (count_) { count(stats_.discarded); --count_; } head_ = 0; }
  bool emit(Kind kind, std::uint8_t id, std::int8_t delta) {
    if (count_ == queue_.size() || !increment(sequence_)) {
      count(stats_.discarded); discard(); loss_ = true; return false;
    }
    queue_[(head_ + count_++) % queue_.size()] = {epoch_,sequence_,time_,kind,id,delta};
    count(stats_.emitted); return true;
  }
  static bool valid(Sample s) { return !(s.pressed & ~15u) && s.ab < 4; }
  bool configured() const { return owner_ && debounce_ms_; }
public:
  static constexpr unsigned button_count = 4, capacity = 8;
  Model(std::uintptr_t owner, std::uint32_t debounce_ms) : owner_(owner), debounce_ms_(debounce_ms) {}
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  // New epoch discards queued input and establishes levels without actions.
  // Used on initial capture/wake/loss, never to replay an uncertain press.
  Result baseline(std::uintptr_t owner, Sample sample) {
    if (owner != owner_) return Result::NOT_OWNER;
    if (!configured()) return Result::CONFIG;
    if (!valid(sample)) return Result::BAD_SAMPLE;
    if (!increment(epoch_)) { discard(); loss_ = true; return Result::LIMIT; }
    discard(); sequence_ = 0; time_ = sample.time_ms; ab_ = sample.ab; quarter_ = 0;
    for (unsigned i = 0; i < buttons_.size(); ++i) {
      const bool down = sample.pressed & (1u << i);
      buttons_[i] = {time_,down,down};
    }
    ready_ = true; loss_ = false; return Result::OK;
  }
  Result feed(std::uintptr_t owner, Sample sample) {
    if (owner != owner_) return Result::NOT_OWNER;
    if (!configured()) return Result::CONFIG;
    if (!ready_) return Result::NOT_READY;
    if (loss_) return Result::LOSS;
    if (!valid(sample) || sample.time_ms < time_) { count(stats_.malformed); return Result::BAD_SAMPLE; }
    time_ = sample.time_ms;
    // Same-timestamp ties are explicitly button-ID order, then encoder.
    for (unsigned i = 0; i < buttons_.size(); ++i) {
      auto &b = buttons_[i]; const bool down = sample.pressed & (1u << i);
      if (down != b.candidate) { b.candidate = down; b.since = time_; }
      if (b.candidate != b.stable && time_ - b.since >= debounce_ms_) {
        b.stable = b.candidate;
        if (!emit(down ? Kind::PRESS : Kind::RELEASE,std::uint8_t(i),0)) return Result::LOSS;
      }
    }
    if (sample.ab != ab_) {
      if ((sample.ab ^ ab_) == 3) { quarter_ = 0; count(stats_.encoder_invalid); }
      else {
        constexpr std::int8_t steps[16] = {0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0};
        quarter_ += steps[ab_ * 4 + sample.ab];
        if (quarter_ == 4 || quarter_ == -4) {
          const auto delta = std::int8_t(quarter_ / 4); quarter_ = 0; ab_ = sample.ab;
          if (!emit(Kind::ENCODER_CYCLE,0,delta)) return Result::LOSS;
        }
      }
      ab_ = sample.ab;
    }
    return Result::OK;
  }
  Result pop(std::uintptr_t owner, Event &out, bool &present) {
    present = false;
    if (owner != owner_) return Result::NOT_OWNER;
    if (!configured()) return Result::CONFIG;
    if (loss_) return Result::LOSS;
    if (!ready_) return Result::NOT_READY;
    if (count_) { out = queue_[head_]; queue_[head_] = {}; head_ = (head_ + 1) % capacity; --count_; present = true; }
    return Result::OK;
  }
  Result view(std::uintptr_t owner, View &out) const {
    if (owner != owner_) return Result::NOT_OWNER;
    std::uint8_t down = 0;
    for (unsigned i = 0; i < buttons_.size(); ++i) if (buttons_[i].stable) down |= 1u << i;
    out = {stats_,epoch_,down,ready_,loss_,count_}; return Result::OK;
  }
};
static_assert(sizeof(Model) <= 1024, "physical input preparation must remain bounded");
}
