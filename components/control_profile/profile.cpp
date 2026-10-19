#include "control_profile/profile.hpp"

namespace control_profile {
namespace {
class Driver {
  Session &s;
  Clock &clock;
  Reporter &report;
  unsigned iterations = 0;
  bool check(bool ok, const char *name) {
    if (!ok)
      report.failure(name);
    return ok;
  }
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!check(bool(x), #x))                                                   \
      return false;                                                            \
  } while (0)
  bool owner() { return check(clock.is_owner(), "serialized_owner"); }
  Scalar playback(Playback p) {
    Scalar v;
    v.kind = ScalarKind::PLAYBACK;
    v.playback = p;
    return v;
  }
  bool sample(const Id &binding, std::string_view field, Scalar value,
              Origin origin = Origin::LIVE) {
    REQUIRE(owner());
    auto now = clock.now();
    auto observation = s.mock.sample(s.core, binding, field,
                                     Value<Scalar>::of(value), now, origin);
    REQUIRE(s.core.observe(std::move(observation), now));
    REQUIRE(!s.store.blocked);
    return true;
  }
  bool initial(const Id &binding, Origin origin = Origin::LIVE) {
    REQUIRE(sample(binding, "power", Scalar::power_value(Power::ON), origin));
    REQUIRE(sample(binding, "volume.level", Scalar::level({17, 25}), origin));
    REQUIRE(
        sample(binding, "volume.mute", Scalar::boolean_value(false), origin));
    REQUIRE(
        sample(binding, "playback.state", playback(Playback::PLAYING), origin));
    return true;
  }
  bool command(CommandRequest &q, std::string_view operation,
               const Arguments &args = {}, U64 duration = 5000) {
    REQUIRE(owner());
    auto now = clock.now();
    REQUIRE(!s.core
                 .capture(q, s.device, s.target, operation, args,
                          now + duration, now)
                 .error.present);
    return true;
  }
  bool confirmed(std::string_view operation, const Arguments &args = {}) {
    CommandRequest q;
    REQUIRE(command(q, operation, args));
    auto reply = s.core.submit(q, clock.now());
    REQUIRE(reply.result && !reply.error.present);
    REQUIRE(reply.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
    REQUIRE(!s.store.blocked);
    return true;
  }
  bool republish(const Id &binding) {
    REQUIRE(owner());
    for (auto op : {"power.set", "power.toggle", "volume.set", "volume.step",
                    "volume.mute", "playback.play", "playback.pause",
                    "playback.stop", "playback.previous", "playback.next"}) {
      auto cap =
          MockAdapter::operation(binding, op, Feedback::CORRELATED_RESULT);
      REQUIRE(
          !s.core.capability(s.device, s.target, std::move(cap), clock.now())
               .present);
    }
    for (auto field :
         {"power", "volume.level", "volume.mute", "playback.state"}) {
      auto cap = MockAdapter::readable(binding, field);
      REQUIRE(
          !s.core.capability(s.device, s.target, std::move(cap), clock.now())
               .present);
    }
    return true;
  }
  bool conflict() {
    REQUIRE(sample(s.primary, "power", Scalar::power_value(Power::ON)));
    REQUIRE(sample(s.secondary, "power", Scalar::power_value(Power::OFF)));
    REQUIRE(s.core.field(s.device, s.target, "power")->conflict_latched);
    REQUIRE(s.core.begin_resync(s.primary, "power", clock.now()));
    REQUIRE(sample(s.primary, "power", Scalar::power_value(Power::ON),
                   Origin::RESYNC));
    REQUIRE(s.core.field(s.device, s.target, "power")->conflict_latched);
    REQUIRE(s.core.begin_resync(s.secondary, "power", clock.now()));
    REQUIRE(sample(s.secondary, "power", Scalar::power_value(Power::ON),
                   Origin::RESYNC));
    REQUIRE(!s.core.field(s.device, s.target, "power")->conflict_latched);
    return true;
  }
  bool representative() {
    REQUIRE(owner());
    CommandRequest q;
    REQUIRE(command(q, "power.toggle"));
    auto first = s.core.submit(q, clock.now());
    REQUIRE(first.result &&
            first.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
    auto count = s.mock.dispatches;
    auto duplicate = s.core.submit(q, clock.now());
    REQUIRE(duplicate.result == first.result && s.mock.dispatches == count);
    // State changes are separate synthetic observations, never command
    // inference.
    auto old =
        s.mock.sample(s.core, s.primary, "volume.level",
                      Value<Scalar>::of(Scalar::level({17, 25})), clock.now());
    REQUIRE(s.core.observe(old, old.receipt_time.tick_ms));
    REQUIRE(sample(s.primary, "volume.level", Scalar::level({3, 4})));
    old.receipt_time.tick_ms = clock.now();
    REQUIRE(!s.core.observe(old, old.receipt_time.tick_ms));
    auto current = s.core.field(s.device, s.target, "volume.level");
    REQUIRE(current->selected.present);
    REQUIRE((current->selected.value.value.value.rational == Rational{3, 4}));
    REQUIRE(conflict());
    auto stale = s.mock.sample(
        s.core, s.primary, "power",
        Value<Scalar>::of(Scalar::power_value(Power::OFF)), clock.now());
    REQUIRE(!s.core.reconnect(s.primary, clock.now()).present);
    stale.receipt_time.tick_ms = clock.now();
    REQUIRE(!s.core.observe(stale, stale.receipt_time.tick_ms));
    REQUIRE(republish(s.primary));
    REQUIRE(initial(s.primary, Origin::RESYNC));
    s.mock.mode = MockAdapter::Mode::DELAYED;
    REQUIRE(command(q, "playback.pause"));
    auto delayed = s.core.submit(q, clock.now());
    REQUIRE(delayed.result && !delayed.result->terminal);
    auto completion = s.mock.completion();
    REQUIRE(s.core.report(std::move(completion), clock.now()));
    REQUIRE(delayed.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
    REQUIRE(sample(s.primary, "playback.state", playback(Playback::PAUSED)));
    // Known sent work times out without inventing a nonexecution guarantee.
    s.mock.mode = MockAdapter::Mode::NO_RESULT;
    REQUIRE(command(q, "power.toggle", {}, 100));
    auto timeout = s.core.submit(q, clock.now());
    REQUIRE(timeout.result && !timeout.result->terminal);
    auto proof = s.mock.proof(s.mock.last.value.covered_keys, true);
    REQUIRE(clock.wait_until(q.deadline.tick_ms + 1));
    REQUIRE(owner());
    s.core.tick(clock.now());
    REQUIRE(timeout.result->lifecycle == Lifecycle::UNCERTAIN_OUTCOME);
    REQUIRE(s.core.report(std::move(proof), clock.now()));
    REQUIRE(timeout.result->lifecycle == Lifecycle::FAILED);
    REQUIRE(timeout.result->delivery == Delivery::SENT);
    s.mock.mode = MockAdapter::Mode::AMBIGUOUS;
    REQUIRE(command(q, "power.toggle"));
    auto ambiguous = s.core.submit(q, clock.now());
    REQUIRE(ambiguous.result &&
            ambiguous.result->lifecycle == Lifecycle::UNCERTAIN_OUTCOME);
    proof = s.mock.proof(s.mock.last.value.covered_keys, true);
    REQUIRE(command(q, "power.toggle"));
    auto queued = s.core.submit(q, clock.now());
    REQUIRE(queued.result && queued.result->lifecycle == Lifecycle::QUEUED);
    count = s.mock.dispatches;
    s.mock.mode = MockAdapter::Mode::CONFIRMED;
    REQUIRE(s.core.report(std::move(proof), clock.now()));
    REQUIRE(queued.result->lifecycle == Lifecycle::CONFIRMED_COMPLETED);
    REQUIRE(s.mock.dispatches == count + 1);
    REQUIRE(ambiguous.result->delivery == Delivery::AMBIGUOUS);
    REQUIRE(!s.store.blocked);
    return true;
  }
  bool wave(unsigned wave_number) {
    for (unsigned i = 0; i < iterations_per_wave; ++i) {
      REQUIRE(owner());
      REQUIRE(initial(s.primary));
      REQUIRE(sample(s.secondary, "power", Scalar::power_value(Power::ON)));
      REQUIRE(confirmed("power.toggle"));
      Arguments args;
      REQUIRE(
          args.fields.push({Text::literal("level"), Scalar::level({17, 25})}));
      REQUIRE(confirmed("volume.set", args));
      if (i % 4 == 0) {
        auto cap = MockAdapter::operation(s.primary, "power.toggle",
                                          Feedback::CORRELATED_RESULT);
        REQUIRE(
            !s.core.capability(s.device, s.target, std::move(cap), clock.now())
                 .present);
        REQUIRE(conflict());
      }
      if (i % 8 == 0) {
        REQUIRE(!s.core.reconnect(s.primary, clock.now()).present);
        REQUIRE(republish(s.primary));
        REQUIRE(initial(s.primary, Origin::RESYNC));
      }
      ++iterations;
      if (i % 4 == 3)
        report.checkpoint("stress_progress", iterations, s);
      REQUIRE(clock.wait_until(clock.now() + 1)); // yield to idle/watchdog
    }
    report.checkpoint(wave_number == 0 ? "after_stress_wave_1"
                                       : "after_stress_wave_2",
                      iterations, s);
    report.checkpoint("retention_wait_begin", iterations, s);
    REQUIRE(clock.wait_until(clock.now() + 60001));
    REQUIRE(owner());
    s.core.tick(clock.now());
    for (const auto &slot : s.store.records)
      REQUIRE(!slot.get());
    REQUIRE(s.store.effects.size() == 0);
    s.mock.last = {};
    s.mock.last_numeric = {};
    REQUIRE(s.store.devices.size() == 1 && s.store.targets.size() == 1);
    REQUIRE(s.store.bindings.size() == 2 &&
            s.store.targets[0].capabilities.size() == 28);
    REQUIRE(s.store.samples.size() == 5 && s.store.resources.size() == 1);
    REQUIRE(!s.store.blocked);
    report.checkpoint(wave_number == 0 ? "after_purge_wave_1"
                                       : "after_purge_wave_2",
                      iterations, s);
    return true;
  }

public:
  Driver(Session &session, Clock &c, Reporter &r)
      : s(session), clock(c), report(r) {}
  bool run() {
    REQUIRE(owner());
    REQUIRE(!s.store.blocked);
    REQUIRE(s.mock.install(s.core, s.device, s.target, s.primary,
                           Feedback::CORRELATED_RESULT, Ordering::STRICT,
                           clock.now()));
    REQUIRE(s.mock.install(s.core, s.device, s.target, s.secondary,
                           Feedback::CORRELATED_RESULT, Ordering::STRICT,
                           clock.now()));
    SharedResource resource;
    resource.resource_id = Id("mock.profile.shared");
    REQUIRE(resource.member_targets.push({s.device, s.target}));
    Evidence evidence;
    evidence.evidence_id = Id("mock.profile.resource.contract");
    evidence.kind = EvidenceKind::PROJECT_DESIGN;
    evidence.reference =
        Text::literal("local synthetic resource; no physical device claim");
    REQUIRE(resource.evidence.push(std::move(evidence)));
    REQUIRE(
        !s.core.register_resource(std::move(resource), clock.now()).present);
    report.checkpoint("after_registration", iterations, s);
    REQUIRE(initial(s.primary));
    REQUIRE(sample(s.secondary, "power", Scalar::power_value(Power::ON)));
    report.checkpoint("after_initial_state", iterations, s);
    REQUIRE(representative());
    report.checkpoint("after_activity", iterations, s);
    for (unsigned w = 0; w < waves; ++w)
      REQUIRE(wave(w));
    report.checkpoint("profile_complete", iterations, s);
    return true;
  }
#undef REQUIRE
};
} // namespace
bool exercise(Session &s, Clock &clock, Reporter &report) {
  return Driver(s, clock, report).run();
}
} // namespace control_profile
