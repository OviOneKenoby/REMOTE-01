#include "control_home/development.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

using namespace control_home;
using control_home::development::Session;
using control::Memory;
using control::U64;
namespace {
unsigned assertions = 0, multiplier = 1;
U64 stress_iterations = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { \
  std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)
#define START Session s(control::Id("hardening.epoch")); CHECK(s.initialize(10))
Intent intent(const Session &s, U64 token, Action action) {
  const auto v = s.snapshot();
  return {token, action, v.session, v.capability_revision};
}
struct Counts {
  std::size_t allocations = 0, frees = 0, failures = 0;
};
Counts *active = nullptr;
void observe(void *p, std::size_t, bool allocated) {
  if (!p) ++active->failures;
  else if (allocated) ++active->allocations;
  else ++active->frees;
}
struct Measurement {
  Counts counts;
  Memory::Observer previous = Memory::observer;
  explicit Measurement() { active = &counts; Memory::observer = observe; }
  ~Measurement() { Memory::observer = previous; active = nullptr; }
};
bool queue_empty_single_full() {
  START;
  Channel c;
  Intent out;
  const auto bytes = Memory::used;
  CHECK(!c.pop(out) && !c.request(Action::NEXT));
  CHECK(c.rejected == 1 && c.count == 0);
  c.publish(s.snapshot());
  CHECK(c.request(Action::NEXT) && c.pop(out));
  CHECK(out == intent(s, 1, Action::NEXT));
  CHECK(!c.pop(out) && s.device.mock.dispatches == 0);
  for (unsigned i = 0; i < c.capacity; ++i)
    CHECK(c.request(i % 2 ? Action::NEXT : Action::PREVIOUS));
  CHECK(!c.request(Action::TOGGLE) && c.rejected == 2);
  for (unsigned i = 0; i < c.capacity; ++i) {
    CHECK(c.pop(out) && out.token == i + 2);
    CHECK(out.action == (i % 2 ? Action::NEXT : Action::PREVIOUS));
  }
  CHECK(!c.pop(out));
  CHECK(c.request(Action::TOGGLE) && c.pop(out) && out.token == 11);
  CHECK(Memory::used == bytes); // channel operations never allocate
  CHECK(s.handle(out, 11) && s.device.mock.dispatches == 1);
  return true;
}
bool reconnect_buffered_intent() {
  START;
  Channel c;
  c.publish(s.snapshot());
  CHECK(c.request(Action::NEXT));
  Intent old;
  CHECK(c.pop(old));
  CHECK(s.device.command('d', 11) && s.device.command('r', 12));
  // Serial changes precede mailbox publication in the real owner turn.
  CHECK(!s.handle(old, 13) && s.device.mock.dispatches == 0);
  c.publish(s.snapshot());
  CHECK(c.request(Action::NEXT));
  Intent fresh;
  CHECK(c.pop(fresh) && s.handle(fresh, 14));
  CHECK(!s.handle(old, 15) && s.device.mock.dispatches == 1);
  CHECK(std::strcmp(s.snapshot().title, "Mock Two") == 0);
  return true;
}
bool capability_buffered_intent() {
  START;
  Channel c;
  c.publish(s.snapshot());
  CHECK(c.request(Action::NEXT));
  Intent old;
  CHECK(c.pop(old));
  CHECK(s.device.command('-', 11) && s.device.command('+', 12));
  CHECK(!s.handle(old, 13) && s.device.mock.dispatches == 0);
  c.publish(s.snapshot());
  CHECK(c.request(Action::NEXT) && c.pop(old) && s.handle(old, 14));
  CHECK(s.device.mock.dispatches == 1);
  return true;
}
bool withdrawal_survives_reconnect() {
  START;
  CHECK(s.device.command('-', 11));
  CHECK(s.device.command('d', 12) && s.device.command('r', 13));
  CHECK(!s.snapshot().next && s.snapshot().toggle);
  CHECK(!s.handle(intent(s, 1, Action::NEXT), 14));
  CHECK(s.device.mock.dispatches == 0);
  CHECK(s.device.command('+', 15));
  CHECK(s.handle(intent(s, 2, Action::NEXT), 16));
  CHECK(s.device.mock.dispatches == 1);
  return true;
}
bool delayed_burst_discard() {
  START;
  Channel c;
  c.publish(s.snapshot());
  CHECK(s.device.command('l', 11));
  for (unsigned j = 0; j < c.capacity; ++j) CHECK(c.request(Action::NEXT));
  Intent first;
  CHECK(c.pop(first) && s.handle(first, 12));
  CHECK(s.device.pending && s.device.mock.dispatches == 1);
  c.publish(s.snapshot());
  CHECK(c.count == 0 && c.rejected == 7);
  CHECK(!c.request(Action::NEXT) && c.rejected == 8);
  auto refused = intent(s, 9, Action::NEXT);
  CHECK(!s.handle(refused, 13));
  CHECK(s.device.command('f', 14));
  c.publish(s.snapshot());
  CHECK(!c.pop(first));
  CHECK(!s.handle(refused, 15)); // refused token cannot revive on completion
  CHECK(s.device.mock.dispatches == 1);
  CHECK(c.request(Action::NEXT) && c.pop(first));
  CHECK(first.token == 9);
  CHECK(!s.handle(first, 16)); // the explicit refused token was already consumed
  CHECK(c.request(Action::NEXT) && c.pop(first) && s.handle(first, 17));
  CHECK(s.device.mock.dispatches == 2);
  return true;
}
bool publish_prunes_in_fifo_order() {
  START;
  Channel c;
  c.publish(s.snapshot());
  for (unsigned j = 0; j < 6; ++j)
    CHECK(c.request(j % 2 ? Action::NEXT : Action::TOGGLE));
  Intent out;
  CHECK(c.pop(out) && out.token == 1); // head is not zero
  CHECK(s.device.command('u', 11));
  c.publish(s.snapshot());
  CHECK(c.count == 3 && c.rejected == 2);
  for (U64 token : {2u, 4u, 6u}) CHECK(c.pop(out) && out.token == token);
  CHECK(!c.request(Action::TOGGLE));
  CHECK(s.device.command('p', 12));
  c.publish(s.snapshot());
  CHECK(c.request(Action::TOGGLE) && c.pop(out) && out.token == 7);
  CHECK(s.handle(out, 13) && s.snapshot().playback == Playback::PAUSED);
  return true;
}
bool unavailable_queue_restore() {
  START;
  Channel c;
  c.publish(s.snapshot());
  CHECK(c.request(Action::NEXT));
  CHECK(s.device.command('o', 11));
  c.publish(s.snapshot());
  CHECK(c.count == 0 && c.rejected == 1 && !c.request(Action::NEXT));
  CHECK(s.device.command('c', 12));
  c.publish(s.snapshot());
  Intent out;
  CHECK(!c.pop(out));
  CHECK(c.request(Action::NEXT) && c.pop(out) && s.handle(out, 13));
  CHECK(s.device.mock.dispatches == 1);
  CHECK(s.device.command('d', 14));
  c.publish(s.snapshot());
  CHECK(!c.request(Action::PREVIOUS) && !c.request(Action::TOGGLE));
  return true;
}
bool wrapped_queue_compaction() {
  START;
  Channel c;
  c.publish(s.snapshot());
  Intent out;
  for (unsigned j = 0; j < 7; ++j)
    CHECK(c.request(Action::NEXT) && c.pop(out));
  CHECK(c.head == 7);
  for (unsigned j = 0; j < c.capacity; ++j)
    CHECK(c.request(j % 2 ? Action::NEXT : Action::TOGGLE));
  CHECK(s.device.command('u', 11));
  c.publish(s.snapshot());
  CHECK(c.count == 4 && c.rejected == 4);
  for (U64 token : {9u, 11u, 13u, 15u}) CHECK(c.pop(out) && out.token == token);
  CHECK(!c.pop(out));
  CHECK(s.device.command('p', 12));
  c.publish(s.snapshot());
  CHECK(c.request(Action::TOGGLE) && c.pop(out) && out.token == 16);
  return true;
}
bool token_and_generation_edges() {
  START;
  Channel c;
  c.publish(s.snapshot());
  CHECK(!c.push({}) && c.rejected == 1);
  CHECK(!c.request(static_cast<Action>(255)) && c.rejected == 2);
  c.issued = UINT64_MAX - 1;
  CHECK(c.request(Action::NEXT));
  Intent out;
  CHECK(c.pop(out) && out.token == UINT64_MAX);
  CHECK(!c.request(Action::NEXT) && c.issued == UINT64_MAX);
  c.generation = UINT64_MAX;
  auto before = c.latest;
  auto after = before;
  after.volume = 1;
  c.publish(after);
  CHECK(c.latest == before && c.generation == UINT64_MAX);
  CHECK(!s.handle({1, Action::NEXT}, 11)); // missing scope is not executable
  CHECK(!s.handle(intent(s, 1, Action::NEXT), 12)); // no rebase of refused token
  return true;
}
bool duplicate_scope_immutable() {
  START;
  auto original = intent(s, 1, Action::NEXT);
  CHECK(s.handle(original, 11) && s.handle(original, 12));
  auto changed = original;
  ++changed.capability_revision;
  CHECK(!s.handle(changed, 13));
  CHECK(s.device.mock.dispatches == 1);
  CHECK(s.device.command('r', 14));
  CHECK(s.handle(original, 15)); // retained exact duplicate, no execution
  CHECK(s.device.mock.dispatches == 1);
  return true;
}
bool utf8_snapshot_bounds() {
  START;
  const std::string cases[] = {
    std::string(30, 'A') + "\xe2\x82\xac", // 3-byte codepoint crosses limit
    std::string(29, 'B') + "\xc3\xa9",     // 2-byte codepoint fits exactly
    std::string(28, 'C') + "\xf0\x9f\x8e\xb5",
    std::string(31, 'D') + "more", "", std::string(4096, 'E')};
  const std::size_t expected[] = {30, 31, 28, 31, 0, 31};
  for (unsigned j = 0; j < 6; ++j) {
    control::Scalar value;
    value.kind = control::ScalarKind::STRING;
    value.string = control::Text(cases[j]);
    CHECK(value.string.valid());
    auto sample = s.device.mock.sample(s.core, s.b, "media.title",
                                     control::Value<control::Scalar>::of(value), 11 + j);
    CHECK(s.core.observe(sample, 11 + j));
    auto copy = s.snapshot();
    CHECK(std::strlen(copy.title) == expected[j]);
    CHECK(copy.title[31] == 0 && control::utf8(copy.title));
    CHECK(std::strcmp(copy.subtitle, "Synthetic") == 0);
  }
  return true;
}
bool copied_mailbox_lifetime() {
  Snapshot retained;
  Channel c;
  {
    START;
    c.publish(s.snapshot());
    retained = c.latest;
    for (unsigned j = 0; j < 100; ++j) {
      CHECK(s.device.command(j % 2 ? 'p' : 'a', 11 + j));
      c.publish(s.snapshot());
    }
    CHECK(c.generation == 101);
  }
  CHECK(std::strcmp(retained.title, "Mock One") == 0);
  CHECK(retained.playback == Playback::PLAYING && retained.volume == 68);
  CHECK(std::strcmp(c.latest.subtitle, "Synthetic") == 0);
  return true;
}
bool delayed_withdraw_complete() {
  START;
  CHECK(s.device.command('l', 11));
  CHECK(s.handle(intent(s, 1, Action::TOGGLE), 12));
  const auto id = s.device.mock.last.value.request_id;
  CHECK(s.device.command('-', 13));
  CHECK(s.core.result(id)->invalidated && !s.snapshot().toggle);
  CHECK(s.device.command('f', 14));
  CHECK(s.snapshot().playback == Playback::PAUSED && !s.snapshot().next);
  CHECK(s.core.result(id)->lifecycle == control::Lifecycle::CONFIRMED_COMPLETED);
  CHECK(s.device.mock.dispatches == 1);
  CHECK(!s.device.command('f', 15));
  CHECK(s.device.command('+', 16) && s.snapshot().next);
  return true;
}
bool completion_allocation_failure_no_reexecution() {
  START;
  CHECK(s.device.command('l', 11));
  CHECK(s.handle(intent(s, 1, Action::NEXT), 12));
  {
    struct Budget {
      std::size_t old = Memory::limit;
      ~Budget() { Memory::limit = old; }
    } budget;
    Memory::limit = Memory::used; // completion evidence ID cannot allocate
    CHECK(!s.device.command('f', 13) && s.device.pending);
  }
  s.device.acquire(14); // actual execution is observed independently of result
  CHECK(std::strcmp(s.snapshot().title, "Mock Two") == 0);
  CHECK(s.device.command('f', 15) && !s.device.pending);
  CHECK(std::strcmp(s.snapshot().title, "Mock Two") == 0); // never Mock Three
  CHECK(s.device.mock.dispatches == 1 && !s.store.blocked);
  CHECK(!s.device.command('f', 16));
  return true;
}
bool facade_admission_refusal_acknowledgement() {
  bool reproduced = false;
  for (unsigned headroom = 0; headroom <= 8192; headroom += 64) {
    START;
    struct Budget {
      std::size_t old = Memory::limit;
      ~Budget() { Memory::limit = old; }
    } budget;
    Memory::limit = Memory::used + headroom;
    const bool accepted = s.handle(intent(s, 1, Action::NEXT), 11);
    if (s.store.watermark == 1 && !s.store.records[0].get() &&
        !s.device.mock.dispatches && !s.store.blocked) {
      // Core consumed the pair but could not allocate its full record.
      // Reply.error is absent; the returned CommandResult is REJECTED.
      reproduced = true;
      CHECK(!accepted);
    }
  }
  CHECK(reproduced);
  return true;
}
bool late_evidence_preserves_new_media() {
  START;
  const auto old = s.core.field(s.d, s.t, "media.title")->selected.value;
  CHECK(s.device.command('l', 11));
  CHECK(s.handle(intent(s, 1, Action::NEXT), 12));
  const auto completion = s.device.mock.completion();
  const auto id = completion.request_id;
  s.core.tick(3012);
  CHECK(s.core.result(id)->lifecycle == control::Lifecycle::UNCERTAIN_OUTCOME);
  const auto closure = s.core.result(id)->first_terminal_time;
  CHECK(!s.core.media_changed(s.d, s.t, 3013).present);
  CHECK(!s.snapshot().title[0]);
  s.device.acquire(3014);
  auto current = s.snapshot();
  CHECK(!s.core.observe(old, 3014));
  CHECK(s.core.report(completion, 3015));
  CHECK(s.snapshot() == current); // result alone never supplies media state
  CHECK(s.core.result(id)->first_terminal_time == closure);
  CHECK(s.core.result(id)->deadline_elapsed);
  CHECK(s.core.result(id)->lifecycle == control::Lifecycle::CONFIRMED_COMPLETED);
  CHECK(!s.core.report(completion, 3016));
  CHECK(s.snapshot() == current && s.device.mock.dispatches == 1);
  return true;
}
bool session_delayed_no_execution_replay() {
  START;
  CHECK(s.device.command('l', 11));
  CHECK(s.handle(intent(s, 1, Action::NEXT), 12));
  auto old = s.core.field(s.d, s.t, "media.title")->selected.value;
  auto complete = s.device.mock.completion();
  CHECK(s.device.command('d', 13) && s.device.command('r', 14));
  auto current = s.snapshot();
  CHECK(s.device.pending && !current.next && !current.toggle);
  CHECK(!s.device.command('f', 15));
  old.receipt_time.tick_ms = 16;
  CHECK(!s.core.observe(old, 16));
  CHECK(s.core.report(complete, 17)); // resolves historical result, not device
  CHECK(s.snapshot() == current && s.device.mock.dispatches == 1);
  CHECK(!s.handle(intent(s, 2, Action::NEXT), 18));
  return true;
}
bool duplicate_observation_age() {
  START;
  auto sample = s.core.field(s.d, s.t, "playback.state")->selected.value;
  sample.receipt_time.tick_ms = 1000;
  CHECK(!s.core.observe(sample, 1000));
  CHECK(s.core.field(s.d, s.t, "playback.state")->selected.value.receipt_time.tick_ms == 10);
  s.core.tick(5010);
  CHECK(s.snapshot().playback == Playback::UNKNOWN && !s.snapshot().toggle);
  s.device.acquire(5011);
  CHECK(s.snapshot().toggle);
  return true;
}
bool full_route_selection_limit() {
  START;
  control::Id last;
  for (unsigned j = 1; j < control::Limits::bindings; ++j) {
    auto binding = *s.core.binding(s.b);
    binding.binding_id = control::Id("route:" + std::to_string(j));
    binding.owner_priority = j;
    last = binding.binding_id;
    CHECK(!s.core.register_binding(binding, s.device, 11).present);
    CHECK(!s.core.capability(s.d, s.t, control::MockAdapter::readable(last, "volume.level"), 11).present);
    CHECK(s.core.observe(s.device.mock.sample(s.core, last, "volume.level",
      control::Value<control::Scalar>::of(control::Scalar::level({17,25})), 11), 11));
  }
  CHECK(s.store.bindings.size() == control::Limits::bindings);
  CHECK(s.snapshot().volume == 68 && !s.store.blocked);
  CHECK(s.core.observe(s.device.mock.sample(s.core, last, "volume.level",
    control::Value<control::Scalar>::of(control::Scalar::level({99,100})), 12), 12));
  const auto *field = s.core.field(s.d, s.t, "volume.level");
  CHECK(field->conflict_latched && field->conflict_set.size() == control::Limits::bindings);
  CHECK(field->selected.value.binding_id == s.b);
  CHECK(!s.snapshot().volume_known);
  s.core.tick(13);
  CHECK(field->conflict_latched && !s.store.blocked);
  return true;
}
bool serial_byte_contract() {
  START;
  const char *keys = "pau01vdr-+ocslnfm";
  const auto before = s.snapshot();
  const auto commits = s.journal.commits;
  for (unsigned key = 0; key < 256; ++key) {
    auto reply = development::input(s, static_cast<unsigned char>(key), 11, false);
    const bool command = key != 0 && std::strchr(keys, int(key));
    CHECK(reply.kind == (command ? (key == 'm' ? development::InputKind::TELEMETRY : development::InputKind::COMMAND)
      : key >= 32 && key <= 126 ? development::InputKind::UNKNOWN : development::InputKind::IGNORE));
    CHECK(!reply.accepted);
  }
  CHECK(s.snapshot() == before && s.journal.commits == commits);
  CHECK(development::input(s, 'v', 12, true).accepted);
  CHECK(s.snapshot().volume == 25); // actual repository contract, not observed 50
  CHECK(development::input(s, 's', 13, true).accepted && s.snapshot().volume == 25);
  for (unsigned char key : {'\r', '\n', '?', 'X'}) {
    auto prior = s.snapshot();
    CHECK(!development::input(s, key, 14, true).accepted);
    CHECK(s.snapshot() == prior);
  }
  return true;
}
bool allocation_refresh() {
  START;
  Channel c;
  c.publish(s.snapshot());
  const auto used = Memory::used;
  Measurement m;
  for (unsigned j = 0; j < 1000; ++j) {
    c.publish(s.snapshot());
    s.core.tick(11);
  }
  CHECK(m.counts.allocations == 0 && m.counts.frees == 0 && m.counts.failures == 0);
  CHECK(Memory::used == used && c.generation == 1);
  std::printf("ALLOC unchanged_ticks=1000 snapshots=1000 allocations=0 frees=0 live_delta=0\n");
  return true;
}
bool allocation_benchmark() {
  START;
  Channel c;
  c.publish(s.snapshot());
  Measurement m;
  auto phase = [&](const char *name, auto work) {
    m.counts = {};
    const auto used = Memory::used;
    bool ok = work();
    std::printf("ALLOC %s allocations=%zu frees=%zu failures=%zu live_delta=%lld\n",
      name, m.counts.allocations, m.counts.frees, m.counts.failures,
      static_cast<long long>(Memory::used) - static_cast<long long>(used));
    return ok && m.counts.failures == 0;
  };
  auto unchanged = s.snapshot();
  CHECK(phase("1000_snapshot", [&] {
    for (unsigned j = 0; j < 1000; ++j) c.publish(s.snapshot());
    return s.snapshot() == unchanged && c.generation == 1;
  }));
  CHECK(phase("1000_unchanged_tick", [&] {
    for (unsigned j = 0; j < 1000; ++j) s.core.tick(18);
    return s.snapshot() == unchanged;
  }));
  CHECK(phase("100_acquisitions", [&] {
    for (unsigned j = 0; j < 100; ++j) s.device.acquire(18);
    return s.snapshot() == unchanged && s.store.samples.size() == 4;
  }));
  CHECK(phase("100_capability_updates", [&] {
    for (unsigned j = 0; j < 100; ++j) if (!s.device.command('+', 18)) return false;
    auto expected = unchanged;
    expected.capability_revision += 100;
    unchanged = expected;
    return s.snapshot() == expected;
  }));
  CHECK(phase("20Hz_30seconds", [&] {
    for (unsigned j = 1; j <= 600; ++j) {
      const auto now = 18 + j * 50;
      s.core.tick(now);
      if (j % 20 == 0) s.device.acquire(now);
      c.publish(s.snapshot());
    }
    return s.snapshot() == unchanged && !s.store.blocked;
  }));
  return true;
}
bool acquisition_soak() {
  START;
  const unsigned iterations = 2000 * multiplier;
  U64 now = 10;
  s.device.acquire(++now); // settle same-capacity sample storage
  const auto settled = Memory::used;
  std::size_t largest = settled;
  Measurement m;
  for (unsigned j = 0; j < iterations; ++j) {
    s.core.tick(++now);
    s.device.acquire(now);
    auto value = s.snapshot();
    CHECK(value.playback == Playback::PLAYING && value.volume == 68);
    CHECK(std::strcmp(value.title, "Mock One") == 0);
    CHECK(s.store.samples.size() == 4 && !s.store.blocked);
    CHECK(Memory::used <= settled + 128); // bounded ID digit growth, no retained history
    largest = std::max(largest, Memory::used);
  }
  CHECK(m.counts.failures == 0 && m.counts.allocations == m.counts.frees);
  stress_iterations += iterations;
  std::printf("STRESS acquisition iterations=%u allocations=%zu frees=%zu settled=%zu largest=%zu final=%zu\n",
    iterations, m.counts.allocations, m.counts.frees, settled, largest, Memory::used);
  return true;
}
bool burst_soak() {
  START;
  Channel c;
  U64 now = 10;
  unsigned expected = 0;
  const unsigned batches = 250 * multiplier;
  std::size_t settled = 0;
  Measurement m;
  for (unsigned batch = 0; batch < batches; ++batch) {
    s.device.acquire(++now);
    c.publish(s.snapshot());
    for (unsigned j = 0; j < c.capacity; ++j)
      CHECK(c.request(batch % 2 ? Action::TOGGLE : j % 2 ? Action::NEXT : Action::PREVIOUS));
    CHECK(!c.request(Action::NEXT));
    for (unsigned j = 0; j < c.capacity; ++j) {
      Intent out;
      CHECK(c.pop(out) && s.handle(out, ++now));
      c.publish(s.snapshot());
      CHECK(s.device.mock.dispatches == ++expected);
    }
    CHECK(c.count == 0 && c.rejected == batch + 1);
    CHECK(std::strcmp(s.snapshot().title, "Mock One") == 0 && s.snapshot().playback == Playback::PLAYING);
    // Purge a whole batch at a deterministic boundary; the last envelope lives.
    now += 60001;
    s.core.tick(now);
    s.device.acquire(now);
    for (const auto &record : s.store.records) CHECK(!record.get());
    CHECK(s.store.effects.size() == 0 && !s.store.blocked);
    if (batch == 3) settled = Memory::used;
    if (batch > 3) CHECK(Memory::used <= settled + 256);
  }
  CHECK(m.counts.failures == 0);
  stress_iterations += batches * c.capacity;
  std::printf("STRESS burst batches=%u commands=%u queue_rejected=%llu settled=%zu final=%zu allocations=%zu frees=%zu\n",
    batches, expected, static_cast<unsigned long long>(c.rejected), settled, Memory::used, m.counts.allocations, m.counts.frees);
  return true;
}
bool transition_soak() {
  START;
  Channel c;
  U64 now = 10;
  const unsigned cycles = 500 * multiplier;
  const auto initial = Memory::used;
  unsigned expected_dispatches = 0;
  Measurement m;
  for (unsigned cycle = 0; cycle < cycles; ++cycle) {
    s.device.acquire(++now);
    auto old_sample = s.core.field(s.d, s.t, "media.title")->selected.value;
    c.publish(s.snapshot());
    CHECK(c.request(Action::NEXT));
    Intent old;
    CHECK(c.pop(old));
    switch (cycle % 4) {
    case 0:
      CHECK(s.device.command('d', ++now) && s.device.command('r', ++now));
      old_sample.receipt_time.tick_ms = now;
      CHECK(!s.core.observe(old_sample, now));
      break;
    case 1: CHECK(s.device.command('-', ++now) && s.device.command('+', ++now)); break;
    case 2: CHECK(s.device.command('o', ++now) && s.device.command('c', ++now)); break;
    case 3:
      CHECK(s.device.command('l', ++now));
      CHECK(s.handle(intent(s, old.token, Action::TOGGLE), ++now));
      ++expected_dispatches;
      CHECK(!s.handle(intent(s, old.token + 1, Action::NEXT), ++now));
      ++c.issued; // that explicitly injected owner intent consumed this token
      s.core.tick(now += 3001);
      CHECK(s.device.command('f', ++now) && s.device.command('n', ++now));
      break;
    }
    CHECK(!s.handle(old, ++now));
    c.publish(s.snapshot());
    CHECK(c.request(Action::NEXT));
    Intent fresh;
    CHECK(c.pop(fresh) && s.handle(fresh, ++now));
    ++expected_dispatches;
    CHECK(s.device.mock.dispatches == expected_dispatches);
    CHECK(s.device.command('v', ++now) && s.device.command('s', ++now));
    CHECK(s.snapshot().volume == 25 && !s.device.pending && !s.store.blocked);
    now += 60001;
    s.core.tick(now);
    s.device.acquire(now);
    CHECK(s.store.samples.size() == 4 && s.store.effects.size() == 0);
    CHECK(Memory::used < initial + 32768); // finite warmed capacities, not an ESP32 threshold
  }
  CHECK(m.counts.failures == 0);
  stress_iterations += cycles;
  std::printf("STRESS transitions cycles=%u dispatches=%u initial=%zu final=%zu allocations=%zu frees=%zu\n",
    cycles, expected_dispatches, initial, Memory::used, m.counts.allocations, m.counts.frees);
  return true;
}
bool runtime_boundary_checks() {
  auto source = [](const char *name) {
    std::ifstream f(name);
    return std::string(std::istreambuf_iterator<char>(f), {});
  };
  const auto runtime = source("components/control_home/runtime.cpp");
  const auto main = source("main/main.cpp");
  const auto ui = source("components/user_app/user_app.cpp");
  CHECK(!runtime.empty() && !main.empty() && !ui.empty());
  CHECK(runtime.find("char keys[8]") != std::string::npos);
  CHECK(runtime.find("usb_serial_jtag_read_bytes(keys, sizeof keys, 0)") != std::string::npos);
  CHECK(runtime.find("vTaskDelay(pdMS_TO_TICKS(5))") != std::string::npos);
  CHECK(runtime.find("recognized=0 accepted=0") != std::string::npos);
  CHECK(runtime.find("reply.kind == development::InputKind::IGNORE") != std::string::npos);
  CHECK(runtime.find("lv_") == std::string::npos);
  CHECK(main.find("#if defined(REMOTE01_PHASE_A_PROFILE) && REMOTE01_PHASE_A_PROFILE") != std::string::npos);
  CHECK(ui.find("lv_timer_del(home.timer)") != std::string::npos && ui.find("home = {}") != std::string::npos);
  std::printf("SIZE Intent=%zu Snapshot=%zu Channel=%zu Session=%zu (host ABI)\n",
    sizeof(Intent), sizeof(Snapshot), sizeof(Channel), sizeof(Session));
  return true;
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::strcmp(argv[1], "--measure") == 0) {
    const auto before = Memory::used;
    const bool ok = allocation_benchmark();
    return ok && Memory::used == before ? 0 : 1;
  }
  if (argc == 2 && std::strcmp(argv[1], "--soak") == 0) multiplier = 10;
  else if (argc != 1) return 2;
  struct Test { const char *name; bool (*run)(); };
  const Test tests[] = {
#define TEST(x) {#x, x}
    TEST(queue_empty_single_full), TEST(reconnect_buffered_intent),
    TEST(capability_buffered_intent), TEST(withdrawal_survives_reconnect), TEST(delayed_burst_discard),
    TEST(publish_prunes_in_fifo_order), TEST(unavailable_queue_restore), TEST(wrapped_queue_compaction),
    TEST(token_and_generation_edges), TEST(duplicate_scope_immutable),
    TEST(utf8_snapshot_bounds), TEST(copied_mailbox_lifetime),
    TEST(delayed_withdraw_complete), TEST(completion_allocation_failure_no_reexecution),
    TEST(facade_admission_refusal_acknowledgement),
    TEST(late_evidence_preserves_new_media),
    TEST(session_delayed_no_execution_replay), TEST(duplicate_observation_age), TEST(full_route_selection_limit),
    TEST(serial_byte_contract), TEST(allocation_refresh), TEST(allocation_benchmark),
    TEST(acquisition_soak), TEST(burst_soak), TEST(transition_soak),
    TEST(runtime_boundary_checks)
#undef TEST
  };
  unsigned pass = 0, fail = 0;
  for (const auto &test : tests) {
    const auto baseline = Memory::used;
    std::printf("RUN HARDENING %s\n", test.name);
    std::fflush(stdout);
    bool ok = test.run();
    ++assertions;
    if (Memory::used != baseline) {
      std::printf("LEAK %s %zu -> %zu\n", test.name, baseline, Memory::used);
      ok = false;
    }
    std::printf("%s HARDENING %s\n", ok ? "PASS" : "FAIL", test.name);
    if (ok) ++pass; else ++fail;
  }
  std::printf("HARDENING SUMMARY PASS=%u FAIL=%u assertions=%u stress_iterations=%llu mode=%s\n",
    pass, fail, assertions, static_cast<unsigned long long>(stress_iterations), multiplier == 1 ? "default" : "opt-in-soak");
  return fail ? 1 : 0;
}
