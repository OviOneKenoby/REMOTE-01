#pragma once
#include "development.hpp"
#include <array>

namespace control_home::development {
// Called only by the existing serialized owner. No replay or Mock command is
// synthesized by power management; preserve the current capability semantics.
struct PowerSuspend {
  std::array<control::Availability, control::Limits::capabilities> availability{};
  unsigned count = 0;
  control::Reachability reachability = control::Reachability::UNKNOWN;
  bool parked = false, attached = false;
  bool park(Session &s, control::U64 now, bool shutdown = false) {
    if (parked) return true;
    if (!shutdown) {
      if (s.pending()) return false;
      for (const auto &e : s.store.effects) if (!e.resolved()) return false;
    }
    const auto *t = s.core.target(s.d, s.t);
    const auto *b = s.core.binding(s.b);
    if (!t || !b) return false;
    count = 0; reachability = b->reachability;
    for (const auto &cap : t->capabilities) availability[count++] = cap.availability;
    // Close the Phase D inbox and invalidate observations. Reattach creates
    // a new incarnation/session; held pre-sleep events cannot enter it.
    s.suspend();
    attached = false; parked = s.boundary.shutdown(now);
    return parked;
  }
  bool resume(Session &s, control::U64 now) {
    if (!parked) return true;
    if (!attached) {
      if (!s.boundary.attach(s.b, now)) return false;
      attached = true;
    }
    if (!s.boundary.reachability(reachability, now)) return false;
    auto *t = s.core.target(s.d, s.t);
    if (!t || t->capabilities.size() != count) return false;
    for (unsigned i = 0; i < count; ++i) {
      auto cap = t->capabilities[i]; cap.availability = availability[i];
      if (!s.boundary.capability(std::move(cap), now)) return false;
    }
    if (reachability == control::Reachability::REACHABLE) s.acquire(now);
    parked = false;
    return true;
  }
};
} // namespace control_home::development
