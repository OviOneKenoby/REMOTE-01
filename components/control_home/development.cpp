#include "control_home/development.hpp"
#include <cstdio>
#include <cstring>
#include <numeric>

namespace control_home::development {
using namespace control;
bool Device::install(Core &c, ingress::Boundary &boundary, const Id &d, const Id &t, const Id &b, U64 now) {
  boundary_ = &boundary;
  core_ = &c;
  binding_ = b;
  CommonDevice device;
  device.device_id = d;
  device.display_name = Text::literal("SYNTHETIC HOME TEST");
  Target target;
  target.device_id = d;
  target.target_id = t;
  target.display_name = Text::literal("SYNTHETIC HOME TARGET");
  Binding binding;
  binding.device_id = d;
  binding.target_id = t;
  binding.binding_id = b;
  binding.authorization = Authorization::AUTHORIZED;
  binding.reachability = Reachability::REACHABLE;
  binding.model_versions.push(Version{});
  auto evidence =
      MockAdapter::operation(b, "playback.play", Feedback::CORRELATED_RESULT)
          .evidence;
  binding.mapping_evidence = evidence;
  if (c.register_device(device, now).present ||
      c.register_target(target, now).present ||
      c.register_binding(binding, *this, now).present || !boundary.attach(b, now))
    return false;
  for (const auto *op : {"playback.previous", "playback.play", "playback.pause",
                         "playback.next", "volume.set"})
    if (!boundary.capability(MockAdapter::operation(b, op, Feedback::CORRELATED_RESULT), now))
      return false;
  for (const auto *f :
       {"playback.state", "volume.level", "media.title", "media.subtitle"})
    if (!boundary.capability(MockAdapter::readable(b, f), now))
      return false;
  acquire(now);
  return !c.storage().blocked;
}
void Device::effect(const char *op) {
  if (std::strcmp(op, "playback.play") == 0) {
    playback_ = control::Playback::PLAYING;
    known_ = true;
  }
  if (std::strcmp(op, "playback.pause") == 0) {
    playback_ = control::Playback::PAUSED;
    known_ = true;
  }
  if (std::strcmp(op, "playback.next") == 0)
    track_ = (track_ + 1) % 3;
  if (std::strcmp(op, "playback.previous") == 0)
    track_ = (track_ + 2) % 3;
}
void Device::publish(AdapterSink &sink, U64 now) {
  Scalar p;
  p.kind = ScalarKind::PLAYBACK;
  p.playback = playback_;
  sink.publish(mock.sample(*core_, binding_, "playback.state",
                           known_ ? Value<Scalar>::of(p)
                                  : Value<Scalar>::absent(Absence::UNKNOWN),
                           now));
  auto divisor = std::gcd(volume_, 100u);
  sink.publish(mock.sample(
      *core_, binding_, "volume.level",
      Value<Scalar>::of(Scalar::level({volume_ / divisor, 100 / divisor})),
      now));
  Scalar title;
  title.kind = ScalarKind::STRING;
  const char *titles[] = {"Mock One", "Mock Two", "Mock Three"};
  title.string = Text(titles[track_]);
  sink.publish(mock.sample(*core_, binding_, "media.title",
                           Value<Scalar>::of(title), now));
  title.string = Text::literal("Synthetic");
  sink.publish(mock.sample(*core_, binding_, "media.subtitle",
                           Value<Scalar>::of(title), now));
}
void Device::acquire(U64 now, bool queued) {
  if (core_->binding(binding_)->reachability != Reachability::REACHABLE)
    return;
  if (track_ != acquired_track_) {
    if (!boundary_->media_changed(now))
      return;
    acquired_track_ = track_;
  }
  // A new read of the synthetic device, not replay/rejuvenation of cached
  // observations.
  class OwnerSink final : public AdapterSink {
    ingress::Boundary &boundary;
    U64 time;
    bool queued;

  public:
    OwnerSink(ingress::Boundary &b, U64 n, bool q) : boundary(b), time(n), queued(q) {}
    bool publish(StateObservation o) override {
      return queued ? boundary.enqueue(std::move(o), time) : boundary.observe(std::move(o), time);
    }
    bool publish(AdapterEvidence e) override {
      return boundary.report(std::move(e), time);
    }
  } sink(*boundary_, now, queued);
  publish(sink, now);
}
void Device::dispatch(const Dispatch &d, AdapterSink &sink) {
  if (!boundary_->accepting()) {
    mock.mode = MockAdapter::Mode::UNAVAILABLE;
    mock.dispatch(d, sink); // explicit known-unsent evidence; no synthetic effect
    return;
  }
  mock.mode =
      delayed ? MockAdapter::Mode::DELAYED : MockAdapter::Mode::CONFIRMED;
  mock.dispatch(d, sink);
  if (delayed) {
    pending = true;
    pending_effect_applied_ = false;
    std::snprintf(pending_operation_, sizeof pending_operation_, "%.*s",
                  int(d.request.operation_id.view().size()),
                  d.request.operation_id.view().data());
  } else {
    char op[24];
    std::snprintf(op, sizeof op, "%.*s",
                  int(d.request.operation_id.view().size()),
                  d.request.operation_id.view().data());
    if (std::strcmp(op, "volume.set") == 0 && d.mapped_numeric.present) {
      const auto &level = d.mapped_numeric.value;
      if (level.n >= 0 && level.d && level.d <= 100 && 100 % level.d == 0)
        volume_ = unsigned(level.n) * (100 / unsigned(level.d));
    } else {
      effect(op);
    }
    // Synthetic device execution; owner separately acquires observations.
  }
}
bool Device::command(char key, U64 now) {
  if (!boundary_->accepting()) return false;
  auto *b = core_->binding(binding_);
  const auto d = b->device_id, t = b->target_id;
  switch (key) {
  case 'p':
    playback_ = control::Playback::PLAYING;
    known_ = true;
    break;
  case 'a':
    playback_ = control::Playback::PAUSED;
    known_ = true;
    break;
  case 'u':
    known_ = false;
    break;
  case '0':
    volume_ = 0;
    break;
  case '1':
    volume_ = 100;
    break;
  case 'v':
    volume_ = 25;
    break;
  case 'd':
    if (!boundary_->reachability(Reachability::UNREACHABLE, now))
      return false;
    for (auto cap : core_->target(d, t)->capabilities) {
      cap.availability = Availability::OFFLINE;
      if (!boundary_->capability(std::move(cap), now))
        return false;
    }
    return true;
  case 'r':
    if (!boundary_->reconnect(now) || !boundary_->reachability(Reachability::REACHABLE, now))
      return false;
    for (auto cap : core_->target(d, t)->capabilities) {
      cap.availability = Availability::READY;
      if (!boundary_->capability(std::move(cap), now))
        return false;
    }
    break;
  case '-':
    return boundary_->withdraw({CapabilityKey::Kind::OPERATION, Text::literal("playback.next"), binding_}, now);
  case '+':
    return boundary_->capability(MockAdapter::operation(binding_, "playback.next", Feedback::CORRELATED_RESULT), now);
  case 'o':
  case 'c': {
    auto cap = MockAdapter::operation(binding_, "playback.next",
                                      Feedback::CORRELATED_RESULT);
    cap.availability =
        key == 'o' ? Availability::CONTEXT_UNAVAILABLE : Availability::READY;
    return boundary_->capability(std::move(cap), now);
  }
  case 's': {
    auto *f = core_->field(d, t, "volume.level");
    if (!f || !f->selected.present)
      return false;
    auto stale = f->selected.value;
    stale.receipt_time.tick_ms = now;
    stale.value = Value<Scalar>::of(Scalar::level({99, 100}));
    // Older ordinal, not equal/conflicting; rejection cannot rejuvenate truth.
    if (stale.source_sequence.value.ordinal <= 1)
      return false;
    --stale.source_sequence.value.ordinal;
    return !boundary_->enqueue(std::move(stale), now);
  }
  case 'l':
    delayed = true;
    return true;
  case 'n':
    delayed = false;
    return true;
  case 'f':
    if (!pending)
      return false;
    if (core_->binding(binding_)->session_generation !=
            mock.last.value.session_generation ||
        core_->binding(binding_)->reachability != Reachability::REACHABLE)
      return false;
    // Execution and evidence delivery are separate. A failed report must not
    // execute this same pending NEXT/PREVIOUS again on a later f command.
    if (!pending_effect_applied_) {
      effect(pending_operation_);
      pending_effect_applied_ = true;
    }
    if (!boundary_->enqueue(mock.completion(), now))
      return false;
    pending = false;
    break;
  default:
    return false;
  }
  // Serial changes exercise copied async records; periodic reads retain the
  // owner fast path and its measured allocation behavior.
  acquire(now, true);
  return true;
}
} // namespace control_home::development
