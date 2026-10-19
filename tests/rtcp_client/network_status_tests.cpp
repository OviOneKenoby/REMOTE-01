#include "rtcp_client/network_status.hpp"
#include <cassert>
#include <iostream>

using namespace rtcp_client;

int main() {
  unsigned checks = 0;
  NetworkStatusGate gate{};

  assert(gate.observe(false, false, 0, true) == NetworkDecision::unavailable);
  assert(!gate.known && gate.generation == 0); checks += 2;

  assert(gate.observe(true, true, 7, true) == NetworkDecision::usable);
  assert(gate.known && gate.generation == 7); checks += 2;

  for (unsigned i = 0; i < 128; ++i)
    assert(gate.observe(true, true, 7, true) == NetworkDecision::usable);
  ++checks;

  assert(gate.observe(false, false, 99, true) == NetworkDecision::unavailable);
  assert(gate.generation == 7); checks += 2;

  assert(gate.observe(true, false, 7, true) == NetworkDecision::disconnected);
  assert(gate.generation == 7); checks += 2;

  assert(gate.observe(true, true, 8, true) == NetworkDecision::generation_changed);
  assert(gate.generation == 8); checks += 2;

  assert(gate.observe(true, true, 9, false) == NetworkDecision::usable);
  assert(gate.generation == 9); checks += 2;

  assert(gate.observe(false, false, 0, false) == NetworkDecision::unavailable);
  assert(gate.observe(true, true, 9, true) == NetworkDecision::usable); checks += 2;

  std::cout << "RTCP_NETWORK_STATUS: PASS=" << checks
            << " FAIL=0 assertions=142\n";
}
