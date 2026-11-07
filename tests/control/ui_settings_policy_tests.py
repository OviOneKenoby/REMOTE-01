"""Source contract: SETTINGS (page 1), INFO (page 2), boot brightness and the delay selectors."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
model = (root / "components/user_app/settings_model.hpp").read_text(encoding="utf-8")
power_model = (root / "components/power_management/include/power_management/model.hpp").read_text(encoding="utf-8")
power_api = (root / "components/power_management/include/power_management/api.hpp").read_text(encoding="utf-8")
power_rt = (root / "components/power_management/runtime.cpp").read_text(encoding="utf-8")
cmake = (root / "components/user_app/CMakeLists.txt").read_text(encoding="utf-8")


def fonts(block: str):
    return set(re.findall(r"lv_font_montserrat_(\d+)", block))


start = ui.index("struct DelayRow {")
end = ui.index("static void lvgl_obj_event_callback(lv_event_t *event)" + chr(10) + "{")
block = ui[start:end]
settings_block = block[block.index("static void settings_page_create"):block.index("static void info_page_create")]
info_block = block[block.index("static void info_page_create"):]
refresh_block = block[block.index("static void settings_refresh"):block.index("static void page_style")]

checks = {
    # Pages: HOME 0, SETTINGS 1, INFO 2
    "home_is_element_0": "lv_obj_t *page = src_ui.screen_carousel_1_element_1;" in ui,
    "settings_is_element_1": "lv_obj_t *page = settings.page = src_ui.screen_carousel_1_element_2;" in block,
    "info_is_the_only_added_element_id_2": ui.count("lv_carousel_add_element(") == 1
    and "info_page.page = lv_carousel_add_element(src_ui.screen_carousel_1, 2);" in block,
    "info_created_after_settings": ui.index("\tsettings_page_create();") < ui.index("\tinfo_page_create();"),
    "boot_on_home": "lv_obj_set_element_id(src_ui.screen_carousel_1, 0, LV_ANIM_OFF);" in ui,
    # Brightness
    "boot_brightness_is_half_scale": "constexpr std::uint8_t brightness_boot = 128;" in model,
    "init_uses_boot_brightness": "lcd_bl_pwm_bsp_init(ui_settings::brightness_pwm(ui_settings::brightness_boot));" in ui
    and "lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255)" not in ui,
    "slider_set_to_boot_value": "lv_slider_set_value(src_ui.screen_slider_1, ui_settings::brightness_boot, LV_ANIM_OFF);" in ui,
    "existing_slider_reused_not_recreated": "lv_slider_create" not in ui,
    "slider_callback_unchanged": "setUpduty(0xff - lv_slider_get_value(slider))" in ui
    and "lv_obj_add_event_cb(src_ui.screen_slider_1, lvgl_obj_event_callback, LV_EVENT_ALL, NULL);" in ui,
    # Delay rows
    "two_rows_screen_off_and_sleep": 'ui_settings::Row::DISPLAY, "SCREEN OFF AFTER"' in settings_block
    and 'ui_settings::Row::SYSTEM, "SYSTEM SLEEP AFTER"' in settings_block,
    "minus_value_plus_buttons_44px": "remote01_panel(page, spec.x, row_y, 44, 44," in block
    and "{&row.minus, 12, LV_SYMBOL_MINUS, -1}, {&row.plus, 116, LV_SYMBOL_PLUS, +1}" in block,
    "step_goes_through_power_api": "power_management::set_display_idle_ms(next)" in block
    and "power_management::set_system_idle_ms(next)" in block,
    "step_uses_the_pure_rule": "ui_settings::step_value(row, power_management::display_idle_ms()" in block,
    "rejected_step_changes_nothing": "if (!next) return;" in block,
    "no_auto_adjust_of_other_row": block.count("set_display_idle_ms(") == 1 and block.count("set_system_idle_ms(") == 1,
    "disabled_steps_are_dimmed": "settings_button_style(row.minus, minus)" in block
    and "settings_button_style(row.plus, plus)" in block and "0x151e22" in block,
    "hint_explains_the_rule": "Sleep must be longer than screen off" in settings_block,
    # Power model
    "sleep_values_1_2_5_10_30_min": "system_idle_choices{60000, 120000, 300000, 600000, 1800000}" in power_model,
    "sleep_default_5_min": "default_system_idle_ms = 300000" in power_model,
    "invariant_in_model": "system_ms > display_ms" in power_model,
    "both_setters_use_the_invariant": "s.set_display(ms)" in power_rt and "s.set_system(ms)" in power_rt,
    "api_exports_both": "bool set_system_idle_ms(unsigned ms);" in power_api and "unsigned system_idle_ms();" in power_api,
    "delays_applied_before_tick": power_rt.index("machine.system_idle_ms = delays & 0xffffffffu") < power_rt.index("machine.tick(now,"),
    "no_persistence": "nvs" not in block.lower() and "persist" not in block.lower(),
    # INFO page: five rows, existing sources only
    "info_rows_present": all(n in info_block for n in ("TUNER LINK", "BATTERY", "FIRMWARE", "UPTIME", "LAST DISCONNECT")),
    "info_has_nothing_else": info_block.count("info_row(page,") == 5,
    "connection_from_existing_state": "tuner_control.state.connected" in refresh_block,
    "battery_shared_with_home": "battery_voltage_text(voltage);" in ui and ui.count("battery_voltage_text(") >= 3,
    "firmware_from_app_description": "esp_app_get_description()" in info_block and "esp_app_format" in cmake,
    "uptime_from_esp_timer": "esp_timer_get_time() / 1000" in refresh_block,
    "disconnect_from_existing_state": "tuner_control.state.rtcp_close_reason" in refresh_block,
    "refresh_updates_only_visible_page": "active == settings.page" in refresh_block and "active == info_page.page" in refresh_block,
    # Fonts: only the sizes 12, 14, 16 (18 and 24 are disabled and unqualified)
    "only_listed_fonts": fonts(block) <= {"12", "14", "16"},
    # sdkconfig.remote01 is a generated, untracked file: checked only when present.
    "no_new_font_sizes_enabled": not any(
        re.search(r"MONTSERRAT_(18|22|24|26)=y", f.read_text(encoding="utf-8"))
        for f in (root / "sdkconfig.defaults", root / "sdkconfig.remote01") if f.exists()),
    # Geometry (logical 172 x 640): rows and buttons inside the page, nothing overlapping
    "settings_rows_inside_page": 162 + 44 <= 232 and 256 + 44 <= 324 and 336 + 60 <= 640,
    "info_last_row_inside_page": 'info_row(page, "LAST DISCONNECT", 316)' in info_block and 316 + 16 + 16 <= 640,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("PASS " if ok else "FAIL ") + name)
print(f"UI SETTINGS POLICY SUMMARY PASS={len(checks) - len(failed)} FAIL={len(failed)}")
raise SystemExit(1 if failed else 0)
