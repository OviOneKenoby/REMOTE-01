#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace rtcp_client {
struct Association {
  std::array<std::uint8_t,4> ipv4{};
  std::uint16_t port=46411;
  std::array<std::uint8_t,16> pairing_id{};
  std::array<std::uint8_t,32> psk{};
  std::uint64_t session_generation=1;
};
inline constexpr std::size_t association_bytes=68;
bool encode(const Association&,std::array<std::uint8_t,association_bytes>&);
bool decode(std::span<const std::uint8_t>,Association&);
// Read-only. Never initializes, erases, commits, or logs remote_ctl data.
bool load(Association&);
// Reserves the next CCM session generation through the accepted A-010
// fence/record/root commit before any network session can be created.
bool reserve_session(Association&);
}
