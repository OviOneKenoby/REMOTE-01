#include "rtcp_client/endpoint_selection.hpp"
#include <cassert>
#include <cstdio>

using namespace rtcp_client;

static Endpoint endpoint(unsigned last, unsigned port = 46411) {
  return {{192, 168, 1, static_cast<std::uint8_t>(last)},
          static_cast<std::uint16_t>(port)};
}

int main() {
  Association saved{}; saved.ipv4={192,168,1,152}; saved.port=46411;
  EndpointSelection selection;
  assert(selection.current(saved)==endpoint(152));

  const Endpoint advertised[]={endpoint(31),endpoint(31),endpoint(32),{},endpoint(33,0),endpoint(34)};
  selection.replace(advertised,6);
  assert(selection.candidate_count()==3 && selection.current(saved)==endpoint(31));

  selection.begin_attempt();selection.failed_before_authentication();
  assert(selection.current(saved)==endpoint(32));
  selection.begin_attempt();selection.failed_before_authentication();
  assert(selection.current(saved)==endpoint(34));

  selection.begin_attempt();selection.authenticated(saved);
  assert(selection.current(saved)==endpoint(34));
  selection.failed_before_authentication();
  assert(selection.current(saved)==endpoint(34));

  const Endpoint changed[]={endpoint(41),endpoint(42)};
  selection.replace(changed,2);
  assert(selection.current(saved)==endpoint(34));
  selection.begin_attempt();selection.failed_before_authentication();
  assert(selection.current(saved)==endpoint(41));

  selection.begin_attempt();selection.failed_before_authentication();
  assert(selection.current(saved)==endpoint(42));
  selection.begin_attempt();selection.authenticated(saved);
  selection.clear();
  assert(selection.current(saved)==endpoint(152));

  std::puts("RTCP DISCOVERY SELECTION 10/10 PASS");
}
