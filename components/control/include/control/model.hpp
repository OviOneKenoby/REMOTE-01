#pragma once
#include "storage.hpp"
#include <limits>
#include <numeric>

namespace control {
struct Limits {
  static constexpr std::size_t devices = 4, targets = 8, bindings = 16,
                               capabilities = 32, fields = 16, resources = 16,
                               records = 1024, queue = 64, barriers = 1152,
                               identities = 8192, events = 64;
};
using U64 = std::uint64_t;
struct Version {
  std::uint32_t major = 1, minor = 0, patch = 0;
  Optional<std::uint32_t> candidate;
  bool operator==(const Version &) const = default;
};
struct MonoTime {
  Id clock_epoch;
  U64 tick_ms = 0;
  bool operator==(const MonoTime &) const = default;
};
inline bool add(U64 a, U64 b, U64 &out) {
  if (b > std::numeric_limits<U64>::max() - a)
    return false;
  out = a + b;
  return true;
}
struct Rational {
  std::int64_t n = 0;
  U64 d = 1;
  bool valid() const {
    U64 a = n < 0 ? U64(-(n + 1)) + 1 : U64(n);
    return d && std::gcd(a, d) == 1;
  }
  bool normalized() const { return valid() && n >= 0 && U64(n) <= d; }
  bool operator==(const Rational &) const = default;
};
enum class Power { ON, OFF, STANDBY };
enum class Playback { PLAYING, PAUSED, NOT_PLAYING, BUFFERING };
enum class Direction { UP, DOWN };
enum class CatalogKind { SOURCE, CHANNEL, FAVORITE, APPLICATION, CONTENT };
struct SelectionRef {
  CatalogKind kind = CatalogKind::CONTENT;
  Id device_id, target_id;
  U64 device_association_generation = 1, association_generation = 1;
  Id item_id;
  U64 catalog_generation = 1;
  bool operator==(const SelectionRef &) const = default;
};
struct AssetRef {
  Id asset_id;
  bool operator==(const AssetRef &) const = default;
};
enum class ScalarKind {
  BOOL,
  UINT32,
  UINT64,
  INT64,
  RATIONAL,
  STRING,
  POWER,
  PLAYBACK,
  DIRECTION,
  REFERENCE,
  ASSET
};
struct Scalar {
  ScalarKind kind = ScalarKind::BOOL;
  bool boolean = false;
  U64 unsigned_value = 0;
  std::int64_t signed_value = 0;
  Rational rational;
  Text string;
  Power power = Power::OFF;
  Playback playback = Playback::NOT_PLAYING;
  Direction direction = Direction::UP;
  SelectionRef reference;
  AssetRef asset;
  static Scalar boolean_value(bool b) {
    Scalar x;
    x.boolean = b;
    return x;
  }
  static Scalar level(Rational r) {
    Scalar x;
    x.kind = ScalarKind::RATIONAL;
    x.rational = r;
    return x;
  }
  static Scalar power_value(Power p) {
    Scalar x;
    x.kind = ScalarKind::POWER;
    x.power = p;
    return x;
  }
  bool operator==(const Scalar &x) const;
};
struct Argument {
  Text key;
  Scalar value;
  bool operator==(const Argument &) const = default;
};
struct Arguments {
  List<Argument, 16> fields;
  const Scalar *find(std::string_view key) const {
    for (const auto &f : fields)
      if (f.key.view() == key)
        return &f.value;
    return nullptr;
  }
  bool operator==(const Arguments &x) const {
    if (fields.size() != x.fields.size())
      return false;
    for (const auto &f : fields) {
      auto *other = x.find(f.key.view());
      if (!other || !(f.value == *other))
        return false;
    }
    return true;
  }
};
enum class Support { SUPPORTED, UNSUPPORTED, UNKNOWN };
enum class Availability {
  READY,
  OFFLINE,
  UNAUTHORIZED,
  BLOCKED,
  CONTEXT_UNAVAILABLE,
  UNKNOWN
};
enum class Confidence { AUTHORITATIVE, REPORTED, ASSUMED, UNKNOWN };
enum class Authorization { AUTHORIZED, UNAUTHORIZED, BLOCKED, UNKNOWN };
enum class Reachability { REACHABLE, UNREACHABLE, UNKNOWN, UNVERIFIED };
enum class ConnectivityStatus {
  REACHABLE,
  PARTIAL,
  UNREACHABLE,
  UNVERIFIED,
  UNKNOWN
};
enum class Ordering { STRICT, LOCAL_ONLY };
enum class Feedback { NONE, CORRELATED_RESULT };
enum class Guarantee { VERIFIED, UNKNOWN, NOT_APPLICABLE };
enum class Idempotence {
  NON_IDEMPOTENT,
  CONTEXT_DEPENDENT,
  VERIFIED_IDEMPOTENT
};
enum class Context { NONE, CATALOG, MEDIA, SEEK_WINDOW, STATE_PRECONDITION };
enum class Acquisition { VERIFIED_SEQUENCE, VERIFIED_SERIAL_SAMPLE };
enum class Origin { LIVE, CACHE_REPLAY, RESYNC };
enum class ClockQuality { BOUNDED, UNKNOWN };
enum class EvidenceKind {
  PROJECT_DESIGN,
  OFFICIAL_CONTRACT,
  PROTOCOL_TEST,
  OWNER_ASSOCIATION,
  HARDWARE_TEST
};
struct Evidence {
  Id evidence_id;
  EvidenceKind kind = EvidenceKind::PROJECT_DESIGN;
  Text reference;
  List<Text, 16> verified_claims;
  Value<Text> version;
  bool operator==(const Evidence &) const = default;
};
struct RetryPolicy {
  bool automatic = false, fallback_on_no_execution = false;
  std::uint32_t max_attempts = 1;
  U64 delay_ms = 1;
  bool operator==(const RetryPolicy &) const = default;
};
struct ExecutionContract {
  Feedback feedback = Feedback::CORRELATED_RESULT;
  Text completion_claim;
  List<Text, 16> guaranteed_no_execution_errors;
  Guarantee context_guard = Guarantee::UNKNOWN,
            selection_guard = Guarantee::UNKNOWN,
            ordering_fence = Guarantee::UNKNOWN;
  Text mapping_reference;
  bool operator==(const ExecutionContract &) const = default;
};
struct FreshnessPolicy {
  U64 max_age_ms = 0;
  Acquisition acquisition = Acquisition::VERIFIED_SEQUENCE;
  Value<Text> source_clock_contract;
  bool operator==(const FreshnessPolicy &) const = default;
};
struct Sequence {
  Id domain;
  U64 ordinal = 1;
  bool operator==(const Sequence &) const = default;
};
struct Unit {
  List<Text, 16> path;
  enum class Kind {
    NONE,
    NORMALIZED,
    MS,
    COUNT,
    CHANNEL_NUMBER,
    REFERENCE,
    TEXT
  } unit = Kind::NONE;
  bool operator==(const Unit &) const = default;
};
// A bounded acyclic schema graph. Edges hold indexes, never owning recursive
// nodes.
struct SchemaEdge {
  Text name;
  std::uint32_t node = 0;
  bool required = true;
  bool operator==(const SchemaEdge &) const = default;
};
struct SchemaNode {
  enum class Kind {
    BOOL,
    UINT32,
    UINT64,
    INT64,
    RATIONAL,
    STRING,
    ENUM,
    REF,
    ASSET_REF,
    RECORD,
    UNION,
    LIST
  } kind = Kind::BOOL;
  Rational min, max;
  U64 min_u = 0, max_u = 0;
  std::int64_t min_i = 0, max_i = 0;
  std::uint32_t max_bytes = 4096, max_count = 0;
  CatalogKind ref_kind = CatalogKind::CONTENT;
  List<Text, 16> values;
  List<SchemaEdge, 16> edges;
  Text tag;
  Optional<std::uint32_t> element;
  bool operator==(const SchemaNode &) const = default;
};
struct Schema {
  List<SchemaNode, 128> nodes;
  std::uint32_t root = 0;
  bool operator==(const Schema &) const = default;
};
struct NumericPair {
  Rational common, external;
  bool operator==(const NumericPair &) const = default;
};
struct NumericMapping {
  Rational common_min, common_max{1, 1}, external_min, external_max{1, 1};
  enum class Transfer { AFFINE, TABLE } transfer = Transfer::AFFINE;
  List<NumericPair, 65536> table;
  enum class Set { FINITE, GRID } set = Set::GRID;
  List<Rational, 65536> finite;
  Rational first, last{1, 1}, step{1, 100};
  Rational resolution_common{1, 100}, resolution_external{1, 100},
      allowed_error;
  enum class Rounding { EXACT, NEAREST_TIES_UP } rounding = Rounding::EXACT;
  List<Rational, 64> sentinels;
  Absence null_handling = Absence::UNKNOWN;
  enum class OutOfDomain { REJECT } out_of_domain = OutOfDomain::REJECT;
  bool operator==(const NumericMapping &) const = default;
};
struct CapabilityKey {
  enum class Kind { OPERATION, FIELD } kind = Kind::OPERATION;
  Text id;
  Id binding_id;
  bool operator==(const CapabilityKey &) const = default;
};
struct CapabilityDescriptor {
  CapabilityKey key;
  Support support = Support::UNKNOWN;
  Availability availability = Availability::UNKNOWN;
  U64 capability_revision = 1, mapping_revision = 1;
  List<Evidence, 16> evidence;
  Optional<Schema> argument_schema, result_schema, value_schema;
  List<Unit, 16> units;
  Optional<NumericMapping> numeric_mapping;
  List<Context, 5> required_context;
  enum class Observation {
    AVAILABLE,
    UNAVAILABLE,
    UNKNOWN
  } observation = Observation::UNKNOWN;
  Optional<Text> semantic_variant;
  Optional<Idempotence> idempotence;
  Optional<RetryPolicy> retry;
  Optional<FreshnessPolicy> freshness;
  List<Id, Limits::resources> resource_ids;
  Optional<ExecutionContract> execution_evidence;
};
struct ProvenanceHop {
  Id actor_id;
  enum class Role { TARGET, INTERMEDIARY, ADAPTER, CORE } role = Role::TARGET;
  Id evidence_id;
  bool operator==(const ProvenanceHop &) const = default;
};
struct StateObservation {
  Id observation_id, device_id, target_id;
  Text field_id;
  Value<Scalar> value;
  Confidence confidence = Confidence::UNKNOWN;
  Id binding_id;
  List<ProvenanceHop, 16> provenance;
  U64 device_association_generation = 1, association_generation = 1,
      mapping_revision = 1, session_generation = 1;
  MonoTime receipt_time;
  U64 acquisition_order = 1;
  Origin origin = Origin::LIVE;
  Optional<MonoTime> observation_time;
  Optional<Text> source_time;
  Optional<U64> age_at_receipt_ms;
  ClockQuality clock_quality = ClockQuality::UNKNOWN;
  FreshnessPolicy freshness;
  Optional<Sequence> source_sequence;
  Optional<U64> media_context_generation;
  Optional<Id> pending_request_id;
  bool operator==(const StateObservation &) const = default;
};
struct ConflictParticipant {
  Id binding_id;
  U64 conflict_ingress = 1;
  Value<U64> resync_started_ingress;
  Value<StateObservation> resync_observation;
  List<Evidence, 16> exclusion_evidence;
};
struct ConflictState {
  U64 revision = 1;
  List<ConflictParticipant, Limits::bindings> participants;
};
struct FieldSnapshot {
  Value<StateObservation> selected;
  List<StateObservation, Limits::bindings> conflict_set;
  bool conflict_latched = false;
  ConflictState conflict_state;
  Value<StateObservation> optimistic_overlay;
  List<StateObservation, 16> history;
};
struct RouteConnectivity {
  Reachability reachability = Reachability::UNKNOWN;
  Authorization authorization = Authorization::UNKNOWN;
  Optional<MonoTime> last_contact;
};
struct Connectivity {
  ConnectivityStatus status = ConnectivityStatus::UNKNOWN;
  List<std::pair<Id, RouteConnectivity>, Limits::bindings> routes;
};
struct Binding {
  Id binding_id, device_id, target_id;
  U64 device_association_generation = 1, association_generation = 1,
      mapping_revision = 1, session_generation = 1;
  Text adapter_version;
  Value<Text> external_version;
  List<Version, 8> model_versions;
  Authorization authorization = Authorization::UNKNOWN;
  Reachability reachability = Reachability::UNKNOWN;
  List<Evidence, 16> mapping_evidence;
  std::uint32_t owner_priority = 0;
  Optional<MonoTime> last_contact;
  List<Id, 16> transport_refs;
  Optional<Id> private_metadata_ref, credential_ref;
};
struct FieldEntry {
  Text field_id;
  FieldSnapshot snapshot;
};
struct Target {
  Id target_id, device_id;
  U64 association_generation = 1, capability_revision = 1,
      media_context_generation = 1;
  Text display_name;
  List<CapabilityDescriptor, Limits::capabilities> capabilities;
  List<FieldEntry, Limits::fields> fields;
  List<Id, Limits::resources> resource_ids;
  Ordering ordering_mode = Ordering::STRICT;
};
struct CommonDevice {
  Id device_id;
  U64 association_generation = 1;
  Text display_name;
  enum class Class {
    AUDIO_PLAYER,
    TV,
    RECEIVER,
    SET_TOP_BOX,
    GENERIC,
    UNKNOWN
  } device_class = Class::UNKNOWN;
  List<Id, Limits::targets> targets;
  List<Id, Limits::bindings> bindings;
  Connectivity connectivity;
  Optional<Value<Text>> manufacturer, model;
  Optional<Value<AssetRef>> profile, artwork;
};
struct SharedResource {
  Id resource_id;
  List<std::pair<Id, Id>, Limits::targets> member_targets;
  List<Evidence, 16> evidence;
  Ordering ordering_mode = Ordering::STRICT;
};
struct LockKey {
  enum class Kind { TARGET, RESOURCE } kind = Kind::TARGET;
  Id device_id, target_id, resource_id;
  bool operator==(const LockKey &) const = default;
  bool operator<(const LockKey &x) const {
    if (kind != x.kind)
      return kind < x.kind;
    if (kind == Kind::RESOURCE)
      return resource_id < x.resource_id;
    if (!(device_id == x.device_id))
      return device_id < x.device_id;
    return target_id < x.target_id;
  }
};
struct FieldPrecondition {
  Text field_id;
  Id observation_id;
  Value<Scalar> expected_value;
  bool operator==(const FieldPrecondition &) const = default;
};
struct RouteCapture {
  Id binding_id;
  U64 mapping_revision = 1, capability_revision = 1;
  Optional<U64> token_generation;
  ExecutionContract execution_contract;
  bool operator==(const RouteCapture &) const = default;
};
struct StepCapture {
  Id device_id, target_id;
  Text operation_id;
  Arguments arguments;
  U64 capability_revision = 1;
  Id binding_id;
  U64 mapping_revision = 1, device_association_generation = 1,
      association_generation = 1;
  Optional<U64> catalog_generation, token_generation, media_context_generation,
      seek_window_generation;
  List<FieldPrecondition, Limits::fields> preconditions;
  List<RouteCapture, Limits::bindings> route_plan;
  bool operator==(const StepCapture &) const = default;
};
struct RecipeCapture {
  Id recipe_id;
  U64 revision = 1;
  List<StepCapture, 1024> steps;
  List<LockKey, 128> lock_keys;
  bool operator==(const RecipeCapture &) const = default;
};
struct CommandRequest : StepCapture {
  Version common_model_version;
  Id request_id;
  struct Ticket {
    Id clock_epoch;
    U64 serial = 0;
    bool operator==(const Ticket &) const = default;
  } admission_ticket;
  MonoTime deadline;
  Optional<RecipeCapture> recipe_capture;
  bool operator==(const CommandRequest &) const = default;
};
struct CatalogItem {
  Id item_id;
  Text label;
  SelectionRef reference;
  Optional<Text> variant;
  bool operator==(const CatalogItem &) const = default;
};
struct CatalogSnapshot {
  CatalogKind kind = CatalogKind::CONTENT;
  Id device_id, target_id;
  U64 catalog_generation = 1;
  List<CatalogItem, 1024> items;
  bool complete = false;
  bool operator==(const CatalogSnapshot &) const = default;
};
struct SeekWindow {
  U64 generation = 1, min_ms = 0, max_ms = 0, media_context_generation = 1;
  Guarantee guard = Guarantee::UNKNOWN;
  List<Evidence, 16> evidence;
};
enum class ErrorCode {
  UNSUPPORTED_OPERATION,
  INVALID_ARGUMENT,
  INVALID_REFERENCE,
  STALE_PRECONDITION,
  UNAUTHORIZED,
  UNAVAILABLE,
  TIMEOUT,
  AMBIGUOUS_DELIVERY,
  DEADLINE_EXPIRED,
  PARTIAL_EXECUTION,
  REQUEST_HISTORY_EXPIRED,
  REQUEST_ID_MISMATCH,
  ORDERING_BLOCKED,
  CANCELLED
};
enum class RejectionOrigin { CORE, TARGET };
struct ErrorCause {
  ErrorCode code = ErrorCode::UNAVAILABLE;
  Text detail;
  Optional<RejectionOrigin> rejection_origin;
  bool operator==(const ErrorCause &) const = default;
};
struct Error : ErrorCause {
  Optional<ErrorCause> cause;
  Optional<std::uint32_t> step_index;
  bool operator==(const Error &) const = default;
};
inline Error error(ErrorCode code, const char *detail,
                   RejectionOrigin origin = RejectionOrigin::CORE) {
  Error e;
  e.code = code;
  e.detail = Text::literal(detail);
  e.rejection_origin = origin;
  return e;
}
enum class Lifecycle {
  QUEUED,
  DISPATCHED,
  ACCEPTED,
  CONFIRMED_COMPLETED,
  REJECTED,
  FAILED,
  EXPIRED_BEFORE_SEND,
  UNCERTAIN_OUTCOME
};
enum class Delivery { NOT_SENT, PARTIALLY_SENT, SENT, ACKNOWLEDGED, AMBIGUOUS };
enum class Outcome { CONFIRMED, UNCONFIRMED, UNKNOWN, REJECTED };
struct Attempt {
  std::uint32_t attempt_index = 1;
  Id binding_id;
  U64 mapping_revision = 1, session_generation = 1;
  MonoTime dispatch_time;
  Optional<MonoTime> closed_time;
  Optional<std::uint32_t> step_index;
  Delivery delivery = Delivery::NOT_SENT;
  Outcome target_outcome = Outcome::UNKNOWN;
  List<Evidence, 32> evidence;
  bool guaranteed_no_execution = false;
  List<Id, Limits::resources> resource_ids;
  Value<Error> error = Value<Error>::absent(Absence::NOT_APPLICABLE);
};
struct Progress {
  std::uint32_t total = 1, emitted = 0, confirmed = 0;
  bool uncertain_remaining = false;
  bool operator==(const Progress &) const = default;
};
struct CommandResult {
  Id request_id;
  U64 result_revision = 1;
  Lifecycle lifecycle = Lifecycle::QUEUED;
  bool terminal = false;
  Delivery delivery = Delivery::NOT_SENT;
  Outcome target_outcome = Outcome::UNKNOWN;
  Value<Error> error = Value<Error>::absent(Absence::NOT_APPLICABLE);
  List<Attempt, 3072> attempts;
  Progress progress;
  bool deadline_elapsed = false, invalidated = false;
  Optional<CatalogSnapshot> query_result;
  Value<Text> waiting_reason = Value<Text>::absent(Absence::NOT_APPLICABLE);
  Value<MonoTime> first_terminal_time;
  Value<Error> closure_error = Value<Error>::absent(Absence::NOT_APPLICABLE);
};
inline bool Scalar::operator==(const Scalar &x) const {
  if (kind != x.kind)
    return false;
  switch (kind) {
  case ScalarKind::BOOL:
    return boolean == x.boolean;
  case ScalarKind::UINT32:
  case ScalarKind::UINT64:
    return unsigned_value == x.unsigned_value;
  case ScalarKind::INT64:
    return signed_value == x.signed_value;
  case ScalarKind::RATIONAL:
    return rational == x.rational;
  case ScalarKind::STRING:
    return string == x.string;
  case ScalarKind::POWER:
    return power == x.power;
  case ScalarKind::PLAYBACK:
    return playback == x.playback;
  case ScalarKind::DIRECTION:
    return direction == x.direction;
  case ScalarKind::REFERENCE:
    return reference == x.reference;
  case ScalarKind::ASSET:
    return asset == x.asset;
  }
  return false;
}
} // namespace control
