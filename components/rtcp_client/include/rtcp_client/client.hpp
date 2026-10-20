#pragma once
#include "association.hpp"
#include "diagnostic.hpp"
#include "io_status.hpp"
#include "mdns_discovery.hpp"
#include "network_status.hpp"
#include "wire.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace rtcp_client {
class Client {
public:
  enum class State{disabled,disconnected,connecting,server_hello,authenticated,identity,capabilities,ready};
  enum class Event{none,connected,disconnected,identity,capabilities,snapshot,result};
  bool initialize();
  void suspend();
  Event poll(std::uint64_t now_ms);
  bool set_volume(std::uint64_t request,std::uint32_t numerator,std::uint32_t denominator);
  State state()const{return state_;} const wire::Identity&identity()const{return identity_;}
  const wire::Capabilities&capabilities()const{return capabilities_;}
  const wire::Snapshot&snapshot()const{return snapshot_;} const wire::Result&result()const{return result_;}
  std::uint64_t session_generation()const{return association_.session_generation;}
  const Diagnostic&diagnostic()const{return diagnostic_;}
  static constexpr std::size_t static_buffer_bytes(){return 556*2;}
private:
  Association association_{};State state_{State::disabled};int socket_{-1};std::uint64_t retry_at_{0},snapshot_at_{0},deadline_{0};bool awaiting_{false};
  std::array<std::uint8_t,556>rx_{},tx_{};std::size_t rx_used_{0},rx_need_{56},tx_used_{0},tx_sent_{0};
  std::array<std::uint8_t,40>ch_{};std::array<std::uint8_t,56>sh_{};std::array<std::uint8_t,16>cn_{},sn_{};
  std::array<std::uint8_t,16>c2s_key_{},s2c_key_{};std::array<std::uint8_t,4>c2s_prefix_{},s2c_prefix_{};
  std::array<std::uint8_t,8>session_id_{};std::uint64_t tx_counter_{1},rx_counter_{1},pending_request_{0};
  wire::Identity identity_{};wire::Capabilities capabilities_{};wire::Snapshot snapshot_{};wire::Result result_{};
  Diagnostic diagnostic_{};NetworkStatusGate network_status_{};
  EndpointSelection endpoints_{};std::uint64_t discovery_at_{0};
  DiscoveryBatch pending_discovery_{};bool pending_discovery_present_{false};bool discovery_ready_{false};
  void close(CloseReason,bool schedule=true,std::int32_t error=0);bool connect_start();bool flush();ReadStatus receive();bool make_hello();bool accept_server_hello();
  bool send_plain(const std::uint8_t*,std::size_t);Event accept_record();bool queue_next();
};
}
