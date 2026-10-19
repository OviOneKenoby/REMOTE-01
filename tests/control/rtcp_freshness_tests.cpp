#include "control_home/rtcp_device.hpp"
#include <cstdio>

using namespace control;

namespace {
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)

class Adapter final : public control::Adapter {
public:
  control::Id identity() const override { return control::Id("test.rtcp"); }
  control::Text version() const override { return control::Text::literal("1"); }
  void dispatch(const control::Dispatch &, control::AdapterSink &) override {}
};

bool matching_policy_is_admitted_and_mismatch_rejected() {
  Store store;
  MockJournal journal;
  Core core(store, journal, Id("rtcp.policy.epoch"));
  Adapter adapter;
  const Id device_id("rtcp.device"), target_id("rtcp.target"), binding_id("rtcp.binding");
  CommonDevice device; device.device_id=device_id; device.display_name=Text::literal("RTCP");
  Target target; target.device_id=device_id; target.target_id=target_id; target.display_name=Text::literal("RTCP");
  Binding binding; binding.device_id=device_id; binding.target_id=target_id; binding.binding_id=binding_id;
  binding.authorization=Authorization::AUTHORIZED; binding.reachability=Reachability::REACHABLE;
  binding.model_versions.push(Version{});
  binding.mapping_evidence=MockAdapter::readable(binding_id,"volume.level").evidence;
  CHECK(!core.register_device(device,1).present);
  CHECK(!core.register_target(target,1).present);
  CHECK(!core.register_binding(binding,adapter,1).present);
  auto capability=MockAdapter::readable(binding_id,"volume.level");
  capability.freshness=control_home::RtcpDevice::freshness_policy();
  CHECK(!core.capability(device_id,target_id,capability,1).present);
  MockAdapter codec;
  auto observation=codec.sample(core,binding_id,"volume.level",Value<Scalar>::of(Scalar::level({1,2})),2);
  observation.freshness=control_home::RtcpDevice::freshness_policy();
  CHECK(core.observe(observation,2));
  auto mismatch=observation;
  mismatch.observation_id=Id("rtcp.observation.mismatch");
  mismatch.acquisition_order=2;
  mismatch.source_sequence.value.ordinal=2;
  mismatch.receipt_time.tick_ms=3;
  mismatch.freshness.max_age_ms=5000;
  CHECK(!core.observe(std::move(mismatch),3));
  return true;
}

bool volume_is_canonical_and_playback_is_independent() {
CHECK((control_home::RtcpDevice::volume_rational(0) == Rational{0,1}));
CHECK((control_home::RtcpDevice::volume_rational(1) == Rational{1,100}));
CHECK((control_home::RtcpDevice::volume_rational(50) == Rational{1,2}));
CHECK((control_home::RtcpDevice::volume_rational(68) == Rational{17,25}));
CHECK((control_home::RtcpDevice::volume_rational(100) == Rational{1,1}));
  for (std::uint32_t percent=0; percent<=100; ++percent) {
    const auto value=control_home::RtcpDevice::volume_rational(percent);
    CHECK(value.normalized());
    CHECK(value.n*100==static_cast<std::int64_t>(percent*value.d));
  }

  Store store;
  MockJournal journal;
  Core core(store,journal,Id("rtcp.independent.epoch"));
  Adapter adapter;
  const Id device_id("rtcp.device.2"),target_id("rtcp.target.2"),binding_id("rtcp.binding.2");
  CommonDevice device;device.device_id=device_id;device.display_name=Text::literal("RTCP");
  Target target;target.device_id=device_id;target.target_id=target_id;target.display_name=Text::literal("RTCP");
  Binding binding;binding.device_id=device_id;binding.target_id=target_id;binding.binding_id=binding_id;
  binding.authorization=Authorization::AUTHORIZED;binding.reachability=Reachability::REACHABLE;
  binding.model_versions.push(Version{});binding.mapping_evidence=MockAdapter::readable(binding_id,"volume.level").evidence;
  CHECK(!core.register_device(device,1).present);
  CHECK(!core.register_target(target,1).present);
  CHECK(!core.register_binding(binding,adapter,1).present);
  for (auto name:{"volume.level","playback.state"}) {
    auto capability=MockAdapter::readable(binding_id,name);
    capability.freshness=control_home::RtcpDevice::freshness_policy();
    CHECK(!core.capability(device_id,target_id,capability,1).present);
  }
  MockAdapter codec;
  auto invalid_volume=codec.sample(core,binding_id,"volume.level",Value<Scalar>::of(Scalar::level({50,100})),2);
  invalid_volume.freshness=control_home::RtcpDevice::freshness_policy();
  CHECK(!core.observe(std::move(invalid_volume),2));
  Scalar playback;playback.kind=ScalarKind::PLAYBACK;playback.playback=Playback::PLAYING;
  auto playback_observation=codec.sample(core,binding_id,"playback.state",Value<Scalar>::of(playback),2);
  playback_observation.freshness=control_home::RtcpDevice::freshness_policy();
  CHECK(core.observe(std::move(playback_observation),2));
  CHECK(core.field(device_id,target_id,"playback.state")->selected.present);
  return true;
}
}

int main() {
  const bool a=matching_policy_is_admitted_and_mismatch_rejected();
  const bool b=volume_is_canonical_and_playback_is_independent();
  const unsigned passed=unsigned(a)+unsigned(b);
  std::printf("RTCP_VOLUME_ADMISSION: PASS=%u FAIL=%u assertions=%u\n",passed,2-passed,assertions);
  return passed==2?0:1;
}
