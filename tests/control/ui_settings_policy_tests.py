"""Source contract for the settings/info page, 50 % boot brightness and the timeout selector."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
model = (root / "components/user_app/settings_model.hpp").read_text(encoding="utf-8")
power_model = (root / "components/power_management/include/power_management/model.hpp").read_text(encoding="utf-8")
power_rt = (root / "components/power_management/runtime.cpp").read_text(encoding="utf-8")
cmake = (root / "components/user_app/CMakeLists.txt").read_text(encoding="utf-8")


def fonts(block: str):
    return set(re.findall(r"lv_font_montserrat_(\d+)", block))


settings_block = ui[ui.index("struct SettingsWidgets"):ui.index("static void lvgl_obj_event_callback(lv_event_t *event)" + chr(10) + "{")]

checks = {
    # Brightness
    "boot_brightness_is_half_scale": "constexpr std::uint8_t brightness_boot = 128;" in model,
    "init_uses_boot_brightness": "lcd_bl_pwm_bsp_init(ui_settings::brightness_pwm(ui_settings::brightness_boot));" in ui
    and "lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255)" not in ui,
    "slider_set_to_boot_value": "lv_slider_set_value(src_ui.screen_slider_1, ui_settings::brightness_boot, LV_ANIM_OFF);" in ui,
    "existing_slider_reused_not_recreated": "lv_slider_create" not in ui,
    "slider_callback_unchanged": "setUpduty(0xff - lv_slider_get_value(slider))" in ui
    and "lv_obj_add_event_cb(src_ui.screen_slider_1, lvgl_obj_event_callback, LV_EVENT_ALL, NULL);" in ui,
    # Timeout selector
    "five_timeout_buttons": "ui_settings::timeout_choices.size()" in settings_block and "timeout_button[" in settings_block,
    "timeout_goes_through_power_api": "power_management::set_display_idle_ms(ui_settings::timeout_choices[index].ms)" in settings_block,
    "timeout_state_read_from_power_api": "ui_settings::timeout_index(power_management::display_idle_ms())" in settings_block,
    "timeout_value_validated_in_power": "if (!display_idle_choice(ms)) return false;" in power_rt,
    "system_idle_unchanged": "system_idle_ms = 300000" in power_model,
    "display_idle_applied_before_tick": power_rt.index("machine.display_idle_ms = selected_display_idle_ms") < power_rt.index("machine.tick(now,"),
    "timeout_not_persisted": "nvs" not in settings_block.lower() and "persist" not in settings_block.lower(),
    # Information rows: only existing sources
    "info_rows_present": all(
        name in settings_block
        for name in ("TUNER LINK", "BATTERY", "FIRMWARE", "UPTIME", "LAST DISCONNECT")
    ),
    "connection_from_existing_state": "tuner_control.state.connected" in settings_block,
    "battery_shared_with_home": "battery_voltage_text(voltage);" in ui and ui.count("battery_voltage_text(") >= 3,
    "firmware_from_app_description": "esp_app_get_description()" in settings_block and "esp_app_format" in cmake,
    "uptime_from_esp_timer": "esp_timer_get_time() / 1000" in settings_block,
    "disconnect_from_existing_state": "tuner_control.state.rtcp_close_reason" in settings_block,
    # Fonts: only the qualified sizes 12, 14, 16 (18 and 24 are disabled and unqualified)
    "only_qualified_fonts": fonts(settings_block) <= {"12", "14", "16"},
    "no_new_font_sizes_enabled": not re.search(r"MONTSERRAT_(18|22|24|26)=y",
                                               (root / "sdkconfig.remote01").read_text(encoding="utf-8"))
    and not re.search(r"MONTSERRAT_(18|22|24|26)=y",
                      (root / "sdkconfig.defaults").read_text(encoding="utf-8")),
    # Geometry: nothing leaves the 172x640 page
    "timeout_buttons_44px_inside_page": "lv_coord_t(146 + 50 * int(i)), 148, 44" in settings_block
    and 146 + 50 * 4 + 44 <= 640,
    "last_info_row_inside_page": "settings_row(page, \"LAST DISCONNECT\", 588)" in settings_block and 588 + 15 + 16 <= 640,
    "refresh_skips_hidden_page": "lv_carousel_get_element_act(src_ui.screen_carousel_1) != settings.page" in settings_block,
    "no_extra_carousel_element": "lv_carousel_add_element(" not in ui,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("PASS " if ok else "FAIL ") + name)
print(f"UI SETTINGS POLICY SUMMARY PASS={len(checks) - len(failed)} FAIL={len(failed)}")
raise SystemExit(1 if failed else 0)
