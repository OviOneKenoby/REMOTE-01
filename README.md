# REMOTE//01

PlatformIO baseline for the Waveshare ESP32-S3-Touch-LCD-3.49 V2. The active UI retains the V2 factory demo screen and its brightness page, adds the first static native 172x640 REMOTE//01 HOME screen, and rotates the complete display/touch coordinate path 180 degrees.

## Baseline

The baseline is Waveshare's official V2 `ESP-IDF/11_FactoryProgram` example from [waveshareteam/ESP32-S3-Touch-LCD-3.49-V2](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2). It is not the similarly named V1 project. The factory UI is the main/demo view followed by its swipe-left backlight page; the implementation is in `components/ui_bsp/generated/setup_scr_screen.c`, `components/lvgl/src/extra/widgets/carousel/lv_carousel.c`, and `components/user_app/user_app.cpp`.

The official V2 lockfile pins ESP-IDF 5.5.4, LVGL 8.3.11, AXS15231B 1.0.1, TCA9554 expander 2.0.3, and ESP codec 1.3.6. The display and touch controller path remains Waveshare's AXS15231B QSPI implementation. The active build does not start the factory's Wi-Fi/BLE, audio, SD, battery/button, or sensor tasks.

## PlatformIO

Build from this directory:

```powershell
pio run -e remote01
```

The successful build environment and its difference from the official lockfile are recorded in [CHANGELOG.md](CHANGELOG.md). PlatformIO's generic board descriptor is only used to select the ESP32-S3 target; V2-specific 16 MB flash, octal PSRAM mode, display/touch pins and controller configuration come from the Waveshare project files, not from a generic board pin map. The generic board banner may still display its own N8/no-PSRAM label; the active ESP-IDF sdkconfig is the hardware setting used for this build.

## Pages and Rotation

The UI sequence is:

1. Waveshare factory main/demo page
2. Waveshare brightness page with the existing working LVGL slider
3. First static REMOTE//01 HOME screen (owner physical hardware PASS; no control logic)

Both panel axes are mirrored through the AXS15231B panel API. The factory's original raw-touch orientation mapping is then followed by the matching 171-x / 639-y transform before LVGL receives the point. Carousel IDs are 0 (factory), 1 (brightness), and 2 (HOME), at logical X=0, 172, and 344. Physical left swipes advance, bringing the next page in from the right; physical right swipes go back, bringing the previous page in from the left. End-stop directions retain the official factory behavior in LVGL logical coordinates; the spacer is excluded from snapping so only real pages are snap targets. The owner physically validated orientation, touch mapping, snapping, navigation direction and clipping fixes as PASS.

## Physical Display Qualification

**Hardware PASS - 2026-10-03.** The owner physically validated Montserrat 12, 14, 16, 20 and 28 px, fine details, grayscale/color rendering, and LVGL symbols at 12/20 px. The provisional minimum for normal primary UI text is 16 px; 12 px is reserved for metadata, minor status and low-priority information. Dark background + white text + `#C2FF00` accent works well on the physical panel.

The full size-by-size observations and intended uses are recorded in [hardware qualification results](docs/HARDWARE.md#owner-validated-physical-display-qualification). These are owner observations, not host-render predictions. Montserrat 18 and 24 px remain disabled and unqualified. The temporary qualification content was subsequently replaced by the first static HOME screen, also physically validated by the owner as hardware PASS. HOME values are static samples; neither PASS validates a protocol, adapter or end-to-end universal-control path.

See [docs/HARDWARE.md](docs/HARDWARE.md) for verified V2 facts and source citations.
