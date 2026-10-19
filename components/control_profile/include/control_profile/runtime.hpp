#pragma once

namespace control_profile {
// Development-only Phase A boot profile. No HOME facade or interaction.
// Call once from app_main after LVGL initialization/unlocking; returns promptly.
void start();
}
