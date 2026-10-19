#pragma once
#include "control_home/api.hpp"
#include "physical_input/model.hpp"
#include <cstdint>

namespace remote_encoder {
struct View {
  std::uint64_t transitions = 0, detents = 0;
  std::int8_t last_delta = 0;
  bool operator==(const View &) const = default;
};
class Router {
  static constexpr std::uintptr_t owner = 1;
  physical_input::Model model_{owner, 1};
  bool initialized_ = false;
  std::uint8_t last_ab_ = 0;
  View view_{};

public:
  template <typename Emit>
  void sample(std::uint64_t time_ms, bool a, bool b, bool home, Emit emit) {
    const physical_input::Sample sample{
        time_ms, 0, static_cast<std::uint8_t>((a ? 2 : 0) | (b ? 1 : 0))};
    if (!initialized_) {
      initialized_ = model_.baseline(owner, sample) ==
                     physical_input::Result::OK;
      last_ab_ = sample.ab;
      return; // Initial levels never produce an unsolicited command.
    }
    if (sample.ab != last_ab_) {
      last_ab_ = sample.ab;
      if (view_.transitions != UINT64_MAX) ++view_.transitions;
    }
    if (model_.feed(owner, sample) != physical_input::Result::OK)
      return;
    physical_input::Event event;
    bool present = false;
    while (model_.pop(owner, event, present) == physical_input::Result::OK &&
           present) {
      if (event.kind == physical_input::Kind::ENCODER_CYCLE) {
        if (view_.detents != UINT64_MAX) ++view_.detents;
        view_.last_delta = event.delta;
        if (home)
        (void)emit(event.delta > 0 ? control_home::Action::VOLUME_UP
                                   : control_home::Action::VOLUME_DOWN);
      }
    }
  }
  View view() const { return view_; }
};
} // namespace remote_encoder
