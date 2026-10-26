from pathlib import Path


source = (Path(__file__).parents[2] / "components/user_app/user_app.cpp").read_text(
    encoding="utf-8"
)

checks = {
    "popup_uses_dedicated_label": (
        'tuner_control.popup_text = remote01_label(tuner_control.popup, "",'
        in source
    ),
    "popup_wraps": (
        "lv_label_set_long_mode(tuner_control.popup_text, LV_LABEL_LONG_WRAP);"
        in source
    ),
    "popup_has_three_line_height": (
        "lv_obj_set_size(tuner_control.popup_text, 124, 52);" in source
    ),
    "popup_text_is_centered": (
        "lv_obj_set_style_text_align(tuner_control.popup_text, LV_TEXT_ALIGN_CENTER,"
        in source
    ),
    "popup_buttons_remain_below_text": (
        'tuner_control.popup_confirm = tuner_control_button(tuner_control.popup, "Delete", 132, 36);'
        in source
    ),
    "representative_messages_present": all(
        message in source
        for message in (
            "Starting Bluetooth...",
            "Switching to Radio...",
            "TUNER disconnected",
            "Tuning to %s...",
        )
    ),
}

failed = [name for name, passed in checks.items() if not passed]
if failed:
    raise SystemExit("Popup layout FAIL: " + ", ".join(failed))

print(f"Full Control popup layout: PASS ({len(checks)}/{len(checks)})")
