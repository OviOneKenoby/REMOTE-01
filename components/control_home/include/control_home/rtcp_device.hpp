#pragma once
#include "control/ingress.hpp"
#include "control/mock_adapter.hpp"
#include "control_home/volume.hpp"
#include "rtcp_client/client.hpp"

namespace control_home {
class RtcpDevice final:public control::Adapter{
  rtcp_client::Client&client_;control::Core*core_=nullptr;control::ingress::Boundary*boundary_=nullptr;
  control::Id binding_;control::MockAdapter codec_;control::Optional<control::AdapterEvidence>pending_;
  control::U64 observations_=0;
  std::uint64_t dispatches_=0;
  control::Evidence evidence(const char*,const char*)const;
  void availability(control::Availability,control::U64);
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
  bool poll(control::U64);
  bool pending()const{return pending_.present;}
  std::uint64_t dispatches()const{return dispatches_;}
};
}
