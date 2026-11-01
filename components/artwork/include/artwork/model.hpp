#pragma once

#include <cstdint>

namespace artwork {

enum class Source : std::uint8_t { neutral = 0, radio = 1, bluetooth = 2 };
enum class Presentation : std::uint8_t { image = 0, text_fallback = 1 };

constexpr Presentation presentation(bool surface_ready) {
  return surface_ready ? Presentation::image : Presentation::text_fallback;
}

// Full Control uses the same stable values for UNKNOWN/RADIO/BLUETOOTH. Keep
// this boundary protocol-neutral so the renderer never reads RTCP objects.
constexpr Source authoritative_source(bool connected, std::uint8_t source) {
  if (!connected) return Source::neutral;
  return source == std::uint8_t(Source::radio)
             ? Source::radio
             : source == std::uint8_t(Source::bluetooth)
                   ? Source::bluetooth
                   : Source::neutral;
}

struct Model {
  Source source{Source::neutral};
  std::uint32_t revision{1};

  bool observe(bool connected, std::uint8_t authoritative) {
    const auto next = authoritative_source(connected, authoritative);
    if (next == source) return false;
    source = next;
    if (++revision == 0) revision = 1;
    return true;
  }
};

} // namespace artwork
