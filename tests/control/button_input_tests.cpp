#include "button_inputs.hpp"
#include <cstdio>

namespace {
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)

struct Capture {
  remote_buttons::Id values[8]{};
  unsigned count = 0;
  void operator()(remote_buttons::Id value) { values[count++] = value; }
};

bool independent_identities() {
  remote_buttons::Router router;
  Capture capture;
  router.sample(0, 0, [&](auto id) { capture(id); });
  std::uint64_t now = 1;
  for (unsigned id = 0; id < 4; ++id) {
    const auto mask = std::uint8_t(1u << id);
    router.sample(now, mask, [&](auto value) { capture(value); });
    router.sample(now + 20, mask, [&](auto value) { capture(value); });
    router.sample(now + 21, 0, [&](auto value) { capture(value); });
    router.sample(now + 41, 0, [&](auto value) { capture(value); });
    now += 50;
  }
  CHECK(capture.count == 4);
  for (unsigned id = 0; id < 4; ++id)
    CHECK(capture.values[id] == static_cast<remote_buttons::Id>(id));
  return true;
}

bool debounce_and_no_hold_repeat() {
  remote_buttons::Router router;
  Capture capture;
  router.sample(0, 0, [&](auto id) { capture(id); });
  for (std::uint64_t t = 1; t < 20; ++t)
    router.sample(t, std::uint8_t(t & 1), [&](auto id) { capture(id); });
  CHECK(capture.count == 0);
  router.sample(20, 1, [&](auto id) { capture(id); });
  router.sample(40, 1, [&](auto id) { capture(id); });
  for (std::uint64_t t = 41; t < 500; ++t)
    router.sample(t, 1, [&](auto id) { capture(id); });
  CHECK(capture.count == 1 && capture.values[0] == remote_buttons::Id::ENCODER_KEY);
  router.sample(500, 0, [&](auto id) { capture(id); });
  router.sample(520, 0, [&](auto id) { capture(id); });
  router.sample(521, 1, [&](auto id) { capture(id); });
  router.sample(541, 1, [&](auto id) { capture(id); });
  CHECK(capture.count == 2);
  return true;
}

bool boot_held_is_silent() {
  remote_buttons::Router router;
  Capture capture;
  router.sample(0, 15, [&](auto id) { capture(id); });
  router.sample(100, 15, [&](auto id) { capture(id); });
  CHECK(capture.count == 0);
  router.sample(101, 0, [&](auto id) { capture(id); });
  router.sample(121, 0, [&](auto id) { capture(id); });
  CHECK(capture.count == 0);
  return true;
}
}

int main() {
  const bool results[] = {independent_identities(), debounce_and_no_hold_repeat(),
                          boot_held_is_silent()};
  unsigned pass = 0;
  for (bool result : results) pass += result;
  std::printf("BUTTON INPUT SUMMARY PASS=%u FAIL=%u assertions=%u router_bytes=%u\n",
              pass, 3 - pass, assertions, unsigned(sizeof(remote_buttons::Router)));
  return pass == 3 ? 0 : 1;
}
