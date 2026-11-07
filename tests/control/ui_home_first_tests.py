"""Source contract: HOME is carousel page 0 (then SETTINGS 1, INFO 2), the factory demo page is gone."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
generated = (root / "components/ui_bsp/generated/setup_scr_screen.c").read_text(encoding="utf-8")
header = (root / "components/ui_bsp/generated/gui_guider.h").read_text(encoding="utf-8")

checks = {
    # Exactly two carousel elements: ids 0 (HOME) and 1 (settings/info).
    "two_generated_elements": len(re.findall(r"lv_carousel_add_element\(", generated)) == 2
    and "lv_carousel_add_element(ui->screen_carousel_1, 0)" in generated
    and "lv_carousel_add_element(ui->screen_carousel_1, 1)" in generated,
    # The third page (INFO, id 2) is the only element the app adds.
    "info_is_the_only_added_element": ui.count("lv_carousel_add_element(") == 1
    and "lv_carousel_add_element(src_ui.screen_carousel_1, 2)" in ui,
    "home_is_element_0": "lv_obj_t *page = src_ui.screen_carousel_1_element_1;" in ui,
    "boots_on_home_without_animation": "lv_obj_set_element_id(src_ui.screen_carousel_1, 0, LV_ANIM_OFF);" in ui,
    # Demo objects are removed from the generated UI and from the struct.
    "no_demo_objects_in_generated": not re.search(
        r"screen_(cont_\d|img_\d|canvas_\d|btn_\d|label_(?!22\b)\d+)", generated),
    "no_demo_objects_in_header": not re.search(
        r"screen_(cont_\d|img_\d|canvas_\d|btn_\d|label_(?!22\b)\d+)", header),
    "no_demo_canvas_buffer": "heap_caps_malloc" not in generated,
    "no_demo_animation_task": "example_color_task" not in ui,
    "no_demo_row_users": not re.search(r"screen_(cont|img|label_2[0-1]|canvas|btn)", ui),
    "slider_still_generated_on_element_2": "ui->screen_slider_1 = lv_slider_create(ui->screen_carousel_1_element_2);" in generated,
    "brightness_callback_kept": "setUpduty(0xff - lv_slider_get_value(slider))" in ui,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("PASS " if ok else "FAIL ") + name)
print(f"UI HOME FIRST SUMMARY PASS={len(checks) - len(failed)} FAIL={len(failed)}")
raise SystemExit(1 if failed else 0)
