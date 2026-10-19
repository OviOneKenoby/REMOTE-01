#include "control/ingress.hpp"

namespace control::ingress {
namespace {
template <class E> bool range(E value, E last) {
  using T = std::underlying_type_t<E>;
  return static_cast<T>(value) >= 0 && static_cast<T>(value) <= static_cast<T>(last);
}
bool identifier(const Identifier &v) {
  if (!v.valid() || !v.size) return false;
  for (char c : v.view())
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' || c == '-')) return false;
  return true;
}
bool sample_valid(const Sample &s) {
  if (!identifier(s.id) || !s.field.valid() || !s.field.size ||
      !identifier(s.sequence_domain) || !s.acquisition || !s.ordinal ||
      !s.max_age || s.max_age > 60000 || s.hops > s.provenance.size() ||
      !range(s.kind, ScalarKind::DIRECTION) || !range(s.absence, Absence::UNAVAILABLE) ||
      !range(s.confidence, Confidence::UNKNOWN) || !range(s.origin, Origin::RESYNC) ||
      !range(s.clock_quality, ClockQuality::UNKNOWN) ||
      !range(s.power, Power::STANDBY) || !range(s.playback, Playback::BUFFERING) ||
      !range(s.direction, Direction::DOWN) || !s.text.valid() ||
      !s.clock_contract.valid() || !range(s.clock_contract_absence, Absence::UNAVAILABLE)) return false;
  if (s.kind == ScalarKind::RATIONAL && s.present && !s.rational.valid()) return false;
  if (s.kind == ScalarKind::UINT32 && s.unsigned_value > UINT32_MAX) return false;
  for (unsigned i = 0; i < s.hops; ++i)
    if (!identifier(s.provenance[i].actor) || !identifier(s.provenance[i].evidence) ||
        !range(s.provenance[i].role, ProvenanceHop::Role::CORE)) return false;
  return true;
}
bool result_valid(const Result &r) {
  if (!identifier(r.request) || !identifier(r.evidence_id) || !r.attempt ||
      !r.mapping || !r.session || !range(r.signal, AdapterEvidence::Signal::FUTURE_FENCE) ||
      !range(r.evidence_kind, EvidenceKind::HARDWARE_TEST) || !r.reference.valid() ||
      r.claim_count > r.claims.size() || r.key_count > r.keys.size() ||
      !r.version.valid() || !range(r.version_absence, Absence::UNAVAILABLE) ||
      !r.detail.valid() || !range(r.error, ErrorCode::CANCELLED) ||
      (r.rejection_origin.present && !range(r.rejection_origin.value, RejectionOrigin::TARGET)) ||
      (r.delivery.present && !range(r.delivery.value, Delivery::AMBIGUOUS))) return false;
  if (r.progress.present) {
    auto p = r.progress.value;
    if (!p.total || p.confirmed > p.emitted || p.emitted > p.total) return false;
  }
  for (unsigned i = 0; i < r.claim_count; ++i) if (!r.claims[i].valid()) return false;
  for (unsigned i = 0; i < r.key_count; ++i) {
    auto &k = r.keys[i];
    if (!range(k.kind, LockKey::Kind::RESOURCE) ||
        (k.kind == LockKey::Kind::TARGET && (!identifier(k.device) || !identifier(k.target))) ||
        (k.kind == LockKey::Kind::RESOURCE && !identifier(k.resource))) return false;
  }
  return true;
}
bool unpack(const Sample &s, const Scope &scope, const Binding &b, U64 now, StateObservation &o) {
  if (!sample_valid(s)) return false;
  o.observation_id = Id(s.id.view());
  o.device_id = b.device_id; o.target_id = b.target_id; o.binding_id = b.binding_id;
  o.field_id = Text(s.field.view());
  Scalar value;
  value.kind = s.kind; value.boolean = s.boolean; value.unsigned_value = s.unsigned_value;
  value.signed_value = s.signed_value; value.rational = s.rational;
  value.power = s.power; value.playback = s.playback; value.direction = s.direction;
  if (s.present && s.kind == ScalarKind::STRING && !value.string.assign(s.text.view())) return false;
  o.value = s.present ? Value<Scalar>::of(value) : Value<Scalar>::absent(s.absence);
  o.confidence = s.confidence;
  for (unsigned i = 0; i < s.hops; ++i) {
    ProvenanceHop h{Id(s.provenance[i].actor.view()), s.provenance[i].role,
                    Id(s.provenance[i].evidence.view())};
    if (!h.actor_id.valid() || !h.evidence_id.valid() || !o.provenance.push(std::move(h))) return false;
  }
  o.device_association_generation = scope.device_association;
  o.association_generation = scope.association;
  o.mapping_revision = scope.mapping; o.session_generation = scope.session;
  o.receipt_time = {Id(scope.epoch.view()), now};
  o.acquisition_order = s.acquisition; o.origin = s.origin;
  if (s.observation_time.present) o.observation_time = MonoTime{o.receipt_time.clock_epoch, s.observation_time.value};
  o.age_at_receipt_ms = s.age;
  if (s.age.present) o.age_at_receipt_ms = s.age.value + (now - s.receipt);
  o.clock_quality = s.clock_quality;
  o.freshness.max_age_ms = s.max_age;
  o.freshness.acquisition = Acquisition::VERIFIED_SEQUENCE;
  if (s.clock_contract_present) {
    Text t(s.clock_contract.view());
    if (!t.valid()) return false;
    o.freshness.source_clock_contract = Value<Text>::of(t);
  } else o.freshness.source_clock_contract = Value<Text>::absent(s.clock_contract_absence);
  o.source_sequence = Sequence{Id(s.sequence_domain.view()), s.ordinal};
  o.media_context_generation = s.media;
  return o.observation_id.valid() && o.field_id.valid() && o.receipt_time.clock_epoch.valid() &&
         o.source_sequence.value.domain.valid();
}
bool unpack(const Result &r, const Binding &b, AdapterEvidence &e) {
  if (!result_valid(r)) return false;
  e.request_id = Id(r.request.view()); e.binding_id = b.binding_id;
  e.attempt_index = r.attempt; e.mapping_revision = r.mapping; e.session_generation = r.session;
  e.signal = r.signal; e.delivery = r.delivery; e.progress = r.progress; e.closed = r.closed;
  e.evidence.evidence_id = Id(r.evidence_id.view()); e.evidence.kind = r.evidence_kind;
  if (!e.evidence.reference.assign(r.reference.view())) return false;
  for (unsigned i = 0; i < r.claim_count; ++i) {
    Text claim(r.claims[i].view());
    if (!claim.valid() || !e.evidence.verified_claims.push(claim)) return false;
  }
  if (r.version_present) {
    Text v(r.version.view()); if (!v.valid()) return false;
    e.evidence.version = Value<Text>::of(v);
  } else e.evidence.version = Value<Text>::absent(r.version_absence);
  if (r.failure_present) {
    Error f; f.code = r.error; f.detail = Text(r.detail.view()); f.rejection_origin = r.rejection_origin;
    if (!f.detail.valid()) return false;
    e.failure = f;
  }
  for (unsigned i = 0; i < r.key_count; ++i) {
    LockKey k; k.kind = r.keys[i].kind;
    if (k.kind == LockKey::Kind::TARGET) {
      k.device_id = Id(r.keys[i].device.view()); k.target_id = Id(r.keys[i].target.view());
      if (!k.device_id.valid() || !k.target_id.valid()) return false;
    } else { k.resource_id = Id(r.keys[i].resource.view()); if (!k.resource_id.valid()) return false; }
    if (!e.covered_keys.push(std::move(k))) return false;
  }
  return e.request_id.valid() && e.evidence.evidence_id.valid();
}
} // namespace
bool pack(const StateObservation &o, Sample &s) {
  s.~Sample();
  new (&s) Sample{};
  if (o.source_time.present || o.pending_request_id.present ||
      !o.source_sequence.present || o.freshness.acquisition != Acquisition::VERIFIED_SEQUENCE ||
      o.provenance.size() > s.provenance.size() ||
      (o.observation_time.present && !(o.observation_time.value.clock_epoch == o.receipt_time.clock_epoch))) return false;
  if (!s.id.set(o.observation_id.text.view()) || !s.field.set(o.field_id.view()) ||
      !s.sequence_domain.set(o.source_sequence.value.domain.text.view())) return false;
  s.present = o.value.present; s.absence = o.value.reason;
  if (s.present) {
    auto &v = o.value.value;
    s.kind = v.kind; s.boolean = v.boolean; s.unsigned_value = v.unsigned_value;
    s.signed_value = v.signed_value; s.rational = v.rational; s.power = v.power;
    s.playback = v.playback; s.direction = v.direction;
    if (v.kind == ScalarKind::STRING && !s.text.set(v.string.view())) return false;
  }
  s.confidence = o.confidence; s.hops = o.provenance.size();
  for (unsigned i = 0; i < s.hops; ++i) {
    s.provenance[i].role = o.provenance[i].role;
    if (!s.provenance[i].actor.set(o.provenance[i].actor_id.text.view()) ||
        !s.provenance[i].evidence.set(o.provenance[i].evidence_id.text.view())) return false;
  }
  s.receipt = o.receipt_time.tick_ms; s.acquisition = o.acquisition_order;
  s.ordinal = o.source_sequence.value.ordinal; s.max_age = o.freshness.max_age_ms;
  s.origin = o.origin; s.clock_quality = o.clock_quality; s.age = o.age_at_receipt_ms;
  s.media = o.media_context_generation;
  if (o.observation_time.present) s.observation_time = o.observation_time.value.tick_ms;
  s.clock_contract_present = o.freshness.source_clock_contract.present;
  s.clock_contract_absence = o.freshness.source_clock_contract.reason;
  if (s.clock_contract_present && !s.clock_contract.set(o.freshness.source_clock_contract.value.view())) return false;
  return sample_valid(s);
}
bool pack(const AdapterEvidence &e, Result &r) {
  r.~Result();
  new (&r) Result{};
  if (e.evidence.verified_claims.size() > r.claims.size() ||
      e.covered_keys.size() > r.keys.size() ||
      (e.failure.present && (e.failure.value.cause.present || e.failure.value.step_index.present))) return false;
  if (!r.request.set(e.request_id.text.view()) || !r.evidence_id.set(e.evidence.evidence_id.text.view()) ||
      !r.reference.set(e.evidence.reference.view())) return false;
  r.attempt = e.attempt_index; r.mapping = e.mapping_revision; r.session = e.session_generation;
  r.signal = e.signal; r.evidence_kind = e.evidence.kind; r.delivery = e.delivery;
  r.progress = e.progress; r.closed = e.closed;
  r.claim_count = e.evidence.verified_claims.size();
  for (unsigned i = 0; i < r.claim_count; ++i) if (!r.claims[i].set(e.evidence.verified_claims[i].view())) return false;
  r.version_present = e.evidence.version.present; r.version_absence = e.evidence.version.reason;
  if (r.version_present && !r.version.set(e.evidence.version.value.view())) return false;
  r.failure_present = e.failure.present;
  if (r.failure_present) {
    r.error = e.failure.value.code; r.rejection_origin = e.failure.value.rejection_origin;
    if (!r.detail.set(e.failure.value.detail.view())) return false;
  }
  r.key_count = e.covered_keys.size();
  for (unsigned i = 0; i < r.key_count; ++i) {
    auto &k = e.covered_keys[i]; auto &p = r.keys[i]; p.kind = k.kind;
    if (!p.device.set(k.device_id.text.view()) || !p.target.set(k.target_id.text.view()) ||
        !p.resource.set(k.resource_id.text.view())) return false;
  }
  return result_valid(r);
}
Status Inbox::post(const Event &e, U64 *issued) {
  if (!lock()) {
    increment(rejected_); increment(busy_); fault_.store(true); return Status::BUSY;
  }
  Status result;
  if (!open_ || fault_.load()) { increment(rejected_); result = Status::CLOSED; }
  else if (count_ == capacity || serial_ == UINT64_MAX) {
    increment(rejected_); increment(overflow_); fault_.store(true); result = Status::FULL;
  } else {
    auto &slot = queue_[(head_ + count_) % capacity];
    slot = e; slot.queue_serial = ++serial_; if (issued) *issued = serial_; ++count_;
    increment(posted_); result = Status::POSTED;
  }
  unlock(); return result;
}
bool Inbox::scope(Scope &s) {
  if (!lock()) return false;
  bool ok = open_ && !fault_.load(); if (ok) s = scope_;
  unlock(); return ok;
}
Scope Boundary::current() const {
  Scope s; auto *b = core_.binding(binding_); if (!b) return s;
  auto *t = core_.target(b->device_id, b->target_id);
  auto *d = core_.device(b->device_id); if (!t || !d) return s;
  s.epoch.set(core_.storage().clock_epoch.text.view()); s.binding.set(binding_.text.view());
  s.incarnation = incarnation_; s.session = b->session_generation; s.mapping = b->mapping_revision;
  s.device_association = d->association_generation; s.association = t->association_generation;
  s.revision = t->capability_revision; s.media = t->media_context_generation; return s;
}
void Boundary::refresh() {
  if (inbox.lock()) { inbox.scope_ = current(); inbox.unlock(); }
}
bool Boundary::matches(const Scope &s, bool revision, bool media) const {
  auto n = current();
  return active_ && s.incarnation && s.session && s.mapping && s.association && s.device_association &&
         s.epoch == n.epoch && s.binding == n.binding && s.incarnation == n.incarnation &&
         s.session == n.session && s.mapping == n.mapping && s.association == n.association &&
         s.device_association == n.device_association && (!revision || s.revision == n.revision) &&
         (!media || s.media == n.media);
}
bool Boundary::attach(const Id &b, U64 now) {
  if (!owner() || active_ || incarnation_ == UINT64_MAX || !b.valid() ||
      !core_.binding(b) || core_.storage().blocked || !inbox.lock()) return false;
  inbox.open_ = false; inbox.head_ = inbox.count_ = 0; inbox.unlock();
  binding_ = b;
  if (incarnation_ && core_.reconnect(b, now).present) return false;
  ++incarnation_; active_ = true;
  if (!inbox.lock()) { active_ = false; return false; }
  inbox.scope_ = current(); inbox.fault_.store(false); inbox.open_ = true; inbox.unlock();
  ++counts.lifecycle; return true;
}
bool Boundary::shutdown(U64 now) {
  if (!owner()) return false;
  const bool was_active = active_;
  active_ = false;
  if (!inbox.lock()) {
    inbox.fault_.store(true);
    if (was_active) core_.reachability(binding_, Reachability::UNREACHABLE, now);
    return false;
  }
  inbox.open_ = false; counts.discarded += inbox.count_; inbox.count_ = inbox.head_ = 0;
  inbox.unlock();
  if (!was_active) return true;
  ++counts.lifecycle;
  return !core_.reachability(binding_, Reachability::UNREACHABLE, now).present;
}
Status Boundary::process_one(U64 now) {
  if (!owner()) return Status::WRONG_OWNER;
  if (inbox.fault_.load()) {
    shutdown(now); return Status::FAULT; // never erase historical effects/barriers
  }
  if (!active_) return Status::CLOSED;
  refresh();
  if (!inbox.lock()) return Status::BUSY;
  if (!inbox.count_) { inbox.unlock(); return Status::EMPTY; }
  scratch_ = inbox.queue_[inbox.head_];
  inbox.head_ = (inbox.head_ + 1) % Inbox::capacity; --inbox.count_; inbox.unlock();
  auto result = apply(scratch_, now);
  if (result == Status::APPLIED) ++counts.applied;
  else if (result == Status::STALE) ++counts.stale;
  else if (result == Status::INVALID) ++counts.invalid;
  else ++counts.refused;
  refresh(); return result;
}
Status Boundary::apply(const Event &e, U64 now) {
  if (!range(e.kind, Kind::SHUTDOWN)) return Status::INVALID;
  const auto *s = std::get_if<Sample>(&e.payload);
  const auto *r = std::get_if<Result>(&e.payload);
  const auto *cap = std::get_if<CapabilityChange>(&e.payload);
  if (!matches(e.scope, e.kind == Kind::SAMPLE || e.kind == Kind::AVAILABILITY || e.kind == Kind::WITHDRAW,
               e.kind == Kind::SAMPLE && s && s->media.present)) return Status::STALE;
  auto *b = core_.binding(binding_);
  switch (e.kind) {
  case Kind::SAMPLE: {
    if (!s || !sample_valid(*s) || s->receipt > now ||
        (s->observation_time.present && s->observation_time.value > s->receipt) ||
        (s->age.present && s->age.value > UINT64_MAX - (now - s->receipt))) return Status::INVALID;
    StateObservation o;
    // Core first receives this record now; retain acquisition time and add
    // staging delay to verified age. Receipt alone never proves freshness.
    if (!unpack(*s, e.scope, *b, now, o)) return Status::ALLOCATION_REFUSED;
    return observe(std::move(o), now) ? Status::APPLIED : Status::CORE_REJECTED;
  }
  case Kind::RESULT: {
    if (!r || !result_valid(*r)) return Status::INVALID;
    AdapterEvidence evidence;
    if (!unpack(*r, *b, evidence)) return Status::ALLOCATION_REFUSED;
    return report(std::move(evidence), now) ? Status::APPLIED : Status::CORE_REJECTED;
  }
  case Kind::AVAILABILITY:
  case Kind::WITHDRAW: {
    if (!cap || !cap->id.valid() || !cap->id.size || !range(cap->kind, CapabilityKey::Kind::FIELD) ||
        !range(cap->availability, Availability::UNKNOWN)) return Status::INVALID;
    CapabilityKey key{cap->kind, Text(cap->id.view()), binding_};
    if (!key.id.valid()) return Status::CORE_REJECTED;
    if (e.kind == Kind::WITHDRAW) return withdraw(key, now) ? Status::APPLIED : Status::CORE_REJECTED;
    for (const auto &existing : core_.target(b->device_id, b->target_id)->capabilities)
      if (existing.key == key) {
        auto copy = existing; copy.availability = cap->availability;
        return capability(std::move(copy), now) ? Status::APPLIED : Status::CORE_REJECTED;
      }
    return Status::CORE_REJECTED; // availability cannot create support/schema
  }
  default:
    if (!std::holds_alternative<std::monostate>(e.payload)) return Status::INVALID;
    if (e.kind == Kind::REACHABILITY) {
      if (!range(e.reachability, Reachability::UNVERIFIED)) return Status::INVALID;
      return reachability(e.reachability, now) ? Status::APPLIED : Status::CORE_REJECTED;
    }
    if (e.kind == Kind::RECONNECT) return reconnect(now) ? Status::APPLIED : Status::CORE_REJECTED;
    if (e.kind == Kind::MEDIA) return media_changed(now) ? Status::APPLIED : Status::CORE_REJECTED;
    return shutdown(now) ? Status::APPLIED : Status::CORE_REJECTED;
  }
}
bool Boundary::capability(CapabilityDescriptor c, U64 now) {
  if (!owner() || inbox.fault_.load() || !active_ || !(c.key.binding_id == binding_)) return false;
  auto *b = core_.binding(binding_);
  bool ok = !core_.capability(b->device_id, b->target_id, std::move(c), now).present; refresh(); return ok;
}
bool Boundary::withdraw(const CapabilityKey &key, U64 now) {
  if (!owner() || inbox.fault_.load() || !active_ || !(key.binding_id == binding_)) return false;
  auto *b = core_.binding(binding_);
  bool ok = !core_.withdraw(b->device_id, b->target_id, key, now).present; refresh(); return ok;
}
bool Boundary::reachability(Reachability value, U64 now) {
  if (!owner() || inbox.fault_.load() || !active_ || !range(value, Reachability::UNVERIFIED)) return false;
  bool ok = !core_.reachability(binding_, value, now).present; if (ok) ++counts.lifecycle;
  refresh(); return ok;
}
bool Boundary::reconnect(U64 now) {
  if (!owner() || inbox.fault_.load() || !active_) return false;
  bool ok = !core_.reconnect(binding_, now).present; if (ok) ++counts.lifecycle;
  refresh(); return ok;
}
bool Boundary::media_changed(U64 now) {
  if (!owner() || inbox.fault_.load() || !active_) return false;
  auto *b = core_.binding(binding_);
  bool ok = !core_.media_changed(b->device_id, b->target_id, now).present; refresh(); return ok;
}
bool Boundary::observe(StateObservation o, U64 now) {
  if (!owner() || inbox.fault_.load() || !active_ || !(o.binding_id == binding_)) return false;
  return core_.observe(std::move(o), now);
}
bool Boundary::report(AdapterEvidence e, U64 now) {
  if (!owner() || inbox.fault_.load() || !active_ || !(e.binding_id == binding_)) return false;
  return core_.report(std::move(e), now);
}
bool Boundary::enqueue(StateObservation o, U64 now) {
  if (!owner() || inbox.fault_.load() || !active_) return false;
  scratch_.scope = current(); scratch_.kind = Kind::SAMPLE;
  scratch_.payload.emplace<Sample>();
  if (!pack(o, std::get<Sample>(scratch_.payload)) ||
      !(o.binding_id == binding_) || o.receipt_time.clock_epoch.text.view() != scratch_.scope.epoch.view() ||
      o.session_generation != scratch_.scope.session || o.mapping_revision != scratch_.scope.mapping ||
      o.device_association_generation != scratch_.scope.device_association ||
      o.association_generation != scratch_.scope.association) return false;
  U64 issued = 0;
  if (inbox.post(scratch_, &issued) != Status::POSTED) return false;
  for (unsigned i = 0; i < Inbox::capacity; ++i) {
    auto result = process_one(now);
    if (scratch_.queue_serial == issued) return result == Status::APPLIED;
    if (result == Status::FAULT || result == Status::CLOSED || result == Status::BUSY) return false;
  }
  return false;
}
bool Boundary::enqueue(AdapterEvidence e, U64 now) {
  if (!owner() || inbox.fault_.load() || !active_ || !(e.binding_id == binding_)) return false;
  scratch_.scope = current(); scratch_.kind = Kind::RESULT;
  scratch_.payload.emplace<Result>();
  if (!pack(e, std::get<Result>(scratch_.payload))) return false;
  U64 issued = 0;
  if (inbox.post(scratch_, &issued) != Status::POSTED) return false;
  for (unsigned i = 0; i < Inbox::capacity; ++i) {
    auto result = process_one(now);
    if (scratch_.queue_serial == issued) return result == Status::APPLIED;
    if (result == Status::FAULT || result == Status::CLOSED || result == Status::BUSY) return false;
  }
  return false;
}
} // namespace control::ingress
