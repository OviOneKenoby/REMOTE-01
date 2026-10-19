#include "control/core.hpp"
#include <cstdio>
#include <span>

extern "C" void remote01_control_core_anchor() {}

namespace control {
namespace {
Optional<Error> fail(ErrorCode c, const char *d) { return error(c, d); }
bool is_media(std::string_view f) {
  return f.starts_with("media.") || f == "playback.state" ||
         f == "content.current";
}
bool is_write(std::string_view op) { return !op.ends_with(".enumerate"); }
bool context(const CapabilityDescriptor &d, Context c) {
  for (auto x : d.required_context)
    if (x == c)
      return true;
  return false;
}
bool primitive(const Scalar &v) {
  switch (v.kind) {
  case ScalarKind::UINT32:
    return v.unsigned_value <= UINT32_MAX;
  case ScalarKind::RATIONAL:
    return v.rational.valid();
  case ScalarKind::STRING:
    return v.string.valid();
  case ScalarKind::REFERENCE:
    return v.reference.device_id.valid() && v.reference.target_id.valid() &&
           v.reference.item_id.valid() && v.reference.catalog_generation &&
           v.reference.association_generation &&
           v.reference.device_association_generation;
  case ScalarKind::ASSET:
    return v.asset.asset_id.valid();
  case ScalarKind::POWER:
    return v.power <= Power::STANDBY;
  case ScalarKind::PLAYBACK:
    return v.playback <= Playback::BUFFERING;
  case ScalarKind::DIRECTION:
    return v.direction <= Direction::DOWN;
  default:
    return true;
  }
}
bool value_type(std::string_view f, const Value<Scalar> &v) {
  if (!v.present)
    return v.reason <= Absence::UNAVAILABLE;
  const auto k = v.value.kind;
  if (f == "power")
    return k == ScalarKind::POWER;
  if (f == "volume.level")
    return k == ScalarKind::RATIONAL && v.value.rational.normalized();
  if (f == "volume.mute")
    return k == ScalarKind::BOOL;
  if (f == "playback.state")
    return k == ScalarKind::PLAYBACK;
  if (f == "source.current" || f == "content.current")
    return k == ScalarKind::REFERENCE;
  if (f == "media.duration_ms" || f == "media.position_ms")
    return k == ScalarKind::UINT64;
  if (f == "media.artwork")
    return k == ScalarKind::ASSET;
  if (f == "media.title" || f == "media.artist" || f == "media.subtitle")
    return k == ScalarKind::STRING;
  return false;
}
bool synthetic_numeric_profile(const NumericMapping &n) {
  if (!(n.common_min == Rational{0, 1}) || !(n.common_max == Rational{1, 1}) ||
      n.sentinels.size())
    return false;
  if (n.transfer == NumericMapping::Transfer::TABLE)
    return n.set == NumericMapping::Set::FINITE && n.table.size() == 2 &&
           n.finite.size() == 2 && n.table[0].common == Rational{0, 1} &&
           n.table[1].common == Rational{1, 1} &&
           n.table[0].external == n.finite[0] &&
           n.table[1].external == n.finite[1] &&
           n.rounding == NumericMapping::Rounding::NEAREST_TIES_UP &&
           n.allowed_error == Rational{1, 2};
  return n.set == NumericMapping::Set::GRID &&
         n.external_min == Rational{0, 1} && n.external_max == Rational{1, 1} &&
         n.first == Rational{0, 1} && n.last == Rational{1, 1} &&
         n.step == Rational{1, 100} &&
         n.rounding == NumericMapping::Rounding::EXACT &&
         n.allowed_error == Rational{0, 1};
}
bool foundation_operation(std::string_view op) {
  for (auto x :
       {"power.set", "power.toggle", "volume.set", "volume.step", "volume.mute",
        "playback.play", "playback.pause", "playback.stop", "playback.previous",
        "playback.next", "source.select"})
    if (op == x)
      return true;
  return false;
}
int rational_compare(const Rational &a, const Rational &b) {
  if (a.n < 0 && b.n >= 0)
    return -1;
  if (a.n >= 0 && b.n < 0)
    return 1;
  U64 an = a.n < 0 ? U64(-(a.n + 1)) + 1 : U64(a.n),
      bn = b.n < 0 ? U64(-(b.n + 1)) + 1 : U64(b.n), ad = a.d, bd = b.d;
  int sign = a.n < 0 ? -1 : 1;
  while (true) {
    auto aq = an / ad, bq = bn / bd;
    if (aq != bq)
      return aq < bq ? -sign : sign;
    auto ar = an % ad, br = bn % bd;
    if (!ar || !br)
      return ar == br ? 0 : (!ar ? -sign : sign);
    an = ad;
    ad = ar;
    bn = bd;
    bd = br;
    sign = -sign;
  }
}
bool accepts(const SchemaNode &n, const Scalar &v) {
  using K = SchemaNode::Kind;
  switch (n.kind) {
  case K::BOOL:
    return v.kind == ScalarKind::BOOL;
  case K::UINT32:
  case K::UINT64:
    return v.kind == (n.kind == K::UINT32 ? ScalarKind::UINT32
                                          : ScalarKind::UINT64) &&
           v.unsigned_value >= n.min_u && v.unsigned_value <= n.max_u;
  case K::INT64:
    return v.kind == ScalarKind::INT64 && v.signed_value >= n.min_i &&
           v.signed_value <= n.max_i;
  case K::RATIONAL:
    return v.kind == ScalarKind::RATIONAL && v.rational.valid() &&
           rational_compare(v.rational, n.min) >= 0 &&
           rational_compare(v.rational, n.max) <= 0;
  case K::STRING:
    return v.kind == ScalarKind::STRING && v.string.valid() &&
           v.string.view().size() <= n.max_bytes;
  case K::REF:
    return v.kind == ScalarKind::REFERENCE && v.reference.kind == n.ref_kind;
  case K::ASSET_REF:
    return v.kind == ScalarKind::ASSET;
  case K::ENUM: {
    std::string_view token;
    if (v.kind == ScalarKind::POWER)
      token = v.power == Power::ON    ? "ON"
              : v.power == Power::OFF ? "OFF"
                                      : "STANDBY";
    else if (v.kind == ScalarKind::DIRECTION)
      token = v.direction == Direction::UP ? "UP" : "DOWN";
    else if (v.kind == ScalarKind::PLAYBACK)
      token = v.playback == Playback::PLAYING       ? "PLAYING"
              : v.playback == Playback::PAUSED      ? "PAUSED"
              : v.playback == Playback::NOT_PLAYING ? "NOT_PLAYING"
                                                    : "BUFFERING";
    else
      return false;
    for (const auto &x : n.values)
      if (x.view() == token)
        return true;
    return false;
  }
  default:
    return false;
  }
}
bool accepts_arguments(const Schema &s, const Arguments &args) {
  if (s.root >= s.nodes.size())
    return false;
  const auto &root = s.nodes[s.root];
  if (root.kind != SchemaNode::Kind::RECORD)
    return false;
  for (const auto &f : args.fields) {
    bool found = false;
    for (const auto &edge : root.edges)
      if (edge.name == f.key) {
        if (edge.node >= s.nodes.size() ||
            !accepts(s.nodes[edge.node], f.value))
          return false;
        found = true;
      }
    if (!found)
      return false;
  }
  for (const auto &edge : root.edges)
    if (edge.required && !args.find(edge.name.view()))
      return false;
  return true;
}
bool schema_graph(const Schema &s) {
  if (!s.nodes.valid() || !s.nodes.size() || s.root >= s.nodes.size())
    return false;
  std::array<unsigned char, 128> marks{};
  auto visit = [&](auto &&self, unsigned index) -> bool {
    if (index >= s.nodes.size() || marks[index] == 1)
      return false;
    if (marks[index] == 2)
      return true;
    marks[index] = 1;
    const auto &n = s.nodes[index];
    if (!n.edges.valid() || !n.values.valid())
      return false;
    if ((n.kind == SchemaNode::Kind::UINT32 &&
         (n.max_u > UINT32_MAX || n.min_u > n.max_u)) ||
        (n.kind == SchemaNode::Kind::UINT64 && n.min_u > n.max_u) ||
        (n.kind == SchemaNode::Kind::INT64 && n.min_i > n.max_i))
      return false;
    if (n.kind == SchemaNode::Kind::RATIONAL &&
        (!n.min.valid() || !n.max.valid() ||
         rational_compare(n.min, n.max) > 0))
      return false;
    if (n.kind == SchemaNode::Kind::ENUM && !n.values.size())
      return false;
    if (n.kind == SchemaNode::Kind::STRING && n.max_bytes > 4096)
      return false;
    if (n.kind == SchemaNode::Kind::UNION &&
        (!n.tag.valid() || !n.tag.view().size() || !n.edges.size()))
      return false;
    if (n.kind == SchemaNode::Kind::LIST &&
        (!n.element.present || !self(self, n.element.value)))
      return false;
    for (const auto &edge : n.edges)
      if (!edge.name.valid() || !edge.name.view().size() ||
          !self(self, edge.node))
        return false;
    marks[index] = 2;
    return true;
  };
  return visit(visit, s.root);
}
} // namespace
Core::Core(Store &s, Journal &j, Id epoch) : s_(s), journal_(j) {
  if (!epoch.valid()) {
    s_.blocked = true;
    return;
  }
  if (s_.clock_epoch.valid() && !(s_.clock_epoch == epoch)) {
    for (auto &r : s_.records)
      r.reset();
    for (auto &b : s_.bindings) {
      if (b.session_generation == UINT64_MAX)
        s_.blocked = true;
      else
        ++b.session_generation;
      b.reachability = Reachability::UNKNOWN;
    }
    s_.samples.clear(); // conflict participants live independently in target
                        // snapshots
    s_.watermark = 0;
    s_.epoch_base = s_.lifetime_counter;
    s_.now = 0;
  }
  s_.clock_epoch = std::move(epoch);
  persist();
}
bool Core::persist() {
  if (!journal_.commit(s_)) {
    s_.blocked = true;
    return false;
  }
  return true;
}
bool Core::begin(U64 now) {
  if (processing_ || now < s_.now || s_.ingress == UINT64_MAX)
    return false;
  processing_ = true;
  s_.now = now;
  ++s_.ingress;
  maintenance();
  return true;
}
void Core::end() {
  std::size_t consumed = 0;
  while (consumed++ < Limits::events) {
    if (!events_.size())
      schedule();
    if (!events_.size())
      break;
    auto e = std::move(events_[0]);
    events_.erase(0);
    if (s_.ingress == UINT64_MAX) {
      s_.blocked = true;
      break;
    }
    ++s_.ingress;
    maintenance();
    if (e.state)
      ingest(std::move(e.observation));
    else
      evidence(e.evidence);
  }
  if (events_.size())
    s_.blocked = true;
  for (auto &t : s_.targets)
    for (const auto &f : t.fields)
      reduce(t, f.field_id.view());
  persist();
  processing_ = false;
}
CommonDevice *Core::device(const Id &id) {
  for (auto &d : s_.devices)
    if (d.device_id == id)
      return &d;
  return nullptr;
}
Target *Core::target(const Id &d, const Id &id) {
  for (auto &t : s_.targets)
    if (t.device_id == d && t.target_id == id)
      return &t;
  return nullptr;
}
Binding *Core::binding(const Id &id) {
  for (auto &b : s_.bindings)
    if (b.binding_id == id)
      return &b;
  return nullptr;
}
Record *Core::record(const Id &id) {
  for (auto &p : s_.records)
    if (p.get() && p.get()->request.request_id == id)
      return p.get();
  return nullptr;
}
Effect *Core::effect(const Id &id, std::uint32_t idx) {
  for (auto &e : s_.effects)
    if (e.request_id == id && e.attempt_index == idx)
      return &e;
  return nullptr;
}
const CommandResult *Core::result(const Id &id) const {
  for (const auto &p : s_.records)
    if (p.get() && p.get()->request.request_id == id)
      return &p.get()->result;
  return nullptr;
}
const FieldSnapshot *Core::field(const Id &d, const Id &id,
                                 std::string_view f) const {
  for (const auto &t : s_.targets)
    if (t.device_id == d && t.target_id == id)
      for (const auto &x : t.fields)
        if (x.field_id.view() == f)
          return &x.snapshot;
  return nullptr;
}
CapabilityDescriptor *Core::descriptor(Target &t, const Id &b,
                                       std::string_view id,
                                       CapabilityKey::Kind k) {
  for (auto &c : t.capabilities)
    if (c.key.binding_id == b && c.key.id.view() == id && c.key.kind == k)
      return &c;
  return nullptr;
}
const CapabilityDescriptor *Core::descriptor(const Target &t, const Id &b,
                                             std::string_view id,
                                             CapabilityKey::Kind k) const {
  for (const auto &c : t.capabilities)
    if (c.key.binding_id == b && c.key.id.view() == id && c.key.kind == k)
      return &c;
  return nullptr;
}
Optional<Error> Core::register_device(CommonDevice d, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  if (!d.device_id.valid() || !d.display_name.valid() ||
      d.association_generation != 1)
    e = fail(ErrorCode::INVALID_ARGUMENT, "DEVICE_SCHEMA");
  else if (device(d.device_id))
    e = fail(ErrorCode::INVALID_REFERENCE, "IDENTITY_REUSE");
  else if (!s_.devices.push(std::move(d)))
    e = fail(ErrorCode::UNAVAILABLE, "DEVICE_CAPACITY");
  end();
  return e;
}
Optional<Error> Core::register_target(Target t, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *d = device(t.device_id);
  if (!t.target_id.valid() || !t.display_name.valid() ||
      t.association_generation != 1 || t.capability_revision != 1 ||
      t.media_context_generation != 1)
    e = fail(ErrorCode::INVALID_ARGUMENT, "TARGET_SCHEMA");
  else if (!d)
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (target(t.device_id, t.target_id))
    e = fail(ErrorCode::INVALID_REFERENCE, "IDENTITY_REUSE");
  else if (d->targets.size() == Limits::targets ||
           !s_.targets.push(std::move(t)))
    e = fail(ErrorCode::UNAVAILABLE, "TARGET_CAPACITY");
  else if (!d->targets.push(s_.targets[s_.targets.size() - 1].target_id)) {
    s_.targets.erase(s_.targets.size() - 1);
    e = fail(ErrorCode::UNAVAILABLE, "MEMORY_CAPACITY");
  }
  end();
  return e;
}
Optional<Error> Core::register_binding(Binding b, Adapter &a, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *d = device(b.device_id);
  auto *t = target(b.device_id, b.target_id);
  if (!b.binding_id.valid() || !a.identity().valid() ||
      b.mapping_revision != 1 || b.session_generation != 1)
    e = fail(ErrorCode::INVALID_ARGUMENT, "BINDING_SCHEMA");
  else if (!d || !t || b.association_generation != t->association_generation ||
           b.device_association_generation != d->association_generation)
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (binding(b.binding_id))
    e = fail(ErrorCode::INVALID_REFERENCE, "IDENTITY_REUSE");
  else {
    b.adapter_version = a.version();
    bool bs = s_.bindings.push(b);
    if (!bs)
      e = fail(ErrorCode::UNAVAILABLE, "BINDING_CAPACITY");
    else if (!s_.routes.push(AdapterRoute{b.binding_id, &a})) {
      s_.bindings.erase(s_.bindings.size() - 1);
      e = fail(ErrorCode::UNAVAILABLE, "MEMORY_CAPACITY");
    } else if (!d->bindings.push(b.binding_id)) {
      s_.routes.erase(s_.routes.size() - 1);
      s_.bindings.erase(s_.bindings.size() - 1);
      e = fail(ErrorCode::UNAVAILABLE, "MEMORY_CAPACITY");
    }
  }
  update_connectivity();
  end();
  return e;
}
Optional<Error> Core::register_resource(SharedResource r, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  if (!r.resource_id.valid() || !r.evidence.size())
    e = fail(ErrorCode::INVALID_ARGUMENT, "RESOURCE_EVIDENCE");
  for (const auto &x : s_.resources)
    if (x.resource_id == r.resource_id)
      e = fail(ErrorCode::INVALID_REFERENCE, "IDENTITY_REUSE");
  for (const auto &x : r.member_targets)
    if (!target(x.first, x.second))
      e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  for (const auto &member : r.member_targets) {
    auto *t = target(member.first, member.second);
    if (t && t->capability_revision == UINT64_MAX)
      e = fail(ErrorCode::UNAVAILABLE, "GENERATION_EXHAUSTED");
  }
  if (!e.present) {
    if (!s_.resources.push(r))
      e = fail(ErrorCode::UNAVAILABLE, "RESOURCE_CAPACITY");
    else {
      for (const auto &member : r.member_targets) {
        auto *t = target(member.first, member.second);
        ++t->capability_revision;
        for (auto &c : t->capabilities)
          c.capability_revision = t->capability_revision;
      }
      revalidate();
    }
  }
  end();
  return e;
}
Optional<Error> Core::capability(const Id &d, const Id &tid,
                                 CapabilityDescriptor c, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *t = target(d, tid);
  auto *b = binding(c.key.binding_id);
  if (!t || !b || !(b->device_id == d) || !(b->target_id == tid))
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (!c.key.id.valid() || !c.mapping_revision ||
           c.mapping_revision != b->mapping_revision ||
           t->capability_revision == UINT64_MAX)
    e = fail(ErrorCode::INVALID_ARGUMENT, "DESCRIPTOR_SCHEMA");
  else if (c.support == Support::SUPPORTED) {
    if (!c.evidence.size())
      e = fail(ErrorCode::INVALID_ARGUMENT, "MISSING_EVIDENCE");
    else if (c.key.kind == CapabilityKey::Kind::OPERATION) {
      if (!c.argument_schema.present || !c.result_schema.present ||
          !c.idempotence.present || !c.retry.present ||
          !c.execution_evidence.present)
        e = fail(ErrorCode::INVALID_ARGUMENT, "MISSING_CONTRACT");
      else if (!foundation_operation(c.key.id.view()))
        e = fail(ErrorCode::UNAVAILABLE, "OPERATION_SLICE_NOT_IMPLEMENTED");
      else if (!schema_graph(c.argument_schema.value) ||
               !schema_graph(c.result_schema.value))
        e = fail(ErrorCode::INVALID_ARGUMENT, "SCHEMA_GRAPH");
      else if (c.retry.value.automatic ||
               c.retry.value.fallback_on_no_execution ||
               c.retry.value.max_attempts != 1 || c.retry.value.delay_ms != 1)
        e = fail(ErrorCode::UNAVAILABLE, "RETRY_SLICE_NOT_IMPLEMENTED");
      else if (c.numeric_mapping.present &&
               !synthetic_numeric_profile(c.numeric_mapping.value))
        e = fail(ErrorCode::UNAVAILABLE, "NUMERIC_PROFILE_NOT_IMPLEMENTED");
    } else if (!c.value_schema.present || !schema_graph(c.value_schema.value) ||
               !c.freshness.present || !c.freshness.value.max_age_ms ||
               c.freshness.value.max_age_ms > 60000 ||
               c.argument_schema.present || c.result_schema.present ||
               c.retry.present || c.idempotence.present)
      e = fail(ErrorCode::INVALID_ARGUMENT, "FIELD_CONTRACT");
    else if (c.freshness.value.acquisition != Acquisition::VERIFIED_SEQUENCE)
      e = fail(ErrorCode::UNAVAILABLE, "SERIAL_ACQUISITION_NOT_IMPLEMENTED");
  }
  if (!e.present) {
    auto *old = descriptor(*t, c.key.binding_id, c.key.id.view(), c.key.kind);
    c.capability_revision = t->capability_revision + 1;
    if (old)
      *old = std::move(c);
    else if (!t->capabilities.push(std::move(c)))
      e = fail(ErrorCode::UNAVAILABLE, "CAPABILITY_CAPACITY");
    if (!e.present) {
      ++t->capability_revision;
      for (auto &x : t->capabilities)
        x.capability_revision = t->capability_revision;
      revalidate();
    }
  }
  end();
  return e;
}
void Core::update_connectivity() {
  for (auto &d : s_.devices) {
    d.connectivity.routes.clear();
    std::size_t usable = 0, unreachable = 0, unverified = 0, total = 0;
    for (const auto &b : s_.bindings)
      if (b.device_id == d.device_id) {
        ++total;
        if (b.reachability == Reachability::REACHABLE &&
            b.authorization == Authorization::AUTHORIZED)
          ++usable;
        if (b.reachability == Reachability::UNREACHABLE)
          ++unreachable;
        if (b.reachability == Reachability::UNVERIFIED)
          ++unverified;
        RouteConnectivity r{b.reachability, b.authorization, b.last_contact};
        if (!d.connectivity.routes.push({b.binding_id, r}))
          s_.blocked = true;
      }
    d.connectivity.status =
        total && usable == total        ? ConnectivityStatus::REACHABLE
        : usable                        ? ConnectivityStatus::PARTIAL
        : total && unreachable == total ? ConnectivityStatus::UNREACHABLE
        : unverified                    ? ConnectivityStatus::UNVERIFIED
                                        : ConnectivityStatus::UNKNOWN;
  }
}
Optional<Error> Core::reachability(const Id &id, Reachability value, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *b = binding(id);
  if (!b)
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (value != Reachability::REACHABLE &&
           target(b->device_id, b->target_id)->capability_revision == UINT64_MAX)
    e = fail(ErrorCode::UNAVAILABLE, "GENERATION_EXHAUSTED");
  else {
    b->reachability = value;
    if (value == Reachability::REACHABLE)
      b->last_contact = MonoTime{s_.clock_epoch, now};
    else {
      for (auto &sample : s_.samples)
        if (sample.binding_id == id)
          sample.invalid = true;
      auto *t = target(b->device_id, b->target_id);
      ++t->capability_revision;
      for (auto &cap : t->capabilities) {
        cap.capability_revision = t->capability_revision;
        if (cap.key.binding_id == id)
          cap.availability = value == Reachability::UNREACHABLE
                                 ? Availability::OFFLINE : Availability::UNKNOWN;
      }
    }
    update_connectivity();
    revalidate();
  }
  end();
  return e;
}
Optional<Error> Core::withdraw(const Id &d, const Id &tid,
                             const CapabilityKey &key, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *t = target(d, tid);
  if (!t)
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (t->capability_revision == UINT64_MAX)
    e = fail(ErrorCode::UNAVAILABLE, "GENERATION_EXHAUSTED");
  else {
    for (std::size_t i = 0; i < t->capabilities.size(); ++i)
      if (t->capabilities[i].key == key) {
        t->capabilities.erase(i);
        ++t->capability_revision;
        for (auto &cap : t->capabilities)
          cap.capability_revision = t->capability_revision;
        revalidate();
        break;
      }
  }
  end();
  return e;
}
Optional<Error> Core::reconnect(const Id &id, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *b = binding(id);
  if (!b)
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (b->session_generation == UINT64_MAX)
    e = fail(ErrorCode::UNAVAILABLE, "GENERATION_EXHAUSTED");
  else {
    ++b->session_generation;
    b->reachability = Reachability::UNKNOWN;
    for (auto &x : s_.samples)
      if (x.binding_id == id)
        x.invalid = true;
    auto *t = target(b->device_id, b->target_id);
    if (t && t->capability_revision < UINT64_MAX) {
      ++t->capability_revision;
      for (auto &c : t->capabilities) {
        c.capability_revision = t->capability_revision;
        if (c.key.binding_id == id)
          c.availability = Availability::UNKNOWN;
      }
    } else
      s_.blocked = true;
    revalidate();
  }
  update_connectivity();
  end();
  return e;
}
Optional<Error> Core::media_changed(const Id &d, const Id &id, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *t = target(d, id);
  if (!t)
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (t->media_context_generation == UINT64_MAX)
    e = fail(ErrorCode::UNAVAILABLE, "GENERATION_EXHAUSTED");
  else {
    ++t->media_context_generation;
    for (auto &x : s_.samples)
      if (x.observation.device_id == d && x.target_id == id &&
          is_media(x.field_id.view()))
        x.invalid = true;
    revalidate();
  }
  end();
  return e;
}
U64 Core::begin_resync(const Id &b, std::string_view f, U64 now) {
  if (!begin(now))
    return 0;
  for (auto &x : s_.samples)
    if (x.binding_id == b && x.field_id.view() == f)
      x.resync_started = s_.ingress;
  U64 ticket = s_.ingress;
  end();
  return ticket;
}
bool Core::fresh(const StateObservation &o) const {
  if (!(o.receipt_time.clock_epoch == s_.clock_epoch) ||
      o.receipt_time.tick_ms > s_.now)
    return false;
  auto *b = const_cast<Core *>(this)->binding(o.binding_id);
  auto *t = const_cast<Core *>(this)->target(o.device_id, o.target_id);
  auto *d = const_cast<Core *>(this)->device(o.device_id);
  if (!b || !t || !d || b->session_generation != o.session_generation ||
      b->mapping_revision != o.mapping_revision ||
      t->association_generation != o.association_generation ||
      d->association_generation != o.device_association_generation)
    return false;
  auto *c = descriptor(*t, o.binding_id, o.field_id.view(),
                       CapabilityKey::Kind::FIELD);
  if (!c || c->support != Support::SUPPORTED || !c->freshness.present ||
      !(c->freshness.value == o.freshness))
    return false;
  if (is_media(o.field_id.view()) &&
      (!o.media_context_generation.present ||
       o.media_context_generation.value != t->media_context_generation))
    return false;
  U64 age = 0;
  bool bounded = false;
  if (o.age_at_receipt_ms.present) {
    age = o.age_at_receipt_ms.value;
    bounded = true;
  }
  if (o.observation_time.present && o.clock_quality == ClockQuality::BOUNDED &&
      o.observation_time.value.clock_epoch == s_.clock_epoch &&
      o.observation_time.value.tick_ms <= o.receipt_time.tick_ms) {
    age = std::max(age,
                   o.receipt_time.tick_ms - o.observation_time.value.tick_ms);
    bounded = true;
  }
  U64 current = 0;
  if (!bounded || !add(age, s_.now - o.receipt_time.tick_ms, current) ||
      current >= o.freshness.max_age_ms)
    return false;
  if (o.value.present && o.value.value.kind == ScalarKind::REFERENCE) {
    const auto &r = o.value.value.reference;
    bool found = false;
    for (const auto &c : s_.catalogs)
      if (c.snapshot.device_id == o.device_id &&
          c.snapshot.target_id == o.target_id && c.snapshot.kind == r.kind &&
          c.snapshot.catalog_generation == r.catalog_generation)
        for (const auto &it : c.snapshot.items)
          if (it.item_id == r.item_id && it.reference == r)
            found = true;
    if (!found)
      return false;
  }
  return true;
}
bool Core::ingest(StateObservation o) {
  auto *b = binding(o.binding_id);
  auto *t = target(o.device_id, o.target_id);
  auto *d = device(o.device_id);
  if (!b || !t || !d || !(b->device_id == o.device_id) ||
      !(b->target_id == o.target_id) || !o.observation_id.valid() ||
      !o.acquisition_order || !value_type(o.field_id.view(), o.value) ||
      (o.value.present && !primitive(o.value.value)))
    return false;
  auto *c = descriptor(*t, o.binding_id, o.field_id.view(),
                       CapabilityKey::Kind::FIELD);
  if (!c || c->support != Support::SUPPORTED || !c->freshness.present ||
      !(o.freshness == c->freshness.value))
    return false;
  if (o.value.present &&
      (!c->value_schema.present ||
       !accepts(c->value_schema.value.nodes[c->value_schema.value.root],
                o.value.value)))
    return false;
  if (o.session_generation != b->session_generation ||
      o.mapping_revision != b->mapping_revision ||
      o.association_generation != t->association_generation ||
      o.device_association_generation != d->association_generation)
    return false;
  if (is_media(o.field_id.view()) &&
      (!o.media_context_generation.present ||
       o.media_context_generation.value != t->media_context_generation))
    return false;
  if ((o.value.present && o.confidence == Confidence::UNKNOWN) ||
      (!o.value.present && o.confidence != Confidence::UNKNOWN) ||
      o.confidence == Confidence::ASSUMED)
    return false; // overlays deliberately disabled
  if (!(o.receipt_time.clock_epoch == s_.clock_epoch) ||
      o.receipt_time.tick_ms != s_.now)
    return false;
  if (o.observation_time.present &&
      (!(o.observation_time.value.clock_epoch == s_.clock_epoch) ||
       o.observation_time.value.tick_ms > o.receipt_time.tick_ms))
    return false;
  SourceSample *old = nullptr;
  for (auto &x : s_.samples)
    if (x.binding_id == o.binding_id && x.field_id == o.field_id)
      old = &x;
  bool conflict = false;
  if (o.freshness.acquisition == Acquisition::VERIFIED_SEQUENCE) {
    if (!o.source_sequence.present || !o.source_sequence.value.domain.valid() ||
        !o.source_sequence.value.ordinal)
      return false;
    if (old && old->observation.source_sequence.present &&
        old->observation.session_generation == o.session_generation &&
        old->observation.mapping_revision == o.mapping_revision) {
      const auto &a = old->observation.source_sequence.value;
      const auto &v = o.source_sequence.value;
      if (!(a.domain == v.domain)) {
        if (b->session_generation == UINT64_MAX ||
            t->capability_revision == UINT64_MAX) {
          s_.blocked = true;
          return false;
        }
        ++b->session_generation;
        ++t->capability_revision;
        b->reachability = Reachability::UNKNOWN;
        for (auto &cap : t->capabilities) {
          cap.capability_revision = t->capability_revision;
          if (cap.key.binding_id == b->binding_id)
            cap.availability = Availability::UNKNOWN;
        }
        for (auto &sample : s_.samples)
          if (sample.binding_id == b->binding_id)
            sample.invalid = true;
        revalidate();
        update_connectivity();
        return false;
      }
      if (v.ordinal < a.ordinal)
        return false;
      if (v.ordinal == a.ordinal) {
        if (o.value == old->observation.value &&
            o.media_context_generation ==
                old->observation.media_context_generation)
          return false;
        old->invalid = true;
        conflict = true;
      }
    }
  } else if (old && o.acquisition_order <= old->observation.acquisition_order)
    return false;
  if (!old) {
    SourceSample x;
    x.target_id = o.target_id;
    x.binding_id = o.binding_id;
    x.field_id = o.field_id;
    if (!s_.samples.push(std::move(x)))
      return false;
    old = &s_.samples[s_.samples.size() - 1];
  }
  auto started = old->resync_started;
  old->observation = std::move(o);
  old->invalid = conflict;
  old->resync_started = started;
  FieldEntry *f = nullptr;
  for (auto &x : t->fields)
    if (x.field_id == old->field_id)
      f = &x;
  if (!f) {
    FieldEntry x;
    x.field_id = old->field_id;
    if (!t->fields.push(std::move(x)))
      return false;
    f = &t->fields[t->fields.size() - 1];
  }
  if (conflict) {
    auto &snap = f->snapshot;
    snap.conflict_latched = true;
    bool found = false;
    for (auto &p : snap.conflict_state.participants)
      if (p.binding_id == old->binding_id)
        found = true;
    if (!found) {
      ConflictParticipant p;
      p.binding_id = old->binding_id;
      if (!snap.conflict_state.participants.push(std::move(p))) {
        s_.blocked = true;
        return false;
      }
    }
    for (auto &p : snap.conflict_state.participants) {
      p.conflict_ingress = s_.ingress;
      p.resync_observation = Value<StateObservation>{};
      p.resync_started_ingress = Value<U64>{};
    }
    increment(snap.conflict_state.revision);
  }
  reduce(*t, old->field_id.view());
  return !conflict;
}
void Core::reduce(Target &t, std::string_view name) {
  FieldEntry *entry = nullptr;
  for (auto &f : t.fields)
    if (f.field_id.view() == name)
      entry = &f;
  if (!entry)
    return;
  auto &snap = entry->snapshot;
  // Bounded transient selection pointers: no owning heap state is needed.
  std::array<const SourceSample *, Limits::bindings> candidates{};
  std::size_t count = 0;
  for (const auto &x : s_.samples)
    if (x.target_id == t.target_id && x.observation.device_id == t.device_id &&
        x.field_id.view() == name && !x.invalid && fresh(x.observation) &&
        x.observation.value.present) {
      if (count == candidates.size()) {
        s_.blocked = true;
        return;
      }
      candidates[count++] = &x;
    }
  std::span<const SourceSample *> eligible(candidates.data(), count);
  std::sort(eligible.begin(), eligible.end(),
            [this](const auto *a, const auto *b) {
              const auto &x = a->observation;
              const auto &y = b->observation;
              if (x.confidence != y.confidence)
                return x.confidence < y.confidence;
              auto *xb = binding(x.binding_id);
              auto *yb = binding(y.binding_id);
              if (xb->owner_priority != yb->owner_priority)
                return xb->owner_priority < yb->owner_priority;
              if (!(x.binding_id == y.binding_id))
                return x.binding_id < y.binding_id;
              return x.observation_id < y.observation_id;
            });
  auto previous_conflicts = std::move(snap.conflict_set);
  if (eligible.size()) {
    // Preserve identical evidence without deep-copying its provenance list.
    if (!snap.selected.present ||
        !(snap.selected.value == eligible[0]->observation))
      snap.selected = Value<StateObservation>::of(eligible[0]->observation);
    bool disagree = false;
    for (const auto *x : eligible)
      if (!(x->observation.value == eligible[0]->observation.value))
        disagree = true;
    if (disagree) {
      bool episode = !snap.conflict_latched;
      for (const auto *x : eligible) {
        for (const auto &previous : previous_conflicts)
          if (previous.binding_id == x->binding_id &&
              !(previous.value == x->observation.value))
            episode = true;
        if (!snap.conflict_set.push(x->observation)) {
          s_.blocked = true;
          return;
        }
        bool found = false;
        for (auto &p : snap.conflict_state.participants)
          if (p.binding_id == x->binding_id) {
            found = true;
            if (p.resync_observation.present &&
                !(p.resync_observation.value.value == x->observation.value))
              episode = true;
          }
        if (!found) {
          ConflictParticipant p;
          p.binding_id = x->binding_id;
          p.conflict_ingress = s_.ingress;
          if (!snap.conflict_state.participants.push(std::move(p))) {
            s_.blocked = true;
            return;
          }
          episode = true;
        }
      }
      snap.conflict_latched = true;
      if (episode) {
        for (auto &p : snap.conflict_state.participants) {
          p.conflict_ingress = s_.ingress;
          p.resync_started_ingress = Value<U64>{};
          p.resync_observation = Value<StateObservation>{};
        }
        increment(snap.conflict_state.revision);
      }
    }
  } else {
    bool unknown = false, unavailable = false;
    std::size_t total = 0, unsupported = 0;
    for (const auto &b : s_.bindings)
      if (b.device_id == t.device_id && b.target_id == t.target_id) {
        ++total;
        auto *c = descriptor(t, b.binding_id, name, CapabilityKey::Kind::FIELD);
        if (!c || c->support == Support::UNKNOWN) {
          unknown = true;
          continue;
        }
        if (c->support == Support::UNSUPPORTED) {
          ++unsupported;
          continue;
        }
        const SourceSample *sample = nullptr;
        for (const auto &x : s_.samples)
          if (x.binding_id == b.binding_id && x.field_id.view() == name &&
              !x.invalid && fresh(x.observation))
            sample = &x;
        if (sample && !sample->observation.value.present) {
          auto r = sample->observation.value.reason;
          if (r == Absence::UNKNOWN)
            unknown = true;
          else if (r == Absence::UNAVAILABLE)
            unavailable = true;
          else if (r == Absence::UNSUPPORTED)
            ++unsupported;
        } else if (c->observation ==
                       CapabilityDescriptor::Observation::UNAVAILABLE ||
                   c->availability == Availability::OFFLINE ||
                   c->availability == Availability::UNAUTHORIZED ||
                   c->availability == Availability::BLOCKED ||
                   c->availability == Availability::CONTEXT_UNAVAILABLE)
          unavailable = true;
        else
          unknown = true;
      }
    snap.selected = Value<StateObservation>::absent(
        !total || unknown      ? Absence::UNKNOWN
        : unsupported == total ? Absence::UNSUPPORTED
        : unavailable          ? Absence::UNAVAILABLE
                               : Absence::NOT_APPLICABLE);
  }
  auto &participants = snap.conflict_state.participants;
  std::sort(
      participants.begin(), participants.end(),
      [](const auto &a, const auto &b) { return a.binding_id < b.binding_id; });
  if (snap.conflict_latched) {
    bool all = participants.size() > 0;
    Optional<Value<Scalar>> agreed;
    for (auto &p : participants) {
      const SourceSample *x = nullptr;
      for (const auto &s : s_.samples)
        if (s.binding_id == p.binding_id && s.field_id.view() == name)
          x = &s;
      bool good = x && !x->invalid && fresh(x->observation) &&
                  x->observation.origin == Origin::RESYNC &&
                  x->resync_started > p.conflict_ingress;
      if (!good) {
        all = false;
        continue;
      }
      p.resync_started_ingress = Value<U64>::of(x->resync_started);
      p.resync_observation = Value<StateObservation>::of(x->observation);
      if (!agreed.present)
        agreed = x->observation.value;
      else if (!(agreed.value == x->observation.value))
        all = false;
    }
    for (const auto *x : eligible)
      if (agreed.present && !(x->observation.value == agreed.value))
        all = false;
    if (all) {
      snap.conflict_latched = false;
      participants.clear();
      increment(snap.conflict_state.revision);
    }
  }
}

bool Core::known_operation(std::string_view op) const {
  static constexpr std::string_view ops[] = {"power.set",
                                             "power.toggle",
                                             "volume.set",
                                             "volume.step",
                                             "volume.mute",
                                             "playback.play",
                                             "playback.pause",
                                             "playback.stop",
                                             "playback.previous",
                                             "playback.next",
                                             "playback.seek",
                                             "playback.rewind",
                                             "playback.fast_forward",
                                             "navigation.up",
                                             "navigation.down",
                                             "navigation.left",
                                             "navigation.right",
                                             "navigation.select",
                                             "navigation.back",
                                             "navigation.home",
                                             "source.select",
                                             "channel.select",
                                             "channel.step",
                                             "favorites.activate",
                                             "application.launch",
                                             "content.launch",
                                             "source.enumerate",
                                             "channel.enumerate",
                                             "favorites.enumerate",
                                             "application.enumerate",
                                             "content.enumerate"};
  for (auto x : ops)
    if (x == op)
      return true;
  return op.starts_with("vendor.");
}
bool Core::schema(const CommandRequest &q) const {
  if (!q.operation_id.valid() || !q.arguments.fields.valid())
    return false;
  for (std::size_t i = 0; i < q.arguments.fields.size(); ++i) {
    const auto &f = q.arguments.fields[i];
    if (!f.key.valid() || !primitive(f.value))
      return false;
    for (std::size_t j = 0; j < i; ++j)
      if (f.key == q.arguments.fields[j].key)
        return false;
  }
  auto op = q.operation_id.view();
  auto has = [&](std::string_view k, ScalarKind t) {
    auto *p = q.arguments.find(k);
    return p && p->kind == t;
  };
  if (!known_operation(op))
    return true;
  if (op == "power.set")
    return q.arguments.fields.size() == 1 && has("state", ScalarKind::POWER);
  if (op == "volume.set")
    return q.arguments.fields.size() == 1 &&
           has("level", ScalarKind::RATIONAL) &&
           q.arguments.find("level")->rational.normalized();
  if (op == "volume.mute")
    return q.arguments.fields.size() == 1 && has("muted", ScalarKind::BOOL);
  if (op == "volume.step" || op == "channel.step")
    return q.arguments.fields.size() == 2 &&
           has("direction", ScalarKind::DIRECTION) &&
           has("count", ScalarKind::UINT32) &&
           q.arguments.find("count")->unsigned_value >= 1 &&
           q.arguments.find("count")->unsigned_value <= 65535;
  if (op == "playback.seek")
    return q.arguments.fields.size() == 1 &&
           has("position_ms", ScalarKind::UINT64);
  if (op == "source.select")
    return q.arguments.fields.size() == 1 &&
           has("source", ScalarKind::REFERENCE) &&
           q.arguments.find("source")->reference.kind == CatalogKind::SOURCE;
  if (op == "favorites.activate")
    return q.arguments.fields.size() == 1 &&
           has("favorite", ScalarKind::REFERENCE) &&
           q.arguments.find("favorite")->reference.kind ==
               CatalogKind::FAVORITE;
  if (op == "content.launch")
    return q.arguments.fields.size() == 2 && has("tag", ScalarKind::STRING) &&
           q.arguments.find("tag")->string.view() == "REFERENCE" &&
           has("content", ScalarKind::REFERENCE) &&
           q.arguments.find("content")->reference.kind == CatalogKind::CONTENT;
  // No ready descriptors for parameterized/vendor/channel/app operations in
  // this slice.
  if (op == "channel.select" || op == "application.launch" ||
      op.starts_with("vendor."))
    return true;
  return q.arguments.fields.size() == 0;
}
Optional<Error> Core::validate(const CommandRequest &q, bool handover) {
  if (!(q.common_model_version == Version{}))
    return fail(ErrorCode::INVALID_ARGUMENT, "VERSION_MISMATCH");
  if (!q.device_id.valid() || !q.target_id.valid() || !q.binding_id.valid() ||
      !q.capability_revision || !q.mapping_revision ||
      !q.device_association_generation || !q.association_generation ||
      !q.route_plan.size())
    return fail(ErrorCode::INVALID_ARGUMENT, "MISSING_CAPTURE");
  if (!schema(q))
    return fail(ErrorCode::INVALID_ARGUMENT, "SCHEMA");
  if (q.recipe_capture.present || q.route_plan.size() != 1)
    return fail(ErrorCode::UNAVAILABLE, "FOUNDATION_SLICE_NOT_IMPLEMENTED");
  if (!(q.route_plan[0].binding_id == q.binding_id) ||
      q.route_plan[0].mapping_revision != q.mapping_revision ||
      q.route_plan[0].capability_revision != q.capability_revision)
    return fail(ErrorCode::INVALID_ARGUMENT, "SCHEMA");
  auto *d = device(q.device_id);
  auto *t = target(q.device_id, q.target_id);
  auto *b = binding(q.binding_id);
  if (!d || !t || !b || !(b->device_id == q.device_id) ||
      !(b->target_id == q.target_id))
    return fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  for (const auto &f : q.arguments.fields)
    if (f.value.kind == ScalarKind::REFERENCE) {
      const auto &r = f.value.reference;
      if (!(r.device_id == q.device_id) || !(r.target_id == q.target_id))
        return fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
    }
  if (d->association_generation != q.device_association_generation ||
      t->association_generation != q.association_generation ||
      t->capability_revision != q.capability_revision ||
      b->mapping_revision != q.mapping_revision)
    return fail(ErrorCode::STALE_PRECONDITION, "CAPTURE_CHANGED");
  if (q.media_context_generation.present &&
      q.media_context_generation.value != t->media_context_generation)
    return fail(ErrorCode::STALE_PRECONDITION, "CAPTURE_CHANGED");
  for (const auto &f : q.arguments.fields)
    if (f.value.kind == ScalarKind::REFERENCE) {
      const auto &r = f.value.reference;
      const CatalogRecord *found = nullptr;
      for (const auto &c : s_.catalogs)
        if (c.snapshot.device_id == q.device_id &&
            c.snapshot.target_id == q.target_id && c.snapshot.kind == r.kind)
          found = &c;
      if (!found)
        return fail(ErrorCode::UNAVAILABLE, "NO_VERIFIED_CONTEXT");
      if (!q.catalog_generation.present || !q.token_generation.present)
        return fail(ErrorCode::INVALID_ARGUMENT, "MISSING_CAPTURE");
      if (r.association_generation != t->association_generation ||
          r.device_association_generation != d->association_generation ||
          found->snapshot.catalog_generation != r.catalog_generation ||
          q.catalog_generation.value != r.catalog_generation ||
          q.token_generation.value != found->token_generation)
        return fail(ErrorCode::STALE_PRECONDITION, "CAPTURE_CHANGED");
      bool item = false;
      for (const auto &i : found->snapshot.items)
        if (i.item_id == r.item_id)
          item = true;
      if (!item)
        return fail(ErrorCode::INVALID_REFERENCE, "UNKNOWN_ITEM");
    }
  auto *c = descriptor(*t, q.binding_id, q.operation_id.view(),
                       CapabilityKey::Kind::OPERATION);
  if (!c || c->support == Support::UNKNOWN)
    return fail(ErrorCode::UNSUPPORTED_OPERATION, "UNKNOWN_SUPPORT");
  if (c->support == Support::UNSUPPORTED)
    return fail(ErrorCode::UNSUPPORTED_OPERATION, "UNSUPPORTED");
  if (!c->execution_evidence.present ||
      !(c->execution_evidence.value == q.route_plan[0].execution_contract))
    return fail(ErrorCode::STALE_PRECONDITION, "CAPTURE_CHANGED");
  if (!c->argument_schema.present ||
      !accepts_arguments(c->argument_schema.value, q.arguments))
    return fail(ErrorCode::INVALID_ARGUMENT, "DOMAIN");
  if (b->authorization == Authorization::UNAUTHORIZED ||
      b->authorization == Authorization::BLOCKED)
    return fail(ErrorCode::UNAUTHORIZED, "AUTHORIZATION");
  if (context(*c, Context::MEDIA) &&
      (!q.media_context_generation.present ||
       c->execution_evidence.value.context_guard != Guarantee::VERIFIED))
    return fail(ErrorCode::UNAVAILABLE, "NO_VERIFIED_CONTEXT");
  if (context(*c, Context::CATALOG) &&
      (!q.catalog_generation.present || !q.token_generation.present ||
       c->execution_evidence.value.selection_guard != Guarantee::VERIFIED))
    return fail(ErrorCode::UNAVAILABLE, "NO_VERIFIED_CONTEXT");
  if (q.operation_id.view() == "playback.seek") {
    const SeekWindow *w = nullptr;
    for (const auto &x : s_.windows)
      if (x.first == q.target_id)
        w = &x.second;
    if (!w || w->guard != Guarantee::VERIFIED)
      return fail(ErrorCode::UNAVAILABLE, "NO_VERIFIED_SEEK_WINDOW");
    if (!q.seek_window_generation.present)
      return fail(ErrorCode::INVALID_ARGUMENT, "MISSING_CAPTURE");
    if (q.seek_window_generation.value != w->generation)
      return fail(ErrorCode::STALE_PRECONDITION, "CAPTURE_CHANGED");
    auto pos = q.arguments.find("position_ms")->unsigned_value;
    if (pos < w->min_ms || pos > w->max_ms)
      return fail(ErrorCode::INVALID_ARGUMENT, "DOMAIN");
  }
  if (q.operation_id.view() == "volume.set") {
    if (!c->numeric_mapping.present ||
        !synthetic_numeric_profile(c->numeric_mapping.value))
      return fail(ErrorCode::UNSUPPORTED_OPERATION, "UNKNOWN_SUPPORT");
    const auto &r = q.arguments.find("level")->rational;
    if (c->numeric_mapping.value.transfer == NumericMapping::Transfer::AFFINE &&
        (r.d > 100 || 100 % r.d))
      return fail(ErrorCode::INVALID_ARGUMENT, "DOMAIN");
  }
  for (const auto &p : q.preconditions) {
    const auto *f = field(q.device_id, q.target_id, p.field_id.view());
    if (!f || f->conflict_latched || !f->selected.present ||
        !fresh(f->selected.value) ||
        !(f->selected.value.observation_id == p.observation_id) ||
        !(f->selected.value.value == p.expected_value))
      return fail(ErrorCode::STALE_PRECONDITION, "FIELD_PRECONDITION");
  }
  if (!handover) {
    if (!(q.deadline.clock_epoch == s_.clock_epoch) ||
        q.deadline.tick_ms > s_.now + std::min(U64(60000), UINT64_MAX - s_.now))
      return fail(ErrorCode::INVALID_ARGUMENT, "DEADLINE");
  }
  return {};
}
Id Core::expected_id(U64 serial) const {
  U64 n = 0;
  if (!add(s_.epoch_base, serial, n))
    return {};
  char b[48];
  std::snprintf(b, sizeof b, "intent:%llu", static_cast<unsigned long long>(n));
  return Id(b);
}
bool Core::matches_id(const Id &id, U64 serial) const {
  U64 number = 0;
  if (!add(s_.epoch_base, serial, number))
    return false;
  char bytes[48];
  std::snprintf(bytes, sizeof bytes, "intent:%llu",
                static_cast<unsigned long long>(number));
  return id.text.view() == bytes;
}
Reply Core::capture(CommandRequest &q, const Id &d, const Id &tid,
                    std::string_view op, const Arguments &args, U64 deadline,
                    U64 now) {
  if (!begin(now))
    return {nullptr, fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP")};
  auto *dev = device(d);
  auto *t = target(d, tid);
  Binding *selected = nullptr;
  CapabilityDescriptor *cap = nullptr;
  if (t)
    for (auto &b : s_.bindings)
      if (b.device_id == d && b.target_id == tid) {
        auto *c =
            descriptor(*t, b.binding_id, op, CapabilityKey::Kind::OPERATION);
        if (!c || c->support != Support::SUPPORTED)
          continue;
        if (!selected ||
            ((b.authorization == Authorization::AUTHORIZED) !=
                     (selected->authorization == Authorization::AUTHORIZED)
                 ? b.authorization == Authorization::AUTHORIZED
             : b.owner_priority != selected->owner_priority
                 ? b.owner_priority < selected->owner_priority
                 : b.binding_id < selected->binding_id)) {
          selected = &b;
          cap = c;
        }
      }
  Optional<Error> e;
  if (!dev || !t)
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (!selected)
    e = fail(ErrorCode::UNSUPPORTED_OPERATION, "UNKNOWN_SUPPORT");
  else if (s_.watermark == UINT64_MAX || s_.lifetime_counter == UINT64_MAX)
    e = fail(ErrorCode::UNAVAILABLE, "TICKET_EXHAUSTED");
  else {
    q = CommandRequest{};
    q.device_id = d;
    q.target_id = tid;
    q.operation_id = Text(op);
    q.arguments = args;
    q.request_id = expected_id(s_.watermark + 1);
    q.admission_ticket = {s_.clock_epoch, s_.watermark + 1};
    q.deadline = {s_.clock_epoch, deadline};
    q.binding_id = selected->binding_id;
    q.capability_revision = t->capability_revision;
    q.mapping_revision = selected->mapping_revision;
    q.device_association_generation = dev->association_generation;
    q.association_generation = t->association_generation;
    if (context(*cap, Context::MEDIA))
      q.media_context_generation = t->media_context_generation;
    if (context(*cap, Context::SEEK_WINDOW)) {
      for (const auto &w : s_.windows)
        if (w.first == tid)
          q.seek_window_generation = w.second.generation;
      if (!q.seek_window_generation.present)
        e = fail(ErrorCode::UNAVAILABLE, "NO_VERIFIED_SEEK_WINDOW");
    }
    if (context(*cap, Context::CATALOG))
      for (const auto &f : args.fields)
        if (f.value.kind == ScalarKind::REFERENCE) {
          q.catalog_generation = f.value.reference.catalog_generation;
          for (const auto &c : s_.catalogs)
            if (c.snapshot.device_id == d && c.snapshot.target_id == tid &&
                c.snapshot.kind == f.value.reference.kind)
              q.token_generation = c.token_generation;
          if (!q.token_generation.present)
            e = fail(ErrorCode::UNAVAILABLE, "NO_VERIFIED_CONTEXT");
        }
    RouteCapture route;
    route.binding_id = selected->binding_id;
    route.mapping_revision = selected->mapping_revision;
    route.capability_revision = t->capability_revision;
    route.execution_contract = cap->execution_evidence.value;
    route.token_generation = q.token_generation;
    if (!q.route_plan.push(std::move(route)) || !q.arguments.fields.valid() ||
        !q.request_id.valid())
      e = fail(ErrorCode::UNAVAILABLE, "MEMORY_CAPACITY");
  }
  end();
  return {nullptr, e};
}
List<LockKey, Limits::resources + 1> Core::keys(const CommandRequest &q) {
  List<LockKey, Limits::resources + 1> result;
  if (!is_write(q.operation_id.view()))
    return result;
  LockKey targetkey;
  targetkey.device_id = q.device_id;
  targetkey.target_id = q.target_id;
  if (!result.push(targetkey)) {
    s_.blocked = true;
    return result;
  }
  auto *t = target(q.device_id, q.target_id);
  auto addkey = [&](const Id &id) {
    LockKey k;
    k.kind = LockKey::Kind::RESOURCE;
    k.resource_id = id;
    for (const auto &x : result)
      if (x == k)
        return;
    if (!result.push(std::move(k)))
      s_.blocked = true;
  };
  if (t) {
    for (const auto &id : t->resource_ids)
      addkey(id);
    auto *c = descriptor(*t, q.binding_id, q.operation_id.view(),
                         CapabilityKey::Kind::OPERATION);
    if (c)
      for (const auto &id : c->resource_ids)
        addkey(id);
  }
  for (const auto &r : s_.resources)
    for (const auto &member : r.member_targets)
      if (member.first == q.device_id && member.second == q.target_id)
        addkey(r.resource_id);
  std::sort(result.begin(), result.end());
  return result;
}
bool Core::blocked(const List<LockKey, Limits::resources + 1> &keys,
                   const Id &own) const {
  for (const auto &e : s_.effects)
    if (!(e.request_id == own))
      for (const auto &k : e.keys)
        if (!k.no_future)
          for (const auto &wanted : keys)
            if (k.key == wanted)
              return true;
  return false;
}
void Core::close(Record &r, Lifecycle state, Outcome outcome, Value<Error> e) {
  auto &x = r.result;
  bool first = !x.terminal;
  x.lifecycle = state;
  x.target_outcome = outcome;
  x.error = std::move(e);
  x.terminal = true;
  x.waiting_reason = Value<Text>::absent(Absence::NOT_APPLICABLE);
  if (first) {
    x.first_terminal_time = Value<MonoTime>::of({s_.clock_epoch, s_.now});
    x.closure_error = x.error;
  }
}
Reply Core::submit(const CommandRequest &q, U64 now) {
  if (!begin(now))
    return {nullptr, fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP")};
  Optional<Error> e;
  Record *existing = nullptr;
  if (!q.request_id.valid() || !q.admission_ticket.clock_epoch.valid() ||
      !q.admission_ticket.serial)
    e = fail(ErrorCode::INVALID_ARGUMENT, "INVALID_TICKET");
  else if (!(q.admission_ticket.clock_epoch == s_.clock_epoch))
    e = fail(ErrorCode::REQUEST_HISTORY_EXPIRED, "OLD_EPOCH");
  else if (!matches_id(q.request_id, q.admission_ticket.serial))
    e = fail(ErrorCode::REQUEST_ID_MISMATCH, "ID_TICKET_PAIR");
  else if ((existing = record(q.request_id))) {
    if (!(existing->request == q))
      e = fail(ErrorCode::REQUEST_ID_MISMATCH, "ENVELOPE");
    const auto *result = &existing->result;
    end();
    return {result, e};
  } else if (q.admission_ticket.serial <= s_.watermark)
    e = fail(ErrorCode::REQUEST_HISTORY_EXPIRED, "HISTORY_EXPIRED");
  else if (s_.watermark == UINT64_MAX || s_.lifetime_counter == UINT64_MAX ||
           q.admission_ticket.serial != s_.watermark + 1)
    e = fail(ErrorCode::REQUEST_ID_MISMATCH, "UNISSUED_PAIR");
  if (e.present) {
    end();
    return {nullptr, e};
  }
  ++s_.watermark;
  ++s_.lifetime_counter; // compact, bijective ID-ticket tombstone
  e = validate(q, false);
  if (!e.present && q.deadline.tick_ms <= s_.now)
    e = fail(ErrorCode::DEADLINE_EXPIRED, "DEADLINE");
  std::size_t free = Limits::records, queued = 0;
  for (std::size_t i = 0; i < Limits::records; ++i) {
    if (!s_.records[i].get()) {
      if (free == Limits::records)
        free = i;
    } else if (s_.records[i].get()->result.lifecycle == Lifecycle::QUEUED)
      ++queued;
  }
  if (!e.present && free == Limits::records)
    e = fail(ErrorCode::UNAVAILABLE, "DEDUP_CAPACITY");
  else if (!e.present && queued == Limits::queue)
    e = fail(ErrorCode::UNAVAILABLE, "QUEUE_FULL");
  if (free == Limits::records) {
    end();
    return refused(q, e.present
                          ? e.value
                          : error(ErrorCode::UNAVAILABLE, "DEDUP_CAPACITY"));
  }
  auto &p = s_.records[free];
  if (!p.create()) {
    end();
    return refused(q, error(ErrorCode::UNAVAILABLE, "MEMORY_CAPACITY"));
  }
  auto &r = *p.get();
  r.request = q;
  r.result.request_id = q.request_id;
  auto *count = q.arguments.find("count");
  if (count && count->kind == ScalarKind::UINT32 &&
      count->unsigned_value >= 1 && count->unsigned_value <= 65535)
    r.result.progress.total = static_cast<std::uint32_t>(count->unsigned_value);
  if (!r.request.arguments.fields.valid() || !r.request.route_plan.valid())
    e = fail(ErrorCode::UNAVAILABLE, "MEMORY_CAPACITY");
  if (!e.present) {
    r.keys = keys(q);
    r.contract = q.route_plan[0].execution_contract;
    if (!r.keys.valid())
      e = fail(ErrorCode::UNAVAILABLE, "MEMORY_CAPACITY");
  }
  if (e.present)
    close(r,
          e.value.code == ErrorCode::DEADLINE_EXPIRED
              ? Lifecycle::EXPIRED_BEFORE_SEND
              : Lifecycle::REJECTED,
          e.value.code == ErrorCode::DEADLINE_EXPIRED ? Outcome::UNKNOWN
                                                      : Outcome::REJECTED,
          Value<Error>::of(e.value));
  const auto *output = &r.result;
  end();
  return {output, {}};
}
Reply Core::refused(const CommandRequest &q, const Error &e) {
  refusal_ = CommandResult{};
  refusal_.request_id = q.request_id;
  refusal_.lifecycle = Lifecycle::REJECTED;
  refusal_.terminal = true;
  refusal_.target_outcome = Outcome::REJECTED;
  refusal_.error = Value<Error>::of(e);
  refusal_.closure_error = refusal_.error;
  refusal_.first_terminal_time = Value<MonoTime>::of({s_.clock_epoch, s_.now});
  return {&refusal_, {}};
}
void Core::schedule() {
  if (s_.blocked)
    return;
  // Selection is serial, not array position (slots are reused after purge).
  List<Record *, Limits::queue> ready;
  for (auto &p : s_.records)
    if (p.get() && !p.get()->result.terminal &&
        p.get()->result.attempts.size() == 0) {
      if (!ready.push(p.get())) {
        s_.blocked = true;
        return;
      }
    }
  std::sort(ready.begin(), ready.end(), [](auto *a, auto *b) {
    return a->request.admission_ticket.serial <
           b->request.admission_ticket.serial;
  });
  for (auto *r : ready) {
    auto &q = r->request;
    auto &out = r->result;
    if (s_.now >= q.deadline.tick_ms)
      continue;
    auto *b = binding(q.binding_id);
    auto *t = target(q.device_id, q.target_id);
    if (!b || !t)
      continue;
    auto *c = descriptor(*t, q.binding_id, q.operation_id.view(),
                         CapabilityKey::Kind::OPERATION);
    if (!c)
      continue;
    const char *wait = nullptr;
    if (b->authorization == Authorization::UNKNOWN)
      wait = "AUTHORIZATION_UNKNOWN";
    else if (c->availability != Availability::READY)
      wait = c->availability == Availability::CONTEXT_UNAVAILABLE
                 ? "CONTEXT_WAIT"
                 : "AVAILABILITY_WAIT";
    else if (blocked(r->keys, q.request_id))
      wait = "ORDERING_BLOCKED";
    else if (s_.effects.size() == Limits::barriers)
      wait = "AVAILABILITY_WAIT";
    if (wait) {
      auto val = Value<Text>::of(Text::literal(wait));
      if (!(out.waiting_reason == val)) {
        out.waiting_reason = val;
        increment(out.result_revision);
      }
      continue;
    }
    auto invalid = validate(q, true);
    if (invalid.present) {
      close(*r, Lifecycle::REJECTED, Outcome::REJECTED,
            Value<Error>::of(invalid.value));
      increment(out.result_revision);
      continue;
    }
    Effect ef;
    ef.request_id = q.request_id;
    ef.binding_id = q.binding_id;
    ef.device_id = q.device_id;
    ef.target_id = q.target_id;
    ef.mapping_revision = q.mapping_revision;
    ef.session_generation = b->session_generation;
    ef.feedback = r->contract.feedback;
    ef.progress = out.progress;
    for (const auto &k : r->keys) {
      EffectKey item;
      item.key = k;
      item.mode = t->ordering_mode;
      if (k.kind == LockKey::Kind::RESOURCE) {
        item.mode = Ordering::STRICT;
        for (const auto &rs : s_.resources)
          if (rs.resource_id == k.resource_id)
            item.mode = rs.ordering_mode;
      }
      if (!ef.keys.push(std::move(item))) {
        s_.blocked = true;
        return;
      }
    }
    Attempt a;
    a.binding_id = q.binding_id;
    a.mapping_revision = q.mapping_revision;
    a.session_generation = b->session_generation;
    a.dispatch_time = {s_.clock_epoch, s_.now};
    for (const auto &k : r->keys)
      if (k.kind == LockKey::Kind::RESOURCE)
        if (!a.resource_ids.push(k.resource_id)) {
          s_.blocked = true;
          return;
        }
    if (!out.attempts.push(std::move(a))) {
      s_.blocked = true;
      return;
    }
    if (!s_.effects.push(std::move(ef))) {
      out.attempts.erase(0); // atomic reservation: no orphan attempt on OOM
      s_.blocked = true;
      return;
    }
    out.lifecycle = Lifecycle::DISPATCHED;
    increment(out.result_revision);
    out.waiting_reason = Value<Text>::absent(Absence::NOT_APPLICABLE);
    if (!persist())
      return;
    Adapter *adapter = nullptr;
    for (const auto &route : s_.routes)
      if (route.binding_id == q.binding_id)
        adapter = route.adapter;
    if (adapter) {
      Optional<Rational> mapped;
      if (q.operation_id.view() == "volume.set") {
        const auto &level = q.arguments.find("level")->rational;
        const auto &mapping = c->numeric_mapping.value;
        if (mapping.transfer == NumericMapping::Transfer::TABLE)
          mapped = mapping.table[U64(level.n) >= level.d - U64(level.n) ? 1 : 0]
                       .external;
        else
          mapped = level;
      }
      out.attempts[0].delivery = Delivery::AMBIGUOUS;
      out.delivery = Delivery::AMBIGUOUS;
      out.target_outcome = Outcome::UNKNOWN;
      if (!persist())
        return;
      adapter->dispatch({q, out.attempts[0], mapped}, *this);
    } else {
      r->stopped = true;
      r->stop_error = error(ErrorCode::UNAVAILABLE, "ADAPTER_UNAVAILABLE");
      auto *effect_ = effect(q.request_id, 1);
      for (auto &k : effect_->keys) {
        k.no_past = true;
        k.no_future = true;
      }
      out.attempts[0].guaranteed_no_execution = true;
      fold(*r);
    }
  }
}
void Core::fold(Record &r) {
  auto &x = r.result;
  auto *ef = effect(r.request.request_id, 1);
  if (!x.attempts.size() || !ef)
    return;
  auto &a = x.attempts[0];
  x.delivery = a.delivery;
  bool uncertain =
      ((a.delivery == Delivery::AMBIGUOUS || ef->uncertain_extent) &&
       !a.guaranteed_no_execution && !ef->complete) ||
      (r.contract.feedback == Feedback::CORRELATED_RESULT && !ef->complete &&
       !ef->zero());
  x.progress.uncertain_remaining = uncertain;
  if (x.progress.emitted > 0 && x.progress.emitted < x.progress.total &&
      a.delivery != Delivery::AMBIGUOUS)
    x.delivery = Delivery::PARTIALLY_SENT;
  if (ef->complete && x.progress.confirmed == x.progress.total &&
      ef->resolved()) {
    x.delivery = Delivery::ACKNOWLEDGED;
    close(r, Lifecycle::CONFIRMED_COMPLETED, Outcome::CONFIRMED,
          Value<Error>::absent(Absence::NOT_APPLICABLE));
    return;
  }
  if (ef->refused && ef->zero()) {
    close(r, Lifecycle::REJECTED, Outcome::REJECTED,
          Value<Error>::of(r.stop_error.present
                               ? r.stop_error.value
                               : error(ErrorCode::UNAVAILABLE, "TARGET_REFUSAL",
                                       RejectionOrigin::TARGET)));
    return;
  }
  if (!r.stopped && !x.terminal) {
    bool full =
        x.progress.emitted == x.progress.total &&
        (a.delivery == Delivery::SENT || a.delivery == Delivery::ACKNOWLEDGED);
    if (full && r.contract.feedback == Feedback::NONE) {
      bool local = true;
      for (auto &k : ef->keys)
        if (k.mode != Ordering::LOCAL_ONLY)
          local = false;
      if (local)
        for (auto &k : ef->keys)
          k.no_future = true;
      close(r, Lifecycle::ACCEPTED, Outcome::UNCONFIRMED,
            Value<Error>::absent(Absence::NOT_APPLICABLE));
    } else {
      x.lifecycle = full ? Lifecycle::ACCEPTED : Lifecycle::DISPATCHED;
      x.target_outcome =
          a.delivery == Delivery::AMBIGUOUS || a.delivery == Delivery::NOT_SENT
              ? Outcome::UNKNOWN
              : Outcome::UNCONFIRMED;
    }
    return;
  }
  auto cause = r.stop_error.present
                   ? r.stop_error.value
                   : error(ErrorCode::AMBIGUOUS_DELIVERY, "UNRESOLVED");
  if (uncertain) {
    Error e = cause;
    if (e.code != ErrorCode::TIMEOUT) {
      e = error(ErrorCode::AMBIGUOUS_DELIVERY, "UNRESOLVED");
      e.cause = static_cast<const ErrorCause &>(cause);
    }
    close(r, Lifecycle::UNCERTAIN_OUTCOME,
          x.delivery == Delivery::AMBIGUOUS ? Outcome::UNKNOWN
                                            : Outcome::UNCONFIRMED,
          Value<Error>::of(e));
  } else if (x.progress.emitted && x.progress.emitted < x.progress.total) {
    auto e = error(ErrorCode::PARTIAL_EXECUTION, "KNOWN_PREFIX");
    e.cause = static_cast<const ErrorCause &>(cause);
    close(r, Lifecycle::FAILED, Outcome::UNCONFIRMED, Value<Error>::of(e));
  } else if (!r.stopped && x.progress.emitted == x.progress.total)
    close(r, Lifecycle::ACCEPTED, Outcome::UNCONFIRMED,
          Value<Error>::absent(Absence::NOT_APPLICABLE));
  else
    close(r, Lifecycle::FAILED,
          a.delivery == Delivery::NOT_SENT ? Outcome::UNKNOWN
                                           : Outcome::UNCONFIRMED,
          Value<Error>::of(cause));
}
void Core::evidence(const AdapterEvidence &e) {
  auto *ef = effect(e.request_id, e.attempt_index);
  auto *r = record(e.request_id);
  if (!ef || !(e.binding_id == ef->binding_id) ||
      e.mapping_revision != ef->mapping_revision ||
      e.session_generation != ef->session_generation ||
      !e.evidence.evidence_id.valid())
    return;
  for (const auto &id : ef->evidence_ids)
    if (id == e.evidence.evidence_id)
      return;
  if (e.progress.present &&
      (e.progress.value.total == 0 ||
       e.progress.value.confirmed > e.progress.value.emitted ||
       e.progress.value.emitted > e.progress.value.total ||
       (e.progress.value.total != ef->progress.total)))
    return;
  if (e.signal == AdapterEvidence::Signal::COMPLETE &&
      (ef->zero() || ef->refused))
    return;
  if ((e.signal == AdapterEvidence::Signal::NO_EXECUTION ||
       e.signal == AdapterEvidence::Signal::REFUSE) &&
      ef->complete)
    return;
  if (e.progress.present &&
      (e.progress.value.emitted < ef->progress.emitted ||
       e.progress.value.confirmed < ef->progress.confirmed))
    return;
  if ((e.signal == AdapterEvidence::Signal::NO_EXECUTION ||
       e.signal == AdapterEvidence::Signal::REFUSE) &&
      ef->progress.confirmed)
    return;
  if (r && e.delivery.present &&
      e.signal != AdapterEvidence::Signal::NO_EXECUTION &&
      e.signal != AdapterEvidence::Signal::FUTURE_FENCE) {
    auto previous = r->result.attempts[e.attempt_index - 1].delivery;
    if (previous == Delivery::ACKNOWLEDGED &&
        e.delivery.value != Delivery::ACKNOWLEDGED)
      return;
    if (previous == Delivery::SENT && e.delivery.value != Delivery::SENT &&
        e.delivery.value != Delivery::ACKNOWLEDGED)
      return;
  }
  if (e.signal == AdapterEvidence::Signal::COMPLETE) {
    if (!e.progress.present ||
        e.progress.value.confirmed != e.progress.value.total ||
        e.progress.value.uncertain_remaining || !e.delivery.present ||
        e.delivery.value != Delivery::ACKNOWLEDGED)
      return;
    for (const auto &k : ef->keys) {
      bool covered = false;
      for (const auto &key : e.covered_keys)
        if (key == k.key)
          covered = true;
      if (!covered)
        return;
    }
    for (const auto &k : ef->keys)
      if (k.no_past)
        return;
  }
  for (const auto &key : e.covered_keys) {
    bool found = false;
    for (const auto &k : ef->keys)
      if (k.key == key)
        found = true;
    if (!found)
      return;
  }
  if (!ef->evidence_ids.push(e.evidence.evidence_id)) {
    s_.blocked = true;
    return;
  }
  bool proof = e.signal == AdapterEvidence::Signal::NO_EXECUTION ||
               e.signal == AdapterEvidence::Signal::REFUSE ||
               e.signal == AdapterEvidence::Signal::FUTURE_FENCE ||
               e.signal == AdapterEvidence::Signal::COMPLETE;
  if (proof)
    for (auto &k : ef->keys)
      for (const auto &key : e.covered_keys)
        if (k.key == key) {
          k.no_future = true;
          if (e.signal == AdapterEvidence::Signal::NO_EXECUTION ||
              e.signal == AdapterEvidence::Signal::REFUSE)
            k.no_past = true;
        }
  if (e.signal == AdapterEvidence::Signal::COMPLETE) {
    ef->complete = true;
  }
  if (e.signal == AdapterEvidence::Signal::REFUSE) {
    ef->refused = true;
  }
  if (e.progress.present) {
    ef->progress = e.progress.value;
    ef->uncertain_extent = e.progress.value.uncertain_remaining;
  }
  if (!r)
    return; // independent mandatory effect state survives cache purge/reboot
  auto &a = r->result.attempts[e.attempt_index - 1];
  if (!a.evidence.push(e.evidence)) {
    s_.blocked = true;
    return;
  }
  if (e.delivery.present && e.signal != AdapterEvidence::Signal::NO_EXECUTION &&
      e.signal != AdapterEvidence::Signal::FUTURE_FENCE)
    a.delivery = e.delivery.value;
  if (e.delivery.present && e.delivery.value == Delivery::NOT_SENT &&
      e.signal == AdapterEvidence::Signal::FAIL) {
    for (auto &k : ef->keys) {
      k.no_past = true;
      k.no_future = true;
    }
  }
  if (e.signal == AdapterEvidence::Signal::COMPLETE) {
    a.delivery = Delivery::ACKNOWLEDGED;
    a.target_outcome = Outcome::CONFIRMED;
  }
  a.guaranteed_no_execution = ef->zero();
  if (e.progress.present) {
    auto &p = r->result.progress;
    p.emitted = std::max(p.emitted, e.progress.value.emitted);
    p.confirmed = std::max(p.confirmed, e.progress.value.confirmed);
  }
  if (e.closed && !a.closed_time.present)
    a.closed_time = MonoTime{s_.clock_epoch, s_.now};
  if (e.signal == AdapterEvidence::Signal::FAIL ||
      e.signal == AdapterEvidence::Signal::REFUSE) {
    r->stopped = true;
    r->stop_error = e.failure.present
                        ? e.failure.value
                        : error(ErrorCode::UNAVAILABLE, "TARGET_FAILURE",
                                RejectionOrigin::TARGET);
    a.error = Value<Error>::of(r->stop_error.value);
  }
  if (e.closed && ((a.delivery == Delivery::AMBIGUOUS && !ef->zero()) ||
                   (r->contract.feedback == Feedback::CORRELATED_RESULT &&
                    !ef->complete && !ef->zero()))) {
    r->stopped = true;
    if (!r->stop_error.present)
      r->stop_error = error(ErrorCode::AMBIGUOUS_DELIVERY, "CLOSED_UNRESOLVED");
  }
  fold(*r);
  increment(r->result.result_revision);
}
void Core::purge() {
  while (true) {
    std::size_t selected = Limits::records;
    U64 earliest = UINT64_MAX, serial = UINT64_MAX;
    for (std::size_t i = 0; i < Limits::records; ++i) {
      auto *r = s_.records[i].get();
      if (!r || !r->result.terminal || !r->result.first_terminal_time.present)
        continue;
      U64 at = 0;
      if (!add(r->result.first_terminal_time.value.tick_ms, 60000, at))
        continue;
      at = std::max(at, r->request.deadline.tick_ms);
      if (s_.now >= at &&
          (selected == Limits::records || at < earliest ||
           (at == earliest && r->request.admission_ticket.serial < serial))) {
        selected = i;
        earliest = at;
        serial = r->request.admission_ticket.serial;
      }
    }
    if (selected == Limits::records)
      break;
    s_.records[selected].reset();
  }
  for (std::size_t i = 0; i < s_.effects.size();) {
    const auto &e = s_.effects[i];
    if (!record(e.request_id) &&
        (e.complete || e.zero() ||
         (e.feedback == Feedback::NONE && e.resolved() && !e.uncertain_extent &&
          e.progress.emitted == e.progress.total)))
      s_.effects.erase(i);
    else
      ++i;
  }
}
void Core::maintenance() {
  purge();
  for (auto &p : s_.records)
    if (p.get()) {
      auto &r = *p.get();
      auto &x = r.result;
      if (s_.now >= r.request.deadline.tick_ms) {
        bool changed = false;
        if (!x.deadline_elapsed) {
          x.deadline_elapsed = true;
          changed = true;
        }
        if (!x.terminal) {
          r.stopped = true;
          if (!x.attempts.size())
            close(r, Lifecycle::EXPIRED_BEFORE_SEND, Outcome::UNKNOWN,
                  Value<Error>::of(
                      error(ErrorCode::DEADLINE_EXPIRED, "DEADLINE")));
          else {
            r.stop_error = error(effect(r.request.request_id, 1)->zero()
                                     ? ErrorCode::DEADLINE_EXPIRED
                                     : ErrorCode::TIMEOUT,
                                 "DEADLINE");
            fold(r);
          }
          changed = true;
        }
        if (changed) {
          if (x.result_revision == UINT64_MAX)
            s_.blocked = true;
          else
            ++x.result_revision;
        }
      }
    }
}
void Core::revalidate() {
  for (auto &p : s_.records)
    if (p.get() && !p.get()->result.terminal) {
      auto &r = *p.get();
      if (s_.now >= r.request.deadline.tick_ms)
        continue;
      auto e = validate(r.request, true);
      if (!e.present)
        continue;
      if (!r.result.attempts.size())
        close(r, Lifecycle::REJECTED, Outcome::REJECTED,
              Value<Error>::of(e.value));
      else {
        r.result.invalidated = true;
        r.stop_error = e.value;
        r.stopped = true; // awaiting correlated evidence remains active until
                          // its resolution/deadline
        if (r.result.attempts[0].closed_time.present)
          fold(r);
      }
      increment(r.result.result_revision);
    }
}
Reply Core::cancel(const Id &id, U64 now) {
  if (!begin(now))
    return {nullptr, fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP")};
  auto *r = record(id);
  if (!r) {
    end();
    return {nullptr,
            fail(ErrorCode::REQUEST_HISTORY_EXPIRED, "HISTORY_EXPIRED")};
  }
  if (!r->result.terminal) {
    r->stopped = true;
    r->stop_error = error(ErrorCode::CANCELLED, "CANCELLED");
    if (!r->result.attempts.size())
      close(*r, Lifecycle::REJECTED, Outcome::REJECTED,
            Value<Error>::of(r->stop_error.value));
    else
      fold(*r);
    increment(r->result.result_revision);
  }
  auto *out = &r->result;
  end();
  return {out, {}};
}
void Core::tick(U64 now) {
  if (begin(now))
    end();
}
bool Core::observe(StateObservation o, U64 now) {
  if (!begin(now))
    return false;
  bool ok = ingest(std::move(o));
  end();
  return ok;
}
bool Core::report(AdapterEvidence e, U64 now) {
  if (!begin(now))
    return false;
  auto *before = effect(e.request_id, e.attempt_index);
  std::size_t n = before ? before->evidence_ids.size() : 0;
  evidence(e);
  bool accepted = before && before->evidence_ids.size() > n;
  end();
  return accepted;
}
bool Core::publish(AdapterEvidence e) {
  if (!processing_)
    return false;
  Event x;
  x.evidence = std::move(e);
  return events_.push(std::move(x));
}
bool Core::publish(StateObservation o) {
  if (!processing_)
    return false;
  Event x;
  x.state = true;
  x.observation = std::move(o);
  return events_.push(std::move(x));
}
Optional<Error> Core::seek_window(const Id &id, SeekWindow w, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  if (!w.generation || w.min_ms > w.max_ms || !w.media_context_generation)
    e = fail(ErrorCode::INVALID_ARGUMENT, "WINDOW_SCHEMA");
  else {
    bool found = false;
    for (auto &x : s_.windows)
      if (x.first == id) {
        x.second = std::move(w);
        found = true;
      }
    if (!found && !s_.windows.push({id, std::move(w)}))
      e = fail(ErrorCode::UNAVAILABLE, "WINDOW_CAPACITY");
    revalidate();
  }
  end();
  return e;
}
Optional<Error> Core::catalog(CatalogSnapshot in, const Id &bid, U64 mapping,
                              U64 session, U64 now) {
  if (!begin(now))
    return fail(ErrorCode::UNAVAILABLE, "EVENT_OWNERSHIP");
  Optional<Error> e;
  auto *b = binding(bid);
  auto *d = device(in.device_id);
  auto *t = target(in.device_id, in.target_id);
  if (!b || !t || !d || !(b->device_id == in.device_id) ||
      !(b->target_id == in.target_id))
    e = fail(ErrorCode::INVALID_REFERENCE, "SCOPE");
  else if (b->mapping_revision != mapping || b->session_generation != session)
    e = fail(ErrorCode::STALE_PRECONDITION, "CAPTURE_CHANGED");
  for (std::size_t i = 0; i < in.items.size(); ++i) {
    if (!in.items[i].item_id.valid() || !in.items[i].label.valid())
      e = fail(ErrorCode::INVALID_ARGUMENT, "CATALOG_SCHEMA");
    for (std::size_t j = 0; j < i; ++j)
      if (in.items[i].item_id == in.items[j].item_id)
        e = fail(ErrorCode::INVALID_ARGUMENT, "DUPLICATE_ITEM");
  }
  if (!e.present) {
    CatalogRecord *old = nullptr;
    for (auto &c : s_.catalogs)
      if (c.snapshot.device_id == in.device_id &&
          c.snapshot.target_id == in.target_id && c.snapshot.kind == in.kind)
        old = &c;
    if (!old) {
      CatalogRecord c;
      c.snapshot.kind = in.kind;
      c.snapshot.device_id = in.device_id;
      c.snapshot.target_id = in.target_id;
      c.binding_id = bid;
      c.mapping_revision = mapping;
      c.session_generation = session;
      if (!s_.catalogs.push(std::move(c)))
        e = fail(ErrorCode::UNAVAILABLE, "CATALOG_CAPACITY");
      else
        old = &s_.catalogs[s_.catalogs.size() - 1];
    }
    if (old) {
      CatalogSnapshot merged = old->snapshot;
      bool changed = false;
      if (in.complete) {
        if (merged.items.size() != in.items.size())
          changed = true;
        else
          for (std::size_t i = 0; i < in.items.size(); ++i)
            if (!(in.items[i].item_id == merged.items[i].item_id))
              changed = true;
        merged.items = in.items;
      } else {
        List<CatalogItem, 1024> added;
        for (const auto &item : in.items) {
          bool known = false;
          for (auto &i : merged.items)
            if (i.item_id == item.item_id) {
              i.label = item.label;
              i.variant = item.variant;
              known = true;
            }
          if (!known && !added.push(item))
            e = fail(ErrorCode::UNAVAILABLE, "CATALOG_CAPACITY");
        }
        std::sort(added.begin(), added.end(), [](const auto &a, const auto &b) {
          return a.item_id < b.item_id;
        });
        if (added.size())
          changed = true;
        for (const auto &i : added)
          if (!merged.items.push(i))
            e = fail(ErrorCode::UNAVAILABLE, "CATALOG_CAPACITY");
      }
      if (changed) {
        if (merged.catalog_generation == UINT64_MAX)
          e = fail(ErrorCode::UNAVAILABLE, "GENERATION_EXHAUSTED");
        else
          ++merged.catalog_generation;
      }
      if (!e.present) {
        merged.complete = in.complete;
        for (auto &i : merged.items) {
          i.reference = {in.kind,
                         in.device_id,
                         in.target_id,
                         d->association_generation,
                         t->association_generation,
                         i.item_id,
                         merged.catalog_generation};
        }
        old->snapshot = std::move(merged);
        old->session_generation = session;
        old->mapping_revision = mapping;
        revalidate();
      }
    }
  }
  end();
  return e;
}
} // namespace control
