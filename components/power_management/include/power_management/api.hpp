#pragma once
#include <cstdint>
#include "power_management/external_power.hpp"

// Neutral infrastructure values; no Core, LVGL, adapter or hardware handles.
namespace power_management {
enum class Request { NONE, SLEEP, SHUTDOWN };
struct BatteryView { bool valid = false, simulated = false; unsigned millivolts = 0; };
bool touch(bool pressed);
// Thread-safe physical input activity. It refreshes/wakes display-idle through
// the same POWER-owned counter as touch, but adds no SYSTEM_SLEEP wake source.
void activity();
bool commands_allowed();
BatteryView battery_view();
ExternalView external_view();
bool development_key(unsigned char);
bool development_command(unsigned char);
Request owner_request();
void owner_ack(bool parked, bool refused = false);
bool ui_pause_requested();
void ui_ack(bool parked);
// app_main passes an already initialized board expander through hardware.cpp.
void start();
} // namespace power_management
