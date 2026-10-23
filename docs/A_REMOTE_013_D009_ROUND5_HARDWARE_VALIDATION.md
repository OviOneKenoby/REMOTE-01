# A-REMOTE-013 / D-009 Round 5 hardware validation

Status: **TEST SESSION CLOSED / PARTIAL HARDWARE PASS**

Date: 2026-10-23

## Tested firmware and rollback

- REMOTE functional source: `bfce47816a8b2b8a92f41d76c6a4bc4975d25361`.
- REMOTE application: 2,578,352 bytes, SHA-256
  `6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`.
- Application partition: factory slot at `0x10000`, capacity `0x800000`.
- Preserved A-REMOTE-012 rollback: 2,526,848 bytes, SHA-256
  `b6053c04c03114a24a59f178da4eb03d4bada77faded43b6009667579c21d5da`.
- TUNER D-009 application used for interoperability: 2,040,704 bytes,
  SHA-256
  `1e6c5968f539170656e833d023086c57986b1aa73a6c2fe982dee2b31b1ff1f6a`.

The REMOTE binary and TUNER artifact identities were checked from the retained
private bytes. The REMOTE target was the expected ESP32-S3 QFN56 rev. 0.2 with
16 MiB flash on its native USB Serial/JTAG interface. Local and separate-USB
full-flash, application and `remote_ctl` rollback material matched the recorded
manifests before installation.

## Installation evidence

One application-only write was made at `0x10000`; bootloader, partition table,
NVS, OTA metadata, `remote_ctl`, pairing and credentials were outside the write
range. An independent 2,578,352-byte readback matched the application SHA-256
exactly. The device booted normally, retained its stored Wi-Fi and secure RTCP
association, authenticated to TUNER and restored authoritative volume control.

The bounded post-boot snapshot reported no allocation failure, an authenticated
RTCP session, no command in flight, 21,323 bytes internal free heap, 11,259
bytes recorded minimum internal heap, a 10,752-byte largest internal block,
7,623,608 bytes free PSRAM and 16,904 bytes Core-task stack margin. These are
installation-checkpoint values, not representative-use acceptance measurements.

## Owner-observed PASS results

- Carousel navigation 0 -> 1 -> 2, brightness, touch and HOME at index 2.
- Existing encoder volume control and authoritative volume synchronization.
- Radio Playing/Paused state, central icon and upper state presentation.
- REMOTE Pause and Play change actual TUNER playback, with presentation updated
  from authoritative state.
- Local TUNER Pause and Resume update REMOTE correctly.
- Selected station and exact combined ICY StreamTitle presentation, including
  scrolling and replacement/invalidation after a station change.
- In Radio Browser/discovered context, Previous and Next are unavailable.
- A TUNER restart invalidates station, metadata, playback, volume and playback
  controls; reconnect restores current station, metadata, playback, controls
  and encoder volume without stale presentation.
- Bluetooth A2DP audio remains functional. Radio metadata and radio playback
  controls are withdrawn in Bluetooth mode. Returning to radio restores station,
  StreamTitle, playback state and Play/Pause availability.

Bluetooth artist/title on REMOTE and Bluetooth AVRCP control are outside D-009.
The owner observed that the upper Playing text/icon did not change when playback
was paused on the phone. This is retained as a source-semantics observation and
is not classified as a D-009 radio defect.

## Not tested / remaining acceptance

1. Previous/Next success and wrap with at least two entries in the fixed saved
   station list.
2. Any approved forced-invalid-command proof that Favorites are not mutated.
3. REMOTE sleep/wake followed by RTCP and D-009 state resynchronization.
4. A safe REMOTE restart test if a supported method is available.
5. Representative resource/stability measurements covering MP3, A2DP and web
   coexistence.
6. Remaining mandatory items in the approved D-009 hardware protocol.

The unrelated intermittent QMI8658 motion-wake limitation remains separate.
No unsupported Bluetooth capability is an acceptance requirement for D-009.

## Verdict

`REMOTE D-009 ROUND 5 - TEST SESSION CLOSED / PARTIAL HARDWARE PASS`

Overall A-REMOTE-013 remains `HARDWARE ACCEPTANCE PENDING`. REMOTE PR #4 and
TUNER PR #7 remain open and unmerged. REMOTE PR #4 retains its intentional
A-REMOTE-014 mDNS dependency.
