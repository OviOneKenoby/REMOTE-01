#pragma once

#include "rtcp_client/wire.hpp"
#include <cstring>
#include <span>

namespace control_home::btx {

inline std::size_t format_title(const rtcp_client::wire::NowPlaying &value,
                                std::span<char> output) {
  if (output.empty()) return 0;
  output[0] = 0;
  if (value.btx_source !=
      std::uint8_t(rtcp_client::wire::BtxSource::bluetooth))
    return 0;
  if (value.btx_connection ==
      std::uint8_t(rtcp_client::wire::BtxConnection::disconnected)) {
    constexpr char disconnected[] = "Phone disconnected";
    if (output.size() < sizeof disconnected) return 0;
    std::memcpy(output.data(), disconnected, sizeof disconnected);
    return sizeof disconnected - 1;
  }
  if (value.btx_connection !=
      std::uint8_t(rtcp_client::wire::BtxConnection::connected))
    return 0;
  const auto size = std::size_t(value.artist_size) +
                    (value.artist_size && value.title_size ? 3 : 0) +
                    value.title_size;
  if (output.size() <= size) return 0;
  std::size_t at = 0;
  if (value.artist_size) {
    std::memcpy(output.data(), value.artist.data(), value.artist_size);
    at += value.artist_size;
  }
  if (value.artist_size && value.title_size) {
    std::memcpy(output.data() + at, " - ", 3);
    at += 3;
  }
  if (value.title_size) {
    std::memcpy(output.data() + at, value.title.data(), value.title_size);
    at += value.title_size;
  }
  output[at] = 0;
  return at;
}

} // namespace control_home::btx
