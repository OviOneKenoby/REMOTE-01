"""Production wiring policy for encoder activity and SYSTEM_SLEEP limits."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
user = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
router = (root / "components/user_app/encoder_volume.hpp").read_text(encoding="utf-8")
api = (root / "components/power_management/include/power_management/api.hpp").read_text(encoding="utf-8")
runtime = (root / "components/power_management/runtime.cpp").read_text(encoding="utf-8")

checks = {
    "valid_cycle_only": "if (event.kind == physical_input::Kind::ENCODER_CYCLE)" in router
    and "activity();" in router,
    "production_routes_to_power": "[] { power_management::activity(); }" in user,
    "thread_safe_counter": "void activity() { physical_activity.fetch_add(1); }" in runtime,
    "touch_path_preserved": "if (pressed) activity();" in runtime,
    "power_owner_refreshes_idle": "if (activity != seen_activity)" in runtime
    and "else machine.activity(now);" in runtime,
    "display_idle_wake_preserved": "machine.wake(now, Wake::TOUCH)" in runtime,
    "no_encoder_lvgl_call": "lv_" not in router,
    "no_new_encoder_wake_source": "esp_sleep_enable" not in router
    and "esp_sleep_enable" not in api,
    "bounded_no_allocation": "new " not in router and "malloc" not in router,
}

failed = [name for name, ok in checks.items() if not ok]
print(f"ENCODER_ACTIVITY_POLICY: PASS={len(checks)-len(failed)} "
      f"FAIL={len(failed)} assertions={len(checks)}")
if failed:
    print("FAILED:", ", ".join(failed))
    raise SystemExit(1)
