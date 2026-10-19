#include "control/mock_adapter.hpp"
#include <cstdio>
#include <limits>
using namespace control;
static int checks = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      std::printf("FAIL line %d: %s\n", __LINE__, #x);                         \
      return false;                                                            \
    }                                                                          \
  } while (0)
struct Fixture {
  Store store;
  MockJournal journal;
  Core core;
  MockAdapter mock;
  Id d{"mock.device"}, t{"mock.target"}, b{"mock.binding"};
  Fixture(Feedback f = Feedback::CORRELATED_RESULT,
          Ordering o = Ordering::STRICT)
      : core(store, journal, Id("epoch.1")) {
    if (!mock.install(core, d, t, b, f, o))
      std::abort();
  }
  CommandRequest request(std::string_view op = "power.toggle",
                         Arguments args = {}, U64 deadline = 1000,
                         U64 now = 0) {
    CommandRequest q;
    auto r = core.capture(q, d, t, op, args, deadline, now);
    if (r.error.present)
      std::abort();
    return q;
  }
  AdapterEvidence late() { return mock.completion(); }
};
static Arguments step(std::uint32_t count = 3) {
  Arguments a;
  Scalar d;
  d.kind = ScalarKind::DIRECTION;
  d.direction = Direction::UP;
  Scalar n;
  n.kind = ScalarKind::UINT32;
  n.unsigned_value = count;
  a.fields.push({Text::literal("direction"), d});
  a.fields.push({Text::literal("count"), n});
  return a;
}
static Arguments level(Rational r) {
  Arguments a;
  a.fields.push({Text::literal("level"), Scalar::level(r)});
  return a;
}
static bool registration() {
  Fixture f;
  CHECK(f.core.device(f.d));
  CHECK(f.core.target(f.d, f.t));
  CHECK(f.core.binding(f.b));
  CHECK(f.core.target(f.d, f.t)->capabilities.size() == 14);
  CHECK(f.mock.identity() == Id("adapter.mock"));
  return true;
}
static bool duplicate_identity() {
  Fixture f;
  CommonDevice d;
  d.device_id = f.d;
  CHECK(f.core.register_device(d, 0).value.code ==
        ErrorCode::INVALID_REFERENCE);
  return true;
}
static bool valid_observation() {
  Fixture f;
  auto o = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  CHECK(f.core.observe(o, 0));
  CHECK(f.core.field(f.d, f.t, "power")->selected.present);
  CHECK(f.core.field(f.d, f.t, "power")->selected.value.value.value.power ==
        Power::ON);
  return true;
}
static bool stale_age() {
  Fixture f;
  auto o = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  o.age_at_receipt_ms = 5000;
  CHECK(f.core.observe(o, 0));
  CHECK(!f.core.field(f.d, f.t, "power")->selected.present);
  CHECK(f.core.field(f.d, f.t, "power")->selected.reason == Absence::UNKNOWN);
  return true;
}
static bool unknown_age() {
  Fixture f;
  auto o = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  o.age_at_receipt_ms = {};
  CHECK(f.core.observe(o, 0));
  CHECK(!f.core.field(f.d, f.t, "power")->selected.present);
  return true;
}
static bool absent_reasons() {
  Fixture f;
  for (auto reason :
       {Absence::UNKNOWN, Absence::UNAVAILABLE, Absence::UNSUPPORTED}) {
    auto o =
        f.mock.sample(f.core, f.b, "power", Value<Scalar>::absent(reason), 0);
    CHECK(f.core.observe(o, 0));
    CHECK(f.core.field(f.d, f.t, "power")->selected.reason == reason);
  }
  return true;
}
static bool success() {
  Fixture f;
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  CHECK(r.result);
  CHECK(r.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
  CHECK(r.result->delivery == Delivery::ACKNOWLEDGED);
  CHECK(r.result->progress.confirmed == 1);
  CHECK(f.mock.dispatches == 1);
  CHECK(!f.core.field(f.d, f.t, "power"));
  return true;
}
static bool unsupported() {
  Fixture f;
  auto q = f.request();
  q.operation_id = Text::literal("not.a.common.operation");
  q.arguments = {};
  auto r = f.core.submit(q, 0);
  CHECK(r.result->lifecycle == Lifecycle::REJECTED);
  CHECK(r.result->error.value.code == ErrorCode::UNSUPPORTED_OPERATION);
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool duplicate_active() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::NO_RESULT;
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  CHECK(!r.result->terminal);
  auto r2 = f.core.submit(q, 1);
  CHECK(r2.result == r.result);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool duplicate_terminal() {
  Fixture f;
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  auto r2 = f.core.submit(q, 1000);
  CHECK(r2.result == r.result);
  CHECK(r2.result->deadline_elapsed);
  CHECK(r2.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool duplicate_changed() {
  Fixture f;
  auto q = f.request();
  f.core.submit(q, 0);
  q.deadline.tick_ms = 999;
  auto r = f.core.submit(q, 0);
  CHECK(r.error.value.code == ErrorCode::REQUEST_ID_MISMATCH);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool argument_map_order() {
  Fixture f;
  auto q = f.request("volume.step", step());
  f.core.submit(q, 0);
  std::swap(q.arguments.fields[0], q.arguments.fields[1]);
  auto r = f.core.submit(q, 0);
  CHECK(!r.error.present);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool stale_capability() {
  Fixture f;
  auto q = f.request();
  auto c =
      MockAdapter::operation(f.b, "power.toggle", Feedback::CORRELATED_RESULT);
  CHECK(!f.core.capability(f.d, f.t, std::move(c), 0).present);
  auto r = f.core.submit(q, 0);
  CHECK(r.result->error.value.code == ErrorCode::STALE_PRECONDITION);
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool old_session() {
  Fixture f;
  auto o = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  CHECK(!f.core.reconnect(f.b, 0).present);
  CHECK(!f.core.observe(o, 0));
  return true;
}
static bool expired_before() {
  Fixture f;
  auto q = f.request();
  auto r = f.core.submit(q, 1000);
  CHECK(r.result->lifecycle == Lifecycle::EXPIRED_BEFORE_SEND);
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool expiry_after() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::NO_RESULT;
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  f.core.tick(1000);
  CHECK(r.result->lifecycle == Lifecycle::UNCERTAIN_OUTCOME);
  CHECK(r.result->delivery == Delivery::SENT);
  CHECK(r.result->error.value.code == ErrorCode::TIMEOUT);
  return true;
}
static bool late_result() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::DELAYED;
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  f.core.tick(1000);
  auto first = r.result->first_terminal_time;
  auto closure = r.result->closure_error;
  CHECK(f.core.report(f.late(), 1001));
  CHECK(r.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
  CHECK(r.result->first_terminal_time == first);
  CHECK(r.result->closure_error == closure);
  CHECK(r.result->deadline_elapsed);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool immediate_refusal() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::REJECT;
  auto r = f.core.submit(f.request(), 0);
  CHECK(r.result->lifecycle == Lifecycle::REJECTED);
  CHECK(r.result->target_outcome == Outcome::REJECTED);
  CHECK(r.result->attempts[0].guaranteed_no_execution);
  return true;
}
static bool unavailable() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::UNAVAILABLE;
  auto r = f.core.submit(f.request(), 0);
  CHECK(r.result->lifecycle == Lifecycle::FAILED);
  CHECK(r.result->delivery == Delivery::NOT_SENT);
  CHECK(r.result->error.value.code == ErrorCode::UNAVAILABLE);
  return true;
}
static bool open_loop() {
  Fixture f(Feedback::NONE, Ordering::LOCAL_ONLY);
  f.mock.mode = MockAdapter::Mode::OPEN_LOOP;
  auto r = f.core.submit(f.request(), 0);
  CHECK(r.result->lifecycle == Lifecycle::ACCEPTED);
  CHECK(r.result->terminal);
  CHECK(r.result->target_outcome == Outcome::UNCONFIRMED);
  CHECK(!r.result->error.present);
  CHECK(f.store.effects[0].resolved());
  return true;
}
static bool strict_open_loop() {
  Fixture f(Feedback::NONE);
  f.mock.mode = MockAdapter::Mode::OPEN_LOOP;
  auto r = f.core.submit(f.request(), 0);
  CHECK(r.result->lifecycle == Lifecycle::ACCEPTED);
  CHECK(!f.store.effects[0].resolved());
  f.core.tick(1000);
  CHECK(r.result->lifecycle == Lifecycle::ACCEPTED);
  CHECK(r.result->deadline_elapsed);
  return true;
}
static bool known_partial() {
  Fixture f(Feedback::NONE, Ordering::LOCAL_ONLY);
  f.mock.mode = MockAdapter::Mode::PARTIAL;
  auto r = f.core.submit(f.request("volume.step", step()), 0);
  CHECK(r.result->lifecycle == Lifecycle::FAILED);
  CHECK(r.result->delivery == Delivery::PARTIALLY_SENT);
  CHECK(r.result->error.value.code == ErrorCode::PARTIAL_EXECUTION);
  CHECK(r.result->error.value.cause.value.code == ErrorCode::UNAVAILABLE);
  CHECK(!r.result->progress.uncertain_remaining);
  CHECK(!f.store.effects[0].resolved());
  return true;
}
static bool ambiguous() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto r = f.core.submit(f.request(), 0);
  CHECK(r.result->lifecycle == Lifecycle::UNCERTAIN_OUTCOME);
  CHECK(r.result->delivery == Delivery::AMBIGUOUS);
  CHECK(r.result->target_outcome == Outcome::UNKNOWN);
  return true;
}
static bool conflict_resource() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::NO_RESULT;
  auto a = f.core.submit(f.request(), 0);
  CHECK(a.result);
  auto b = f.core.submit(f.request(), 0);
  CHECK(b.result->lifecycle == Lifecycle::QUEUED);
  CHECK(b.result->waiting_reason.value.view() == "ORDERING_BLOCKED");
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool proof_only_covered() {
  Fixture f;
  SharedResource a;
  a.resource_id = Id("resource.a");
  a.member_targets.push({f.d, f.t});
  Evidence ev;
  ev.evidence_id = Id("owner.resource");
  ev.kind = EvidenceKind::OWNER_ASSOCIATION;
  a.evidence.push(ev);
  CHECK(!f.core.register_resource(a, 0).present);
  a.resource_id = Id("resource.b");
  CHECK(!f.core.register_resource(a, 0).present);
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto r = f.core.submit(f.request(), 0);
  List<LockKey, Limits::resources + 1> covered;
  covered.push(f.store.effects[0].keys[1].key);
  auto p = f.mock.proof(covered, true);
  CHECK(f.core.report(p, 1));
  CHECK(f.store.effects[0].keys[1].no_future);
  CHECK(!f.store.effects[0].keys[0].no_future);
  CHECK(!f.store.effects[0].keys[2].no_future);
  CHECK(!r.result->attempts[0].guaranteed_no_execution);
  CHECK(r.result->attempts[0].delivery == Delivery::AMBIGUOUS);
  CHECK(r.result->lifecycle == Lifecycle::UNCERTAIN_OUTCOME);
  return true;
}
static bool whole_proof_late() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  auto first = r.result->first_terminal_time;
  auto p = f.mock.proof(f.mock.last.value.covered_keys, true);
  CHECK(f.core.report(p, 1));
  CHECK(r.result->lifecycle == Lifecycle::FAILED);
  CHECK(r.result->delivery == Delivery::AMBIGUOUS);
  CHECK(r.result->attempts[0].delivery == Delivery::AMBIGUOUS);
  CHECK(r.result->attempts[0].guaranteed_no_execution);
  CHECK(r.result->first_terminal_time == first);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool future_only() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto r = f.core.submit(f.request(), 0);
  CHECK(f.core.report(f.mock.proof(f.mock.last.value.covered_keys, false), 1));
  CHECK(f.store.effects[0].resolved());
  CHECK(!r.result->attempts[0].guaranteed_no_execution);
  CHECK(r.result->lifecycle == Lifecycle::UNCERTAIN_OUTCOME);
  return true;
}
static bool stale_media() {
  Fixture f;
  auto o = f.mock.sample(f.core, f.b, "playback.state", Value<Scalar>::of([] {
                           Scalar s;
                           s.kind = ScalarKind::PLAYBACK;
                           s.playback = Playback::PLAYING;
                           return s;
                         }()),
                         0);
  CHECK(!f.core.media_changed(f.d, f.t, 0).present);
  CHECK(!f.core.observe(o, 0));
  return true;
}
static bool purge_boundary() {
  Fixture f;
  auto q = f.request();
  f.core.submit(q, 0);
  CHECK(f.core.submit(q, 59999).result);
  auto r = f.core.submit(q, 60000);
  CHECK(r.error.value.code == ErrorCode::REQUEST_HISTORY_EXPIRED);
  CHECK(!f.core.result(q.request_id));
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool capacity() {
  Fixture f(Feedback::NONE, Ordering::LOCAL_ONLY);
  f.mock.mode = MockAdapter::Mode::OPEN_LOOP;
  for (unsigned i = 0; i < 1024; ++i) {
    auto r = f.core.submit(f.request(), 0);
    CHECK(r.result);
    CHECK(r.result->lifecycle == Lifecycle::ACCEPTED);
  }
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  CHECK(r.result);
  CHECK(r.result->error.value.detail.view() == "DEDUP_CAPACITY");
  CHECK(f.mock.dispatches == 1024);
  CHECK(f.core.submit(q, 0).error.value.code ==
        ErrorCode::REQUEST_HISTORY_EXPIRED);
  return true;
}
static bool cancellation() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::NO_RESULT;
  auto q = f.request();
  f.core.submit(q, 0);
  auto r = f.core.cancel(q.request_id, 1);
  CHECK(r.result->lifecycle == Lifecycle::UNCERTAIN_OUTCOME);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool queued_expiry() {
  Fixture f;
  auto c =
      MockAdapter::operation(f.b, "power.toggle", Feedback::CORRELATED_RESULT);
  c.availability = Availability::OFFLINE;
  CHECK(!f.core.capability(f.d, f.t, c, 0).present);
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  CHECK(r.result->lifecycle == Lifecycle::QUEUED);
  f.core.tick(1000);
  CHECK(r.result->lifecycle == Lifecycle::EXPIRED_BEFORE_SEND);
  return true;
}
static bool expiry_precedence() {
  Fixture f;
  auto c =
      MockAdapter::operation(f.b, "power.toggle", Feedback::CORRELATED_RESULT);
  c.availability = Availability::OFFLINE;
  f.core.capability(f.d, f.t, c, 0);
  auto q = f.request();
  auto r = f.core.submit(q, 0);
  c.availability = Availability::READY;
  f.core.capability(f.d, f.t, c, 1000);
  CHECK(r.result->lifecycle == Lifecycle::EXPIRED_BEFORE_SEND);
  CHECK(r.result->error.value.code == ErrorCode::DEADLINE_EXPIRED);
  return true;
}
static bool duplicate_observation() {
  Fixture f;
  auto o = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  CHECK(f.core.observe(o, 0));
  o.receipt_time.tick_ms = 4000;
  CHECK(!f.core.observe(o, 4000));
  f.core.tick(5000);
  CHECK(!f.core.field(f.d, f.t, "power")->selected.present);
  return true;
}
static bool sequence_conflict() {
  Fixture f;
  auto o = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  CHECK(f.core.observe(o, 0));
  o.value = Value<Scalar>::of(Scalar::power_value(Power::OFF));
  CHECK(!f.core.observe(o, 0));
  CHECK(f.core.field(f.d, f.t, "power")->conflict_latched);
  return true;
}
static bool conflict_latch() {
  Fixture f;
  Id b2("mock.binding.2");
  CHECK(f.mock.install(f.core, f.d, f.t, b2));
  auto a = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  auto b = f.mock.sample(f.core, b2, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::OFF)), 0);
  CHECK(f.core.observe(a, 0));
  CHECK(f.core.observe(b, 0));
  CHECK(f.core.field(f.d, f.t, "power")->conflict_latched);
  f.core.tick(5000);
  f.core.begin_resync(f.b, "power", 5000);
  a = f.mock.sample(f.core, f.b, "power",
                    Value<Scalar>::of(Scalar::power_value(Power::ON)), 5000,
                    Origin::RESYNC);
  CHECK(f.core.observe(a, 5000));
  CHECK(f.core.field(f.d, f.t, "power")->conflict_latched);
  f.core.begin_resync(b2, "power", 5000);
  b = f.mock.sample(f.core, b2, "power",
                    Value<Scalar>::of(Scalar::power_value(Power::ON)), 5000,
                    Origin::RESYNC);
  CHECK(f.core.observe(b, 5000));
  CHECK(!f.core.field(f.d, f.t, "power")->conflict_latched);
  return true;
}
static bool purge_late_proof() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto q = f.request();
  f.core.submit(q, 0);
  auto p = f.mock.proof(f.mock.last.value.covered_keys, true);
  f.core.tick(60000);
  CHECK(!f.core.result(q.request_id));
  CHECK(f.store.effects.size() == 1);
  CHECK(f.core.report(p, 60001));
  CHECK(!f.core.result(q.request_id));
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool journal_failure() {
  Fixture f;
  f.journal.writable = false;
  auto r = f.core.submit(f.request(), 0);
  CHECK(r.result);
  CHECK(f.store.blocked);
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool reboot() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto q = f.request();
  f.core.submit(q, 0);
  auto p = f.mock.proof(f.mock.last.value.covered_keys, true);
  Core after(f.store, f.journal, Id("epoch.2"));
  CHECK(!after.result(q.request_id));
  CHECK(f.store.effects.size() == 1);
  CHECK(after.submit(q, 0).error.value.code ==
        ErrorCode::REQUEST_HISTORY_EXPIRED);
  CHECK(after.report(p, 0));
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool version_mismatch() {
  Fixture f;
  auto q = f.request();
  q.common_model_version.major = 2;
  auto r = f.core.submit(q, 0);
  CHECK(r.result->error.value.code == ErrorCode::INVALID_ARGUMENT);
  CHECK(r.result->error.value.detail.view() == "VERSION_MISMATCH");
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool malformed_count() {
  Fixture f;
  auto q = f.request("volume.step", step(0));
  auto r = f.core.submit(q, 0);
  CHECK(r.result->error.value.code == ErrorCode::INVALID_ARGUMENT);
  CHECK(r.result->progress.total == 1);
  return true;
}
static bool numeric_exact() {
  Fixture f;
  auto q = f.request("volume.set", level({17, 25}));
  CHECK(f.core.submit(q, 0).result->lifecycle ==
        Lifecycle::CONFIRMED_COMPLETED);
  auto invalid = f.request("volume.set", level({1, 3}));
  CHECK(f.core.submit(invalid, 0).result->error.value.code ==
        ErrorCode::INVALID_ARGUMENT);
  return true;
}
static bool native_table() {
  Fixture f;
  auto c =
      MockAdapter::operation(f.b, "volume.set", Feedback::CORRELATED_RESULT);
  auto &m = c.numeric_mapping.value;
  m.transfer = NumericMapping::Transfer::TABLE;
  m.set = NumericMapping::Set::FINITE;
  m.rounding = NumericMapping::Rounding::NEAREST_TIES_UP;
  m.allowed_error = {1, 2};
  m.table.push({{0, 1}, {0, 1}});
  m.table.push({{1, 1}, {10, 1}});
  m.finite.push({0, 1});
  m.finite.push({10, 1});
  m.external_max = {10, 1};
  CHECK(!f.core.capability(f.d, f.t, std::move(c), 0).present);
  CHECK(f.core.submit(f.request("volume.set", level({1, 2})), 0)
            .result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
  CHECK((f.mock.last_numeric.value == Rational{10, 1}));
  CHECK(f.core.submit(f.request("volume.set", level({1, 3})), 0)
            .result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
  CHECK((f.mock.last_numeric.value == Rational{0, 1}));
  return true;
}
static bool known_ongoing_partial() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::ONGOING_PARTIAL;
  auto q = f.request("volume.step", step());
  auto r = f.core.submit(q, 0);
  CHECK(r.result->lifecycle == Lifecycle::DISPATCHED);
  CHECK(r.result->delivery == Delivery::PARTIALLY_SENT);
  CHECK(r.result->progress.emitted == 1);
  return true;
}
static bool proof_idempotent() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto r = f.core.submit(f.request(), 0);
  auto p = f.mock.proof(f.mock.last.value.covered_keys, true);
  CHECK(f.core.report(p, 1));
  auto revision = r.result->result_revision;
  CHECK(!f.core.report(p, 1));
  CHECK(r.result->result_revision == revision);
  return true;
}
static bool contradiction_quarantined() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto r = f.core.submit(f.request(), 0);
  CHECK(f.core.report(f.mock.proof(f.mock.last.value.covered_keys, true), 1));
  CHECK(!f.core.report(f.late(), 2));
  CHECK(r.result->lifecycle == Lifecycle::FAILED);
  return true;
}
static bool unknown_power_toggle() {
  Fixture f(Feedback::NONE, Ordering::LOCAL_ONLY);
  f.mock.mode = MockAdapter::Mode::OPEN_LOOP;
  auto o = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::absent(Absence::UNKNOWN), 0);
  CHECK(f.core.observe(o, 0));
  f.core.submit(f.request(), 0);
  CHECK(f.core.field(f.d, f.t, "power")->selected.reason == Absence::UNKNOWN);
  return true;
}
static bool no_hidden_retry() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto q = f.request();
  f.core.submit(q, 0);
  f.core.tick(1000);
  f.core.tick(2000);
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool bounded_allocator() {
  auto previous = Memory::limit;
  Memory::limit = Memory::used;
  Text t;
  CHECK(!t.assign("no-room"));
  Memory::limit = previous;
  CHECK(!t.valid());
  return true;
}
static bool monotonic_rejection() {
  Fixture f;
  f.core.tick(20);
  auto q = f.request("power.toggle", {}, 1000, 20);
  auto r = f.core.submit(q, 19);
  CHECK(r.error.present);
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool deadline_overflow() {
  Fixture f;
  auto q = f.request();
  q.deadline.tick_ms = UINT64_MAX;
  CHECK(f.core.submit(q, 0).result->error.value.code ==
        ErrorCode::INVALID_ARGUMENT);
  return true;
}
static bool unknown_contributor() {
  Fixture f;
  Id b2("mock.binding.2");
  CHECK(f.mock.install(f.core, f.d, f.t, b2));
  auto c = MockAdapter::readable(f.b, "power");
  c.support = Support::UNSUPPORTED;
  CHECK(!f.core.capability(f.d, f.t, c, 0).present);
  auto o = f.mock.sample(f.core, b2, "power",
                         Value<Scalar>::absent(Absence::UNKNOWN), 0);
  CHECK(f.core.observe(o, 0));
  CHECK(f.core.field(f.d, f.t, "power")->selected.reason == Absence::UNKNOWN);
  return true;
}
static bool catalog_stale_reference() {
  Fixture f;
  auto c =
      MockAdapter::operation(f.b, "source.select", Feedback::CORRELATED_RESULT);
  CHECK(!f.core.capability(f.d, f.t, c, 0).present);
  CatalogSnapshot cat;
  cat.kind = CatalogKind::SOURCE;
  cat.device_id = f.d;
  cat.target_id = f.t;
  cat.complete = true;
  CatalogItem i;
  i.item_id = Id("mock.source.1");
  i.label = Text::literal("synthetic input");
  cat.items.push(i);
  CHECK(!f.core.catalog(cat, f.b, 1, 1, 0).present);
  Scalar v;
  v.kind = ScalarKind::REFERENCE;
  v.reference = f.store.catalogs[0].snapshot.items[0].reference;
  Arguments args;
  args.fields.push({Text::literal("source"), v});
  auto q = f.request("source.select", args);
  i.item_id = Id("mock.source.2");
  cat.items.push(i);
  CHECK(!f.core.catalog(cat, f.b, 1, 1, 0).present);
  auto r = f.core.submit(q, 0);
  CHECK(r.result->error.value.code == ErrorCode::STALE_PRECONDITION);
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool catalog_partial_order() {
  Fixture f;
  CatalogSnapshot cat;
  cat.kind = CatalogKind::SOURCE;
  cat.device_id = f.d;
  cat.target_id = f.t;
  CatalogItem i;
  i.item_id = Id("z");
  i.label = Text::literal("z");
  cat.items.push(i);
  CHECK(!f.core.catalog(cat, f.b, 1, 1, 0).present);
  auto generation = f.store.catalogs[0].snapshot.catalog_generation;
  cat.items.clear();
  i.item_id = Id("c");
  cat.items.push(i);
  i.item_id = Id("a");
  cat.items.push(i);
  CHECK(!f.core.catalog(cat, f.b, 1, 1, 0).present);
  const auto &result = f.store.catalogs[0].snapshot;
  CHECK(result.catalog_generation == generation + 1);
  CHECK(result.items[0].item_id == Id("z"));
  CHECK(result.items[1].item_id == Id("a"));
  CHECK(result.items[2].item_id == Id("c"));
  CHECK(!f.core.catalog(cat, f.b, 1, 1, 0).present);
  CHECK(f.store.catalogs[0].snapshot.catalog_generation == generation + 1);
  return true;
}
static bool queue_capacity() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::NO_RESULT;
  f.core.submit(f.request(), 0);
  for (unsigned n = 0; n < 64; ++n)
    CHECK(f.core.submit(f.request(), 0).result->lifecycle == Lifecycle::QUEUED);
  CHECK(f.core.submit(f.request(), 0).result->error.value.detail.view() ==
        "QUEUE_FULL");
  CHECK(f.mock.dispatches == 1);
  return true;
}
static bool partial_completion_quarantined() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  f.core.submit(f.request(), 0);
  auto e = f.late();
  e.covered_keys.clear();
  CHECK(!f.core.report(e, 1));
  CHECK(!f.store.effects[0].resolved());
  return true;
}
static bool history_no_regression() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::NO_RESULT;
  auto r = f.core.submit(f.request("volume.step", step()), 0);
  auto e = f.mock.last.value;
  e.evidence.evidence_id = Id("weaker");
  e.progress = Progress{3, 1, 0, false};
  e.delivery = Delivery::PARTIALLY_SENT;
  CHECK(!f.core.report(e, 1));
  CHECK(r.result->progress.emitted == 3);
  CHECK(r.result->delivery == Delivery::SENT);
  return true;
}
static bool descriptor_schema_cycle() {
  Fixture f;
  auto c =
      MockAdapter::operation(f.b, "power.toggle", Feedback::CORRELATED_RESULT);
  c.result_schema.value.nodes[0].edges[0].node = 0;
  CHECK(f.core.capability(f.d, f.t, c, 0).value.code ==
        ErrorCode::INVALID_ARGUMENT);
  return true;
}
static bool sequence_reset_all_fields() {
  Fixture f;
  auto a = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  auto b = f.mock.sample(f.core, f.b, "volume.mute",
                         Value<Scalar>::of(Scalar::boolean_value(true)), 0);
  CHECK(f.core.observe(a, 0));
  CHECK(f.core.observe(b, 0));
  a.source_sequence.value.domain = Id("reset");
  CHECK(!f.core.observe(a, 0));
  CHECK(f.core.binding(f.b)->session_generation == 2);
  CHECK(!f.core.field(f.d, f.t, "volume.mute")->selected.present);
  auto recovered = f.mock.sample(
      f.core, f.b, "power", Value<Scalar>::of(Scalar::power_value(Power::ON)),
      0, Origin::RESYNC);
  recovered.source_sequence.value.domain = Id("reset");
  CHECK(f.core.observe(recovered, 0));
  CHECK(f.core.binding(f.b)->session_generation == 2);
  CHECK(f.core.field(f.d, f.t, "power")->selected.present);
  return true;
}
static bool descriptor_invalidates_state() {
  Fixture f;
  auto a = f.mock.sample(f.core, f.b, "power",
                         Value<Scalar>::of(Scalar::power_value(Power::ON)), 0);
  CHECK(f.core.observe(a, 0));
  auto c = MockAdapter::readable(f.b, "power");
  c.support = Support::UNSUPPORTED;
  CHECK(!f.core.capability(f.d, f.t, c, 0).present);
  CHECK(!f.core.field(f.d, f.t, "power")->selected.present);
  CHECK(f.core.field(f.d, f.t, "power")->selected.reason ==
        Absence::UNSUPPORTED);
  return true;
}
static bool proof_unblocks_new_intent() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto old = f.request();
  f.core.submit(old, 0);
  auto proof = f.mock.proof(f.mock.last.value.covered_keys, true);
  auto next = f.request();
  auto r = f.core.submit(next, 0);
  CHECK(r.result->lifecycle == Lifecycle::QUEUED);
  f.mock.mode = MockAdapter::Mode::CONFIRMED;
  CHECK(f.core.report(proof, 1));
  CHECK(r.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
  CHECK(f.mock.dispatches == 2);
  CHECK(f.core.result(old.request_id)->lifecycle == Lifecycle::FAILED);
  return true;
}
static bool resource_change_stales_capture() {
  Fixture f;
  auto q = f.request();
  SharedResource r;
  r.resource_id = Id("resource.new");
  r.member_targets.push({f.d, f.t});
  Evidence e;
  e.evidence_id = Id("resource.claim");
  r.evidence.push(e);
  CHECK(!f.core.register_resource(r, 0).present);
  CHECK(f.core.submit(q, 0).result->error.value.code ==
        ErrorCode::STALE_PRECONDITION);
  CHECK(f.mock.dispatches == 0);
  return true;
}

static bool purged_extent_retained() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::AMBIGUOUS;
  auto q = f.request("volume.step", step());
  f.core.submit(q, 0);
  auto completion = f.late();
  f.core.tick(60000);
  completion.progress = Progress{1, 1, 1, false};
  CHECK(!f.core.report(completion, 60001));
  CHECK(f.store.effects.size() == 1);
  CHECK(!f.store.effects[0].resolved());
  return true;
}
static bool narrower_argument_domain() {
  Fixture f;
  auto c =
      MockAdapter::operation(f.b, "power.set", Feedback::CORRELATED_RESULT);
  c.argument_schema.value.nodes[1].values.clear();
  c.argument_schema.value.nodes[1].values.push(Text::literal("OFF"));
  CHECK(!f.core.capability(f.d, f.t, c, 0).present);
  Arguments args;
  args.fields.push({Text::literal("state"), Scalar::power_value(Power::ON)});
  CHECK(
      f.core.submit(f.request("power.set", args), 0).result->error.value.code ==
      ErrorCode::INVALID_ARGUMENT);
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool deadline_single_revision() {
  Fixture f;
  f.mock.mode = MockAdapter::Mode::NO_RESULT;
  auto r = f.core.submit(f.request(), 0);
  auto revision = r.result->result_revision;
  f.core.tick(1000);
  CHECK(r.result->result_revision == revision + 1);
  return true;
}

static bool local_effect_reclamation() {
  Fixture f(Feedback::NONE, Ordering::LOCAL_ONLY);
  f.mock.mode = MockAdapter::Mode::OPEN_LOOP;
  f.core.submit(f.request(), 0);
  CHECK(f.store.effects.size() == 1);
  f.core.tick(60000);
  CHECK(f.store.effects.size() == 0);
  return true;
}

static bool allocation_failure_sweep() {
  auto limit = Memory::limit;
  for (unsigned extra = 0; extra < 4096; extra += 128) {
    Fixture f;
    auto q = f.request();
    Memory::limit = Memory::used + extra;
    auto r = f.core.submit(q, 0);
    CHECK(r.result || r.error.present);
    Memory::limit = limit;
    f.core.tick(1000);
    CHECK(f.mock.dispatches <= 1);
    if (f.core.result(q.request_id)) {
      const auto *x = f.core.result(q.request_id);
      if (x->attempts.size())
        CHECK(f.store.effects.size() == 1);
    }
  }
  return true;
}

static bool allocation_refusal_typed() {
  Fixture f;
  auto q = f.request();
  auto limit = Memory::limit;
  Memory::limit = Memory::used;
  auto r = f.core.submit(q, 0);
  Memory::limit = limit;
  CHECK(r.result);
  CHECK(r.result->error.value.code == ErrorCode::UNAVAILABLE);
  CHECK(r.result->error.value.detail.view() == "MEMORY_CAPACITY");
  CHECK(f.mock.dispatches == 0);
  return true;
}
static bool acknowledgement_history() {
  Fixture f;
  auto r = f.core.submit(f.request(), 0);
  auto e = f.mock.last.value;
  e.evidence.evidence_id = Id("weaker.delivery");
  e.signal = AdapterEvidence::Signal::PROGRESS;
  e.delivery = Delivery::SENT;
  CHECK(!f.core.report(e, 1));
  CHECK(r.result->attempts[0].delivery == Delivery::ACKNOWLEDGED);
  return true;
}

int main() {
  struct Test {
    const char *name;
    bool (*run)();
  };
  const Test tests[] = {
#define T(x) {#x, x}
      T(registration),
      T(duplicate_identity),
      T(valid_observation),
      T(stale_age),
      T(unknown_age),
      T(absent_reasons),
      T(success),
      T(unsupported),
      T(duplicate_active),
      T(duplicate_terminal),
      T(duplicate_changed),
      T(argument_map_order),
      T(stale_capability),
      T(old_session),
      T(expired_before),
      T(expiry_after),
      T(late_result),
      T(immediate_refusal),
      T(unavailable),
      T(open_loop),
      T(strict_open_loop),
      T(known_partial),
      T(ambiguous),
      T(conflict_resource),
      T(proof_only_covered),
      T(whole_proof_late),
      T(future_only),
      T(stale_media),
      T(purge_boundary),
      T(capacity),
      T(cancellation),
      T(queued_expiry),
      T(expiry_precedence),
      T(duplicate_observation),
      T(sequence_conflict),
      T(conflict_latch),
      T(purge_late_proof),
      T(journal_failure),
      T(reboot),
      T(version_mismatch),
      T(malformed_count),
      T(numeric_exact),
      T(native_table),
      T(known_ongoing_partial),
      T(proof_idempotent),
      T(contradiction_quarantined),
      T(unknown_power_toggle),
      T(no_hidden_retry),
      T(bounded_allocator),
      T(monotonic_rejection),
      T(deadline_overflow),
      T(unknown_contributor),
      T(catalog_stale_reference),
      T(catalog_partial_order),
      T(queue_capacity),
      T(partial_completion_quarantined),
      T(history_no_regression),
      T(descriptor_schema_cycle),
      T(sequence_reset_all_fields),
      T(descriptor_invalidates_state),
      T(proof_unblocks_new_intent),
      T(resource_change_stales_capture),
      T(purged_extent_retained),
      T(narrower_argument_domain),
      T(deadline_single_revision),
      T(local_effect_reclamation),
      T(allocation_failure_sweep),
      T(allocation_refusal_typed),
      T(acknowledgement_history)
#undef T
  };
  unsigned pass = 0, failures = 0;
  for (const auto &t : tests) {
    auto before = Memory::used;
    bool ok = t.run();
    if (Memory::used != before) {
      std::printf("LEAK %s: %zu -> %zu\n", t.name, before, Memory::used);
      ok = false;
    }
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", t.name);
    if (ok)
      ++pass;
    else
      ++failures;
  }
  std::printf(
      "RESULT %u PASS / %u FAIL; %d assertions; peak allocations %zu bytes\n",
      pass, failures, checks, Memory::peak);
  std::printf("SIZE Text=%zu Id=%zu Request=%zu Result=%zu Observation=%zu "
              "Descriptor=%zu Record=%zu Effect=%zu Store=%zu Core=%zu\n",
              sizeof(Text), sizeof(Id), sizeof(CommandRequest),
              sizeof(CommandResult), sizeof(StateObservation),
              sizeof(CapabilityDescriptor), sizeof(Record), sizeof(Effect),
              sizeof(Store), sizeof(Core));
  return failures ? 1 : 0;
}
