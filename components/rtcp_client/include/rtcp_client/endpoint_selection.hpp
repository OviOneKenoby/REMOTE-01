#pragma once

#include "association.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace rtcp_client {

struct Endpoint {
  std::array<std::uint8_t,4> ipv4{};
  std::uint16_t port=0;
  friend bool operator==(const Endpoint&,const Endpoint&)=default;
};

inline constexpr std::size_t discovery_candidate_limit=4;

class EndpointSelection {
  std::array<Endpoint,discovery_candidate_limit> candidates_{};
  Endpoint trusted_{};
  std::uint8_t count_=0,index_=0;
  bool trusted_present_=false,current_authenticated_=false;
public:
  void clear(){candidates_={};trusted_={};count_=index_=0;trusted_present_=current_authenticated_=false;}
  void replace(const Endpoint*values,std::size_t count){
    candidates_={};count_=index_=0;current_authenticated_=false;
    for(std::size_t i=0;i<count&&count_<candidates_.size();++i){
      if(!values[i].port)continue;
      std::uint8_t any=0;
      for(auto b:values[i].ipv4)any|=b;
      if(!any)continue;
      bool duplicate=false;
      for(std::uint8_t j=0;j<count_;++j)duplicate|=candidates_[j]==values[i];
      if(!duplicate)candidates_[count_++]=values[i];
    }
  }
  Endpoint current(const Association&fallback)const{
    if(trusted_present_)return trusted_;
    if(index_<count_)return candidates_[index_];
    return {fallback.ipv4,fallback.port};
  }
  void begin_attempt(){current_authenticated_=false;}
  void failed_before_authentication(){
    if(current_authenticated_)return;
    if(trusted_present_){trusted_={};trusted_present_=false;return;}
    if(index_<count_)++index_;
  }
  void authenticated(const Association&fallback){trusted_=current(fallback);trusted_present_=true;current_authenticated_=true;candidates_={};count_=index_=0;}
  bool using_discovered()const{return !trusted_present_&&index_<count_;}
  std::size_t candidate_count()const{return count_;}
};

} // namespace rtcp_client
