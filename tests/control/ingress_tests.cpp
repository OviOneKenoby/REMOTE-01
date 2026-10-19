#include "control_home/development.hpp"
#include <cstdio>
#include <cstring>
#include <thread>
#include <string>
#include <cstdlib>

using namespace control;
using namespace control::ingress;
using control_home::development::Session;
namespace {
unsigned assertions = 0, multiplier = 1;
U64 iterations = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)
#define START Session s(Id("ingress.epoch")); CHECK(s.initialize(10))
Event sample(Session &s, U64 now, Rational volume = {1, 4}) {
  Event e; e.kind = Kind::SAMPLE; s.boundary.inbox.scope(e.scope);
  e.payload.emplace<Sample>();
  auto o = s.device.mock.sample(s.core, s.b, "volume.level", Value<Scalar>::of(Scalar::level(volume)), now);
  if (!pack(o, std::get<Sample>(e.payload))) std::abort();
  return e;
}
Event lifecycle(Session &s, Kind kind) {
  Event e; e.kind = kind; s.boundary.inbox.scope(e.scope); return e;
}
Event result(Session &s, AdapterEvidence evidence) {
  Event e; e.kind = Kind::RESULT; s.boundary.inbox.scope(e.scope); e.payload.emplace<Result>();
  if (!pack(evidence, std::get<Result>(e.payload))) std::abort();
  return e;
}
bool fifo_owned_copy() {
  START;
  auto e = sample(s, 11, {1, 4});
  const auto used = Memory::used;
  for (unsigned i = 0; i < Inbox::capacity; ++i) {
    auto &v = std::get<Sample>(e.payload); v.ordinal += i; v.id.set("fifo." + std::to_string(i));
    v.rational = {static_cast<std::int64_t>(i), 1}; // decoded generic rational, Core domain will reject >1
    if (i < 2) v.rational = i ? Rational{1, 1} : Rational{0, 1};
    CHECK(s.boundary.inbox.post(e) == Status::POSTED);
  }
  e = {}; CHECK(Memory::used == used); // no transferred Text/refcount/borrow
  CHECK(s.boundary.process_one(11) == Status::APPLIED && s.snapshot().volume == 0);
  CHECK(s.boundary.process_one(11) == Status::APPLIED && s.snapshot().volume == 100);
  for (unsigned i = 2; i < Inbox::capacity; ++i) CHECK(s.boundary.process_one(11) == Status::CORE_REJECTED);
  CHECK(s.boundary.process_one(11) == Status::EMPTY);
  // Wrapped slots preserve order and sample identity.
  auto a = sample(s, 12, {1, 2}), b = sample(s, 12, {1, 4});
  std::get<Sample>(a.payload).ordinal += 30;
  std::get<Sample>(b.payload).ordinal += 30;
  CHECK(s.boundary.inbox.post(a) == Status::POSTED && s.boundary.inbox.post(b) == Status::POSTED);
  CHECK(s.boundary.process_one(12) == Status::APPLIED && s.snapshot().volume == 50);
  CHECK(s.boundary.process_one(12) == Status::APPLIED && s.snapshot().volume == 25);
  return true;
}
bool overflow_fails_closed() {
  START;
  auto e = sample(s, 11);
  for (unsigned i = 0; i < Inbox::capacity; ++i) CHECK(s.boundary.inbox.post(e) == Status::POSTED);
  CHECK(s.boundary.inbox.post(e) == Status::FULL);
  CHECK(s.boundary.inbox.counts().overflow == 1 && s.boundary.inbox.counts().rejected == 1);
  CHECK(!s.boundary.observe(s.device.mock.sample(s.core, s.b, "volume.level", Value<Scalar>::of(Scalar::level({1, 1})), 11), 11));
  CHECK(s.boundary.process_one(11) == Status::FAULT);
  CHECK(s.snapshot().connection == control_home::Connection::UNAVAILABLE);
  CHECK(s.boundary.counts.discarded == Inbox::capacity);
  const auto revision = s.snapshot().capability_revision, commits = s.journal.commits;
  CHECK(s.boundary.process_one(12) == Status::FAULT);
  CHECK(s.snapshot().capability_revision == revision && s.journal.commits == commits);
  CHECK(s.boundary.inbox.post(e) == Status::CLOSED);
  CHECK(s.boundary.attach(s.b, 13));
  CHECK(s.core.binding(s.b)->session_generation == e.scope.session + 1);
  CHECK(s.boundary.inbox.post(e) == Status::POSTED && s.boundary.process_one(13) == Status::STALE);
  return true;
}
bool reconnect_old_queue() {
  START;
  auto old = sample(s, 11);
  auto reconnect = lifecycle(s, Kind::RECONNECT);
  CHECK(s.boundary.inbox.post(reconnect) == Status::POSTED);
  CHECK(s.boundary.inbox.post(old) == Status::POSTED);
  CHECK(s.boundary.process_one(11) == Status::APPLIED);
  CHECK(s.boundary.process_one(11) == Status::STALE);
  CHECK(!s.snapshot().volume_known && !s.snapshot().next);
  CHECK(s.boundary.reachability(Reachability::REACHABLE, 12));
  CHECK(!s.snapshot().next && !s.snapshot().volume_known); // no implicit READY/truth migration
  CHECK(s.device.command('r', 13));
  CHECK(s.snapshot().volume_known && s.snapshot().next);
  return true;
}
bool revisions_and_media() {
  START;
  auto old = sample(s, 11);
  CHECK(s.boundary.inbox.post(old) == Status::POSTED);
  CHECK(s.device.command('-', 11) && s.device.command('+', 11));
  CHECK(s.boundary.process_one(11) == Status::STALE);
  Event cap = lifecycle(s, Kind::AVAILABILITY);
  auto &c = cap.payload.emplace<CapabilityChange>(); c.id.set("playback.next"); c.availability = Availability::CONTEXT_UNAVAILABLE;
  auto after = sample(s, 12);
  CHECK(s.boundary.inbox.post(cap) == Status::POSTED && s.boundary.inbox.post(after) == Status::POSTED);
  CHECK(s.boundary.process_one(12) == Status::APPLIED && !s.snapshot().next);
  CHECK(s.boundary.process_one(12) == Status::STALE);
  CHECK(s.device.command('c', 12));
  auto observation = s.core.field(s.d, s.t, "media.title")->selected.value;
  Event media; media.kind = Kind::SAMPLE; s.boundary.inbox.scope(media.scope);
  CHECK(pack(observation, media.payload.emplace<Sample>()));
  CHECK(s.boundary.inbox.post(media) == Status::POSTED && s.boundary.media_changed(13));
  CHECK(s.boundary.process_one(13) == Status::STALE && s.snapshot().title[0] == '\0');
  return true;
}
bool stale_age_and_duplicate() {
  START;
  auto e = sample(s, 11);
  CHECK(s.boundary.inbox.post(e) == Status::POSTED && s.boundary.process_one(5011) == Status::APPLIED);
  CHECK(!s.snapshot().volume_known); // queued time is not fresh acquisition
  auto fresh = sample(s, 5012);
  CHECK(s.boundary.inbox.post(fresh) == Status::POSTED && s.boundary.process_one(5012) == Status::APPLIED);
  auto duplicate = fresh; std::get<Sample>(duplicate.payload).receipt = 6000;
  CHECK(s.boundary.inbox.post(duplicate) == Status::POSTED);
  CHECK(s.boundary.process_one(6000) == Status::CORE_REJECTED);
  s.core.tick(10012); CHECK(!s.snapshot().volume_known);
  return true;
}
bool normalization_limits() {
  START;
  auto e = sample(s, 11);
  auto bad = e; std::get<Sample>(bad.payload).receipt = 13;
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(12) == Status::INVALID);
  bad = e; std::get<Sample>(bad.payload).age = UINT64_MAX;
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(12) == Status::INVALID);
  bad = e; std::get<Sample>(bad.payload).observation_time = 13;
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(12) == Status::INVALID);
  auto &v = std::get<Sample>(e.payload); v.age = {}; v.observation_time = {}; v.clock_quality = ClockQuality::UNKNOWN;
  CHECK(s.boundary.inbox.post(e) == Status::POSTED && s.boundary.process_one(12) == Status::APPLIED);
  s.core.tick(5010); CHECK(!s.snapshot().volume_known); // no fabricated zero age
  return true;
}
bool fault_pending_no_execution() {
  START;
  CHECK(s.device.command('l', 11));
  auto v = s.snapshot(); CHECK(s.handle({1, control_home::Action::NEXT, v.session, v.capability_revision}, 12));
  auto e = sample(s, 13);
  for (unsigned i = 0; i < Inbox::capacity; ++i) CHECK(s.boundary.inbox.post(e) == Status::POSTED);
  CHECK(s.boundary.inbox.post(e) == Status::FULL);
  CHECK(!s.device.command('f', 13)); // reject before once-only synthetic execution
  CHECK(s.device.pending && s.device.mock.dispatches == 1);
  CHECK(s.boundary.process_one(13) == Status::FAULT);
  CHECK(s.boundary.attach(s.b, 14) && s.device.command('r', 14));
  CHECK(!s.device.command('f', 15)); // replacement never replays old work
  CHECK(std::strcmp(s.snapshot().title, "Mock One") == 0);
  return true;
}
bool malformed_and_profile_bounds() {
  START;
  auto before = s.snapshot(); auto e = sample(s, 11);
  auto bad = e; std::get<Sample>(bad.payload).text.size = 5000;
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(11) == Status::INVALID);
  bad = e; std::get<Sample>(bad.payload).hops = 5;
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(11) == Status::INVALID);
  bad = e; std::get<Sample>(bad.payload).kind = ScalarKind::REFERENCE;
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(11) == Status::INVALID);
  bad = e; std::get<Sample>(bad.payload).kind = static_cast<ScalarKind>(-1);
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(11) == Status::INVALID);
  bad = e; bad.payload.emplace<Result>();
  CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(11) == Status::INVALID);
  CHECK(s.snapshot() == before && s.boundary.counts.invalid == 5);
  Event unknown = lifecycle(s, Kind::AVAILABILITY);
  auto &cap = unknown.payload.emplace<CapabilityChange>(); cap.id.set("navigation.home"); cap.availability = Availability::READY;
  CHECK(s.boundary.inbox.post(unknown) == Status::POSTED && s.boundary.process_one(11) == Status::CORE_REJECTED);
  CHECK(s.snapshot() == before); // missing descriptor cannot fabricate support
  auto o = s.device.mock.sample(s.core, s.b, "volume.level", Value<Scalar>::of(Scalar::level({1, 4})), 11);
  o.source_time = Text::literal("not in this bounded profile"); Sample packet;
  CHECK(!pack(o, packet));
  Bytes<4096> text; CHECK(text.set(std::string(4096, 'x')) && !text.set(std::string(4097, 'x')));
  CHECK(!text.set(std::string_view("\xff", 1)));
  return true;
}
bool wrong_owner_and_producer() {
  bool authorized = true;
  auto check = [](void *p) { return *static_cast<bool *>(p); };
  Session s(Id("owner.epoch"), check, &authorized); CHECK(s.initialize(10));
  auto e = sample(s, 11); auto before = s.snapshot(); auto used = Memory::used;
  // Real producer thread sees only the pointer-free inbox; no Core allocation.
  Status status = Status::INVALID;
  std::thread producer([&] { status = s.boundary.inbox.post(e); }); producer.join();
  CHECK(status == Status::POSTED && Memory::used == used && s.snapshot() == before);
  authorized = false;
  CHECK(s.boundary.process_one(11) == Status::WRONG_OWNER);
  CHECK(!s.boundary.reconnect(11) && !s.boundary.shutdown(11));
  CHECK(s.snapshot() == before);
  authorized = true;
  CHECK(s.boundary.process_one(11) == Status::APPLIED && s.snapshot().volume == 25);
  return true;
}
bool shutdown_queued() {
  START;
  auto e = sample(s, 11); CHECK(s.boundary.inbox.post(e) == Status::POSTED);
  CHECK(s.boundary.shutdown(11));
  CHECK(s.boundary.inbox.post(e) == Status::CLOSED && s.boundary.process_one(11) == Status::CLOSED);
  CHECK(s.boundary.counts.discarded == 1 && !s.snapshot().volume_known);
  CHECK(s.boundary.attach(s.b, 12));
  CHECK(s.boundary.inbox.post(e) == Status::POSTED && s.boundary.process_one(12) == Status::STALE);
  return true;
}
bool results_historical_and_late() {
  START;
  CHECK(s.device.command('l', 11));
  auto snap = s.snapshot();
  CHECK(s.handle({1, control_home::Action::NEXT, snap.session, snap.capability_revision}, 12));
  auto e = result(s, s.device.mock.completion());
  auto id = s.device.mock.last.value.request_id;
  CHECK(s.boundary.inbox.post(e) == Status::POSTED);
  CHECK(s.boundary.reachability(Reachability::UNREACHABLE, 13));
  CHECK(s.boundary.reconnect(14) && s.boundary.reachability(Reachability::REACHABLE, 14));
  CHECK(s.boundary.process_one(14) == Status::STALE);
  e.scope = {}; CHECK(s.boundary.inbox.scope(e.scope)); // explicitly current delivery envelope, original inner attempt
  CHECK(s.boundary.inbox.post(e) == Status::POSTED);
  CHECK(s.boundary.process_one(3013) == Status::APPLIED);
  CHECK(s.core.result(id)->lifecycle == Lifecycle::CONFIRMED_COMPLETED && s.core.result(id)->deadline_elapsed);
  CHECK(s.device.mock.dispatches == 1 && s.snapshot().title[0] == 0);
  CHECK(s.boundary.inbox.post(e) == Status::POSTED && s.boundary.process_one(3014) == Status::CORE_REJECTED);
  CHECK(s.device.mock.dispatches == 1); // result not a state update/second execution
  return true;
}
bool unknown_result_and_failure() {
  START;
  Result r; r.request.set("nonexistent"); r.evidence_id.set("evidence.test"); r.mapping = r.session = 1;
  r.signal = AdapterEvidence::Signal::COMPLETE;
  Event e = lifecycle(s, Kind::RESULT); e.payload = r;
  auto before = s.snapshot();
  CHECK(s.boundary.inbox.post(e) == Status::POSTED && s.boundary.process_one(11) == Status::CORE_REJECTED);
  CHECK(s.snapshot() == before && s.device.mock.dispatches == 0);
  auto packet = sample(s, 12); CHECK(s.boundary.inbox.post(packet) == Status::POSTED);
  auto limit = Memory::limit; Memory::limit = Memory::used;
  const auto status = s.boundary.process_one(12); Memory::limit = limit;
  CHECK(status == Status::ALLOCATION_REFUSED && s.snapshot() == before);
  CHECK(s.boundary.process_one(12) == Status::EMPTY); // refusal is final, not hidden retry
  CHECK(s.boundary.inbox.post(packet) == Status::POSTED && s.boundary.process_one(13) == Status::APPLIED);
  CHECK(s.snapshot().volume == 25);
  return true;
}
bool cold_boot() {
  Event old, completion;
  {
    Session first(Id("boot.first")); CHECK(first.initialize(10));
    old = sample(first, 11);
    CHECK(first.device.command('l', 11));
    auto v = first.snapshot(); CHECK(first.handle({1, control_home::Action::NEXT, v.session, v.capability_revision}, 12));
    completion = result(first, first.device.mock.completion());
    CHECK(first.boundary.inbox.post(old) == Status::POSTED && first.boundary.shutdown(13));
  }
  Session second(Id("boot.second"));
  CHECK(second.snapshot().playback == control_home::Playback::UNKNOWN && !second.snapshot().volume_known);
  CHECK(second.snapshot().title[0] == 0 && !second.snapshot().next && !second.device.pending);
  CHECK(second.boundary.inbox.post(old) == Status::CLOSED && second.store.effects.size() == 0);
  CHECK(second.initialize(10));
  CHECK(second.boundary.inbox.post(old) == Status::POSTED && second.boundary.inbox.post(completion) == Status::POSTED);
  CHECK(second.boundary.process_one(11) == Status::STALE && second.boundary.process_one(11) == Status::STALE);
  CHECK(second.device.mock.dispatches == 0 && std::strcmp(second.snapshot().title, "Mock One") == 0);
  auto fresh = sample(second, 12);
  CHECK(second.boundary.inbox.post(fresh) == Status::POSTED && second.boundary.process_one(12) == Status::APPLIED);
  CHECK(second.snapshot().volume == 25);
  return true;
}
bool journal_blocks_egress() {
  START;
  s.journal.writable = false;
  auto v = s.snapshot();
  s.handle({1, control_home::Action::NEXT, v.session, v.capability_revision}, 11);
  CHECK(s.store.blocked && s.device.mock.dispatches == 0);
  return true;
}
bool command_egress_owned_and_dedup() {
  START;
  auto v = s.snapshot(); control_home::Intent intent{1, control_home::Action::NEXT, v.session, v.capability_revision};
  CHECK(s.handle(intent, 11));
  auto captured = s.device.mock.last.value;
  CHECK(s.handle(intent, 12) && s.device.mock.dispatches == 1);
  CHECK(captured.request_id == s.device.mock.last.value.request_id && captured.session_generation == v.session);
  CHECK(s.device.command('d', 13) && s.device.command('r', 14));
  CHECK(s.handle(intent, 15) && s.device.mock.dispatches == 1); // retained duplicate returns history
  CHECK(s.device.mock.last.value.session_generation == captured.session_generation);
  return true;
}
bool bridge_processes_fifo_before_ack() {
  START;
  auto old = sample(s, 11, {1, 2}); CHECK(s.boundary.inbox.post(old) == Status::POSTED);
  CHECK(s.device.command('v', 12));
  CHECK(s.snapshot().volume == 25 && s.boundary.process_one(12) == Status::EMPTY);
  CHECK(s.boundary.counts.applied == 5); // prior sample plus four serial acquisition fields
  return true;
}
bool stale_mapping_association_epoch() {
  START;
  auto e = sample(s, 11); auto original = s.snapshot();
  for (unsigned i = 0; i < 5; ++i) {
    auto bad = e;
    if (i == 0) ++bad.scope.mapping;
    if (i == 1) ++bad.scope.association;
    if (i == 2) ++bad.scope.device_association;
    if (i == 3) bad.scope.epoch.set("different.boot");
    if (i == 4) ++bad.scope.incarnation;
    CHECK(s.boundary.inbox.post(bad) == Status::POSTED && s.boundary.process_one(11) == Status::STALE);
  }
  CHECK(s.snapshot() == original && s.boundary.counts.stale == 5);
  return true;
}
bool event_stress() {
  START;
  U64 now = 10; std::size_t warmed = 0, largest = 0;
  const unsigned cycles = 1000 * multiplier;
  for (unsigned i = 0; i < cycles; ++i) {
    auto old = sample(s, ++now); auto reconnect = lifecycle(s, Kind::RECONNECT);
    CHECK(s.boundary.inbox.post(reconnect) == Status::POSTED && s.boundary.inbox.post(old) == Status::POSTED);
    CHECK(s.boundary.process_one(now) == Status::APPLIED && s.boundary.process_one(now) == Status::STALE);
    CHECK(s.device.command('r', ++now));
    auto fresh = sample(s, now);
    CHECK(s.boundary.inbox.post(fresh) == Status::POSTED && s.boundary.process_one(now) == Status::APPLIED);
    CHECK(s.snapshot().volume == 25);
    auto cap = lifecycle(s, Kind::AVAILABILITY); auto &c = cap.payload.emplace<CapabilityChange>();
    c.id.set("playback.next"); c.availability = Availability::CONTEXT_UNAVAILABLE;
    CHECK(s.boundary.inbox.post(cap) == Status::POSTED && s.boundary.process_one(now) == Status::APPLIED);
    CHECK(!s.snapshot().next && s.device.command('c', ++now));
    CHECK(s.boundary.media_changed(++now)); s.device.acquire(now);
    CHECK(s.snapshot().playback == control_home::Playback::PLAYING);
    CHECK(s.device.command('l', ++now));
    auto v = s.snapshot(); CHECK(s.handle({i + 1, control_home::Action::NEXT, v.session, v.capability_revision}, ++now));
    auto delayed = result(s, s.device.mock.completion());
    now += 3001;
    CHECK(s.device.command('f', now)); // queued result then independent acquisition; late completion
    CHECK(s.boundary.reachability(Reachability::UNREACHABLE, ++now));
    CHECK(s.boundary.inbox.post(delayed) == Status::POSTED);
    CHECK(s.boundary.reconnect(++now));
    CHECK(s.boundary.process_one(now) == Status::STALE);
    CHECK(s.device.command('r', ++now) && s.device.command('n', now));
    now += 60001; s.core.tick(now); s.device.acquire(now);
    CHECK(s.store.samples.size() == 4 && s.store.effects.size() == 0 && !s.store.blocked);
    if (i == 5) warmed = Memory::used;
    if (i > 5) CHECK(Memory::used <= warmed + 256);
    largest = std::max(largest, Memory::used);
    CHECK(s.boundary.shutdown(++now)); CHECK(s.boundary.attach(s.b, ++now));
    CHECK(s.device.command('r', ++now));
  }
  iterations += cycles;
  std::printf("INGRESS STRESS cycles=%u posted=%u applied=%llu stale=%llu max_payload=%zu final_payload=%zu\n", cycles,
      s.boundary.inbox.counts().posted, (unsigned long long)s.boundary.counts.applied,
      (unsigned long long)s.boundary.counts.stale, largest, Memory::used);
  return true;
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::strcmp(argv[1], "--soak") == 0) multiplier = 10;
  struct Test { const char *name; bool (*run)(); };
  const Test tests[] = {
#define TEST(x) {#x, x}
    TEST(fifo_owned_copy), TEST(overflow_fails_closed), TEST(reconnect_old_queue), TEST(revisions_and_media),
    TEST(stale_age_and_duplicate), TEST(normalization_limits), TEST(fault_pending_no_execution), TEST(malformed_and_profile_bounds), TEST(wrong_owner_and_producer),
    TEST(shutdown_queued), TEST(results_historical_and_late), TEST(unknown_result_and_failure), TEST(cold_boot),
    TEST(journal_blocks_egress), TEST(command_egress_owned_and_dedup), TEST(bridge_processes_fifo_before_ack),
    TEST(stale_mapping_association_epoch), TEST(event_stress)
  };
  unsigned pass = 0, fail = 0;
  for (auto test : tests) {
    auto used = Memory::used; std::printf("RUN INGRESS %s\n", test.name); std::fflush(stdout);
    bool ok = test.run(); ++assertions; ok = ok && Memory::used == used;
    std::printf("%s INGRESS %s\n", ok ? "PASS" : "FAIL", test.name); ok ? ++pass : ++fail;
  }
  std::printf("INGRESS SIZE Event=%zu Inbox=%zu Boundary=%zu Session=%zu\n", sizeof(Event), sizeof(Inbox), sizeof(Boundary), sizeof(Session));
  std::printf("INGRESS SUMMARY PASS=%u FAIL=%u assertions=%u stress_iterations=%llu\n", pass, fail, assertions, (unsigned long long)iterations);
  return fail ? 1 : 0;
}
