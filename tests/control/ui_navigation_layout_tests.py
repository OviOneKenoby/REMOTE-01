"""Focused source/layout contract for the portrait HOME and top navigation."""
from pathlib import Path
import re


root = Path(__file__).resolve().parents[2]
source = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
generated = (root / "components/ui_bsp/generated/setup_scr_screen.c").read_text(
    encoding="utf-8"
)


def constant(name: str) -> int:
    match = re.search(rf"constexpr lv_coord_t {name} = (\d+);", source)
    if not match:
        raise SystemExit(f"UI navigation layout FAIL: missing {name}")
    return int(match.group(1))


screen_width = 172
screen_height = 640
button_width = constant("kTopNavigationButtonWidth")
button_height = constant("kTopNavigationButtonHeight")
back_x = constant("kTopNavigationBackX")
exit_x = constant("kTopNavigationExitX")
button_y = constant("kTopNavigationY")

checks = {
    "portrait_geometry_is_hardware_configured": (
        "lv_obj_set_size(ui->screen_cont_2, 172, 640);" in generated
    ),
    "shared_view_matches_display": (
        "tuner_control.view = remote01_panel(src_ui.screen, 0, 0, 172, 640," in source
    ),
    "visible_and_clickable_objects_are_shared": (
        "lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);" in source
    ),
    "back_uses_expanded_geometry": all(
        token in source
        for token in (
            "kTopNavigationY, kTopNavigationButtonHeight);",
            "lv_obj_set_pos(back, kTopNavigationBackX, kTopNavigationY);",
            "lv_obj_set_width(back, kTopNavigationButtonWidth);",
        )
    ),
    "exit_uses_expanded_geometry": all(
        token in source
        for token in (
            "lv_obj_set_pos(exit, kTopNavigationExitX, kTopNavigationY);",
            "lv_obj_set_width(exit, kTopNavigationButtonWidth);",
        )
    ),
    "buttons_are_separated": exit_x - (back_x + button_width) >= 8,
    "buttons_stay_in_bounds": (
        back_x >= 0
        and exit_x + button_width <= screen_width
        and button_y >= 0
        and button_y + button_height <= screen_height
    ),
    "buttons_do_not_overlap_list_content": button_y + button_height <= 50,
    "home_label_is_bounded_before_arrow": (
        'remote01_single_line(row, navigation[i], &lv_font_montserrat_14,' in source
        and "text, 0, 0, 116, LV_LABEL_LONG_CLIP," in source
    ),
    "home_label_keeps_font_and_compacts_only_long_title": (
        "if (i == 2) {" in source
        and "lv_obj_set_style_text_letter_space(name, -1, LV_PART_MAIN);" in source
    ),
    "home_label_remains_left_aligned": (
        "LV_TEXT_ALIGN_LEFT);" in source
        and "lv_obj_align(name, LV_ALIGN_LEFT_MID, 10, 0);" in source
    ),
    "home_arrow_position_is_preserved": (
        "lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);" in source
    ),
}

failed = [name for name, passed in checks.items() if not passed]
if failed:
    raise SystemExit("UI navigation layout FAIL: " + ", ".join(failed))

print(f"UI navigation layout: PASS ({len(checks)}/{len(checks)})")
