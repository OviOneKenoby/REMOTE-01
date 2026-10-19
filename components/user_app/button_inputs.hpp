#pragma once
#include "physical_input/model.hpp"
#include <cstdint>

namespace remote_buttons {
enum class Id : std::uint8_t {
  ENCODER_KEY = 0,
  BUTTON_1 = 1,
  BUTTON_2 = 2,
  BUTTON_3 = 3,
};

class Router {
  static constexpr std::uintptr_t owner = 2;
  static constexpr std::uint32_t debounce_ms = 20;
  physical_input::Model model_{owner, debounce_ms};
  bool initialized_ = false;

public:
  template <typename Emit>
  void sample(std::uint64_t time_ms, std::uint8_t pressed, Emit emit) {
    const physical_input::Sample sample{time_ms, pressed, 0};
    if (!initialized_) {
      initialized_ = model_.baseline(owner, sample) == physical_input::Result::OK;
      return; // Initial or boot-held levels never create an event.
    }
    const auto result = model_.feed(owner, sample);
    if (result != physical_input::Result::OK) {
      // Fail closed and re-establish levels. Never replay uncertain input.
      initialized_ = model_.baseline(owner, sample) == physical_input::Result::OK;
      return;
    }
    physical_input::Event event{};
    bool present = false;
    while (model_.pop(owner, event, present) == physical_input::Result::OK && present) {
      if (event.kind == physical_input::Kind::PRESS && event.button < 4)
        emit(static_cast<Id>(event.button));
    }
  }
};

static_assert(sizeof(Router) <= 1024, "button input router must remain bounded");
} // namespace remote_buttons
