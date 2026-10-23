#pragma once
#include "api.hpp"
#include "control/core.hpp"

namespace control_home {
enum class VolumeFailureCause : std::uint8_t {
  NONE,
  TOKEN,
  NOT_ACCEPTING,
  SCOPE,
  SESSION,
  CAPABILITY,
  TARGET,
  DEADLINE,
  CAPTURE,
  PRECONDITION,
  CORE_ADMISSION,
  TARGET_REFUSAL,
  EXPIRED_BEFORE_SEND,
  GUARANTEED_NO_EXECUTION
};
// Only the serialized Core owner may construct/call this class.
class Facade {
  control::Core &core_;
  control::Id device_, target_, binding_;
  std::uint64_t watermark_ = 0;
  Intent last_intent_{};
  control::Optional<control::CommandRequest> last_request_;
  VolumeFailureCause last_volume_failure_ = VolumeFailureCause::NONE;
  const control::StateObservation *selected(const char *) const;
  bool available(const char *) const;

public:
  Facade(control::Core &c, control::Id d, control::Id t, control::Id b)
      : core_(c), device_(d), target_(t), binding_(b) {}
  Snapshot snapshot() const;
  bool handle(Intent, control::U64 now, bool accepting = true);
  VolumeResolution volume_resolution(std::uint64_t token) const;
  VolumeFailureCause last_volume_failure() const { return last_volume_failure_; }
  VolumeFailureCause volume_failure(std::uint64_t token) const;
};
} // namespace control_home
