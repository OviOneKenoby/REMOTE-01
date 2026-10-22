#include "power_management/model.hpp"
#include "power_management/qmi_wom.hpp"
#include "power_management/output.hpp"
#include "power_management/reset_record.hpp"
#include "power_management/wake_irq.hpp"
#include "control_home/power_suspend.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

using namespace power_management;
using control_home::development::Session;
using control_home::development::PowerSuspend;
namespace {
unsigned assertions = 0, multiplier = 1, cycles_done = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)
#define START Session s(control::Id("power.test")); CHECK(s.initialize(10))
void samples(Battery &b, unsigned mv, unsigned count, Time &now) {
  for (unsigned i = 0; i < count; ++i) { now += 2000; b.sample(true, 1234, mv, now); }
}
bool battery_truth_and_faults() {
  Battery b; Time now = 0;
  CHECK(!b.valid && !b.shutdown_required());
  samples(b, 4000, 5, now); CHECK(b.valid && b.filtered_mv == 4000 && b.condition == Condition::NORMAL);
  samples(b, 2800, 1, now); CHECK(b.filtered_mv == 4000 && b.condition == Condition::NORMAL);
  b.sample(false, 0, 0, ++now); CHECK(!b.valid && b.condition == Condition::UNKNOWN && !b.shutdown_required());
  b.sample(true, 0, 2499, ++now); CHECK(!b.valid);
  b.sample(true, 0, 4501, ++now); CHECK(!b.valid);
  b.sample(true, 1300, 3800, ++now); CHECK(b.valid && b.filtered_mv == 3800 && b.raw == 1300);
  return true;
}
bool battery_hysteresis() {
  Battery b; Time now = 0;
  samples(b, 3550, 15, now); CHECK(b.condition == Condition::LOW);
  samples(b, 3650, 30, now); CHECK(b.condition == Condition::LOW);
  samples(b, 3800, 30, now); CHECK(b.condition == Condition::NORMAL);
  samples(b, 3200, 40, now); CHECK(b.condition == Condition::CRITICAL);
  samples(b, 3500, 30, now); CHECK(b.condition == Condition::CRITICAL);
  samples(b, 3650, 30, now); CHECK(b.condition == Condition::LOW);
  return true;
}
bool critical_qualification_and_sources() {
  Battery b; Time now = 0;
  samples(b, 3200, 5, now); CHECK(b.qualifying && b.condition == Condition::LOW);
  samples(b, 3200, 14, now); CHECK(b.condition != Condition::CRITICAL);
  samples(b, 3200, 1, now); CHECK(b.condition == Condition::CRITICAL && !b.shutdown_required());
  b.external = External::PRESENT; CHECK(!b.shutdown_required());
  b.external = External::ABSENT; CHECK(b.shutdown_required());
  b.sample(false, 0, 0, ++now); CHECK(!b.shutdown_required());
  samples(b, 3200, 5, now); CHECK(b.condition != Condition::CRITICAL);
  samples(b, 3800, 30, now); CHECK(b.condition == Condition::NORMAL && !b.qualifying);
  return true;
}
bool exact_threshold_convergence() {
  Battery b; Time now = 0;
  samples(b, 4000, 5, now); samples(b, 3400, 100, now);
  CHECK(b.filtered_mv == 3400 && b.condition == Condition::CRITICAL);
  samples(b, 3550, 50, now); CHECK(b.filtered_mv == 3550 && b.condition == Condition::LOW);
  samples(b, 3700, 50, now); CHECK(b.filtered_mv == 3700 && b.condition == Condition::NORMAL);
  CHECK(!b.shutdown_required());
  return true;
}
bool idle_wake_and_fail_safe() {
  Machine m(100);
  m.tick(30099, true, External::ABSENT); CHECK(m.state == State::ACTIVE);
  m.tick(30100, true, External::ABSENT); CHECK(m.state == State::DISPLAY_IDLE);
  m.wake(31000, Wake::TOUCH); CHECK(m.state == State::ACTIVE && m.wakes == 1);
  m.tick(61000, true, External::ABSENT); CHECK(m.state == State::DISPLAY_IDLE);
  m.wake(62000, Wake::MOTION); CHECK(m.reason == Wake::MOTION && m.wakes == 2);
  m.tick(362000, false, External::ABSENT); CHECK(m.state == State::DISPLAY_IDLE && m.refusals == 1);
  m.tick(400000, true, External::ABSENT); CHECK(m.state == State::DISPLAY_IDLE);
  m.tick(422000, true, External::ABSENT); CHECK(m.state == State::SYSTEM_SLEEP);
  m.refuse_sleep(423000); CHECK(m.state == State::DISPLAY_IDLE);
  m.wake(424000, Wake::FAULT); CHECK(m.state == State::ACTIVE);
  m.state = State::CRITICAL_SHUTDOWN; m.tick(900000, true, External::ABSENT); CHECK(m.state == State::CRITICAL_SHUTDOWN);
  return true;
}
bool encoder_activity_display_policy() {
  Machine m(0);
  // Activity intervals below the existing 30 s display timeout keep ACTIVE.
  for (Time now : {29000ull, 58000ull, 87000ull}) {
    m.tick(now, true, External::ABSENT);
    CHECK(m.state == State::ACTIVE);
    m.activity(now);
  }
  m.tick(116999, true, External::ABSENT);
  CHECK(m.state == State::ACTIVE);
  m.tick(117000, true, External::ABSENT);
  CHECK(m.state == State::DISPLAY_IDLE);
  // The runtime maps the shared physical-activity counter to this existing
  // display-idle wake path. It does not configure a SYSTEM_SLEEP wake source.
  m.wake(118000, Wake::TOUCH);
  CHECK(m.state == State::ACTIVE && m.reason == Wake::TOUCH && m.wakes == 1);
  m.tick(148000, true, External::ABSENT);
  CHECK(m.state == State::DISPLAY_IDLE);
  return true;
}
bool first_touch_consumed() {
  TouchGate g;
  CHECK(g.accept(true, true, false, false)); CHECK(!g.accept(false, true, false, false));
  CHECK(!g.accept(true, false, false, false)); CHECK(!g.accept(true, true, false, false));
  CHECK(!g.accept(false, true, false, false)); CHECK(g.accept(true, true, false, false));
  CHECK(!g.accept(true, true, true, true)); CHECK(!g.accept(true, true, false, false));
  CHECK(!g.accept(false, true, false, false)); CHECK(g.accept(true, true, false, false));
  return true;
}
bool reset_record_validation_and_torn_write() {
  using namespace reset_diagnostic;
  History h{}; CHECK(latest(h) == -1);
  std::memset(&h, 0xa5, sizeof(h)); CHECK(latest(h) == -1);
  auto r = begin(h, 9, 0); CHECK(r.w[BOOT] == 1 && r.w[RUNTIME] == 0);
  const int first = latest(h); CHECK(first >= 0);
  auto copy = h.copies[first / 2][first % 2];
  for (unsigned i = 0; i < WORDS; ++i) {
    auto corrupt = copy; corrupt.w[i] ^= 1; CHECK(!valid(corrupt));
  }
  auto bad = copy; bad.w[VERSION] = 2; bad.w[CHECKSUM] = checksum(bad); CHECK(!valid(bad));
  bad = copy; bad.w[STATE] = 5; bad.w[CHECKSUM] = checksum(bad); CHECK(!valid(bad));
  bad = copy; bad.w[EVENT] = unsigned(Event::COUNT); bad.w[CHECKSUM] = checksum(bad); CHECK(!valid(bad));
  r.w[RUNTIME] = 1; r.w[STATE] = 0; r.w[MV] = 3970; r.w[VALID] = 1;
  r.w[EVENT] = unsigned(Event::ADC_OK); save(h, r);
  const int committed = latest(h); CHECK(committed != first);
  // Simulate reset before final magic, and before checksum completion.
  auto &torn = h.copies[first / 2][first % 2]; torn.w[MAGIC] = 0;
  torn.w[SEQUENCE] = h.copies[committed / 2][committed % 2].w[SEQUENCE] + 1;
  CHECK(latest(h) == committed);
  torn.w[MAGIC] = 0x52303144u; CHECK(latest(h) == committed);
  auto next = begin(h, 6, 0); CHECK(next.w[BOOT] == 2 && next.w[RUNTIME] == 0);
  CHECK(boot_record(h, 1) == committed);
  const auto &old = h.copies[committed / 2][committed % 2];
  CHECK(old.w[RUNTIME] == 1 && old.w[MV] == 3970 && old.w[EVENT] == unsigned(Event::ADC_OK));
  return true;
}
bool reset_history_bounded_and_wrap() {
  using namespace reset_diagnostic;
  History h{};
  for (unsigned boot = 1; boot <= 1000; ++boot) {
    auto r = begin(h, boot % 15, 0); CHECK(r.w[BOOT] == boot);
    for (unsigned event = 0; event < 10; ++event) {
      r.w[EVENT] = unsigned(Event::ADC_OK); r.w[UPTIME_MS] = event * 2000;
      r.w[RUNTIME] = 1; save(h, r);
    }
    for (unsigned age = 0; age < std::min(boot, 4u); ++age) {
      int i = boot_record(h, boot - age); CHECK(i >= 0);
      CHECK(h.copies[i / 2][i % 2].w[RUNTIME] == 1);
    }
  }
  CHECK(sizeof(h) == 640);
  History wrap{}; Record r{}; r.w[BOOT] = 0xffffffffu;
  save(wrap, r);
  auto &last = wrap.copies[3][0]; last.w[SEQUENCE] = 0xffffffffu; last.w[CHECKSUM] = checksum(last);
  auto n = begin(wrap, 9, 0); CHECK(n.w[BOOT] == 0);
  CHECK(latest(wrap) == 0 && wrap.copies[0][0].w[SEQUENCE] == 0);
  CHECK(newer(0, 0xffffffffu) && !newer(0xffffffffu, 0));
  return true;
}
struct WakeProbe {
  bool masked = false, level = false, low = true, sleeping = false;
  unsigned failure = 0, step = 0, storms = 0, calls = 0;
  bool op() { return ++step != failure; }
  void check() { if (low && level && !masked) ++storms; }
  bool mask() { if (!op()) return false; masked = true; return true; }
  // Models the actual IDF side effect, including a partly failed arm.
  bool arm() { level = true; check(); return op(); }
  bool sleep() { ++calls; sleeping = true; check(); return op(); }
  bool disarm() { return op(); }
  bool edge() { if (!op()) return false; level = false; return true; }
  bool unmask() { if (!op()) return false; masked = false; check(); return true; }
};
bool wake_irq_level_never_enabled_awake() {
  WakeProbe original; CHECK(original.arm()); CHECK(original.storms == 1);
  // A held-low input immediately reasserts level status; notification alone
  // does not acknowledge the expander. Simulate no tick progress under storm.
  for (unsigned i = 0; i < 10000; ++i) original.check();
  CHECK(original.storms == 10001);
  WakeProbe fixed;
  for (unsigned i = 0; i < 1000; ++i) {
    CHECK(level_wake_cycle(fixed) == WakeCycle::OK);
    CHECK(!fixed.masked && !fixed.level && fixed.storms == 0);
  }
  CHECK(fixed.calls == 1000);
  return true;
}
bool wake_irq_failure_cleanup() {
  const WakeCycle expected[]{WakeCycle::MASK_FAILED, WakeCycle::ARM_FAILED,
    WakeCycle::SLEEP_FAILED, WakeCycle::RESTORE_FAILED, WakeCycle::RESTORE_FAILED,
    WakeCycle::RESTORE_FAILED};
  for (unsigned fail = 1; fail <= 6; ++fail) {
    WakeProbe p; p.failure = fail;
    CHECK(level_wake_cycle(p) == expected[fail - 1]); CHECK(p.storms == 0);
    if (fail >= 4) CHECK(p.masked); // Never re-enable an unsafe level interrupt.
    else CHECK(!p.masked && !p.level);
    CHECK(p.calls == (fail < 3 ? 0u : 1u));
  }
  return true;
}
bool automatic_sleep_external_independent() {
  for(auto external:{External::PRESENT,External::ABSENT,External::UNKNOWN}) {
    Machine m;
    m.tick(29999,true,external);CHECK(m.state==State::ACTIVE);
    m.tick(30000,true,external);CHECK(m.state==State::DISPLAY_IDLE);
    m.tick(299999,true,external);CHECK(m.state==State::DISPLAY_IDLE);
    m.tick(300000,false,external);CHECK(m.state==State::DISPLAY_IDLE && m.sleep_requests==0);
    m.tick(359999,true,external);CHECK(m.state==State::DISPLAY_IDLE);
    m.tick(360000,true,external);CHECK(m.state==State::SYSTEM_SLEEP && m.sleep_requests==1);
    m.wake(360001,Wake::MOTION);CHECK(m.state==State::ACTIVE && m.reason==Wake::MOTION);
    CHECK(m.state!=State::CRITICAL_SHUTDOWN);
  }
  return true;
}
bool expander_display_and_release_faults() {
  struct Outputs {
    unsigned display_calls = 0, release_calls = 0;
    bool fail = false;
    bool display(bool) { ++display_calls; return !fail; }
    bool release_hold() { ++release_calls; return !fail; }
  } b;
  bool display = true, hold = true;
  b.fail = true; CHECK(!set_display(b, display, false) && display);
  CHECK(!controlled_release(b, true, true, display, hold) && hold && b.release_calls == 0);
  b.fail = false; CHECK(set_display(b, display, false) && !display);
  CHECK(set_display(b, display, false) && b.display_calls == 2);
  CHECK(!controlled_release(b, false, true, display, hold) && hold);
  CHECK(!controlled_release(b, true, false, display, hold) && hold && b.release_calls == 0);
  b.fail = true; CHECK(!controlled_release(b, true, true, display, hold) && hold && b.release_calls == 1);
  b.fail = false; CHECK(controlled_release(b, true, true, display, hold) && !hold);
  CHECK(!controlled_release(b, true, true, display, hold) && b.release_calls == 2);
  CHECK(set_display(b, display, true) && display);
  return true;
}
struct Bus {
  std::array<unsigned char, 256> reg{};
  unsigned clock = 0, operations = 0, fail_at = 0, writes = 0;
  bool reset_ack = true, command_ack = true;
  Bus() { reg[0] = 5; }
  unsigned now() { return clock; }
  void delay(unsigned ms) { clock += ms; }
  bool read(unsigned r, unsigned char &v) {
    if (++operations == fail_at) return false;
    v = reg[r]; return true;
  }
  bool write(unsigned r, unsigned char v) {
    if (++operations == fail_at) return false;
    ++writes; reg[r] = v;
    if (r == 0x60 && reset_ack) reg[0x4d] = 0x80;
    if (r == 0x0a && command_ack) reg[0x2d] = v ? 0x80 : 0;
    return true;
  }
};
bool motion_config_and_ack() {
  Bus b; CHECK(Wom::configure(b));
  CHECK(b.reg[2] == 0x48 && b.reg[3] == 0x2d && b.reg[8] == 1);
  CHECK(b.reg[0x0b] == 200 && b.reg[0x0c] == 0x84);
  bool motion = false; b.reg[0x2f] = 4; CHECK(Wom::status(b, motion) && motion);
  b.reg[0x2f] = 0; CHECK(Wom::status(b, motion) && !motion);
  b.fail_at = b.operations + 1; CHECK(!Wom::status(b, motion) && !motion);
  return true;
}
bool motion_failure_matrix() {
  Bus good; CHECK(Wom::configure(good));
  for (unsigned failure = 1; failure <= good.operations; ++failure) {
    Bus b; b.fail_at = failure; CHECK(!Wom::configure(b)); CHECK(b.clock <= 2500);
  }
  Bus id; id.reg[0] = 0; CHECK(!Wom::configure(id) && id.writes == 0);
  Bus reset; reset.reset_ack = false; CHECK(!Wom::configure(reset) && reset.clock == 500 && reset.reg[8] == 0);
  Bus ack; ack.command_ack = false; CHECK(!Wom::configure(ack) && ack.clock == 1000 && ack.reg[8] == 0);
  return true;
}
bool owner_session_fence() {
  START; PowerSuspend p;
  auto before = s.snapshot();
  control_home::Channel channel; channel.publish(before);
  CHECK(channel.request(control_home::Action::NEXT));
  control_home::Intent old; CHECK(channel.pop(old)); CHECK(channel.request(control_home::Action::NEXT));
  control::ingress::Event held; held.kind = control::ingress::Kind::RECONNECT;
  CHECK(s.boundary.inbox.scope(held.scope)); CHECK(s.boundary.inbox.post(held) == control::ingress::Status::POSTED);
  CHECK(p.park(s, 20)); channel.publish(s.snapshot());
  CHECK(channel.count == 0 && !s.snapshot().next);
  CHECK(s.boundary.inbox.post(held) == control::ingress::Status::CLOSED);
  CHECK(p.resume(s, 30)); auto after = s.snapshot(); channel.publish(after);
  CHECK(after.session > before.session && after.next && after.volume_known && after.volume == before.volume);
  CHECK(!s.handle(old, 31) && s.device.mock.dispatches == 0);
  CHECK(s.boundary.inbox.post(held) == control::ingress::Status::POSTED);
  CHECK(s.boundary.process_one(32) == control::ingress::Status::STALE);
  CHECK(channel.request(control_home::Action::NEXT)); control_home::Intent fresh; CHECK(channel.pop(fresh));
  CHECK(s.handle(fresh, 33) && s.device.mock.dispatches == 1); CHECK(s.handle(fresh, 34) && s.device.mock.dispatches == 1);
  return true;
}
bool capabilities_and_disconnect_preserved() {
  START; PowerSuspend p;
  CHECK(s.device.command('-', 11)); CHECK(s.device.command('o', 12));
  const auto count = s.core.target(s.d, s.t)->capabilities.size();
  CHECK(!s.snapshot().next && p.park(s, 20) && p.resume(s, 30));
  CHECK(!s.snapshot().next && s.core.target(s.d, s.t)->capabilities.size() == count);
  CHECK(s.device.command('-', 31)); auto withdrawn = s.core.target(s.d, s.t)->capabilities.size();
  CHECK(p.park(s, 40) && p.resume(s, 50)); CHECK(s.core.target(s.d, s.t)->capabilities.size() == withdrawn && !s.snapshot().next);
  CHECK(s.device.command('d', 51)); CHECK(p.park(s, 60) && p.resume(s, 70));
  CHECK(s.snapshot().connection != control_home::Connection::CONNECTED && !s.snapshot().toggle);
  return true;
}
bool pending_refuses_sleep() {
  START; PowerSuspend p;
  CHECK(s.device.command('l', 11)); auto v = s.snapshot();
  CHECK(s.handle({1, control_home::Action::NEXT, v.session, v.capability_revision}, 12));
  CHECK(s.device.pending && !p.park(s, 13) && !p.parked);
  auto id = s.device.mock.last.value.request_id;
  CHECK(s.device.command('f', 3014)); CHECK(s.boundary.process_one(3014) == control::ingress::Status::EMPTY);
  CHECK(s.core.result(id)->lifecycle == control::Lifecycle::CONFIRMED_COMPLETED && s.core.result(id)->deadline_elapsed);
  CHECK(!s.device.pending); s.core.tick(3015);
  for (const auto &e : s.store.effects) if (!e.resolved()) { CHECK(!p.park(s, 3016)); return true; }
  CHECK(p.park(s, 3016)); CHECK(p.resume(s, 3017)); CHECK(s.device.mock.dispatches == 1);
  return true;
}
bool wrong_owner_and_shutdown_recovery() {
  bool owner = true;
  Session s(control::Id("power.owner"), [](void *p) { return *static_cast<bool *>(p); }, &owner);
  CHECK(s.initialize(10)); PowerSuspend p;
  auto generation = s.snapshot().session; owner = false;
  CHECK(!p.park(s, 20)); CHECK(s.snapshot().session == generation);
  owner = true; CHECK(p.park(s, 21, true)); owner = false; CHECK(!p.resume(s, 22));
  owner = true; CHECK(p.resume(s, 23)); CHECK(s.snapshot().toggle);
  return true;
}
bool repeated_power_cycles() {
  START; PowerSuspend p; Machine m; Time now = 100;
  std::size_t warmed = 0, maximum = 0;
  const auto cycles = 1000 * multiplier;
  for (unsigned i = 0; i < cycles; ++i) {
    now += 300000; m.tick(now, true, External::ABSENT); CHECK(m.state == State::SYSTEM_SLEEP);
    auto v = s.snapshot(); CHECK(p.park(s, now)); CHECK(p.resume(s, ++now));
    CHECK(s.snapshot().session > v.session && s.snapshot().volume_known);
    CHECK(s.device.mock.dispatches == 0);
    m.wake(now, i % 2 ? Wake::MOTION : Wake::TOUCH); CHECK(m.state == State::ACTIVE);
    if (i == 5) warmed = control::Memory::used;
    if (i > 5) CHECK(control::Memory::used <= warmed + 256);
    maximum = std::max(maximum, control::Memory::used);
    CHECK(s.store.effects.size() == 0 && s.boundary.inbox.counts().overflow == 0);
  }
  cycles_done += cycles;
  std::printf("POWER STRESS cycles=%u maximum_payload=%zu final_payload=%zu dispatches=%u\n", cycles, maximum, control::Memory::used, s.device.mock.dispatches);
  return true;
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::strcmp(argv[1], "--soak") == 0) multiplier = 10;
  struct Test { const char *name; bool (*run)(); };
  const Test tests[] = {
#define TEST(x) {#x, x}
    TEST(battery_truth_and_faults), TEST(battery_hysteresis), TEST(critical_qualification_and_sources), TEST(exact_threshold_convergence),
    TEST(idle_wake_and_fail_safe), TEST(encoder_activity_display_policy), TEST(automatic_sleep_external_independent), TEST(first_touch_consumed), TEST(expander_display_and_release_faults), TEST(motion_config_and_ack), TEST(motion_failure_matrix),
    TEST(owner_session_fence), TEST(capabilities_and_disconnect_preserved), TEST(pending_refuses_sleep),
    TEST(wrong_owner_and_shutdown_recovery), TEST(repeated_power_cycles)
    , TEST(reset_record_validation_and_torn_write), TEST(reset_history_bounded_and_wrap),
    TEST(wake_irq_level_never_enabled_awake), TEST(wake_irq_failure_cleanup)
  };
  unsigned pass = 0, fail = 0;
  for (auto t : tests) {
    const auto used = control::Memory::used; bool ok = t.run();
    ++assertions; ok = ok && control::Memory::used == used;
    std::printf("%s POWER %s\n", ok ? "PASS" : "FAIL", t.name); ok ? ++pass : ++fail;
  }
  std::printf("POWER SUMMARY PASS=%u FAIL=%u assertions=%u stress_iterations=%u policy_bytes=%zu suspend_bytes=%zu\n", pass, fail, assertions, cycles_done, sizeof(Battery) + sizeof(Machine), sizeof(PowerSuspend));
  return fail ? 1 : 0;
}
