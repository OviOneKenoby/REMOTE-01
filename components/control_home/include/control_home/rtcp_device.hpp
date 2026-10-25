#pragma once
#include "control/ingress.hpp"
#include "control/mock_adapter.hpp"
#include "control_home/volume.hpp"
#include "control_home/rtcp_d009_policy.hpp"
#include "rtcp_client/client.hpp"

namespace control_home {
struct RtcpDeviceTestAccess;
class RtcpDevice final:public control::Adapter{
  friend struct RtcpDeviceTestAccess;
  rtcp_client::Client&client_;control::Core*core_=nullptr;control::ingress::Boundary*boundary_=nullptr;
  control::Id binding_;control::MockAdapter codec_;control::Optional<control::AdapterEvidence>pending_;
  control::U64 observations_=0;
  std::uint64_t dispatches_=0;
  control::U64 btx_observed_at_=0;
  bool btx_authoritative_=false;
  d009::MediaFence d009_media_fence_{}, btx_media_fence_{};
  rtcp_client::wire::Snapshot last_d009_snapshot_{};
  bool last_d009_snapshot_present_=false;
  control::Evidence evidence(const char*,const char*)const;
  void availability(control::Availability,control::U64);
  bool capability(control::CapabilityKey::Kind,const char*,control::Availability,control::U64);
  bool negotiated(control::U64);
  bool context(const rtcp_client::wire::NowPlaying&,control::U64);
  bool btx_context(const rtcp_client::wire::NowPlaying&,control::U64);
  bool publish(const char*,control::Value<control::Scalar>,std::uint64_t,control::U64,bool);
  bool media(const d009::FenceDecision&,control::U64);
  bool handle(rtcp_client::Client::Event,control::U64);
public:
  explicit RtcpDevice(rtcp_client::Client&c):client_(c){}
  static control::FreshnessPolicy freshness_policy() {
    control::FreshnessPolicy policy;
    policy.max_age_ms = 2000;
    policy.acquisition = control::Acquisition::VERIFIED_SEQUENCE;
    policy.source_clock_contract = control::Value<control::Text>::of(
        control::Text::literal("same authenticated request/reply sample"));
    return policy;
  }
  static control::Rational volume_rational(std::uint32_t percent) {
    return control_home::volume_rational(percent);
  }
  control::Id identity()const override{return control::Id("adapter.rtcp.tuner.v1");}
  control::Text version()const override{return control::Text::literal("rtcp/1+ccm/1.1.0");}
  void dispatch(const control::Dispatch&,control::AdapterSink&)override;
  bool install(control::Core&,control::ingress::Boundary&,const control::Id&,const control::Id&,const control::Id&,control::U64);
  bool poll(control::U64,bool command_waiting=false);
  bool command_ready()const{return client_.command_ready();}
  bool pending()const{return pending_.present;}
  std::uint64_t dispatches()const{return dispatches_;}
};
}
