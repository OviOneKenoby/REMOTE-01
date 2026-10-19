#pragma once
#include <array>
#include <cstdint>
#include <limits>

namespace rtcp_client {
enum class DiagnosticStage : std::uint8_t {
  disabled, disconnected, tcp_connecting, tcp_connected, client_hello_sent,
  server_hello_receiving, server_hello_received, server_frame_valid,
  server_proof_valid, client_finish_sent, identity_request_sent,
  identity_response_received, identity_accepted, complete
};
enum class DiagnosticFailure : std::uint8_t {
  none, connect, timeout, eof, short_read, invalid_frame, proof,
  send, receive, crypto, identity
};
enum class CloseReason : std::uint8_t {
  none, suspend, network_lost, connect_failed, connect_timeout,
  response_timeout, peer_eof, short_read, invalid_frame, proof_failed,
  send_failed, receive_failed, crypto_failed, unexpected_record,
  queue_failed
};
enum class DiagnosticCounter : std::size_t {
  attempts, tcp_connected, client_hello_sent, server_hello_received,
  server_frame_valid, server_proof_valid, client_finish_sent,
  identity_request_sent, identity_response_received, identity_accepted,
  failures, count
};
struct Diagnostic {
  DiagnosticStage stage = DiagnosticStage::disabled;
  DiagnosticFailure failure = DiagnosticFailure::none;
  std::int32_t error = 0;
  std::uint16_t server_hello_bytes = 0;
  std::array<std::uint32_t, static_cast<std::size_t>(DiagnosticCounter::count)> counters{};
  CloseReason close_reason = CloseReason::none;
  std::uint32_t close_count = 0;
  std::uint64_t previous_poll_ms = std::numeric_limits<std::uint64_t>::max();
  std::uint32_t last_poll_interval_ms = 0;
  std::uint32_t max_poll_interval_ms = 0;
  std::uint32_t last_response_timeout_elapsed_ms = 0;
  std::int32_t last_response_timeout_queued_bytes = -1;
  std::uint32_t response_timeouts_with_unread = 0;
  std::uint8_t last_response_timeout_state = 0;
  void polled(std::uint64_t now_ms) {
    if(previous_poll_ms!=std::numeric_limits<std::uint64_t>::max() && now_ms>=previous_poll_ms) {
      const auto delta=now_ms-previous_poll_ms;
      last_poll_interval_ms=delta>std::numeric_limits<std::uint32_t>::max()?std::numeric_limits<std::uint32_t>::max():std::uint32_t(delta);
      if(last_poll_interval_ms>max_poll_interval_ms) max_poll_interval_ms=last_poll_interval_ms;
    }
    previous_poll_ms=now_ms;
  }
  void response_timeout(std::uint8_t state,std::uint64_t elapsed_ms,std::int32_t queued_bytes) {
    last_response_timeout_state=state;
    last_response_timeout_elapsed_ms=elapsed_ms>std::numeric_limits<std::uint32_t>::max()?std::numeric_limits<std::uint32_t>::max():std::uint32_t(elapsed_ms);
    last_response_timeout_queued_bytes=queued_bytes;
    if(queued_bytes>0 && response_timeouts_with_unread!=std::numeric_limits<std::uint32_t>::max()) ++response_timeouts_with_unread;
  }
  void mark(DiagnosticStage s, DiagnosticCounter c) {
    stage = s; failure = DiagnosticFailure::none; error = 0;
    auto &v = counters[static_cast<std::size_t>(c)];
    if (v != std::numeric_limits<std::uint32_t>::max()) ++v;
  }
  void fail(DiagnosticFailure f, std::int32_t e = 0) {
    failure = f; error = e;
    auto &v = counters[static_cast<std::size_t>(DiagnosticCounter::failures)];
    if (v != std::numeric_limits<std::uint32_t>::max()) ++v;
  }
  void closed(CloseReason reason, std::int32_t e = 0) {
    close_reason = reason; error = e;
    if (close_count != std::numeric_limits<std::uint32_t>::max()) ++close_count;
  }
};
} // namespace rtcp_client
