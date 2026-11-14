#pragma once
// Factory-reset flow: long hold -> confirmation modal -> arm + confirm the
// existing Wi-Fi settings reset -> restart. Pure logic: no LVGL, no hardware.
// The storage side is the existing network_manager settings API (recovery.hpp):
// ARM_WIFI_RESET publishes a one-time ticket, CONFIRM_WIFI_RESET with that
// ticket erases the Wi-Fi credentials. Nothing else is erased.
#include <cstdint>

namespace factory_reset {

constexpr std::uint32_t hold_ms = 2000;           // button must be held this long
constexpr std::uint32_t modal_timeout_ms = 10000; // modal cancels itself
constexpr std::uint32_t arm_wait_ms = 3000;       // wait for the ticket
constexpr std::uint32_t confirm_wait_ms = 5000;   // wait for the erase result
constexpr std::uint32_t restart_show_ms = 1200;   // "Restarting..." is shown this long
constexpr std::uint32_t error_show_ms = 4000;     // error text stays this long

enum class State : std::uint8_t { IDLE, HOLDING, MODAL, ARMING, CONFIRMING, RESTARTING, ERROR };
enum class Action : std::uint8_t { NONE, REQUEST_ARM, REQUEST_CONFIRM, REQUEST_RESTART };

// Mirror of network_manager::SettingsResult (same order; checked by the host test).
enum class Result : unsigned { NONE, ACCEPTED, ARMED, REFUSED, STORAGE_ERROR, SERVICE_ERROR };

struct View { // copy of network_manager::SettingsView
  unsigned generation = 0;
  std::uint64_t ticket = 0;
  Result result = Result::NONE;
};
struct Inputs {
  bool blocked = false;           // OTA or another critical operation is running
  bool commands_allowed = true;   // display on / not sleeping
  View view{};
};

struct Machine {
  State state = State::IDLE;
  std::uint32_t since = 0;        // start of the current state
  std::uint64_t ticket_before = 0;
  std::uint64_t ticket = 0;
  unsigned generation = 0;
  bool arm_sent = false, confirm_sent = false, restart_sent = false;
  bool erased = false; // the storage layer reported ACCEPTED

  bool modal_open() const {
    return state == State::MODAL || state == State::ARMING || state == State::CONFIRMING ||
           state == State::RESTARTING || state == State::ERROR;
  }
  // 0..1000 while HOLDING.
  unsigned progress_permille(std::uint32_t now) const {
    if (state != State::HOLDING) return 0;
    const std::uint32_t d = now - since;
    return d >= hold_ms ? 1000u : static_cast<unsigned>(d * 1000u / hold_ms);
  }

  void press(std::uint32_t now, const Inputs &in) {
    if (state != State::IDLE || in.blocked || !in.commands_allowed) return;
    state = State::HOLDING; since = now;
  }
  // A release (or lost press) before the hold time does nothing.
  void release() { if (state == State::HOLDING) state = State::IDLE; }
  void cancel() { if (state == State::MODAL) state = State::IDLE; }
  void outside_touch() { cancel(); }
  void dismiss_error() { if (state == State::ERROR) state = State::IDLE; }
  void confirm(std::uint32_t now, const Inputs &in) {
    if (state != State::MODAL || in.blocked || !in.commands_allowed) return;
    state = State::ARMING; since = now;
    ticket_before = in.view.ticket; arm_sent = confirm_sent = restart_sent = false; erased = false;
  }

  Action tick(std::uint32_t now, const Inputs &in) {
    const std::uint32_t age = now - since;
    switch (state) {
    case State::IDLE: return Action::NONE;
    case State::HOLDING:
      if (in.blocked || !in.commands_allowed) { state = State::IDLE; return Action::NONE; }
      if (age >= hold_ms) { state = State::MODAL; since = now; }
      return Action::NONE;
    case State::MODAL:
      // Screen off, sleep, a critical operation or the timeout all cancel.
      if (in.blocked || !in.commands_allowed || age >= modal_timeout_ms) state = State::IDLE;
      return Action::NONE;
    case State::ARMING:
      if (in.blocked) return fail(now);
      if (!arm_sent) { arm_sent = true; generation = in.view.generation; return Action::REQUEST_ARM; }
      if (in.view.result == Result::ARMED && in.view.ticket != 0 && in.view.ticket != ticket_before &&
          in.view.generation == generation) {
        ticket = in.view.ticket; state = State::CONFIRMING; since = now; confirm_sent = true;
        return Action::REQUEST_CONFIRM;
      }
      if (age >= arm_wait_ms) return fail(now);
      return Action::NONE;
    case State::CONFIRMING:
      // The owner clears the ticket when it processes the confirmation.
      if (in.view.ticket == 0 && in.view.result == Result::ACCEPTED) { erased = true; state = State::RESTARTING; since = now; return Action::NONE; }
      if (in.view.ticket == 0 && (in.view.result == Result::STORAGE_ERROR || in.view.result == Result::REFUSED ||
                                  in.view.result == Result::SERVICE_ERROR))
        return fail(now);
      if (age >= confirm_wait_ms) return fail(now);
      return Action::NONE;
    case State::RESTARTING:
      if (!restart_sent && age >= restart_show_ms) { restart_sent = true; return Action::REQUEST_RESTART; }
      return Action::NONE;
    case State::ERROR:
      if (age >= error_show_ms) state = State::IDLE;
      return Action::NONE;
    }
    return Action::NONE;
  }
  // The restart request itself was refused: stay on screen with an error.
  void restart_failed(std::uint32_t now) { if (state == State::RESTARTING) fail(now); }

private:
  Action fail(std::uint32_t now) { state = State::ERROR; since = now; return Action::NONE; }
};

} // namespace factory_reset
