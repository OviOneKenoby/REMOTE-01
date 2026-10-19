from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
generated = (root / "components/ui_bsp/generated/setup_scr_screen.c").read_text(encoding="utf-8")

assert "setUpduty(0xff - lv_slider_get_value(slider))" in source
assert "lv_obj_add_event_cb(src_ui.screen_slider_1, lvgl_obj_event_callback" in source
assert "volume_slider" not in source
assert 'lv_label_set_text(src_ui.screen_label_22, "TUNER VOLUME")' not in source
assert 'lv_label_set_text(ui->screen_label_22, "The slider controls the backlight\\n")' in generated
assert "home_text(home.volume, volume)" in source
assert "lv_obj_set_width(home.level, width)" in source
assert "if (!control_home::read(s, home.generation)) return" in source
assert '"%u>%u"' in source
assert "s.desired_volume_known" in source and "s.desired_volume" in source
assert "lv_timer_create(home_refresh, 20, nullptr)" in source

print("UI VOLUME BINDING SUMMARY PASS=1 FAIL=0 assertions=11")
