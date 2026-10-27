#pragma once

#include "control_home/full_control.hpp"
#include <cstdint>

namespace control_home {

enum class SourceConfirmationDecision : std::uint8_t {
  WAIT,
  SUCCESS,
  REJECTED,
  EXPIRED,
};

// UI-owned, bounded reconciliation state. It never dispatches or retries a
// command; it only combines one correlated result with fresh authoritative
// source state, including state reacquired after a reconnect.
struct SourceConfirmation {
  bool active = false;
  bool accepted = false;
  bool state_confirmed = false;
  bool session_changed = false;
  TunerSource target = TunerSource::UNKNOWN;
  std::uint8_t error = 0;
  std::uint64_t command_id = 0;
  std::uint64_t pre_generation = 0;
  std::uint64_t initial_session = 0;
  std::uint32_t expires_ms = 0;

  void start(TunerSource requested, std::uint64_t generation,
             std::uint64_t session, std::uint32_t now_ms,
             std::uint32_t lifetime_ms = 15000) {
    *this = {};
    active = true;
    target = requested;
    pre_generation = generation;
    initial_session = session;
    expires_ms = now_ms + lifetime_ms;
  }

  void clear() { *this = {}; }

  SourceConfirmationDecision observe(const FullControlSnapshot &state,
                                     std::uint64_t session,
                                     std::uint32_t now_ms) {
    if (!active) return SourceConfirmationDecision::WAIT;
    if (static_cast<std::int32_t>(now_ms - expires_ms) >= 0) {
      clear();
      return SourceConfirmationDecision::EXPIRED;
    }
    if (!state.connected) {
      session_changed = true;
      return SourceConfirmationDecision::WAIT;
    }
    if (initial_session && session && session != initial_session)
      session_changed = true;
    if (state.command_id) command_id = state.command_id;
    if (state.result_present && state.requested_source == target) {
      if (!state.result_accepted) {
        error = state.last_error;
        active = false;
        return SourceConfirmationDecision::REJECTED;
      }
      accepted = true;
    }
    const bool fresh = state.source_generation &&
                       state.source_generation != pre_generation;
    if (fresh && state.source == target) state_confirmed = true;
    // In the original session both halves are required. After reconnect, the
    // result may no longer be retained, so a fresh authoritative generation
    // is sufficient for read-only reconciliation; no mutation is replayed.
    if (state_confirmed && (accepted || session_changed)) {
      active = false;
      return SourceConfirmationDecision::SUCCESS;
    }
    return SourceConfirmationDecision::WAIT;
  }
};

}  // namespace control_home
