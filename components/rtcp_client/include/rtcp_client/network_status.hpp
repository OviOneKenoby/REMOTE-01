#pragma once
#include <cstdint>

namespace rtcp_client {
enum class NetworkDecision : std::uint8_t {
  unavailable,
  usable,
  disconnected,
  generation_changed,
};

struct NetworkStatusGate {
  bool known = false;
  unsigned generation = 0;

  NetworkDecision observe(bool available, bool connected,
                          unsigned current_generation,
                          bool transport_active) {
    if (!available) return NetworkDecision::unavailable;
    if (!connected) {
      known = true;
      generation = current_generation;
      return NetworkDecision::disconnected;
    }
    if (!known) {
      known = true;
      generation = current_generation;
      return NetworkDecision::usable;
    }
    if (generation != current_generation) {
      generation = current_generation;
      return transport_active ? NetworkDecision::generation_changed
                              : NetworkDecision::usable;
    }
    return NetworkDecision::usable;
  }
};
}
