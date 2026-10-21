#include "control/mock_adapter.hpp"
#include <cstdio>

extern "C" void remote01_control_mock_anchor() {}

namespace control {
namespace {
Schema single(SchemaNode::Kind kind) {
  Schema s;
  SchemaNode n;
  n.kind = kind;
  s.nodes.push(std::move(n));
  return s;
}
Evidence contract_evidence() {
  Evidence e;
  e.evidence_id = Id("contract.mock.1");
  e.kind = EvidenceKind::OFFICIAL_CONTRACT;
  e.reference = Text::literal("components/control/mock_adapter.cpp: in-process "
                              "synthetic contract only");
  e.verified_claims.push(Text::literal(
      "synthetic adapter semantics; no real-device or network claim"));
  return e;
}
Schema args_schema(std::string_view op) {
  Schema s;
  SchemaNode root;
  root.kind = SchemaNode::Kind::RECORD;
  s.nodes.push(root);
  auto field = [&](const char *name, SchemaNode::Kind kind) {
    SchemaNode node;
    node.kind = kind;
    node.min_u = 0;
    node.max_u = UINT64_MAX;
    if (kind == SchemaNode::Kind::RATIONAL) {
      node.min = {0, 1};
      node.max = {1, 1};
    }
    if (std::string_view(name) == "count") {
      node.min_u = 1;
      node.max_u = 65535;
    }
    if (std::string_view(name) == "state")
      for (const auto *v : {"ON", "OFF", "STANDBY"})
        node.values.push(Text::literal(v));
    if (std::string_view(name) == "direction")
      for (const auto *v : {"UP", "DOWN"})
        node.values.push(Text::literal(v));
    auto idx = static_cast<std::uint32_t>(s.nodes.size());
    s.nodes.push(std::move(node));
    s.nodes[0].edges.push({Text::literal(name), idx, true});
  };
  if (op == "power.set")
    field("state", SchemaNode::Kind::ENUM);
  if (op == "volume.set")
    field("level", SchemaNode::Kind::RATIONAL);
  if (op == "volume.mute")
    field("muted", SchemaNode::Kind::BOOL);
  if (op == "volume.step") {
    field("direction", SchemaNode::Kind::ENUM);
    field("count", SchemaNode::Kind::UINT32);
  }
  if (op == "source.select") {
    field("source", SchemaNode::Kind::REF);
    s.nodes[1].ref_kind = CatalogKind::SOURCE;
  }
  return s;
}
// Closed, acyclic metadata for the complete typed local CommandResult.
struct Graph {
  Schema s;
  using K = SchemaNode::Kind;
  unsigned node(K k) {
    SchemaNode n;
    n.kind = k;
    unsigned i = s.nodes.size();
    if (!s.nodes.push(n))
      return UINT32_MAX;
    return i;
  }
  unsigned record() { return node(K::RECORD); }
  void field(unsigned r, const char *name, unsigned n, bool required = true) {
    s.nodes[r].edges.push({Text::literal(name), n, required});
  }
  unsigned number(bool small = false, U64 minimum = 0) {
    auto n = node(small ? K::UINT32 : K::UINT64);
    s.nodes[n].min_u = minimum;
    s.nodes[n].max_u = small ? UINT32_MAX : UINT64_MAX;
    return n;
  }
  unsigned text(unsigned max = 4096) {
    auto n = node(K::STRING);
    s.nodes[n].max_bytes = max;
    return n;
  }
  unsigned tokens(std::initializer_list<const char *> values) {
    auto n = node(K::ENUM);
    for (auto v : values)
      s.nodes[n].values.push(Text::literal(v));
    return n;
  }
  unsigned list(unsigned element, unsigned count) {
    auto n = node(K::LIST);
    s.nodes[n].element = element;
    s.nodes[n].max_count = count;
    return n;
  }
  unsigned value(unsigned element) {
    auto n = node(K::UNION);
    s.nodes[n].tag = Text::literal("state");
    auto p = record();
    field(p, "value", element);
    auto a = record();
    field(a, "reason",
          tokens({"UNKNOWN", "UNSUPPORTED", "NOT_APPLICABLE", "UNAVAILABLE"}));
    field(n, "PRESENT", p);
    field(n, "ABSENT", a);
    return n;
  }
};
Schema result_schema() {
  Graph g;
  auto root = g.record();
  auto id = g.text(128), str = g.text(), u = g.number(),
       positive = g.number(false, 1), index = g.number(true, 1),
       boolean = g.node(Graph::K::BOOL);
  auto delivery = g.tokens(
      {"NOT_SENT", "PARTIALLY_SENT", "SENT", "ACKNOWLEDGED", "AMBIGUOUS"});
  auto outcome = g.tokens({"CONFIRMED", "UNCONFIRMED", "UNKNOWN", "REJECTED"});
  auto lifecycle = g.tokens({"QUEUED", "DISPATCHED", "ACCEPTED",
                             "CONFIRMED_COMPLETED", "REJECTED", "FAILED",
                             "EXPIRED_BEFORE_SEND", "UNCERTAIN_OUTCOME"});
  auto code = g.tokens(
      {"UNSUPPORTED_OPERATION", "INVALID_ARGUMENT", "INVALID_REFERENCE",
       "STALE_PRECONDITION", "UNAUTHORIZED", "UNAVAILABLE", "TIMEOUT",
       "AMBIGUOUS_DELIVERY", "DEADLINE_EXPIRED", "PARTIAL_EXECUTION",
       "REQUEST_HISTORY_EXPIRED", "REQUEST_ID_MISMATCH", "ORDERING_BLOCKED",
       "CANCELLED"});
  auto origin = g.tokens({"CORE", "TARGET"});
  auto cause = g.record();
  g.field(cause, "code", code);
  g.field(cause, "detail", str);
  g.field(cause, "rejection_origin", origin, false);
  auto err = g.record();
  g.field(err, "code", code);
  g.field(err, "detail", str);
  g.field(err, "rejection_origin", origin, false);
  g.field(err, "cause", cause, false);
  g.field(err, "step_index", index, false);
  auto error_value = g.value(err);
  auto time = g.record();
  g.field(time, "clock_epoch", id);
  g.field(time, "tick_ms", u);
  auto evidence = g.record();
  g.field(evidence, "evidence_id", id);
  g.field(evidence, "kind",
          g.tokens({"PROJECT_DESIGN", "OFFICIAL_CONTRACT", "PROTOCOL_TEST",
                    "OWNER_ASSOCIATION", "HARDWARE_TEST"}));
  g.field(evidence, "reference", str);
  g.field(evidence, "verified_claims", g.list(str, 16));
  g.field(evidence, "version", g.value(str));
  auto attempt = g.record();
  g.field(attempt, "attempt_index", index);
  g.field(attempt, "binding_id", id);
  g.field(attempt, "mapping_revision", positive);
  g.field(attempt, "session_generation", positive);
  g.field(attempt, "dispatch_time", time);
  g.field(attempt, "closed_time", time, false);
  g.field(attempt, "step_index", index, false);
  g.field(attempt, "delivery", delivery);
  g.field(attempt, "target_outcome", outcome);
  g.field(attempt, "evidence", g.list(evidence, 32));
  g.field(attempt, "guaranteed_no_execution", boolean);
  g.field(attempt, "resource_ids", g.list(id, Limits::resources));
  g.field(attempt, "error", error_value);
  auto progress = g.record();
  g.field(progress, "total", index);
  g.field(progress, "emitted", g.number(true));
  g.field(progress, "confirmed", g.number(true));
  g.field(progress, "uncertain_remaining", boolean);
  auto kind =
      g.tokens({"SOURCE", "CHANNEL", "FAVORITE", "APPLICATION", "CONTENT"});
  auto reference = g.record();
  g.field(reference, "kind", kind);
  g.field(reference, "device_id", id);
  g.field(reference, "target_id", id);
  g.field(reference, "device_association_generation", positive);
  g.field(reference, "association_generation", positive);
  g.field(reference, "item_id", id);
  g.field(reference, "catalog_generation", positive);
  auto item = g.record();
  g.field(item, "item_id", id);
  g.field(item, "label", str);
  g.field(item, "reference", reference);
  g.field(item, "variant", str, false);
  auto catalog = g.record();
  g.field(catalog, "kind", kind);
  g.field(catalog, "device_id", id);
  g.field(catalog, "target_id", id);
  g.field(catalog, "catalog_generation", positive);
  g.field(catalog, "items", g.list(item, 1024));
  g.field(catalog, "complete", boolean);
  g.field(root, "request_id", id);
  g.field(root, "result_revision", positive);
  g.field(root, "lifecycle", lifecycle);
  g.field(root, "terminal", boolean);
  g.field(root, "delivery", delivery);
  g.field(root, "target_outcome", outcome);
  g.field(root, "error", error_value);
  g.field(root, "attempts", g.list(attempt, 3072));
  g.field(root, "progress", progress);
  g.field(root, "deadline_elapsed", boolean);
  g.field(root, "invalidated", boolean);
  g.field(root, "query_result", catalog, false);
  g.field(root, "waiting_reason", g.value(str));
  g.field(root, "first_terminal_time", g.value(time));
  g.field(root, "closure_error", error_value);
  return std::move(g.s);
}
} // namespace
Id MockAdapter::next(const char *prefix) {
  char b[64];
  std::snprintf(b, sizeof b, "%s:%llu", prefix,
                static_cast<unsigned long long>(++serial_));
  return Id(b);
}
CapabilityDescriptor MockAdapter::operation(const Id &bid, std::string_view op,
                                            Feedback feedback) {
  CapabilityDescriptor c;
  c.key = {CapabilityKey::Kind::OPERATION, Text(op), bid};
  c.support = Support::SUPPORTED;
  c.availability = Availability::READY;
  c.evidence.push(contract_evidence());
  c.argument_schema = args_schema(op);
  c.result_schema = result_schema();
  c.idempotence = op == "power.toggle" || op == "volume.step" ||
                          op == "playback.previous" || op == "playback.next"
                      ? Idempotence::NON_IDEMPOTENT
                      : Idempotence::CONTEXT_DEPENDENT;
  c.retry = RetryPolicy{};
  ExecutionContract ex;
  ex.feedback = feedback;
  ex.completion_claim =
      Text::literal("synthetic logical effect and no outstanding work");
  ex.mapping_reference = Text::literal("adapter.mock/1");
  ex.context_guard = Guarantee::VERIFIED;
  ex.selection_guard = Guarantee::VERIFIED;
  ex.ordering_fence = Guarantee::VERIFIED;
  c.execution_evidence = ex;
  if (op == "playback.stop")
    c.semantic_variant =
        Text::literal("synthetic logical stop; no observed playback inference");
  if (op.starts_with("playback."))
    c.required_context.push(Context::MEDIA);
  if (op == "source.select")
    c.required_context.push(Context::CATALOG);
  if (op == "volume.set") {
    c.numeric_mapping = NumericMapping{};
    Unit u;
    u.path.push(Text::literal("level"));
    u.unit = Unit::Kind::NORMALIZED;
    c.units.push(std::move(u));
  }
  return c;
}
CapabilityDescriptor MockAdapter::readable(const Id &bid, std::string_view f) {
  CapabilityDescriptor c;
  c.key = {CapabilityKey::Kind::FIELD, Text(f), bid};
  c.support = Support::SUPPORTED;
  c.availability = Availability::READY;
  c.evidence.push(contract_evidence());
  c.value_schema = single(f == "volume.mute"    ? SchemaNode::Kind::BOOL
                          : f == "volume.level" ? SchemaNode::Kind::RATIONAL
                          : (f == "media.title" || f == "media.artist" || f == "media.subtitle") ? SchemaNode::Kind::STRING
                                                : SchemaNode::Kind::ENUM);
  auto &schema = c.value_schema.value.nodes[0];
  if (f == "volume.level") {
    schema.min = {0, 1};
    schema.max = {1, 1};
  }
  if (f == "power")
    for (const auto *v : {"ON", "OFF", "STANDBY"})
      schema.values.push(Text::literal(v));
  if (f == "playback.state")
    for (const auto *v : {"PLAYING", "PAUSED", "NOT_PLAYING", "BUFFERING"})
      schema.values.push(Text::literal(v));
  FreshnessPolicy p;
  p.max_age_ms = 5000;
  p.acquisition = Acquisition::VERIFIED_SEQUENCE;
  p.source_clock_contract =
      Value<Text>::of(Text::literal("bounded synthetic acquisition"));
  c.freshness = p;
  c.observation = CapabilityDescriptor::Observation::AVAILABLE;
  return c;
}
bool MockAdapter::install(Core &core, const Id &d, const Id &t, const Id &b,
                          Feedback f, Ordering order, U64 now) {
  if (!core.device(d)) {
    CommonDevice device;
    device.device_id = d;
    device.display_name = Text::literal("SYNTHETIC MOCK DEVICE");
    if (core.register_device(std::move(device), now).present)
      return false;
  }
  if (!core.target(d, t)) {
    Target target;
    target.device_id = d;
    target.target_id = t;
    target.display_name = Text::literal("SYNTHETIC MOCK TARGET");
    target.ordering_mode = order;
    if (core.register_target(std::move(target), now).present)
      return false;
  }
  Binding bind;
  bind.device_id = d;
  bind.target_id = t;
  bind.binding_id = b;
  bind.authorization = Authorization::AUTHORIZED;
  bind.reachability = Reachability::REACHABLE;
  bind.model_versions.push(Version{});
  bind.mapping_evidence.push(contract_evidence());
  if (core.register_binding(std::move(bind), *this, now).present)
    return false;
  for (auto op : {"power.set", "power.toggle", "volume.set", "volume.step",
                  "volume.mute", "playback.play", "playback.pause",
                  "playback.stop", "playback.previous", "playback.next"})
    if (core.capability(d, t, operation(b, op, f), now).present)
      return false;
  for (auto name : {"power", "volume.level", "volume.mute", "playback.state"})
    if (core.capability(d, t, readable(b, name), now).present)
      return false;
  return true;
}
StateObservation MockAdapter::sample(Core &c, const Id &bid, std::string_view f,
                                     Value<Scalar> v, U64 now, Origin origin) {
  const auto *b = c.binding(bid);
  const auto *t = c.target(b->device_id, b->target_id);
  const auto *d = c.device(b->device_id);
  StateObservation o;
  o.observation_id = next("mock.observation");
  o.device_id = d->device_id;
  o.target_id = t->target_id;
  o.field_id = Text(f);
  o.value = std::move(v);
  o.confidence =
      o.value.present ? Confidence::AUTHORITATIVE : Confidence::UNKNOWN;
  o.binding_id = bid;
  o.device_association_generation = d->association_generation;
  o.association_generation = t->association_generation;
  o.mapping_revision = b->mapping_revision;
  o.session_generation = b->session_generation;
  o.receipt_time = {c.storage().clock_epoch, now};
  o.age_at_receipt_ms = 0;
  o.clock_quality = ClockQuality::BOUNDED;
  o.origin = origin;
  o.acquisition_order = serial_;
  o.source_sequence = Sequence{Id("mock.sequence"), serial_};
  for (const auto &cap : t->capabilities)
    if (cap.key.kind == CapabilityKey::Kind::FIELD &&
        cap.key.binding_id == bid && cap.key.id.view() == f)
      o.freshness = cap.freshness.value;
  if (f.starts_with("media.") || f == "playback.state" ||
      f == "content.current")
    o.media_context_generation = t->media_context_generation;
  ProvenanceHop hop;
  hop.actor_id = identity();
  hop.evidence_id = Id("contract.mock.1");
  o.provenance.push(std::move(hop));
  return o;
}
void MockAdapter::dispatch(const Dispatch &d, AdapterSink &sink) {
  ++dispatches;
  last_numeric = d.mapped_numeric;
  AdapterEvidence e;
  e.request_id = d.request.request_id;
  e.binding_id = d.attempt.binding_id;
  e.mapping_revision = d.attempt.mapping_revision;
  e.session_generation = d.attempt.session_generation;
  e.evidence = contract_evidence();
  e.evidence.evidence_id = next("mock.result");
  LockKey key;
  key.device_id = d.request.device_id;
  key.target_id = d.request.target_id;
  e.covered_keys.push(key);
  for (const auto &id : d.attempt.resource_ids) {
    LockKey r;
    r.kind = LockKey::Kind::RESOURCE;
    r.resource_id = id;
    e.covered_keys.push(std::move(r));
  }
  auto *count = d.request.arguments.find("count");
  std::uint32_t total =
      count ? static_cast<std::uint32_t>(count->unsigned_value) : 1;
  e.progress = Progress{total, total, 0, false};
  e.delivery = Delivery::SENT;
  switch (mode) {
  case Mode::CONFIRMED:
    e.signal = AdapterEvidence::Signal::COMPLETE;
    e.delivery = Delivery::ACKNOWLEDGED;
    e.progress.value.confirmed = total;
    e.closed = true;
    break;
  case Mode::OPEN_LOOP:
    e.closed = true;
    break;
  case Mode::REJECT:
    e.signal = AdapterEvidence::Signal::REFUSE;
    e.progress = Progress{total, 0, 0, false};
    e.delivery = Delivery::NOT_SENT;
    e.closed = true;
    e.failure = error(ErrorCode::UNAVAILABLE, "EXPLICIT_REFUSAL",
                      RejectionOrigin::TARGET);
    break;
  case Mode::UNAVAILABLE:
    e.signal = AdapterEvidence::Signal::FAIL;
    e.progress = Progress{total, 0, 0, false};
    e.delivery = Delivery::NOT_SENT;
    e.closed = true;
    e.failure = error(ErrorCode::UNAVAILABLE, "SYNTHETIC_UNAVAILABLE",
                      RejectionOrigin::TARGET);
    break;
  case Mode::PARTIAL:
    e.signal = AdapterEvidence::Signal::FAIL;
    e.progress = Progress{total, 1, 0, false};
    e.delivery = Delivery::PARTIALLY_SENT;
    e.closed = true;
    e.failure =
        error(ErrorCode::UNAVAILABLE, "KNOWN_PREFIX", RejectionOrigin::TARGET);
    break;
  case Mode::ONGOING_PARTIAL:
    e.progress = Progress{total, 1, 0, false};
    e.delivery = Delivery::PARTIALLY_SENT;
    break;
  case Mode::AMBIGUOUS:
    e.delivery = Delivery::AMBIGUOUS;
    e.progress = Progress{total, 0, 0, true};
    e.closed = true;
    break;
  case Mode::NO_EXECUTION:
    e.delivery = Delivery::AMBIGUOUS;
    e.progress = Progress{total, 0, 0, true};
    e.closed = true;
    break;
  case Mode::DELAYED:
  case Mode::NO_RESULT:
    break;
  }
  last = e;
  if (!sink.publish(e))
    return;
  if (mode == Mode::NO_EXECUTION) {
    auto p = proof(e.covered_keys, true);
    p.evidence.evidence_id = next("mock.proof");
    sink.publish(std::move(p));
  }
}
AdapterEvidence MockAdapter::completion() const {
  auto e = last.value;
  e.evidence.evidence_id = Id("mock.late.complete");
  e.signal = AdapterEvidence::Signal::COMPLETE;
  e.delivery = Delivery::ACKNOWLEDGED;
  e.progress.value.emitted = e.progress.value.total;
  e.progress.value.confirmed = e.progress.value.total;
  e.closed = true;
  e.failure = {};
  return e;
}
AdapterEvidence
MockAdapter::proof(const List<LockKey, Limits::resources + 1> &keys,
                   bool past) const {
  auto e = last.value;
  e.evidence.evidence_id = Id(past ? "mock.zero.proof" : "mock.future.fence");
  e.signal = past ? AdapterEvidence::Signal::NO_EXECUTION
                  : AdapterEvidence::Signal::FUTURE_FENCE;
  e.covered_keys = keys;
  e.delivery = {};
  e.progress = {};
  e.failure = {};
  e.closed = false;
  return e;
}
} // namespace control
