#include "rtcp_client/association.hpp"
#ifndef RTCP_HOST_TEST
#include "remote_persistence/backend.hpp"
#include "remote_persistence/esp_nvs_store.hpp"
#endif
#include <cstring>

namespace rtcp_client {
namespace {
void put16(std::uint8_t*p,std::uint16_t v){p[0]=v>>8;p[1]=v;}
void put64(std::uint8_t*p,std::uint64_t v){for(int i=7;i>=0;--i){p[i]=v;v>>=8;}}
std::uint16_t get16(const std::uint8_t*p){return std::uint16_t(p[0])<<8|p[1];}
std::uint64_t get64(const std::uint8_t*p){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v=(v<<8)|p[i];return v;}
}
bool encode(const Association&a,std::array<std::uint8_t,association_bytes>&o){
  if(!a.port||!a.session_generation)return false;
  o={};std::memcpy(o.data(),"A11A",4);o[4]=1;o[5]=0;put16(o.data()+6,a.port);
  std::memcpy(o.data()+8,a.ipv4.data(),4);std::memcpy(o.data()+12,a.pairing_id.data(),16);
  std::memcpy(o.data()+28,a.psk.data(),32);put64(o.data()+60,a.session_generation);return true;
}
bool decode(std::span<const std::uint8_t>p,Association&a){
  if(p.size()!=association_bytes||std::memcmp(p.data(),"A11A",4)||p[4]!=1||p[5])return false;
  Association n;n.port=get16(p.data()+6);std::memcpy(n.ipv4.data(),p.data()+8,4);
  std::memcpy(n.pairing_id.data(),p.data()+12,16);std::memcpy(n.psk.data(),p.data()+28,32);
  n.session_generation=get64(p.data()+60);
  std::uint8_t any_ip=0,any_id=0,any_key=0;for(auto v:n.ipv4)any_ip|=v;for(auto v:n.pairing_id)any_id|=v;for(auto v:n.psk)any_key|=v;
  if(!n.port||!n.session_generation||!any_ip||!any_id||!any_key)return false;
  a=n;return true;
}
bool load(Association&a){
#ifdef RTCP_HOST_TEST
  (void)a;return false;
#else
  remote_persistence::EspNvsStore store;
  if(store.open(remote_persistence::EspNvsStore::Access::read_only)!=remote_persistence::StoreResult::ok)return false;
  remote_persistence::Backend backend(store);auto recovered=backend.recover();
  return recovered.status==remote_persistence::Status::ok&&decode(recovered.payload,a);
#endif
}
bool reserve_session(Association&a){
#ifdef RTCP_HOST_TEST
  (void)a;return false;
#else
  if(a.session_generation==UINT64_MAX)return false;
  remote_persistence::EspNvsStore store;
  if(store.open(remote_persistence::EspNvsStore::Access::read_write)!=remote_persistence::StoreResult::ok)return false;
  remote_persistence::Backend backend(store);auto recovered=backend.recover();Association current;
  if(recovered.status!=remote_persistence::Status::ok||!decode(recovered.payload,current)||
     current.session_generation!=a.session_generation)return false;
  ++current.session_generation;std::array<std::uint8_t,association_bytes>payload{};
  if(!encode(current,payload)||backend.commit(recovered.next_generation,payload)!=remote_persistence::Status::ok)return false;
  a=current;return true;
#endif
}
}
