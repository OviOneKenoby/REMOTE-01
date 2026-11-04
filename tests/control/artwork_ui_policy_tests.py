"""Bounds and ownership checks for the HOME artwork foundation."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
surface = (root / "components/artwork/surface.cpp").read_text(encoding="utf-8")
online = (root / "components/artwork/online_runtime.cpp").read_text(encoding="utf-8")
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
    "single_worker": online.count("xTaskCreatePinnedToCoreWithCaps(") == 1,
    "idle_priority_background_worker":
        "worker_stack_bytes, nullptr, tskIDLE_PRIORITY" in online,
    "bounded_encoded_buffer": "encoded_limit = 128 * 1024" in (root / "components/artwork/include/artwork/online.hpp").read_text(encoding="utf-8"),
    "verified_musicbrainz_bound": "search_limit = 64 * 1024" in
        (root / "components/artwork/include/artwork/online.hpp").read_text(encoding="utf-8"),
    "psram_worker_buffers": online.count("heap_caps_malloc(") == 4 and
                            online.count("MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT") >= 4,
    "psram_worker_state": "heap_caps_calloc(" in online and
                          "RuntimeState *runtime" in online,
    "psram_worker_stack": "xTaskCreatePinnedToCoreWithCaps(" in online and
                          "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in online,
    "tls_worker_stack_budget": "worker_stack_bytes = 16 * 1024" in online and
                               "stack_min_bytes=%u" in online and
                               "uxTaskGetStackHighWaterMark(nullptr)" in online,
    "psram_ui_state": "ArtworkUiState *artwork_ui" in ui and
                      "sizeof(ArtworkUiState), MALLOC_CAP_SPIRAM" in ui,
    "allocation_failure_cleanup": all(token in online for token in (
        "heap_caps_free(encoded)", "heap_caps_free(decoded)",
        "heap_caps_free(station_logo_pixels)", "heap_caps_free(jpeg_pool)")),
    "no_worker_lvgl_calls": "lv_obj_" not in online and "lv_img_" not in online,
    "tls_bundle_required": "esp_crt_bundle_attach" in online,
    "ota_exclusion": "ota_service::busy()" in online,
    "display_idle_background_lookup":
        "power_management::background_network_allowed()" in online,
}

failed = [name for name, ok in checks.items() if not ok]
print(f"ARTWORK_UI_POLICY: PASS={len(checks)-len(failed)} FAIL={len(failed)} assertions={len(checks)}")
if failed:
    print("FAILED:", ", ".join(failed))
    raise SystemExit(1)
