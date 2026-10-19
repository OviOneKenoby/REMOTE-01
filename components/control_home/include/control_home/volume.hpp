#pragma once
#include "control/model.hpp"
#include <numeric>

namespace control_home {
inline constexpr control::Rational volume_rational(std::uint32_t percent) {
  const auto divisor = std::gcd<std::uint64_t>(percent, 100);
  return {static_cast<std::int64_t>(percent / divisor), 100 / divisor};
}
} // namespace control_home
