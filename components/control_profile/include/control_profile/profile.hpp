#pragma once
#include "control/mock_adapter.hpp"

namespace control_profile {
using namespace control;
// Development-only local runtime; never a real-device authorization journal.
struct Session {
  Store store;
  MockJournal journal;
  MockAdapter mock;
  Id device{"mock.profile.device"}, target{"mock.profile.target"};
  Id primary{"mock.profile.binding.1"}, secondary{"mock.profile.binding.2"};
  Core core;
  explicit Session(Id epoch) : core(store, journal, std::move(epoch)) {}
};
struct Clock {
  virtual ~Clock() = default;
  virtual U64 now() = 0;
  virtual bool wait_until(U64) = 0;
  virtual bool is_owner() const = 0;
};
struct Reporter {
  virtual ~Reporter() = default;
  virtual void checkpoint(const char *, unsigned iterations,
                          const Session &) = 0;
  virtual void failure(const char *) = 0;
};
inline constexpr unsigned waves = 2, iterations_per_wave = 16;
// Clock uses real monotonic elapsed time on ESP32, virtual time in host tests.
// Caller owns construction/destruction and allocator observation on this owner.
bool exercise(Session &, Clock &, Reporter &);
} // namespace control_profile
