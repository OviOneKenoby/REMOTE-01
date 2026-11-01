"""Bounds and ownership checks for the HOME artwork foundation."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
surface = (root / "components/artwork/surface.cpp").read_text(encoding="utf-8")
header = (root / "components/artwork/include/artwork/surface.hpp").read_text(encoding="utf-8")
sdk = (root / "sdkconfig.remote01").read_text(encoding="utf-8")

checks = {
    "panel_geometry_preserved": "remote01_panel(page, 12, 96, 148, 148" in ui,
    "image_centered": "lv_obj_center(home.artwork_image);" in ui,
    "authoritative_source": "artwork_model.observe(tuner_control.state.connected" in ui,
    "neutral_disconnect": "tuner_control.state.connected" in ui,
    "ui_only_refresh": "home_artwork_refresh();" in ui,
    "boot_lifetime_surface": "static artwork::Surface artwork_surface;" in ui,
    "delete_drops_widget_only": "home = {}; // ArtworkSurface remains boot-lifetime" in ui,
    "explicit_psram": "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in surface,
    "single_allocation_gate": "if (attempted_) return ready();" in surface,
    "native_descriptor": "LV_IMG_CF_TRUE_COLOR" in surface,
    "cache_invalidated": "lv_img_cache_invalidate_src(&descriptor_)" in surface,
    "fallback_labels": "home.artwork_fallback" in ui and "home.artwork_caption" in ui,
    "fixed_rgb565_size": "static_assert(bytes == 41472)" in header,
    "no_decoder_enabled": all(token in sdk for token in (
        "# CONFIG_LV_USE_PNG is not set", "# CONFIG_LV_USE_SJPG is not set")),
    "no_network_dependency": "esp_http_client" not in surface,
}

failed = [name for name, ok in checks.items() if not ok]
print(f"ARTWORK_UI_POLICY: PASS={len(checks)-len(failed)} FAIL={len(failed)} assertions={len(checks)}")
if failed:
    print("FAILED:", ", ".join(failed))
    raise SystemExit(1)
