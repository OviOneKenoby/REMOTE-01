#pragma once
#include "control/mock_adapter.hpp"
#include "control/ingress.hpp"
#include "facade.hpp"
#include "control_home/rtcp_device.hpp"

// DEVELOPMENT ONLY / MOCK ONLY. Never included by HOME or its public API.
namespace control_home::development {
inline Snapshot present_snapshot(Snapshot value, bool pending,
                                 bool remote_transport) {
  // RTCP/CCM already serializes commands. Transport busy state must not be
  // presented as loss of an otherwise fresh playback capability. The
  // synthetic development adapter still needs its legacy single-operation
  // gate because it cannot accept a second command while delayed.
  if (pending && !remote_transport)
    value.previous = value.toggle = value.next = false;
  return value;
}
class Device final : public control::Adapter {
  control::Core *core_ = nullptr;
  control::ingress::Boundary *boundary_ = nullptr;
  control::Id binding_;
  control::Playback playback_ = control::Playback::PLAYING;
  bool known_ = true;
  unsigned volume_ = 68, track_ = 0, acquired_track_ = 0;
  char pending_operation_[24]{};
  bool pending_effect_applied_ = false;
  void effect(const char *);
  void publish(control::AdapterSink &, control::U64);

public:
  control::MockAdapter mock;
  bool delayed = false, pending = false;
  control::Id identity() const override { return mock.identity(); }
  control::Text version() const override { return mock.version(); }
  void dispatch(const control::Dispatch &, control::AdapterSink &) override;
  bool install(control::Core &, control::ingress::Boundary &, const control::Id &, const control::Id &,
               const control::Id &, control::U64);
  bool command(char, control::U64); // bounded single-byte serial test commands
  void acquire(control::U64, bool queued = false);
};
struct Session {
  control::Store store;
  control::MockJournal journal;
  rtcp_client::Client rtcp;
  RtcpDevice remote{rtcp};
  Device device;
  bool remote_mode = false;
  control::Id d{"home.synthetic.device"}, t{"home.synthetic.target"},
      b{"home.synthetic.binding"};
  control::Core core;
  control::ingress::Boundary boundary;
  Facade facade;
  static bool synthetic_owner(void *) { return true; }
  explicit Session(control::Id epoch, bool (*owner)(void *) = synthetic_owner,
                   void *context = nullptr)
      : core(store, journal, epoch), boundary(core, owner, context),
        facade(core, d, t, b) {}
  bool initialize(control::U64 now) {
    remote_mode = rtcp.initialize();
    return remote_mode ? remote.install(core, boundary, d, t, b, now)
                       : device.install(core, boundary, d, t, b, now);
  }
  bool handle(Intent intent, control::U64 now) {
    if (remote_mode)
      return facade.handle(intent, now, true);
    const auto dispatches = device.mock.dispatches;
    // Even a refused pending press consumes its token; it cannot replay later.
    bool accepted = facade.handle(intent, now, !device.pending);
    if (!device.pending && device.mock.dispatches != dispatches)
      device.acquire(now);
    return accepted;
  }
  VolumeResolution volume_resolution(std::uint64_t token) const {
    return facade.volume_resolution(token);
  }
  VolumeFailureCause last_volume_failure() const {
    return facade.last_volume_failure();
  }
  VolumeFailureCause volume_failure(std::uint64_t token) const {
    return facade.volume_failure(token);
  }
  Snapshot snapshot() const {
    return present_snapshot(facade.snapshot(), pending(), remote_mode);
  }
  bool poll(control::U64 now, bool command_waiting = false) {
    return !remote_mode || remote.poll(now, command_waiting);
  }
  bool command_ready() const { return !remote_mode || remote.command_ready(); }
  void suspend() { if (remote_mode) rtcp.suspend(); }
  bool pending() const {
    return remote_mode ? remote.pending() : device.pending;
  }
  unsigned dispatches() const {
    return remote_mode ? remote.dispatches() : device.mock.dispatches;
  }
  void acquire(control::U64 now) { if (!remote_mode) device.acquire(now); }
};
// Fixed byte dispatcher for the development hook; no shell or UI dependency.
enum class InputKind { IGNORE, COMMAND, TELEMETRY, UNKNOWN };
struct InputReply {
  InputKind kind;
  bool accepted = false;
};
inline InputReply input(Session &s, unsigned char key, control::U64 now,
                        bool owner) {
  InputKind kind;
  switch (key) {
  case 'p':
  case 'a':
  case 'u':
  case '0':
  case '1':
  case 'v':
  case 'd':
  case 'r':
  case '-':
  case '+':
  case 'o':
  case 'c':
  case 's':
  case 'l':
  case 'n':
  case 'f':
    kind = InputKind::COMMAND;
    break;
  case 'm':
    kind = InputKind::TELEMETRY;
    break;
  default:
    kind = key >= 32 && key <= 126 ? InputKind::UNKNOWN : InputKind::IGNORE;
  }
  if (!owner)
    return {kind, false};
  if (s.remote_mode)
    return {kind, false};
  if (kind == InputKind::COMMAND)
    return {kind, s.device.command(static_cast<char>(key), now)};
  return {kind, kind == InputKind::TELEMETRY};
}
} // namespace control_home::development
