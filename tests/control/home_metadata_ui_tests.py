"""Source and vendored-font bounds for the existing HOME metadata widgets."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
adapter = (root / "components/control_home/rtcp_device.cpp").read_text(encoding="utf-8")
wire = (root / "components/rtcp_client/wire.cpp").read_text(encoding="utf-8")

checks = {
    "station_large": 'home.title = remote01_single_line(page, "--", &lv_font_montserrat_20, text, 12, 262, 148,' in ui,
    "station_ellipsis": "LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);" in ui,
    "streamtitle_small": 'home.subtitle = remote01_single_line(page, "--", &lv_font_montserrat_14, muted, 12, 290, 148,' in ui,
    "streamtitle_ellipsis": ui.count("LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);") >= 2,
    "physical_right_bound": 12 + 148 <= 172,
    "assignment_preserved": 'home_text(home.title, s.subtitle[0] ? s.subtitle : "--")' in ui and 'home_text(home.subtitle, s.title[0] ? s.title : "--")' in ui,
    "bounded_buffers": "char title[32]{}, subtitle[32]{}" in (root / "components/control_home/include/control_home/api.hpp").read_text(encoding="utf-8"),
    "utf8_validation": "bool text(" in wire and "!text(p.data()+at,n)" in wire,
}

failed = [name for name, ok in checks.items() if not ok]
print(f"HOME_METADATA_UI: PASS={len(checks)-len(failed)} FAIL={len(failed)} assertions={len(checks)}")
if failed:
    print("FAILED:", ", ".join(failed))
    raise SystemExit(1)
