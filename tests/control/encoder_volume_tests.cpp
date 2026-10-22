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
bool bounded_pipeline_diagnostics() {
  remote_encoder::Router router; unsigned emitted = 0;
  auto emit = [&](Action) { ++emitted; return emitted != 3; };
  std::uint64_t t = 10;
  router.sample(t, false, false, true, emit);
  const std::uint8_t positive[] = {1,3,2,0};
  const std::uint8_t negative[] = {2,3,1,0};
  for(unsigned cycle=0;cycle<20;++cycle)
    for(auto ab:positive) { t += cycle==7 ? 25 : 2; router.sample(t,(ab&2)!=0,(ab&1)!=0,true,emit); }
  for(unsigned cycle=0;cycle<10;++cycle)
    for(auto ab:negative) { t += 2; router.sample(t,(ab&2)!=0,(ab&1)!=0,true,emit); }
  // A skipped diagonal transition is classified and resynchronizes without
  // inventing a detent.
  t += 2; router.sample(t,true,true,true,emit);
  const auto view=router.view();
  CHECK(view.detents==30&&view.positive==20&&view.negative==10);
  CHECK(view.invalid==1&&view.rejected==1);
  CHECK(view.sample_last_ms==2&&view.sample_max_ms==25);
  CHECK(view.transitions==121&&emitted==30);
  return true;
}
bool same_session_capability_revision_race() {
  Fixture f;
  auto queued = intent(f, 1, Action::VOLUME_DOWN, 67);
  const auto old_revision = queued.capability_revision;
  auto cap = MockAdapter::operation(f.b, "playback.next",
                                    Feedback::CORRELATED_RESULT);
  cap.availability = Availability::CONTEXT_UNAVAILABLE;
  CHECK(!f.core.capability(f.d, f.t, cap, 1).present);
  CHECK(f.facade.snapshot().capability_revision > old_revision);
  // The absolute volume target is re-captured against current authority in
  // the same authenticated session; it is not discarded as locally stale.
  CHECK(f.facade.handle(queued, 2));
  CHECK(f.mock.dispatches == 1 && f.mock.last_numeric.present);
  CHECK((f.mock.last_numeric.value == Rational{67, 100}));
  CHECK(f.facade.last_volume_failure() == VolumeFailureCause::NONE);
  return true;
}
bool failure_sources_are_distinct() {
  Fixture local;
  auto rejected = intent(local, 1, Action::VOLUME_DOWN, 67);
  CHECK(!local.facade.handle(rejected, 1, false));
  CHECK(local.facade.last_volume_failure() ==
        VolumeFailureCause::NOT_ACCEPTING);
  CHECK(local.mock.dispatches == 0);

  Fixture target;
  target.mock.mode = MockAdapter::Mode::REJECT;
  auto refused = intent(target, 1, Action::VOLUME_DOWN, 67);
  CHECK(!target.facade.handle(refused, 1));
  CHECK(target.facade.last_volume_failure() ==
        VolumeFailureCause::TARGET_REFUSAL);
  CHECK(target.mock.dispatches == 1);

  Fixture stale;
  auto old = intent(stale, 1, Action::VOLUME_DOWN, 67);
  CHECK(!stale.core.reconnect(stale.b, 1).present);
  CHECK(stale.facade.snapshot().session != old.session);
  CHECK(!stale.facade.handle(old, 2));
  CHECK(stale.facade.last_volume_failure() == VolumeFailureCause::SESSION);
  CHECK(stale.mock.dispatches == 0);
  return true;
}
bool transport_busy_retains_coalesced_target() {
  Channel c; Snapshot s; s.connection = Connection::CONNECTED;
  s.session = s.capability_revision = 1; s.volume_known = s.volume_set = true;
  s.volume = 70; c.publish(s);

  // The first detent crosses the owner boundary. Nine more arrive while its
  // correlated result and authoritative snapshot are still outstanding.
  CHECK(c.request(Action::VOLUME_UP, 1));
  Intent first; CHECK(c.pop(first, true) && first.volume == 71);
  for (unsigned n = 0; n < 9; ++n)
    CHECK(c.request(Action::VOLUME_UP, 2 + n));
  CHECK(c.resolve(first.token, VolumeResolution::CONFIRMED));
  s.volume = 71; c.publish(s, 20);

  // An RTCP metadata/snapshot exchange may own the record lane here. The
  // successor remains queued, rather than becoming a definite failure.
  Intent successor;
  CHECK(!c.pop(successor, false));
  CHECK(c.count == 1 && c.volume_failed == 0 && c.view().desired_volume == 80);
  CHECK(c.pop(successor, true) && successor.volume == 80);
  CHECK(c.resolve(successor.token, VolumeResolution::CONFIRMED));
  s.volume = 80; c.publish(s, 30);
  CHECK(c.view().volume == 80 && !c.view().volume_pending);
  CHECK(c.volume_completed == 2 && c.volume_failed == 0);

  Channel d; s.volume = 80; d.publish(s);
  CHECK(d.request(Action::VOLUME_DOWN, 31));
  CHECK(d.pop(first, true) && first.volume == 79);
  for (unsigned n = 0; n < 9; ++n)
    CHECK(d.request(Action::VOLUME_DOWN, 32 + n));
  CHECK(d.resolve(first.token, VolumeResolution::CONFIRMED));
  s.volume = 79; d.publish(s, 42);
  CHECK(!d.pop(successor, false));
  CHECK(d.pop(successor, true) && successor.volume == 70);
  CHECK(d.resolve(successor.token, VolumeResolution::CONFIRMED));
  s.volume = 70; d.publish(s, 50);
  CHECK(d.view().volume == 70 && d.volume_completed == 2 &&
        d.volume_failed == 0);

  // Rapid reversal preserves the algebraic target while the lane is busy.
  Channel r; s.volume = 70; r.publish(s);
  CHECK(r.request(Action::VOLUME_UP, 60));
  CHECK(r.pop(first, true) && first.volume == 71);
  for (unsigned n = 0; n < 4; ++n) CHECK(r.request(Action::VOLUME_UP, 61 + n));
  for (unsigned n = 0; n < 2; ++n) CHECK(r.request(Action::VOLUME_DOWN, 65 + n));
  CHECK(r.resolve(first.token, VolumeResolution::CONFIRMED));
  s.volume = 71; r.publish(s, 70);
  CHECK(!r.pop(successor, false));
  CHECK(r.pop(successor, true) && successor.volume == 73);
  CHECK(r.resolve(successor.token, VolumeResolution::CONFIRMED));
  s.volume = 73; r.publish(s, 80);
  CHECK(r.view().volume == 73 && r.volume_failed == 0);
  return true;
}
}
int main() {
  std::setbuf(stdout, nullptr);
  struct Test { const char *name; bool (*run)(); } tests[] = {
      {"routing", routing}, {"ready_and_bounds", ready_and_bounds},
      {"core_volume", core_volume}, {"stale_and_disconnect", stale_and_disconnect},
      {"no_boot_command", no_boot_command},
      {"bounded_pipeline_diagnostics", bounded_pipeline_diagnostics},
      {"same_session_capability_revision_race", same_session_capability_revision_race},
      {"failure_sources_are_distinct", failure_sources_are_distinct},
      {"transport_busy_retains_coalesced_target", transport_busy_retains_coalesced_target}};
  unsigned pass = 0;
  for (auto test : tests) { std::printf("RUN %s\n", test.name); if (!test.run()) return 1; ++pass; }
  std::printf("ENCODER VOLUME SUMMARY PASS=%u FAIL=0 assertions=%u\n", pass, assertions);
}
