# REMOTE//01

The binding behavior and live engineering status for the owner-approved Full
Control project are maintained separately in
[REMOTE Full Control specification](docs/REMOTE_FULL_CONTROL_SPEC.md) and
[implementation record](docs/REMOTE_FULL_CONTROL_IMPLEMENTATION.md). Future UI,
navigation, source-switching, and station-management work must follow the
specification rather than silently changing approved behavior.

The authenticated, browser-based local firmware update design and recovery
boundary are documented in [REMOTE local Wi-Fi OTA](docs/REMOTE_LOCAL_WIFI_OTA.md).

PlatformIO firmware for REMOTE//01 on the Waveshare ESP32-S3-Touch-LCD-3.49B V2, PCB Rev1.1. The accepted A-REMOTE-012 baseline retains the factory demo and brightness pages, provides an active 172x640 HOME status/control page, and rotates the complete display/touch coordinate path 180 degrees.

The installed accepted application is 2,526,848 bytes with SHA-256 `b6053c04c03114a24a59f178da4eb03d4bada77faded43b6009667579c21d5da`. It interoperates with TUNER v1.1.12 through authenticated RTCP/1 for identity, capabilities, authoritative volume/playback observations and correlated `volume.set`. This repository has no public REMOTE firmware release yet.

The A-REMOTE-014 candidate adds runtime `_rtcp._tcp` DNS-SD discovery so a
paired TUNER can be found after DHCP address changes. Discovery supplies only
candidate transport locations: every candidate must still pass the existing
RTCP mutual cryptographic proof and encrypted identity exchange. The saved
endpoint remains a fallback, discovered addresses are not persisted, and no
pairing secret or identifier is advertised. Networks that block multicast may
therefore continue using a reachable saved endpoint but cannot automatically
recover a stale one until multicast discovery is available.

The stacked A-REMOTE-013 D-009 candidate adds authenticated radio playback
commands and authoritative station/now-playing presentation on the existing
HOME page. It retains A-REMOTE-014 discovery, RTCP/1 IDs 1-8, pairing,
single-command-in-flight fencing, sleep/wake behavior, and the existing UI
geometry. This is a software-verified candidate only; it has not been flashed
or jointly validated with the TUNER D-009 candidate.

## Beta status

REMOTE//01 is a beta test and a work in progress; much is still to be done. It
has been tested only by the author, on one REMOTE (Waveshare
ESP32-S3-Touch-LCD-3.49B V2) together with one TUNER (v1.1.12), and has had no
wider validation. Expect bugs and changes, and do not rely on it for anything
important. This repository has no firmware release yet; build it yourself with
PlatformIO.

## Known issues

