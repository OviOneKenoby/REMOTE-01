#pragma once

#include "endpoint_selection.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace rtcp_client {

struct DiscoveryBatch {
  std::array<Endpoint,discovery_candidate_limit> endpoints{};
  std::uint8_t count=0;
  std::int32_t error=0;
};

inline constexpr std::uint32_t discovery_query_timeout_ms=750;
inline constexpr std::uint32_t discovery_query_interval_ms=5000;
inline constexpr std::size_t discovery_worker_stack_bytes=6144;

bool discovery_initialize();
void discovery_set_network(bool available);
bool discovery_request();
bool discovery_take(DiscoveryBatch&out);

} // namespace rtcp_client
