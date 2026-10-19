#include "control_home/facade.hpp"
#include "control_home/volume.hpp"
#include <algorithm>
#include <cstring>

namespace control_home {
using namespace control;
namespace {
bool admitted(const Reply &reply) {
  // Core admission refusals live in CommandResult, not only Reply.error.
  return !reply.error.present && reply.result &&
         reply.result->lifecycle != Lifecycle::REJECTED &&
         reply.result->lifecycle != Lifecycle::EXPIRED_BEFORE_SEND;
}
} // namespace
const StateObservation *Facade::selected(const char *name) const {
  const auto *f = core_.field(device_, target_, name);
  return f && !f->conflict_latched && f->selected.present &&
                 f->selected.value.value.present
             ? &f->selected.value
             : nullptr;
}
bool Facade::available(const char *op) const {
  const auto *b = core_.binding(binding_);
  const auto *t = core_.target(device_, target_);
  if (core_.storage().blocked || !b || !t ||
      b->reachability != Reachability::REACHABLE ||
      b->authorization != Authorization::AUTHORIZED)
    return false;
  for (const auto &cap : t->capabilities)
    if (cap.key.kind == CapabilityKey::Kind::OPERATION &&
        cap.key.binding_id == binding_ && cap.key.id.view() == op)
      return cap.support == Support::SUPPORTED &&
             cap.availability == Availability::READY;
  return false;
}
Snapshot Facade::snapshot() const {
  Snapshot out;
  auto *b = core_.binding(binding_);
  auto *t = core_.target(device_, target_);
  if (!b || !t)
    return out;
  out.session = b->session_generation;
  out.capability_revision = t->capability_revision;
  out.connection = b->reachability == Reachability::REACHABLE &&
                           b->authorization == Authorization::AUTHORIZED
                       ? Connection::CONNECTED
                   : b->reachability == Reachability::UNREACHABLE
                       ? Connection::UNAVAILABLE
                       : Connection::UNKNOWN;
  if (out.connection != Connection::CONNECTED)
    return out;
  auto copy = [&](const char *field, auto &buffer) {
    auto *o = selected(field);
    if (o && o->value.value.kind == ScalarKind::STRING) {
      auto v = o->value.value.string.view();
      auto n = std::min(v.size(), sizeof(buffer) - 1);
      // Do not cut a UTF-8 codepoint at the fixed presentation bound.
      while (n < v.size() && n &&
             (static_cast<unsigned char>(v[n]) & 0xc0) == 0x80)
        --n;
      std::memcpy(buffer, v.data(), n);
    }
  };
  copy("media.title", out.title);
  copy("media.subtitle", out.subtitle);
  auto *p = selected("playback.state");
  if (p && p->value.value.kind == ScalarKind::PLAYBACK)
    out.playback = p->value.value.playback == control::Playback::PLAYING
                       ? Playback::PLAYING
                   : p->value.value.playback == control::Playback::PAUSED
                       ? Playback::PAUSED
                       : Playback::OTHER;
  else if (auto *f = core_.field(device_, target_, "playback.state");
           f && f->conflict_latched)
    out.playback = Playback::CONFLICT;
  if (auto *v = selected("volume.level");
      v && v->value.value.kind == ScalarKind::RATIONAL) {
    const auto &r = v->value.value.rational;
    if (r.normalized()) {
      out.volume_known = true;
      // Presentation rounding only; no command/native conversion.
      out.volume = static_cast<std::uint8_t>(
          static_cast<long double>(r.n) * 100 / r.d + 0.5L);
    }
  }
  out.previous = available("playback.previous");
  out.next = available("playback.next");
  out.volume_set = out.volume_known && available("volume.set");
  out.toggle =
      (out.playback == Playback::PLAYING && available("playback.pause")) ||
      (out.playback == Playback::PAUSED && available("playback.play"));
  return out;
}
bool Facade::handle(Intent intent, U64 now, bool accepting) {
  // Maintenance first: freshness must be evaluated at owner processing time.
  core_.tick(now);
  if (!intent.token || intent.token < watermark_)
    return false;
  if (intent.token == watermark_) {
    if (!accepting || !(intent == last_intent_) || !last_request_.present)
      return false;
    auto reply = core_.submit(last_request_.value, now);
    return admitted(reply);
  }
  watermark_ = intent.token;
  last_intent_ = intent;
  last_request_ = {};
  auto s = snapshot();
  if (!accepting || !intent.session || !intent.capability_revision ||
      intent.session != s.session ||
      intent.capability_revision != s.capability_revision)
    return false;
  const char *op = nullptr;
  switch (intent.action) {
  case Action::PREVIOUS:
    if (s.previous)
      op = "playback.previous";
    break;
  case Action::NEXT:
    if (s.next)
      op = "playback.next";
    break;
  case Action::TOGGLE:
    if (s.toggle)
      op = s.playback == Playback::PLAYING ? "playback.pause" : "playback.play";
    break;
  case Action::VOLUME_DOWN:
  case Action::VOLUME_UP: {
    const bool up = intent.action == Action::VOLUME_UP;
    if (!s.volume_set || !s.volume_known ||
        (up ? s.volume >= 100 : s.volume == 0) ||
        intent.volume > 100 ||
        (up ? intent.volume <= s.volume : intent.volume >= s.volume))
      break;
    op = "volume.set";
    break;
  }
  }
  if (!op || now > UINT64_MAX - 3000)
    return false;
  CommandRequest q;
  control::Arguments arguments;
  if (intent.action == Action::VOLUME_DOWN ||
      intent.action == Action::VOLUME_UP) {
    if (!arguments.fields.push(
            {control::Text::literal("level"),
             control::Scalar::level(volume_rational(intent.volume))}))
      return false;
  }
  auto capture =
      core_.capture(q, device_, target_, op, arguments, now + 3000, now);
  if (capture.error.present)
    return false;
  if (intent.action == Action::TOGGLE) {
    const auto *o = selected("playback.state");
    if (!o || !q.preconditions.push({Text::literal("playback.state"),
                                     o->observation_id, o->value}))
      return false;
  }
  if (intent.action == Action::VOLUME_DOWN ||
      intent.action == Action::VOLUME_UP) {
    const auto *o = selected("volume.level");
    if (!o || !q.preconditions.push({control::Text::literal("volume.level"),
                                     o->observation_id, o->value}))
      return false;
  }
  last_request_ = q;
  auto reply = core_.submit(q, now);
  return admitted(reply);
}
VolumeResolution Facade::volume_resolution(std::uint64_t token) const {
  if (!token || token != last_intent_.token || !last_request_.present)
    return VolumeResolution::PENDING;
  const auto *result = core_.result(last_request_.value.request_id);
  if (!result || !result->terminal)
    return VolumeResolution::PENDING;
  if (result->lifecycle == Lifecycle::REJECTED ||
      result->lifecycle == Lifecycle::EXPIRED_BEFORE_SEND)
    return VolumeResolution::DEFINITE_FAILURE;
  if (result->lifecycle == Lifecycle::CONFIRMED_COMPLETED)
    return VolumeResolution::CONFIRMED;
  if (result->lifecycle == Lifecycle::FAILED) {
    bool no_execution = result->attempts.size() != 0;
    for (const auto &attempt : result->attempts)
      no_execution = no_execution && attempt.guaranteed_no_execution;
    if (no_execution)
      return VolumeResolution::DEFINITE_FAILURE;
  }
  return VolumeResolution::UNCERTAIN;
}
} // namespace control_home
