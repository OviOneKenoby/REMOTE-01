#pragma once
namespace power_management {
// IDF GPIO wake setup changes the awake interrupt type as well. Level wake
// must only be armed with the ISR masked; restore edge before re-enabling it.
enum class WakeCycle { OK, MASK_FAILED, ARM_FAILED, SLEEP_FAILED, RESTORE_FAILED };
template<class Port> WakeCycle level_wake_cycle(Port &p) {
  if (!p.mask()) return WakeCycle::MASK_FAILED;
  const bool armed = p.arm();
  const bool slept = armed && p.sleep();
  const bool disarmed = p.disarm();
  const bool edge = p.edge();
  const bool restored = disarmed && edge && p.unmask();
  if (!restored) return WakeCycle::RESTORE_FAILED;
  if (!armed) return WakeCycle::ARM_FAILED;
  return slept ? WakeCycle::OK : WakeCycle::SLEEP_FAILED;
}
}
