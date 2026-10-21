"""Source-boundary checks for the frozen D-009 HOME integration."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
client = (root / "components/rtcp_client/client.cpp").read_text(encoding="utf-8")
adapter = (root / "components/control_home/rtcp_device.cpp").read_text(encoding="utf-8")
facade = (root / "components/control_home/facade.cpp").read_text(encoding="utf-8")
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")

checks = {
    "snapshot_then_now_playing": "now_playing_due_=capabilities_.d009()&&capabilities_.now_playing" in client,
    "one_in_flight": "pending_request_||awaiting_" in client,
    "no_buffer_growth": "std::array<std::uint8_t,556>rx_{},tx_{}" in (root / "components/rtcp_client/include/rtcp_client/client.hpp").read_text(encoding="utf-8"),
    "fixed_home_page": "lv_carousel_add_element(src_ui.screen_carousel_1, 2)" in ui,
    "existing_controls": "static_cast<control_home::Action>" in ui and "playback_symbols" in ui,
    "existing_text_widgets": "home_text(home.title" in ui and "home_text(home.subtitle" in ui,
    "lvgl_owner_timer": "lv_timer_create(home_refresh" in ui,
    "playback_precondition": "intent.action == Action::TOGGLE || intent.action == Action::PREVIOUS" in facade,
    "media_generation_fence": "media_fence_.generation" in adapter,
    "disconnect_clears_pending": "media_fence_.reset(); pending_ = {}; availability(Availability::OFFLINE" in adapter,
    "d009_capability_gate": "const bool enabled = c.d009()" in adapter,
    "physical_buttons_unassigned": "intentionally unassigned" in ui,
}
failed = [name for name, ok in checks.items() if not ok]
print(f"D009_POLICY: PASS={len(checks)-len(failed)} FAIL={len(failed)} assertions={len(checks)}")
if failed:
    print("FAILED:", ", ".join(failed))
    raise SystemExit(1)
