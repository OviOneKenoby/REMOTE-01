#include "ota_service/model.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace ota_service;

static void authentication() {
  Policy policy;
  assert(policy.challenge(100));
  for (unsigned i = 0; i < maximum_auth_failures - 1; ++i)
    assert(policy.authenticate(101 + i, false) == AuthResult::REFUSED);
  assert(policy.authenticate(110, true) == AuthResult::ACCEPTED);
  assert(!policy.begin_upload(111, false));
  assert(policy.begin_upload(111, true));
  assert(!policy.begin_upload(111, true));
  assert(!policy.challenge(112));
  policy.finish(true);
  assert(policy.state == State::VERIFIED);
  assert(!policy.challenge(113));
  policy.revoke();
  assert(policy.state == State::IDLE);

  assert(policy.challenge(1000));
  for (unsigned i = 0; i < maximum_auth_failures - 1; ++i)
    assert(policy.authenticate(1001 + i, false) == AuthResult::REFUSED);
  assert(policy.authenticate(1010, false) == AuthResult::LOCKED);
  assert(policy.state == State::IDLE);

  assert(policy.challenge(2000));
  assert(policy.authenticate(2000 + challenge_lifetime_ms + 1, true) ==
         AuthResult::EXPIRED);

  assert(policy.challenge(3000));
  assert(policy.authenticate(3001, true) == AuthResult::ACCEPTED);
  assert(policy.begin_upload(3002, true));
  policy.finish(false);
  assert(policy.state == State::FAILED);
  assert(policy.challenge(3003));  // a failed transfer can be retried explicitly
}

static void encoding() {
  unsigned char value[4]{};
  assert(decode_hex("00aA10ff", value, sizeof value));
  const unsigned char wanted[]{0x00, 0xaa, 0x10, 0xff};
  assert(constant_time_equal(value, wanted, sizeof value));
  assert(!decode_hex("00xx10ff", value, sizeof value));
  assert(!decode_hex("00aa", value, sizeof value));
}

static void image_identity() {
  std::array<unsigned char, image_prefix_bytes> image{};
  image[0] = 0xe9;
  image[1] = 5;
  image[12] = 0x09;
  image[32] = 0x32;
  image[33] = 0x54;
  image[34] = 0xcd;
  image[35] = 0xab;
  std::memcpy(image.data() + 80, "REMOTE01", 9);
  assert(inspect_image_prefix(image.data(), image.size()).compatible());
  image[12] = 0;
  assert(!inspect_image_prefix(image.data(), image.size()).chip);
  image[12] = 9;
  std::memcpy(image.data() + 80, "TUNER01", 8);
  assert(!inspect_image_prefix(image.data(), image.size()).project);
  assert(!inspect_image_prefix(image.data(), 100).compatible());
}

static void limits() {
  assert(upload_size_allowed(2600000, 0x300000));
  assert(!upload_size_allowed(0, 0x300000));
  assert(!upload_size_allowed(80, 0x300000));
  assert(!upload_size_allowed(0x300001, 0x300000));
  assert(!upload_size_allowed(0x300000, 0x200000));
}

int main() {
  authentication();
  encoding();
  image_identity();
  limits();
  std::puts("LOCAL OTA MODEL: PASS");
}
