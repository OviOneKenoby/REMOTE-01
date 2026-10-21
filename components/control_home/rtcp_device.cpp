#include "control_home/rtcp_device.hpp"
#include <string>

namespace control_home {
using namespace control;
namespace {
constexpr auto operation = CapabilityKey::Kind::OPERATION;
constexpr auto field = CapabilityKey::Kind::FIELD;

Error d009_error(std::uint8_t value) {
  using d009::ResultClass;
  switch (d009::classify_result(value)) {
  case ResultClass::invalid_argument: return error(ErrorCode::INVALID_ARGUMENT, "D009_INVALID_ARGUMENT", RejectionOrigin::TARGET);
  case ResultClass::unsupported: return error(ErrorCode::UNSUPPORTED_OPERATION, "D009_UNSUPPORTED", RejectionOrigin::TARGET);
  case ResultClass::unavailable: return error(ErrorCode::UNAVAILABLE, "D009_UNAVAILABLE", RejectionOrigin::TARGET);
  case ResultClass::stale: return error(ErrorCode::STALE_PRECONDITION, "D009_STALE", RejectionOrigin::TARGET);
  case ResultClass::execution_failed: return error(ErrorCode::UNAVAILABLE, "D009_EXECUTION_FAILED", RejectionOrigin::TARGET);
  case ResultClass::busy: return error(ErrorCode::ORDERING_BLOCKED, "D009_BUSY", RejectionOrigin::TARGET);
  case ResultClass::internal: return error(ErrorCode::UNAVAILABLE, "D009_INTERNAL", RejectionOrigin::TARGET);
  case ResultClass::invalid: return error(ErrorCode::INVALID_ARGUMENT, "D009_RESERVED_ERROR", RejectionOrigin::TARGET);
  }
  return error(ErrorCode::INVALID_ARGUMENT, "D009_RESERVED_ERROR", RejectionOrigin::TARGET);
}

Optional<std::uint8_t> expected_playback(const CommandRequest &request) {
  for (const auto &precondition : request.preconditions)
    if (precondition.field_id.view() == "playback.state" &&
        precondition.expected_value.present &&
        precondition.expected_value.value.kind == ScalarKind::PLAYBACK)
      return Optional<std::uint8_t>(std::uint8_t(precondition.expected_value.value.playback));
  return {};
}

Value<Scalar> text_value(const char *data, std::size_t size) {
  if (!size) return Value<Scalar>::absent(Absence::UNAVAILABLE);
  Scalar value; value.kind = ScalarKind::STRING;
  value.string = Text(std::string_view(data, size));
  return Value<Scalar>::of(std::move(value));
}
} // namespace

Evidence RtcpDevice::evidence(const char *id, const char *claim) const {
  Evidence value; value.evidence_id = Id(id); value.kind = EvidenceKind::PROJECT_DESIGN;
  value.reference = Text::literal("A-REMOTE-013 RTCP/1 D-009 REMOTE binding");
  value.verified_claims.push(Text(claim));
  value.version = Value<Text>::of(Text::literal("2")); return value;
}

bool RtcpDevice::install(Core &core, ingress::Boundary &boundary, const Id &device_id,
                         const Id &target_id, const Id &binding_id, U64 now) {
  core_ = &core; boundary_ = &boundary; binding_ = binding_id;
  CommonDevice device_value; device_value.device_id = device_id;
  device_value.display_name = Text::literal("TUNER//01");
  device_value.device_class = CommonDevice::Class::AUDIO_PLAYER;
  Target target; target.device_id = device_id; target.target_id = target_id;
  target.display_name = Text::literal("TUNER//01 audio");
  Binding binding; binding.device_id = device_id; binding.target_id = target_id;
  binding.binding_id = binding_id; binding.authorization = Authorization::AUTHORIZED;
  binding.reachability = Reachability::UNREACHABLE; binding.adapter_version = version();
  binding.model_versions.push(Version{1,1,0,{}});
  binding.mapping_evidence.push(evidence("a013.binding", "authenticated RTCP/1 D-009 mapping"));
  if (core.register_device(device_value, now).present || core.register_target(target, now).present ||
      core.register_binding(binding, *this, now).present || !boundary.attach(binding_id, now)) return false;

  for (const auto *name : {"volume.set", "playback.play", "playback.pause",
                           "playback.previous", "playback.next"}) {
    auto descriptor = MockAdapter::operation(binding_id, name, Feedback::CORRELATED_RESULT);
    descriptor.availability = Availability::OFFLINE; descriptor.evidence.clear();
    descriptor.evidence.push(evidence("a013.operation", "correlated authenticated RTCP command"));
    descriptor.execution_evidence.value.mapping_reference = Text::literal(
        std::string_view(name) == "volume.set" ? "RTCP/1 application type 7/8" : "RTCP/1 D-009 type 9/8");
    if (!boundary.capability(std::move(descriptor), now)) return false;
  }
  for (const auto *name : {"volume.level", "playback.state", "media.title",
                           "media.artist", "media.subtitle"}) {
    auto descriptor = MockAdapter::readable(binding_id, name);
    descriptor.availability = Availability::OFFLINE; descriptor.freshness = freshness_policy();
    descriptor.evidence.clear(); descriptor.evidence.push(
        evidence("a013.observation", "fresh authenticated verified-sequence observation"));
    if (!boundary.capability(std::move(descriptor), now)) return false;
  }
  return true;
}

bool RtcpDevice::capability(CapabilityKey::Kind kind, const char *name,
                            Availability value, U64 now) {
  auto *binding = core_->binding(binding_);
  auto *target = binding ? core_->target(binding->device_id, binding->target_id) : nullptr;
  if (!target) return false;
  for (auto descriptor : target->capabilities)
    if (descriptor.key.kind == kind && descriptor.key.binding_id == binding_ &&
        descriptor.key.id.view() == name) {
      if (descriptor.availability == value) return true;
      descriptor.availability = value;
      return boundary_->capability(std::move(descriptor), now);
    }
  return false;
}

void RtcpDevice::availability(Availability value, U64 now) {
  auto *binding = core_->binding(binding_);
  auto *target = binding ? core_->target(binding->device_id, binding->target_id) : nullptr;
  if (!target) return;
  for (auto descriptor : target->capabilities) {
    if (descriptor.availability == value) continue;
    descriptor.availability = value; (void)boundary_->capability(std::move(descriptor), now);
  }
}

bool RtcpDevice::negotiated(U64 now) {
  const auto &c = client_.capabilities(); const bool enabled = c.d009();
  bool ok = capability(field, "volume.level", c.volume ? Availability::READY : Availability::UNKNOWN, now) &&
            capability(field, "playback.state", c.playback ? Availability::READY : Availability::UNKNOWN, now) &&
            capability(operation, "volume.set", c.volume_set ? Availability::READY : Availability::UNKNOWN, now);
  const auto initial = Availability::CONTEXT_UNAVAILABLE;
  ok = capability(operation, "playback.play", enabled && c.play ? initial : Availability::UNKNOWN, now) && ok;
  ok = capability(operation, "playback.pause", enabled && c.pause ? initial : Availability::UNKNOWN, now) && ok;
  ok = capability(operation, "playback.previous", enabled && c.previous ? initial : Availability::UNKNOWN, now) && ok;
  ok = capability(operation, "playback.next", enabled && c.next ? initial : Availability::UNKNOWN, now) && ok;
  const auto metadata = enabled && c.now_playing ? Availability::READY : Availability::UNKNOWN;
  ok = capability(field, "media.title", metadata, now) && ok;
  ok = capability(field, "media.artist", metadata, now) && ok;
  ok = capability(field, "media.subtitle", metadata, now) && ok;
  return ok;
}

bool RtcpDevice::context(const rtcp_client::wire::NowPlaying &value, U64 now) {
  const auto &c = client_.capabilities();
  auto state = [&](bool advertised, bool current) {
    return !c.d009() || !advertised ? Availability::UNKNOWN :
           current ? Availability::READY : Availability::CONTEXT_UNAVAILABLE;
  };
  return capability(operation, "playback.play", state(c.play, value.flags & 0x10), now) &&
         capability(operation, "playback.pause", state(c.pause, value.flags & 0x20), now) &&
         capability(operation, "playback.previous", state(c.previous, value.flags & 0x40), now) &&
         capability(operation, "playback.next", state(c.next, value.flags & 0x80), now);
}

bool RtcpDevice::media(const d009::FenceDecision &decision, U64 now) {
  return decision.accepted && (!decision.new_context || boundary_->media_changed(now));
}

bool RtcpDevice::publish(const char *name, Value<Scalar> value,
                         std::uint64_t sequence, U64 now, bool media_field) {
  StateObservation observation;
  observation.observation_id = Id((std::string("rtcp.observation.") +
                                   std::to_string(++observations_)).c_str());
  auto *binding = core_->binding(binding_);
  auto *target = binding ? core_->target(binding->device_id, binding->target_id) : nullptr;
  auto *device = binding ? core_->device(binding->device_id) : nullptr;
  if (!binding || !target || !device) return false;
  observation.device_id = device->device_id; observation.target_id = target->target_id;
  observation.field_id = Text(name); observation.value = std::move(value);
  observation.confidence = Confidence::REPORTED; observation.binding_id = binding_;
  observation.device_association_generation = device->association_generation;
  observation.association_generation = target->association_generation;
  observation.mapping_revision = binding->mapping_revision;
  observation.session_generation = binding->session_generation;
  observation.receipt_time = {core_->storage().clock_epoch, now};
  observation.age_at_receipt_ms = 0; observation.clock_quality = ClockQuality::BOUNDED;
  observation.origin = Origin::LIVE; observation.acquisition_order = sequence;
  observation.source_sequence = Sequence{Id("rtcp.tuner.sequence"), sequence};
  observation.freshness = freshness_policy();
  if (media_field) observation.media_context_generation = target->media_context_generation;
  ProvenanceHop hop; hop.actor_id = identity(); hop.evidence_id = Id("a013.d009.observation");
  observation.provenance.push(hop);
  return boundary_->observe(std::move(observation), now);
}

void RtcpDevice::dispatch(const Dispatch &dispatch, AdapterSink &sink) {
  AdapterEvidence result; result.request_id = dispatch.request.request_id;
  result.binding_id = dispatch.attempt.binding_id; result.attempt_index = dispatch.attempt.attempt_index;
  result.mapping_revision = dispatch.attempt.mapping_revision;
  result.session_generation = dispatch.attempt.session_generation;
  result.evidence = evidence("a013.dispatch", "one RTCP command correlated to one CCM request");
  LockKey key; key.device_id = dispatch.request.device_id; key.target_id = dispatch.request.target_id;
  result.covered_keys.push(key); result.progress = Progress{1,0,0,false};

  bool sent = false; const auto op = dispatch.request.operation_id.view();
  if (op == "volume.set" && dispatch.mapped_numeric.present)
    sent = client_.set_volume(dispatch.request.admission_ticket.serial,
                              std::uint32_t(dispatch.mapped_numeric.value.n),
                              std::uint32_t(dispatch.mapped_numeric.value.d));
  else {
    rtcp_client::wire::PlaybackOperation wire_op{}; bool known = true;
    if (op == "playback.play") wire_op = rtcp_client::wire::PlaybackOperation::play;
    else if (op == "playback.pause") wire_op = rtcp_client::wire::PlaybackOperation::pause;
    else if (op == "playback.previous") wire_op = rtcp_client::wire::PlaybackOperation::previous;
    else if (op == "playback.next") wire_op = rtcp_client::wire::PlaybackOperation::next;
    else known = false;
    const auto expected = expected_playback(dispatch.request);
    auto *binding = core_->binding(binding_);
    auto *target = binding ? core_->target(binding->device_id, binding->target_id) : nullptr;
    if (known && expected.present && target && dispatch.request.media_context_generation.present &&
        dispatch.request.media_context_generation.value == target->media_context_generation && media_fence_.generation)
      sent = client_.playback(dispatch.request.admission_ticket.serial, wire_op,
                              expected.value, media_fence_.generation);
  }
  if (!sent) {
    result.signal = AdapterEvidence::Signal::FAIL; result.delivery = Delivery::NOT_SENT;
    result.closed = true;
    result.failure = error(ErrorCode::UNAVAILABLE, "RTCP_NOT_SENT", RejectionOrigin::TARGET);
    sink.publish(result); return;
  }
  ++dispatches_; result.delivery = Delivery::SENT; result.progress.value.emitted = 1;
  sink.publish(result); pending_ = result;
}

bool RtcpDevice::poll(U64 now) {
  const auto event = client_.poll(now);
  if (event == rtcp_client::Client::Event::none) return true;
  if (event == rtcp_client::Client::Event::connected) {
    media_fence_.reset(); pending_ = {};
    if (!boundary_->reconnect(now) || !boundary_->reachability(Reachability::REACHABLE, now)) return false;
    availability(Availability::UNKNOWN, now); return true;
  }
  if (event == rtcp_client::Client::Event::disconnected) {
    media_fence_.reset(); pending_ = {}; availability(Availability::OFFLINE, now);
    return boundary_->reachability(Reachability::UNREACHABLE, now);
  }
  if (event == rtcp_client::Client::Event::capabilities) return negotiated(now);
  if (event == rtcp_client::Client::Event::snapshot) {
    const auto &snapshot = client_.snapshot();
    if (!media(media_fence_.snapshot(snapshot.sequence, snapshot.media_generation), now)) return false;
    Scalar volume = Scalar::level(volume_rational(snapshot.volume));
    Scalar playback; playback.kind = ScalarKind::PLAYBACK;
    playback.playback = static_cast<control::Playback>(snapshot.playback);
    // These are independent observations. A rejected volume sample must not
    // suppress the fresh playback sample and temporarily disable HOME control.
    const bool volume_ok = publish("volume.level", Value<Scalar>::of(volume),
                                   snapshot.sequence, now, false);
    const bool playback_ok = publish("playback.state", Value<Scalar>::of(playback),
                                     snapshot.sequence, now, true);
    return volume_ok && playback_ok;
  }
  if (event == rtcp_client::Client::Event::now_playing) {
    const auto &value = client_.now_playing();
    if (!media(media_fence_.metadata(value.sequence, value.media_generation,
                                     value.metadata_revision), now) || !context(value, now)) return false;
    const bool station_valid = value.flags & 0x01;
    const bool metadata_valid = value.flags & 0x02;
    const bool split = value.flags & 0x04;
    const auto station = station_valid ? text_value(value.station.data(), value.station_size)
                                       : Value<Scalar>::absent(Absence::UNAVAILABLE);
    const auto title = metadata_valid
        ? split ? text_value(value.title.data(), value.title_size)
                : text_value(value.combined.data(), value.combined_size)
        : Value<Scalar>::absent(Absence::UNAVAILABLE);
    const auto artist = metadata_valid && split
        ? text_value(value.artist.data(), value.artist_size)
        : Value<Scalar>::absent(Absence::UNAVAILABLE);
    return publish("media.subtitle", station, value.sequence, now, true) &&
           publish("media.title", title, value.sequence, now, true) &&
           publish("media.artist", artist, value.sequence, now, true);
  }
  if (event == rtcp_client::Client::Event::result && pending_.present) {
    auto result = pending_.value; const auto &wire_result = client_.result();
    result.evidence = evidence("a013.result", "authenticated correlated TUNER result");
    result.closed = true;
    if (wire_result.accepted) {
      result.signal = AdapterEvidence::Signal::COMPLETE;
      result.delivery = Delivery::ACKNOWLEDGED; result.progress.value.confirmed = 1;
    } else {
      result.signal = AdapterEvidence::Signal::REFUSE;
      result.delivery = Delivery::ACKNOWLEDGED; result.failure = d009_error(wire_result.error);
    }
    pending_ = {}; return boundary_->report(std::move(result), now);
  }
  return true;
}
} // namespace control_home