- The REMOTE needs a TUNER running compatible firmware (TUNER v1.1.12,
  https://github.com/OviOneKenoby/ESP32-WROVER-Internet-radio) on the same local
  network. The two devices talk over the authenticated RTCP/1 link and must be
  paired.
- After a station is started from a catalog, the REMOTE waits 15,000 ms for the
  TUNER to confirm playback. If the confirmation does not arrive in that time,
  it closes the link and shows "Playback not confirmed" for 3 seconds. On the
  author's hardware this message has appeared for some https stations while the
  audio was playing; the cause has not been established.
- Two behaviours of the TUNER are shown by the REMOTE as they are and are listed
  as "Not fixed" in the TUNER changelog: a stream that fails or stops after it
  has started is not restored, and the link can close shortly after a station
  start.
- This state of `main` contains the RTCP close-correlation and first-disconnect
  capture and the read-only diagnostics pages (`/api/status`, `/api/events`,
  `/api/rtcp-history`, answered only for requests addressed to the device's own
  address). The changelog of the branch `diagnostics/remote-rtcp-first-failure`
  records the hardware result of the HTTP-stack change (commit `ad390a4`) as
  "SOFTWARE PASS; HARDWARE PASS (LIMITED)" and "The hardware result is a short
  observation window", and the root cause of the failure of the preceding build
  as "NOT PROVEN". Long-run stability is unknown.

## Baseline

The baseline is Waveshare's official V2 `ESP-IDF/11_FactoryProgram` example from [waveshareteam/ESP32-S3-Touch-LCD-3.49-V2](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2). It is not the similarly named V1 project. The factory UI is the main/demo view followed by its swipe-left backlight page; the implementation is in `components/ui_bsp/generated/setup_scr_screen.c`, `components/lvgl/src/extra/widgets/carousel/lv_carousel.c`, and `components/user_app/user_app.cpp`.

The official V2 lockfile pins ESP-IDF 5.5.4, LVGL 8.3.11, AXS15231B 1.0.1, TCA9554 expander 2.0.3, and ESP codec 1.3.6. The production PlatformIO environment instead pins ESP-IDF 5.5.3 and the package versions recorded in `dependencies.lock`. The display and touch controller path remains Waveshare's AXS15231B QSPI implementation. Factory Wi-Fi/BLE, audio and SD demo tasks remain disabled; production networking, provisioning, RTCP, persistence, physical input and power-management components provide the active behavior.

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
3. REMOTE//01 HOME status/control screen with authoritative playback and volume state, provisional desired-volume indication and encoder volume routing

Both panel axes are mirrored through the AXS15231B panel API. The factory's original raw-touch orientation mapping is then followed by the matching 171-x / 639-y transform before LVGL receives the point. Carousel IDs are 0 (factory), 1 (brightness), and 2 (HOME), at logical X=0, 172, and 344. Physical left swipes advance, bringing the next page in from the right; physical right swipes go back, bringing the previous page in from the left. End-stop directions retain the official factory behavior in LVGL logical coordinates; the spacer is excluded from snapping so only real pages are snap targets. The owner physically validated orientation, touch mapping, snapping, navigation direction and clipping fixes as PASS.

## Physical Display Qualification

**Hardware PASS - 2026-10-03.** The owner physically validated Montserrat 12, 14, 16, 20 and 28 px, fine details, grayscale/color rendering, and LVGL symbols at 12/20 px. The provisional minimum for normal primary UI text is 16 px; 12 px is reserved for metadata, minor status and low-priority information. Dark background + white text + `#C2FF00` accent works well on the physical panel.

The full size-by-size observations and intended uses are recorded in [hardware qualification results](docs/HARDWARE.md#owner-validated-physical-display-qualification). These are owner observations, not host-render predictions. Montserrat 18 and 24 px remain disabled and unqualified. Historical static-HOME qualification preceded the authenticated integration; the accepted A-REMOTE-012 baseline now binds HOME volume and playback presentation to authoritative RTCP/CCM observations. Encoder volume, rapid reversal, reconnect, sleep/wake and bounded MP3/A2DP coexistence were hardware accepted at the practical hobby/public-beta level. Quantitative latency was owner-waived and was not measured.

## Security and persistence

Wi-Fi provisioning stores credentials in the default NVS namespace. RTCP association material and generation state use only the dedicated 64 KiB `remote_ctl` NVS partition at `0xE20000`. Stored credentials, PSKs and association payloads are not committed by the production implementation.

`components/esp_wifi_bsp` is an excluded Waveshare demo component. Its `06_WIFI_STA` literals are the public vendor sample values in [Waveshare's official example documentation](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49), not REMOTE operational credentials. The active build excludes this component through the root `CMakeLists.txt`.

## Current limits

- The public source matches the active local source after line-ending normalization, but the historical Git-less build has no immutable source commit proving byte-reproducible derivation of the installed binary.
- Intermittent QMI8658 motion-wake initialization remains a known separate limitation.
- Playback commands and richer metadata from the owner-approved/frozen
  A-REMOTE-013 D-009 profile are implemented on the stacked candidate branch.
  Hardware installation, interoperability, command refusal cases, metadata
  changes, lifecycle recovery, and regression testing remain pending.

See [docs/HARDWARE.md](docs/HARDWARE.md) for verified V2 facts and source citations.

## License

This project is licensed under the GNU General Public License, version 3 (see
`LICENSE`). The GPL applies to the project's own files and to the project's own
changes. Files that come from Waveshare's example, identical or modified, keep
the original Apache License 2.0 notices and terms; vendored libraries keep their
own licences. The list of files, the third-party components and the points that
could not be verified are in `THIRD_PARTY_NOTICES.md`.
