#pragma once
#include "control_home/api.hpp"
#include "physical_input/model.hpp"
#include <cstdint>

namespace remote_encoder {
struct View {
  std::uint64_t transitions = 0, detents = 0;
  std::uint64_t invalid = 0, positive = 0, negative = 0, rejected = 0;
  std::uint32_t sample_last_ms = 0, sample_max_ms = 0;
  std::int8_t last_delta = 0;
  bool operator==(const View &) const = default;
};
class Router {
  static constexpr std::uintptr_t owner = 1;
  physical_input::Model model_{owner, 1};
  bool initialized_ = false;
  std::uint8_t last_ab_ = 0;
  std::uint64_t last_sample_ms_ = 0;
  View view_{};

  static void increment(std::uint64_t &value) {
    if (value != UINT64_MAX) ++value;
  }

public:
  template <typename Emit, typename Activity>
  void sample(std::uint64_t time_ms, bool a, bool b, bool home, Emit emit,
              Activity activity) {
    const physical_input::Sample sample{
        time_ms, 0, static_cast<std::uint8_t>((a ? 2 : 0) | (b ? 1 : 0))};
    if (!initialized_) {
      initialized_ = model_.baseline(owner, sample) ==
                     physical_input::Result::OK;
      last_ab_ = sample.ab;
      last_sample_ms_ = time_ms;
      return; // Initial levels never produce an unsolicited command.
    }
    const auto interval = time_ms >= last_sample_ms_ ? time_ms - last_sample_ms_ : 0;
    view_.sample_last_ms = interval > UINT32_MAX ? UINT32_MAX
                                                 : std::uint32_t(interval);
    if (view_.sample_max_ms < view_.sample_last_ms)
      view_.sample_max_ms = view_.sample_last_ms;
    last_sample_ms_ = time_ms;
    if (sample.ab != last_ab_) {
      last_ab_ = sample.ab;
      increment(view_.transitions);
    }
    if (model_.feed(owner, sample) != physical_input::Result::OK)
      return;
    physical_input::View input{};
    if (model_.view(owner, input) == physical_input::Result::OK)
      view_.invalid = input.stats.encoder_invalid;
    physical_input::Event event;
    bool present = false;
    while (model_.pop(owner, event, present) == physical_input::Result::OK &&
           present) {
      if (event.kind == physical_input::Kind::ENCODER_CYCLE) {
        increment(view_.detents);
        view_.last_delta = event.delta;
        if (event.delta > 0) increment(view_.positive);
        else increment(view_.negative);
        activity();
        if (home && !emit(event.delta > 0 ? control_home::Action::VOLUME_UP
                                         : control_home::Action::VOLUME_DOWN))
          increment(view_.rejected);
      }
    }
  }
  template <typename Emit>
  void sample(std::uint64_t time_ms, bool a, bool b, bool home, Emit emit) {
    sample(time_ms, a, b, home, emit, [] {});
  }
  View view() const { return view_; }
};
} // namespace remote_encoder
