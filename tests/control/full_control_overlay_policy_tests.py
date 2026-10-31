from pathlib import Path

source = (Path(__file__).parents[2] / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
assert "lv_obj_add_flag(tuner_control.list_panel, LV_OBJ_FLAG_CLICKABLE);" in source
assert "lv_obj_add_flag(tuner_control.list_area, LV_OBJ_FLAG_SCROLLABLE);" in source
assert "tuner_control_back_event" in source and "tuner_control_exit_event" in source
assert "tuner_control_close(); // Wake always resumes at HOME" in source

# An accepted country mutation produces the local Name filter immediately; it
# has no following Browser page response that could otherwise dismiss Loading.
country_case = source.split("case control_home::BrowserOperation::SELECT_COUNTRY:", 1)[1].split("case control_home::BrowserOperation::SELECT_NAME:", 1)[0]
assert "tuner_control.navigation.browser_next();" in country_case
assert "tuner_control_popup_hide();" in country_case
assert country_case.index("tuner_control_popup_hide();") < country_case.index("tuner_control_show_name_filters();")
assert "POPUP_DISCONNECTED" in source and '"TUNER disconnected"' in source
print("full-control overlay/popup policy: PASS")
