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
struct ClientTestAccess;
class Client {
public:
  enum class State{disabled,disconnected,connecting,server_hello,authenticated,identity,capabilities,btx_probe,full_control,ready};
  enum class Event{none,connected,disconnected,identity,negotiation,capabilities,full_control,snapshot,source_state,menu_state,catalog,browser,now_playing,btx_state,result};
  bool initialize();
  void suspend();
  Event poll(std::uint64_t now_ms,bool command_waiting=false);
  bool set_volume(std::uint64_t request,std::uint32_t numerator,std::uint32_t denominator);
  bool playback(std::uint64_t request,wire::PlaybackOperation,std::uint8_t expected_state,
                std::uint64_t media_generation);
  bool bluetooth(std::uint64_t request,wire::PlaybackOperation,wire::BtxPlayback expected_state,
                 std::uint64_t source_generation);
  bool source_select(std::uint64_t request,wire::Source,std::uint64_t source_generation);
  bool catalog_query(wire::CatalogCollection,std::uint16_t offset,std::uint8_t limit);
  bool catalog_command(std::uint64_t request,wire::CatalogOperation,
                       wire::CatalogCollection,std::uint16_t token,
                       std::uint64_t revision);
  bool browser_state(wire::BrowserStage,std::uint16_t offset,std::uint8_t limit,
                     std::uint8_t filter,std::uint64_t generation);
  bool browser_command(std::uint64_t request,wire::BrowserOperation,
                       std::uint16_t token,std::uint64_t generation,
                       std::uint64_t revision);
  bool command_ready()const{return state_==State::ready&&!pending_request_&&!awaiting_&&!tx_used_;}
  State state()const{return state_;} const wire::Identity&identity()const{return identity_;}
  const wire::Capabilities&capabilities()const{return capabilities_;}
  const wire::BtxCapabilities&btx_capabilities()const{return btx_capabilities_;}
  const wire::FullControlCapabilities&full_control_capabilities()const{return full_control_capabilities_;}
  const wire::SourceState&source_state()const{return source_state_;}
  const wire::MenuState&menu_state()const{return menu_state_;}
  const wire::CatalogPage&catalog_page()const{return catalog_page_;}
  const wire::BrowserPage&browser_page()const{return browser_page_;}
  const wire::Snapshot&snapshot()const{return snapshot_;} const wire::Result&result()const{return result_;}
  wire::NowPlaying&now_playing(){return now_playing_;}
  const wire::NowPlaying&now_playing()const{return now_playing_;}
  std::uint64_t session_generation()const{return association_.session_generation;}
  const Diagnostic&diagnostic()const{return diagnostic_;}
  static constexpr std::size_t static_buffer_bytes(){return 556*2;}
private:
  friend struct ClientTestAccess;
  enum class Awaited:std::uint8_t{none,identity,capabilities,btx_capabilities,full_control_capabilities,snapshot,source_state,menu_state,catalog,browser,now_playing,btx_state,result};
  Association association_{};State state_{State::disabled};int socket_{-1};std::uint64_t retry_at_{0},snapshot_at_{0},deadline_{0};bool awaiting_{false};
  std::array<std::uint8_t,556>rx_{},tx_{};std::size_t rx_used_{0},rx_need_{56},tx_used_{0},tx_sent_{0};
  std::array<std::uint8_t,40>ch_{};std::array<std::uint8_t,56>sh_{};std::array<std::uint8_t,16>cn_{},sn_{};
  std::array<std::uint8_t,16>c2s_key_{},s2c_key_{};std::array<std::uint8_t,4>c2s_prefix_{},s2c_prefix_{};
  std::array<std::uint8_t,8>session_id_{};std::uint64_t tx_counter_{1},rx_counter_{1},pending_request_{0};
  wire::Identity identity_{};wire::Capabilities capabilities_{};wire::BtxCapabilities btx_capabilities_{};wire::FullControlCapabilities full_control_capabilities_{};wire::SourceState source_state_{};wire::MenuState menu_state_{};wire::CatalogPage catalog_page_{};wire::BrowserPage browser_page_{};wire::Snapshot snapshot_{};wire::NowPlaying now_playing_{};wire::Result result_{};
  wire::BtxNegotiation btx_negotiation_{};
  Awaited awaited_{Awaited::none};bool source_state_due_{false},menu_state_due_{false},now_playing_due_{false},btx_state_due_{false};
  Diagnostic diagnostic_{};NetworkStatusGate network_status_{};
  EndpointSelection endpoints_{};std::uint64_t discovery_at_{0};
  DiscoveryBatch pending_discovery_{};bool pending_discovery_present_{false};bool discovery_ready_{false};
  void close(CloseReason,bool schedule=true,std::int32_t error=0);bool connect_start();bool flush();ReadStatus receive();bool make_hello();bool accept_server_hello();
  bool send_plain(const std::uint8_t*,std::size_t);Event accept_record();bool queue_next();
};
}
