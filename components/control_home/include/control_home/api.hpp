#pragma once
#include <array>
#include <cstdint>
#include <type_traits>

// Protocol-neutral HOME boundary. No Core, LVGL or adapter-owned pointers.
namespace control_home {
enum class Action : std::uint8_t {
  PREVIOUS,
  TOGGLE,
  NEXT,
  VOLUME_DOWN,
  VOLUME_UP
};
enum class Playback : std::uint8_t {
  UNKNOWN,
  PLAYING,
  PAUSED,
  OTHER,
  CONFLICT
};
enum class Connection : std::uint8_t { UNKNOWN, CONNECTED, UNAVAILABLE };
enum class VolumeAdmission : std::uint8_t {
  NONE,
  ACCEPTED,
  NOT_CONNECTED,
  NO_SESSION,
  NO_CAPABILITY_REVISION,
  VOLUME_UNKNOWN,
  VOLUME_SET_UNAVAILABLE,
  AT_MINIMUM,
  AT_MAXIMUM,
  IN_FLIGHT,
  TOKEN_LIMIT,
  QUEUE_FULL
};
enum class VolumeResolution : std::uint8_t {
  PENDING,
  DEFINITE_FAILURE,
  CONFIRMED,
  UNCERTAIN
};
struct Intent {
  std::uint64_t token = 0;
  Action action = Action::TOGGLE;
  // Neutral presentation scope at enqueue, never rebased after reconnect.
  std::uint64_t session = 0, capability_revision = 0;
  std::uint8_t volume = 0;
  std::uint64_t first_input_ms = 0, latest_input_ms = 0;
  bool operator==(const Intent &) const = default;
};
struct Snapshot {
  Connection connection = Connection::UNKNOWN;
  Playback playback = Playback::UNKNOWN;
  char title[32]{}, subtitle[32]{};
  bool volume_known = false, volume_set = false, previous = false,
       toggle = false, next = false;
  std::uint8_t volume = 0;
  // Provisional encoder target. Authoritative volume remains in `volume`.
  bool desired_volume_known = false, volume_pending = false;
  std::uint8_t desired_volume = 0;
  std::uint64_t session = 0, capability_revision = 0;
  bool operator==(const Snapshot &) const = default;
};
static_assert(std::is_trivially_copyable_v<Snapshot>);
// Caller serializes access. ESP implementation uses a short critical section.
struct Channel {
  static constexpr unsigned capacity = 8;
  std::array<Intent, capacity> queue{};
  unsigned head = 0, count = 0;
  std::uint64_t issued = 0, generation = 0, rejected = 0;
  Snapshot latest{};
  bool volume_outstanding = false;
  bool volume_confirmed = false;
  std::uint8_t volume_target = 0;
  std::uint64_t volume_token = 0;
  bool desired_volume_valid = false;
  std::uint8_t desired_volume = 0;
  std::uint64_t volume_attempts = 0, volume_accepted = 0;
  std::uint64_t volume_queued = 0, volume_completed = 0, volume_failed = 0,
                volume_cancelled = 0, authoritative_updates = 0,
                cancel_session = 0, cancel_capability = 0,
                cancel_authority = 0, cancel_divergence = 0;
  std::uint64_t last_input_ms = 0, last_target_queued_ms = 0,
                last_authoritative_ms = 0;
  VolumeAdmission last_volume_admission = VolumeAdmission::NONE;
  static bool enabled(const Snapshot &s, Action action) {
    if (s.connection != Connection::CONNECTED || !s.session ||
        !s.capability_revision)
      return false;
    switch (action) {
    case Action::PREVIOUS: return s.previous;
    case Action::TOGGLE: return s.toggle;
    case Action::NEXT: return s.next;
    case Action::VOLUME_DOWN:
      return s.volume_set && s.volume_known && s.volume > 0;
    case Action::VOLUME_UP:
      return s.volume_set && s.volume_known && s.volume < 100;
    }
    return false;
  }
  bool push(Intent intent) {
    if (!intent.token || count == capacity) {
      ++rejected;
      return false;
    }
    queue[(head + count++) % capacity] = intent;
    return true;
  }
  static void increment(std::uint64_t &value) {
    if (value != UINT64_MAX) ++value;
  }
  enum class CancelReason : std::uint8_t {
    NONE,
    SESSION,
    CAPABILITY,
    AUTHORITY,
    DIVERGENCE
  };
  void cancel_volume(CancelReason reason = CancelReason::NONE) {
    if (reason != CancelReason::NONE &&
        (volume_outstanding || desired_volume_valid)) {
      increment(volume_cancelled);
      switch (reason) {
      case CancelReason::SESSION: increment(cancel_session); break;
      case CancelReason::CAPABILITY: increment(cancel_capability); break;
      case CancelReason::AUTHORITY: increment(cancel_authority); break;
      case CancelReason::DIVERGENCE: increment(cancel_divergence); break;
      case CancelReason::NONE: break;
      }
    }
    volume_outstanding = false;
    volume_confirmed = false;
    volume_target = 0;
    volume_token = 0;
    desired_volume_valid = false;
    desired_volume = 0;
  }
  bool retarget_queued_volume() {
    if (!volume_outstanding)
      return false;
    for (unsigned i = 0; i < count; ++i) {
      auto &intent = queue[(head + i) % capacity];
      if (intent.token == volume_token) {
        if (desired_volume == latest.volume) {
          for (unsigned j = i + 1; j < count; ++j)
            queue[(head + j - 1) % capacity] = queue[(head + j) % capacity];
          --count;
          cancel_volume();
          return true;
        }
        intent.action = desired_volume > latest.volume ? Action::VOLUME_UP
                                                       : Action::VOLUME_DOWN;
        intent.volume = desired_volume;
        intent.latest_input_ms = last_input_ms;
        volume_target = desired_volume;
        return true;
      }
    }
    return false;
  }
  bool start_volume_target(std::uint64_t now = 0) {
    if (volume_outstanding || !desired_volume_valid)
      return true;
    if (desired_volume == latest.volume) {
      desired_volume_valid = false;
      return true;
    }
    if (issued == UINT64_MAX) {
      last_volume_admission = VolumeAdmission::TOKEN_LIMIT;
      cancel_volume();
      return false;
    }
    const auto action = desired_volume > latest.volume ? Action::VOLUME_UP
                                                       : Action::VOLUME_DOWN;
    const auto target = desired_volume;
    const auto token = issued + 1;
    if (!push({token,action,latest.session,latest.capability_revision,target,
               last_input_ms,last_input_ms})) {
      last_volume_admission = VolumeAdmission::QUEUE_FULL;
      cancel_volume();
      return false;
    }
    issued = token;
    increment(volume_queued);
    volume_outstanding = true;
    volume_confirmed = false;
    volume_target = target;
    volume_token = token;
    last_target_queued_ms = now;
    return true;
  }
  VolumeAdmission volume_admission(Action action) const {
    if (latest.connection != Connection::CONNECTED)
      return VolumeAdmission::NOT_CONNECTED;
    if (!latest.session) return VolumeAdmission::NO_SESSION;
    if (!latest.capability_revision)
      return VolumeAdmission::NO_CAPABILITY_REVISION;
    if (!latest.volume_known) return VolumeAdmission::VOLUME_UNKNOWN;
    if (!latest.volume_set) return VolumeAdmission::VOLUME_SET_UNAVAILABLE;
    const auto base = desired_volume_valid ? desired_volume : latest.volume;
    if (action == Action::VOLUME_DOWN && base == 0)
      return VolumeAdmission::AT_MINIMUM;
    if (action == Action::VOLUME_UP && base >= 100)
      return VolumeAdmission::AT_MAXIMUM;
    if (issued == UINT64_MAX) return VolumeAdmission::TOKEN_LIMIT;
    if (!volume_outstanding && count == capacity)
      return VolumeAdmission::QUEUE_FULL;
    return VolumeAdmission::ACCEPTED;
  }
  bool request(Action action, std::uint64_t now = 0) {
    const bool volume = action == Action::VOLUME_DOWN ||
                        action == Action::VOLUME_UP;
    if (volume) {
      increment(volume_attempts);
      last_volume_admission = volume_admission(action);
      if (last_volume_admission != VolumeAdmission::ACCEPTED) {
        ++rejected;
        return false;
      }
    } else if (issued == UINT64_MAX || !enabled(latest, action)) {
      ++rejected;
      return false;
    }
    if (volume) {
      const auto base = desired_volume_valid ? desired_volume : latest.volume;
      desired_volume = static_cast<std::uint8_t>(
          int(base) + (action == Action::VOLUME_UP ? 1 : -1));
      desired_volume_valid = true;
      last_input_ms = now;
      increment(volume_accepted);
      if (volume_outstanding && retarget_queued_volume()) {
        if (generation != UINT64_MAX) ++generation;
        return true;
      }
      if (!start_volume_target(now)) {
        ++rejected;
        return false;
      }
      if (generation != UINT64_MAX) ++generation;
      return true;
    }
    return push({++issued,action,latest.session,latest.capability_revision,0});
  }
  bool resolve(std::uint64_t token, VolumeResolution resolution) {
    if (!volume_outstanding || !token || token != volume_token)
      return false;
    if (resolution == VolumeResolution::DEFINITE_FAILURE) {
      increment(volume_failed);
      cancel_volume();
      if (generation != UINT64_MAX) ++generation;
      return true;
    }
    if (resolution == VolumeResolution::CONFIRMED && !volume_confirmed) {
      increment(volume_completed);
      volume_confirmed = true;
      if (generation != UINT64_MAX) ++generation;
      return true;
    }
    return false;
  }
  bool pop(Intent &out) {
    if (!count)
      return false;
    out = queue[head];
    head = (head + 1) % capacity;
    --count;
    return true;
  }
  void publish(const Snapshot &value, std::uint64_t now = 0) {
    if (value.volume_known) increment(authoritative_updates);
    // Capability revisions are target-wide. D-009 playback-context updates
    // may advance that revision without changing the volume contract. Keep an
    // admitted volume target across such unrelated revisions, but retain the
    // hard fences for connection/session loss and loss of volume authority.
    const bool volume_context_valid =
        value.connection == Connection::CONNECTED && value.session &&
        value.session == latest.session && value.capability_revision &&
        value.volume_set && value.volume_known;
    const bool invalidated = !volume_context_valid;
    const auto active_token = volume_token;
    const bool target_reached =
        volume_outstanding && value.volume_set && value.volume_known &&
        value.volume == volume_target;
    const bool authoritative_divergence =
        volume_outstanding && volume_confirmed && value.volume_set &&
        value.volume_known && value.volume != latest.volume &&
        value.volume != volume_target;
    const bool cancelled = invalidated || authoritative_divergence;
    if (invalidated) {
      const auto reason =
          value.connection != Connection::CONNECTED || !value.session ||
                  value.session != latest.session
              ? CancelReason::SESSION
              : !value.capability_revision ? CancelReason::CAPABILITY
                                           : CancelReason::AUTHORITY;
      cancel_volume(reason);
    }
    else if (authoritative_divergence)
      // A fresh post-result value different from the submitted target is a
      // legitimate external change. Reconcile to it; never overwrite it with
      // a locally accumulated successor based on obsolete authority.
      cancel_volume(CancelReason::DIVERGENCE);
    else if (target_reached) {
      volume_outstanding = false;
      volume_confirmed = false;
      volume_target = 0;
      volume_token = 0;
      last_authoritative_ms = now;
    }
    if (!(latest == value) && generation != UINT64_MAX) {
      // Discard invalid buffered presses now, even if readiness later returns.
      // Compact in FIFO order in the same caller-held critical section.
      unsigned kept = 0;
      for (unsigned i = 0; i < count; ++i) {
        auto intent = queue[(head + i) % capacity];
        if (intent.token == active_token &&
            (cancelled || target_reached))
          continue;
        if (intent.token == volume_token && volume_outstanding &&
            volume_context_valid && intent.volume != value.volume) {
          // The queued command has not crossed the owner boundary yet. Refresh
          // its capture fence to the current target-wide revision; Facade will
          // still perform the full CCM capture/admission check at dispatch.
          intent.session = value.session;
          intent.capability_revision = value.capability_revision;
          intent.action = intent.volume > value.volume ? Action::VOLUME_UP
                                                       : Action::VOLUME_DOWN;
          queue[(head + kept++) % capacity] = intent;
        } else if (intent.session == value.session &&
                   intent.capability_revision == value.capability_revision &&
                   enabled(value, intent.action))
          queue[(head + kept++) % capacity] = intent;
        else
          ++rejected;
      }
      count = kept;
      latest = value;
      ++generation;
    }
    if (target_reached)
      (void)start_volume_target(now);
  }
  Snapshot view() const {
    auto value = latest;
    value.desired_volume_known = desired_volume_valid;
    value.desired_volume = desired_volume_valid ? desired_volume : value.volume;
    value.volume_pending = volume_outstanding;
    return value;
  }
};
// Zero-wait enqueue; false means rejected, never retained for later replay.
bool request(Action);
void record_encoder_state(std::uint64_t transitions, std::uint64_t detents,
                          std::uint64_t invalid, std::uint64_t positive,
                          std::uint64_t negative, std::uint64_t rejected,
                          std::uint32_t sample_last_ms,
                          std::uint32_t sample_max_ms,
                          std::int8_t last_delta);
// Copies the latest coherent value. Owned by caller, valid after later updates.
bool read(Snapshot &, std::uint64_t &generation);
void start();
} // namespace control_home
