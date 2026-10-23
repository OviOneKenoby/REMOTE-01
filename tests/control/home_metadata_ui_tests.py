"""Source and vendored-font bounds for the existing HOME metadata widgets."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
adapter = (root / "components/control_home/rtcp_device.cpp").read_text(encoding="utf-8")
wire = (root / "components/rtcp_client/wire.cpp").read_text(encoding="utf-8")

checks = {
    "station_large": 'home.title = remote01_single_line(page, "--", &lv_font_montserrat_20, text, 12, 262, 148,' in ui,
    "station_ping_pong": 'home.title = remote01_single_line' in ui and ui.count("LV_LABEL_LONG_SCROLL, LV_TEXT_ALIGN_LEFT);") >= 2,
    "streamtitle_small": 'home.subtitle = remote01_single_line(page, "--", &lv_font_montserrat_14, muted, 12, 290, 148,' in ui,
    "streamtitle_ping_pong": ui.count("LV_LABEL_LONG_SCROLL, LV_TEXT_ALIGN_LEFT);") >= 2,
    "bounded_speed": ui.count("lv_obj_set_style_anim_speed(home.") == 2 and ui.count(", 25, LV_PART_MAIN);") >= 2,
    "endpoint_pauses": all(token in ui for token in ("lv_anim_set_delay(animation, 1000)", "lv_anim_set_playback_delay(animation, 1000)", "lv_anim_set_repeat_delay(animation, 1000)")),
    "replacement_restarts": "lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);" in ui and "if (changed) lv_label_set_text(label, text);" in ui,
    "inactive_stops": "home_active && power_management::commands_allowed()" in ui,
    "physical_right_bound": 12 + 148 <= 172,
    "assignment_preserved": 'home_scrolling_text(home.title, s.subtitle[0] ? s.subtitle : "--"' in ui and 'home_scrolling_text(home.subtitle, s.title[0] ? s.title : "--"' in ui,
    "bounded_buffers": "char title[227]{}, subtitle[32]{}" in (root / "components/control_home/include/control_home/api.hpp").read_text(encoding="utf-8"),
    "utf8_validation": "bool text(" in wire and "!text(p.data()+at,n)" in wire,
    "lvgl_bidirectional_contract": "LV_LABEL_LONG_SCROLL,           /**< Keep the size and roll the text back and forth*/" in (root / "components/lvgl/src/widgets/lv_label.h").read_text(encoding="utf-8"),
    "lvgl_clipping_contract": "draw_ctx->clip_area = &txt_clip;" in (root / "components/lvgl/src/widgets/lv_label.c").read_text(encoding="utf-8"),
    "delete_releases_animations": "lv_anim_del(obj, NULL);" in (root / "components/lvgl/src/core/lv_obj.c").read_text(encoding="utf-8"),
}

failed = [name for name, ok in checks.items() if not ok]
print(f"HOME_METADATA_UI: PASS={len(checks)-len(failed)} FAIL={len(failed)} assertions={len(checks)}")
if failed:
    print("FAILED:", ", ".join(failed))
    raise SystemExit(1)
