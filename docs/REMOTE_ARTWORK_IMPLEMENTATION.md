# REMOTE Artwork Implementation

## Stage 1 scope

Artwork Stage 1 supplies a local, hardware-testable image path for the HOME
screen. It implements source-specific Radio and Bluetooth placeholders and a
neutral disconnected/unknown placeholder. Station logos, Radio Browser
favicons, album lookup, artist lookup, HTTPS downloads, image caches and
runtime PNG/JPEG decoding are outside this stage.

The approved future selection order remains:

1. A reliable album cover.
2. A reliable artist photograph.
3. The generic placeholder for the authoritative playback source.

## Architecture and ownership

- `artwork::Model` accepts only the existing authoritative Full Control
  connection/source observation. Disconnected, unknown and invalid source
  values select the neutral placeholder; station names and metadata are never
  used as source heuristics.
- `artwork::Surface` owns one 144 x 144 RGB565 buffer allocated explicitly from
  PSRAM (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`). Its fixed cost is 41,472
  bytes. Allocation is attempted once and the buffer is retained until reboot.
- The corresponding `lv_img_dsc_t` has the same boot lifetime. HOME widgets may
  be destroyed and recreated without retaining a dangling descriptor or pixel
  pointer.
- The existing LVGL/UI owner creates the image widget, observes source changes,
  renders the local placeholder and invalidates the widget. There is no artwork
  task and no LVGL access from networking or RTCP code.
- Unchanged source observations cause no redraw and no allocation. If the
  one-time PSRAM allocation fails, the panel displays its local text fallback.
- The panel remains at `(12, 96)`, 148 x 148 pixels. The 144 x 144 image is
  centered and does not alter adjacent HOME controls, metadata or navigation.

The exposed UI write buffer is intentionally limited to the UI owner. A later
asynchronous decoder/downloader must use a separate bounded staging buffer and
handoff; it must not write the displayed surface concurrently.

## Lifecycle behavior

Initial boot and RTCP disconnect select neutral. Authenticated Radio and
Bluetooth states select their matching images. Reconnection and Radio/Bluetooth
transitions update deterministically. HOME exit deletes only widget pointers;
HOME re-entry reuses the valid surface and current authoritative state. Display
sleep does not delete or transfer buffer ownership, and wake refreshes from the
same state path. Metadata changes do not redraw artwork unless the source also
changes.

## Verification checkpoint

- Artwork model and fallback behavior: **PASS** (20 assertions).
- UI ownership/layout/memory/decoder policy: **PASS** (15 assertions).
- HOME metadata policy: **PASS** (15 assertions).
- UI navigation layout: **PASS** (12 assertions).
- BTX integration policy: **PASS** (15 assertions).
- D-009 policy: **PASS** (16 assertions).
- Local OTA source/partition policy: **PASS**.
- Core: **PASS** (69 cases, 2,425 assertions).
- HOME: **PASS** (42 cases, 3,565 assertions).
- RTCP ownership: **PASS** (4 cases, 92 assertions).
- Broad hardening: 25 PASS / 1 pre-existing FAIL. The separately tracked
  `utf8_snapshot_bounds` length expectation is unchanged and unrelated to the
  artwork files; no speculative correction is included here.
- Production `remote01`: **PASS** after one transient compiler internal error
  in an unchanged generated LVGL image compiled successfully on the single
  incremental retry.

Final firmware identity and partition headroom are recorded in the private
candidate manifest after the source commit.

## Hardware validation

The exact candidate was installed through the previously accepted local Wi-Fi
OTA workflow. The updater reported successful REMOTE//01 image verification
before reboot. Post-reboot diagnostics independently identified project
`REMOTE01`, version `8b0b406` and the candidate ELF SHA-256.

Owner-observed checks passed:

- Radio mode displayed the Radio placeholder.
- Bluetooth mode displayed the Bluetooth placeholder and HOME remained
  responsive.
- Bluetooth to Radio restored the Radio placeholder without a stale image or
  persistent error.
- Leaving and recreating HOME retained the correct source image, metadata and
  touch behavior.
- Display idle followed by SYSTEM_SLEEP/wake restored HOME, artwork, metadata,
  volume and responsive touch without unintended playback behavior.

The final read-only snapshot reported Wi-Fi connected at -52 dBm, RTCP stage
13 with no current failure, authoritative volume connected/known/set, no
outstanding operation, zero allocation/network/power errors, 29,311 bytes free
internal heap (9,728-byte largest block), and 7,533,728 bytes free PSRAM.
Artwork Stage 1 hardware validation is therefore **PASS**. TUNER was not
flashed, reset or otherwise modified by this milestone.

## Next milestone boundary

The next artwork milestone may add one bounded decoder and network worker only
after this lifecycle and memory ownership have passed hardware validation. It
must preserve the authoritative-source model, explicit buffer lifetimes and
LVGL UI ownership established here.
