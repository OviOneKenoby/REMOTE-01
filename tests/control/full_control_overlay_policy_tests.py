from pathlib import Path

source = (Path(__file__).parents[2] / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
assert "lv_obj_add_flag(tuner_control.list_panel, LV_OBJ_FLAG_CLICKABLE);" in source
assert "lv_obj_add_flag(tuner_control.list_area, LV_OBJ_FLAG_SCROLLABLE);" in source
assert "tuner_control_back_event" in source and "tuner_control_exit_event" in source
assert "tuner_control_close(); // Wake always resumes at HOME" in source
assert "POPUP_DISCONNECTED" in source and '"TUNER disconnected"' in source
print("full-control overlay/popup policy: PASS")
