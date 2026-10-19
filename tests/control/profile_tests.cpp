#include "control_profile/profile.hpp"
#include <cstdio>
#include <cstring>
using namespace control_profile;
namespace {
struct TestClock final : Clock {
  U64 time = 20, advance = 0;
  unsigned waits = 0;
  bool owner = true, fail_wait = false;
  U64 now() override { return time += advance; }
  bool is_owner() const override { return owner; }
  bool wait_until(U64 until) override {
    ++waits;
    if (fail_wait)
      return false;
    time = std::max(time, until);
    return true;
  }
};
struct TestReporter final : Reporter {
  unsigned checkpoints = 0, failures = 0, completed = 0, purges = 0;
  void checkpoint(const char *name, unsigned iterations,
                  const Session &) override {
    ++checkpoints;
    completed = iterations;
    if (std::strstr(name, "after_purge"))
      ++purges;
  }
  void failure(const char *why) override {
    ++failures;
    std::printf("EXPECTED_OR_ACTUAL_FAILURE %s\n", why);
  }
};
unsigned asserts = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++asserts;                                                                 \
    if (!(x)) {                                                                \
      std::printf("FAIL %d %s\n", __LINE__, #x);                               \
      return false;                                                            \
    }                                                                          \
  } while (0)
std::size_t observed_live = 0, observed_allocations = 0, observed_frees = 0,
            observed_failures = 0;
void observer(void *p, std::size_t n, bool allocated) {
  if (!p) {
    ++observed_failures;
    return;
  }
  if (allocated) {
    observed_live += n;
    ++observed_allocations;
  } else {
    observed_live -= n;
    ++observed_frees;
  }
}
bool complete_profile(bool advancing) {
  TestClock clock;
  clock.advance = advancing ? 1 : 0;
  TestReporter report;
  auto before = Memory::used;
  {
    Session session(Id("host.profile.epoch"));
    CHECK(exercise(session, clock, report));
    CHECK(report.completed == 32 && report.purges == 2 && !report.failures);
    CHECK(!session.store.blocked);
    CHECK(session.store.effects.size() == 0);
    CHECK(clock.time >= 120022);
    CHECK(session.mock.dispatches == 69);
    CHECK(session.store.targets[0].capabilities.size() == 28);
  }
  CHECK(Memory::used == before);
  return true;
}
bool nominal() { return complete_profile(false); }
bool advancing_clock() { return complete_profile(true); }
bool ownership_rejection() {
  TestClock clock;
  clock.owner = false;
  TestReporter report;
  Session s(Id("host.owner"));
  CHECK(!exercise(s, clock, report));
  CHECK(report.failures > 0);
  CHECK(s.store.devices.size() == 0);
  CHECK(s.mock.dispatches == 0);
  return true;
}
bool wait_rejection() {
  TestClock clock;
  clock.fail_wait = true;
  TestReporter report;
  Session s(Id("host.wait"));
  CHECK(!exercise(s, clock, report));
  CHECK(report.failures > 0);
  CHECK(report.completed == 0);
  return true;
}
bool placement_observation() {
  CHECK(Memory::observer == nullptr);
  Memory::observer = observer;
  bool ok = complete_profile(false);
  Memory::observer = nullptr;
  CHECK(ok);
  CHECK(observed_live == 0);
  CHECK(observed_allocations == observed_frees);
  CHECK(observed_failures == 0);
  CHECK(observed_allocations > 100);
  return true;
}
bool allocation_failure_observation() {
  auto limit = Memory::limit;
  Memory::observer = observer;
  Memory::limit = Memory::used;
  auto failures = observed_failures;
  auto *p = Memory::allocate(1);
  Memory::limit = limit;
  Memory::observer = nullptr;
  CHECK(!p);
  CHECK(observed_failures == failures + 1);
  CHECK(Memory::used == 0);
  return true;
}
} // namespace
int main() {
  struct Test {
    const char *name;
    bool (*run)();
  };
  const Test tests[] = {
      {"nominal", nominal},
      {"advancing_clock", advancing_clock},
      {"ownership_rejection", ownership_rejection},
      {"wait_rejection", wait_rejection},
      {"placement_observation", placement_observation},
      {"allocation_failure_observation", allocation_failure_observation}};
  unsigned passed = 0, failed = 0;
  for (const auto &t : tests) {
    auto before = Memory::used;
    bool ok = t.run();
    if (Memory::used != before)
      ok = false;
    std::printf("%s profile/%s\n", ok ? "PASS" : "FAIL", t.name);
    if (ok)
      ++passed;
    else
      ++failed;
  }
  std::printf("PROFILE RESULT %u PASS / %u FAIL; %u assertions; host virtual "
              "clock only\n",
              passed, failed, asserts);
  return failed ? 1 : 0;
}
