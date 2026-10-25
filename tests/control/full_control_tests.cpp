#include "rtcp_client/wire.hpp"

#include <array>
#include <cstdio>

namespace {
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d expr=%s\n", __LINE__, #x); return 1; } } while (0)

void p64(std::uint8_t *p, std::uint64_t value) {
  for (int i = 7; i >= 0; --i) { p[i] = std::uint8_t(value); value >>= 8; }
}
}

int main() {
  using namespace rtcp_client::wire;
  std::array<std::uint8_t, 32> bytes{};
  CHECK(full_control_capability_request(bytes) == 1 && bytes[0] == 16);
  CHECK(source_state_request(bytes) == 1 && bytes[0] == 18);
  CHECK(source_select(0x0102030405060708ULL, Source::bluetooth, 9, bytes) == 18);
  const std::array<std::uint8_t, 18> expected{
      20,1,2,3,4,5,6,7,8,2,0,0,0,0,0,0,0,9};
  CHECK(std::equal(expected.begin(), expected.end(), bytes.begin()));
  CHECK(!source_select(1, Source::none, 9, bytes));
  CHECK(!source_select(1, Source::radio, 0, bytes));

  const std::array<std::uint8_t, 20> capabilities{
      17,1,0,0,0,0,0,1,7,0xd0,0,0,0,0,0,0,0,0,0,1};
  FullControlCapabilities capability{};
  CHECK(parse(capabilities, capability));
  CHECK(capability.negotiated && capability.source_switch() &&
        capability.freshness_ms == 2000 && capability.revision == 1);
  auto invalid_capability = capabilities;
  invalid_capability[7] = 2;
  CHECK(!parse(invalid_capability, capability));

  std::array<std::uint8_t, 24> state{};
  state[0] = 19; state[1] = 1; state[2] = 2; state[3] = 0; state[4] = 1;
  p64(state.data() + 8, 11); p64(state.data() + 16, 7);
  SourceState value{};
  CHECK(parse(state, value));
  CHECK(value.source == Source::bluetooth && value.bluetooth_connected &&
        value.sequence == 11 && value.generation == 7);
  state[2] = 1;
  CHECK(!parse(state, value)); // radio cannot claim a Bluetooth connection
  state[4] = 0;
  CHECK(parse(state, value) && value.source == Source::radio);

  const std::array<std::uint8_t,16> discovery{
      12,1,0,0x3f,96,127,7,0xd0,0,0,0,0,0,0,0,1};
  BtxCapabilities btx{};
  CHECK(parse(discovery,btx));
  auto reserved = discovery; reserved[3] |= 0x80;
  CHECK(!parse(reserved,btx));
  const std::array<std::uint8_t,15> identity_bytes{
      2,1,1,0,10,'1','.','1','.','1','2','+','f','c','1'};
  Identity identity{};
  CHECK(parse(identity_bytes,identity) && identity.full_control_advertised());
  auto legacy_identity=identity_bytes;legacy_identity[4]=6;
  CHECK(parse({legacy_identity.data(),11},identity)&&!identity.full_control_advertised());
  std::printf("FULL_CONTROL: PASS=1 FAIL=0 assertions=%u\n", assertions);
  return 0;
}
