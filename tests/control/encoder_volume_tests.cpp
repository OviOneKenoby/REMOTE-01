#include "control_home/facade.hpp"
#include "control/mock_adapter.hpp"
#include "encoder_volume.hpp"
#include <cstdio>
using namespace control_home;
using namespace control;
namespace {
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)
struct Fixture {
  Store store; MockJournal journal; Core core; MockAdapter mock;
  Id d{"encoder.device"}, t{"encoder.target"}, b{"encoder.binding"};
  Facade facade;
  Fixture() : core(store, journal, Id("encoder.epoch")),
              facade(core, d, t, b) {
    if (!mock.install(core, d, t, b, Feedback::CORRELATED_RESULT,
                      Ordering::STRICT))
      std::abort();
    auto observation = mock.sample(
        core, b, "volume.level", Value<Scalar>::of(Scalar::level({17, 25})),
        0);
    if (!core.observe(observation, 0)) std::abort();
  }
};
Intent intent(const Fixture &f, std::uint64_t token, Action action,
              std::uint8_t volume) {
  const auto x = f.facade.snapshot();
  return {token, action, x.session, x.capability_revision, volume};
}
bool route_cycle(bool home, bool positive, unsigned &emitted, Action &last) {
  remote_encoder::Router router;
  auto emit = [&](Action action) { ++emitted; last = action; return true; };
  std::uint64_t t = 0;
  router.sample(t++, false, false, home, emit);
  const std::uint8_t positive_sequence[] = {1, 3, 2, 0};
  const std::uint8_t negative_sequence[] = {2, 3, 1, 0};
  for (auto ab : positive ? positive_sequence : negative_sequence)
    router.sample(t++, (ab & 2) != 0, (ab & 1) != 0, home, emit);
  return true;
}
bool routing() {
  unsigned emitted = 0; Action last = Action::TOGGLE;
  CHECK(route_cycle(true, true, emitted, last));
  CHECK(emitted == 1 && last == Action::VOLUME_UP);
  CHECK(route_cycle(true, false, emitted, last));
  CHECK(emitted == 2 && last == Action::VOLUME_DOWN);
  CHECK(route_cycle(false, true, emitted, last) && emitted == 2);
  return true;
}
bool ready_and_bounds() {
  Channel c; Snapshot s; s.connection = Connection::CONNECTED;
  s.session = s.capability_revision = 1; s.volume_known = s.volume_set = true;
  s.volume = 68; c.publish(s);
  CHECK(c.request(Action::VOLUME_UP));
  Intent i; CHECK(c.pop(i) && i.volume == 69);
  CHECK(c.request(Action::VOLUME_UP));
  s.volume = 69; c.publish(s);
  CHECK(c.pop(i) && i.volume == 70);
  s.volume = 70; c.publish(s);
  CHECK(c.request(Action::VOLUME_DOWN)); CHECK(c.pop(i) && i.volume == 69);
  s.connection = Connection::UNAVAILABLE; c.publish(s);
  CHECK(!c.request(Action::VOLUME_DOWN));
  s.connection = Connection::CONNECTED; s.session = 2; s.volume = 0; c.publish(s);
  CHECK(!c.request(Action::VOLUME_DOWN));
  s.volume = 100; c.publish(s); CHECK(!c.request(Action::VOLUME_UP));
  return true;
}
bool core_volume() {
  static Fixture f; auto before = f.facade.snapshot();
  CHECK(before.volume == 68 && before.volume_set);
  CHECK(f.facade.handle(intent(f, 1, Action::VOLUME_UP, 69), 1));
  CHECK(f.mock.dispatches == 1 && f.mock.last_numeric.present);
  CHECK((f.mock.last_numeric.value == Rational{69, 100}));
  CHECK(f.store.records[0].get()->request.operation_id.view() == "volume.set");
  CHECK(f.store.records[0].get()->request.preconditions.size() == 1);
  return true;
}
bool stale_and_disconnect() {
  static Fixture f;
  CHECK(f.facade.handle(intent(f, 1, Action::VOLUME_UP, 70), 1));
  CHECK(f.mock.dispatches == 1);
  const auto stale = intent(f, 2, Action::VOLUME_UP, 71);
  CHECK(!f.core.reachability(f.b, Reachability::UNREACHABLE, 2).present);
  CHECK(!f.facade.handle(stale, 3));
  CHECK(f.mock.dispatches == 1);
  return true;
}
bool no_boot_command() {
  remote_encoder::Router router; unsigned emitted = 0;
  router.sample(0, true, true, true, [&](Action) { ++emitted; return true; });
  CHECK(emitted == 0); return true;
}
}
int main() {
  std::setbuf(stdout, nullptr);
  struct Test { const char *name; bool (*run)(); } tests[] = {
      {"routing", routing}, {"ready_and_bounds", ready_and_bounds},
      {"core_volume", core_volume}, {"stale_and_disconnect", stale_and_disconnect},
      {"no_boot_command", no_boot_command}};
  unsigned pass = 0;
  for (auto test : tests) { std::printf("RUN %s\n", test.name); if (!test.run()) return 1; ++pass; }
  std::printf("ENCODER VOLUME SUMMARY PASS=%u FAIL=0 assertions=%u\n", pass, assertions);
}
