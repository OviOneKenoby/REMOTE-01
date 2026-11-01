# Changelog

## 2026-11-01 - Authenticated local Wi-Fi OTA candidate

- Add a responsive, self-contained `/ota` page to the existing station-mode
  HTTP server for Windows and Android browsers. A two-minute on-device PIN,
  bounded failure lockout and single-use session authorize one upload without
  putting credentials in URLs or logs.
- Stream the application through a 4096-byte PSRAM buffer to ESP-IDF's inactive
  OTA slot. Reject wrong-chip/wrong-project images, invalid headers, oversized
  or interrupted transfers, digest mismatches and concurrent uploads before
  selecting a new boot partition.
- Preserve the accepted partition table, NVS, Wi-Fi, RTCP pairing,
  `remote_ctl`, Favorites/Recent data and USB recovery path. Sleep is inhibited
  during a bounded update and successful verification schedules a controlled
  reboot.
- Record that SHA-256 is integrity checking rather than publisher signing, the
  local page uses plain HTTP on a trusted WPA LAN, and the installed bootloader
  does not provide automatic application rollback.
- Add portable policy/state tests and browser SHA-256 known-answer coverage.
  Focused OTA, HTTP-policy and sleep-boundary checks pass. Production
  `remote01` build at implementation `37cabec` succeeds with 61,356 bytes
  static RAM, 2,633,257 bytes linked flash, and a 2,633,760-byte BIN
  (`6efd529c1f3a20009aa6c2082cf6c987c371398c722c45f931d2fa55b1935f3e`).
  Each 3 MiB OTA slot retains 511,968 bytes exact-image headroom. Initial USB
  installation and one real Wi-Fi OTA remain hardware gates before this
  feature can be accepted or merged.

## 2026-11-01 - Catalog playback transient disconnect presentation

- Retain RTCP socket close, authentication, session invalidation and fresh-query
  recovery when a catalog PLAY confirmation loses its transport.
- Suppress the misleading `TUNER disconnected` card only while the existing
  bounded 15-second deferred-playback confirmation remains active. A sustained
  or unrelated disconnect still follows the normal disconnected UI path.
- Add deterministic coverage for transient recovery and deadline expiry.

Hardware validation passed with source commit `eb2199c`: the application-only
install and independent 2,611,312-byte readback matched SHA-256
`d823480653f2f231e09166e7a24756cab04f56062530d628a8752a2464d1eec8`.
Selecting West City Radio through HOME -> Favorites produced no false disconnect
card, started audio exactly once and returned REMOTE to HOME with RTCP READY and
authoritative volume synchronized. The image retained the earlier embedded
`502d206` descriptor because it was an incremental build; exact source ancestry,
the rebuilt `user_app` object, candidate hash and readback establish the installed
`eb2199c` correction. No functional rebuild was performed merely to alter that
version string.

## 2026-11-01 - Bound the post-deadline RTCP response drain

Hardware validation of the receive-before-timeout correction reproduced one
remaining transient disconnect. TUNER received the post-play catalog query,
generated and fully wrote response type 23, then observed REMOTE peer EOF only
10 ms later. REMOTE had already performed its single receive pass before those
bytes arrived. Application responses now retain the existing three-second
service deadline followed by one fixed 64 ms drain window, sufficient for one
normal poll interval plus jitter. The receive remains nonblocking and bounded;
requests are not retried or replayed. Connect/server-hello deadlines, protocol,
cryptography, session fencing and TUNER firmware are unchanged.

The clean `remote01` build passed at implementation `502d206`: BIN 2,611,280
bytes (SHA-256
`20c19f99d6c72a0b0e05ccf248fbb2c6ef396cceaa912a140aeacd4752bb0419`),
ELF 20,061,024 bytes (SHA-256
`a5d9d970179468a925b728e380cebfcd12614e766f93c81d262a8d3c51d0be30`),
61,180 bytes static RAM, and 5,777,328 bytes application-slot headroom.

## 2026-11-01 - Accept an authenticated RTCP response at the deadline boundary

Paired hardware correlation proved that a post-station-start BTX state request
reached TUNER only near the end of REMOTE's existing three-second deadline.
TUNER generated and fully wrote the correct response 10 ms before REMOTE
closed, but `Client::poll()` enforced expiry before its bounded nonblocking
receive path. REMOTE now performs that existing receive and authenticated
record-accept pass first, then closes only if the transaction is still
outstanding. The deadline, retry policy, single-in-flight contract, wire
format, counters, cryptography and session fencing are unchanged.

Focused deadline and lifecycle regressions cover no response, a still-pending
expired response, and a response accepted exactly at expiry. Hardware
validation with the existing TUNER correlation firmware remains required.

The clean `remote01` production build passed with embedded build ID `c148035`:
BIN 2,611,264 bytes (SHA-256
`a90adfe9d4673beed3136737f6570fab5a85b83ebba2790ae6dd1ba5815af22e`),
ELF 20,060,992 bytes (SHA-256
`c8a4b39494930772f44b71f7bad9b813fc3ceb8e6b30383c4724e4267c86cf42`).
Static RAM use is 61,180 of 327,680 bytes; the 8 MiB application slot has
5,777,344 bytes remaining after the 2,611,264-byte application image.

## 2026-10-31 - Larger top navigation targets and HOME label correction

Increased the shared TUNER CONTROL and Radio Browser BACK/EXIT buttons from
70x34 to 70x42 pixels without changing their positions, order, styling or
actions. The clickable LVGL objects grow with the visible controls, retain an
8-pixel horizontal separation and remain within the 172x640 portrait display.

Bounded the HOME navigation labels to the space before their arrows and applied
one pixel of tighter letter spacing only to `TUNER CONTROL`. This preserves the
Montserrat 14 font and existing row geometry while preventing the 126-pixel
title from colliding with the trailing arrow. Focused layout and navigation
verification plus a `remote01` production build are recorded with the feature
branch; hardware touch and visual confirmation remain pending.

## 2026-10-31 - Full Control 1.3 integrated into main

Fast-forwarded `main` from `d0a6ae5` through the complete Full Control history.
The hardware-validated REMOTE implementation remains `8fdabc6` (2,610,992-byte
BIN, SHA-256
`ba37f42ef9bb520ad94ba0b76bd20f7a08bf38c293e6332f2e6d4af0b89b5c9f`).
Focused D-009, BTX, RTCP discovery, Browser, navigation, overlay, popup,
metadata, catalog-confirmation and diagnostics tests passed at integration
head `e285fc1`.

The clean `remote01` production build at that head passed: BIN 2,611,040 bytes,
SHA-256
`0fa3d4d701e45c3e362391a5b5c463dbec716a3545023ef07e960e208718cbeb`;
ELF SHA-256
`cedcac4e9bb0d992d48104135a7a1faf87614a86507d92ef5d4ff6312ab983d8`;
static RAM 61,180 bytes and exact-image factory-slot headroom 5,777,568
bytes. The hash differs from the installed image because later documentation
and test commits changed the embedded Git identity; no firmware implementation
changed after the hardware-validated `8fdabc6`. This integrated build was not
flashed or hardware-tested. Extended soak and exhaustive Browser error,
empty-result and cancellation paths remain untested.

## 2026-10-31 - Radio Browser paired hardware validation PASS

Installed REMOTE `8fdabc6` with exact 2,610,992-byte application readback
SHA-256 `ba37f42ef9bb520ad94ba0b76bd20f7a08bf38c293e6332f2e6d4af0b89b5c9f`
against TUNER `12b1b24`. The owner completed one Country -> Name -> Genre ->
Results flow, selected one station, and observed no stuck overlay, false
disconnect, duplicate start or presentation error. Audio started exactly once;
metadata, PLAYING and authoritative volume were correct.

Post-test REMOTE remained authenticated at RTCP stage 13 with
`volume_connected/known/set=1/1/1`. Browser counters advanced without a
generation mismatch, and the retained four-entry RTCP close history was
unchanged from baseline. This validates the Browser disconnect recovery and
Country-overlay correction for the tested flow. It does not constitute a
long-duration soak or exhaustive empty/error/cancellation validation.

## 2026-10-31 - Browser disconnect recovery candidate

Hardware validation of the first candidate exposed one additional bounded UI
defect: an accepted Country selection advanced to the local Name filter without
dismissing its `Loading...` overlay. RTCP stayed authenticated, the request and
response counters matched, and TUNER reported HTTP 200 with no TLS or socket
error. REMOTE now dismisses that overlay before rendering the Name filters.
The focused overlay policy regression and clean production build pass at
implementation `8fdabc6`. The resulting 2,610,992-byte BIN SHA-256 is
`ba37f42ef9bb520ad94ba0b76bd20f7a08bf38c293e6332f2e6d4af0b89b5c9f`;
static RAM is 61,180 bytes and factory-slot headroom is 5,777,616 bytes.

Corrected the demonstrated blank Browser screen after a transient authenticated
RTCP disconnect. When transport loss invalidates the Browser snapshot, REMOTE
now fences the interrupted Browser session and returns HOME exactly once after
reconnection instead of exposing the cleared page. Browser START is not
replayed, and non-Browser disconnect handling is unchanged.

The executable navigation policy test covers disconnects during Country and
Results pages, non-Browser disconnects, single-shot reconnect recovery and
explicit cancellation. The clean `remote01` build passed at implementation
`efed8f0`; its 2,610,976-byte BIN has SHA-256
`0dcd32a955db8788af064f15bd063290c867eb45f4ff5439d3ef3940403d22f6`.
Static RAM is 61,180 bytes and factory-slot headroom is 5,777,632 bytes.

TUNER evidence still proves `peer_eof`, not the initiating REMOTE close path.
The pre-reset REMOTE close history was lost when opening native USB/JTAG caused
a USB reset despite disabled DTR/RTS, so no timeout or scheduling correction was
made. The same Radio Browser IPv4 endpoints later returned HTTP 200 from the
development PC; the earlier all-mirror HTTP 503 remains a verified transient
upstream condition rather than a demonstrated firmware request defect.

## 2026-10-29 - Paired hardware follow-up and TUNER-only candidate

The installed REMOTE `9142401` passed Bluetooth metadata/state and phone,
REMOTE and physical-TUNER Play/Pause authority, Bluetooth-to-Radio stopped
semantics, Radio audio/gain recovery, authoritative volume, display sleep/wake
restoration and uninterrupted TUNER playback while REMOTE slept. TUNER has no
playback-state display; prior notes that counted one are superseded.

Favorite playback now starts audibly, but the installed TUNER still performs a
synchronous e-paper redraw before the post-command RTCP query chain settles.
The retained evidence shows response timeout then `ECONNRESET`; TUNER shows
peer EOF and a 3,011,296 us maximum response. Radio Browser still fails before
HTTP status with `ESP_ERR_HTTP_CONNECT` after approximately one second per
endpoint. Error dismissal correctly reveals the Country list and EXIT returns
HOME without interrupting audio.

No REMOTE source or firmware changed. TUNER source
`579f412691df6ffc4d03513d1c64f7a65ea18320` contains the bounded follow-up and
builds successfully; its 2,135,920-byte BIN SHA-256 is
`663983a35e060eb1fee6943512dc60d22ef7414255725a46f329b7c5915aed3c`.
It is not installed and requires separate authorization and focused hardware
validation. The installed REMOTE and rollback artifacts remain unchanged.

## 2026-10-28 - Full Control 1.3 joint hardware-failure corrections ready offline

Corrected the demonstrated paired failures without changing RTCP security,
mutation replay policy, protocol framing, source-switch semantics or the
approved Bluetooth-to-Radio stopped state. A deferred catalog PLAY that has
already been accepted is no longer failed by the first inactive catalog
snapshot: REMOTE performs bounded read-only re-queries under the existing
15-second confirmation deadline and never resends the mutation. On TUNER, the
existing audio task temporarily yields priority only while opening that
deferred stream, allowing the RTCP/main-loop owner to continue servicing the
request; revision revalidation and physical-control preemption remain intact.

The Radio Browser error overlay now has a meaningful underlying Browser page,
so dismissing it cannot expose the hardware-observed blank grey screen. The
paired TUNER correction also raises the asynchronous per-transport connect
timeout from the hardware-failing 250 ms to a still-bounded 1,000 ms while
preserving the two-mirror policy and 30-second absolute job deadline. This is
an evidence-backed timing correction, but actual HTTPS retrieval remains a
hardware test rather than an offline claim.

Focused catalog-confirmation, Full-Control wire, source-confirmation, overlay,
Browser diagnostics, RTCP, BTX, D-009 and integration-policy tests pass. Clean
`remote01` production build at source
`91424013726cf9ede72de68d579842445141c2f2` passes: BIN 2,610,928 bytes,
SHA-256 `e7105144fcca1faf0d6a998b39603262d459132394d36da03cf833cb129508ef`;
ELF 20,057,180 bytes, SHA-256
`0380d8b56bd417e2bc15c1496ec1a0515d1693c075a6dd296331aafa02e1a206`;
61,180 bytes static RAM, 2,610,433 bytes linked program usage and 5,777,680
bytes exact-image slot headroom. Embedded application version is `9142401`.
No device was accessed or flashed. The pre-existing `utf8_snapshot_bounds`
failure remains separately tracked and unchanged.

## 2026-10-28 - Full Control 1.3 `+fc5` joint hardware validation PARTIAL / FAIL

Installed and independently read back the verified REMOTE application
`2ff3c9b`, 2,610,752 bytes, SHA-256
`81986adf59e75b6df1f9af8ca6397c15ca1deb0cccc240146bc68b0e03a9b8dd`,
after the paired TUNER installation. Startup and a 316-second uninterrupted
ACTIVE RTCP window passed; volume, source switching, Bluetooth phone-originated
state, disconnect/reconnect, catalog preservation and SYSTEM_SLEEP/touch-wake
recovery passed within the recorded limits.

Hardware validation remains FAIL overall. Favorite PLAY produced Station
Unavailable then a false disconnect with a 3,008 ms source-state timeout and
ECONNRESET. REMOTE and physical TUNER Bluetooth Play/Pause did not control the
phone. The single Radio Browser START failed both mirrors before HTTP response
with ESP_ERR_HTTP_CONNECT 0x7002; bounded TLS error categories were zero and do
not prove a TLS/certificate failure. Additional RTCP EOF/response-timeout
closes occurred outside the clean idle window. Full evidence is in
`docs/FULL_CONTROL_HARDWARE_VALIDATION.md`; no source/build/pairing/NVS change,
rollback, push, merge or release was performed.

## 2026-10-28 - Full Control 1.3 stability corrections offline verified

Added a four-entry, fixed-size RTCP close-history ring exposed by
`/api/rtcp-history`. Each retained REMOTE record includes uptime, session and
network generation, client state, close reason, socket error, request identity,
awaited response type and pending-response state. Immediate socket errors are
copied before cleanup. This is diagnostic instrumentation; it does not claim
that the remaining transport instability is fixed.

Full Control capability negotiation now uses `+fc5` / protocol 1.4 revision 5
with the deferred-catalog-play feature bit. REMOTE sends catalog PLAY only when
the TUNER advertises bounded deferred admission. Authentication, framing,
timeouts, reconnect policy, revision fencing, no-replay behavior and volume
semantics are unchanged.

Focused RTCP diagnostics (15/15), Full Control (50 assertions) and diagnostics
schema (9/9; 9,102 assertions; 1,000 cycles) pass. Clean `remote01` production
build at source `2ff3c9b0a0cfa07b2bd7282b1e41ef10f7b4c5c7` passes: BIN
2,610,752 bytes, SHA-256
`81986adf59e75b6df1f9af8ca6397c15ca1deb0cccc240146bc68b0e03a9b8dd`;
ELF 20,055,600 bytes, SHA-256
`fa76d949cd9e7933f7e2783dbf9bffc5cba0337404864cadc8c800c2a151de20`;
61,180 bytes static RAM, 2,610,245 bytes program usage and 5,777,856 bytes
exact-image partition headroom. Embedded application version is `2ff3c9b`. No hardware was
accessed or flashed. The separately tracked pre-existing
`utf8_snapshot_bounds` issue remains unrelated.

## 2026-10-27 - Full Control 1.3 targeted corrections offline verified

Corrected two confirmed REMOTE defects. The Radio Browser list overlay now
intercepts pointer input locally, so visually covered TUNER CONTROL rows cannot
receive blank-area touches while visible Browser rows, scrolling, BACK and EXIT
remain interactive. Source selection now retains a bounded confirmation fence
and completes only after both command acceptance and a fresh authoritative
snapshot of the requested source (in either arrival order). Reconnect performs
read-only reconciliation and never replays the source mutation; confirmed
success retains the approved three-second HOME return.

Added bounded REMOTE popup lifecycle evidence (reason, lifecycle transition,
session/close reason and elapsed time from the relevant command), and bounded
TUNER response-duration measurements for source, catalog and Browser commands.
These are diagnostic-only and do not suppress the observed Favorites disconnect
popup. TUNER Browser diagnostics now retain the first HTTPS transport failure,
mirror/stage/error/status, final result, elapsed time and whether the single
whole-job 30-second deadline expired. The pinned Arduino/ESP-IDF client API does
not safely expose a TLS last-error category, so TLS-specific fields remain
explicitly unavailable rather than guessed. The 250 ms transport timeout,
certificate bundle, mirror set, two-mirror policy and protocol are unchanged.

The Browser mirror deadline defect is fixed offline: one absolute job deadline
is created once and is not restarted on fallback. The original `error 8`
hardware cause remains diagnostic-only until one controlled ESP32 attempt
captures the new fields.

REMOTE `/api/status` 503 serialization overflow was reproduced at the former
3,056-byte and intermediate 4,080-byte capacities using maximum-width counters
and long uptime. Fixed diagnostic storage remains exactly 8,192 bytes by
shrinking the event entry from 128 to 24 bytes and assigning a verified bounded
6,384-byte response area. HTTP 503 bodies now distinguish storage unavailable,
zero-wait lock contention and serialization overflow; JSON is never truncated.

Focused Full Control, overlay, source-confirmation, Browser, popup, diagnostic,
D-009, BTX, HOME, sleep/wake, volume and ownership tests pass. The broad runner
passed Core 69/69, Profile 6/6, HOME 42/42 and RTCP ownership 4/4 before stopping
at the separately tracked pre-existing `utf8_snapshot_bounds` assertion; no
related code changed. Clean `remote01` production build at source
`42c31034c64235d0625f226978e6d8900dc18195` passes: BIN 2,608,800 bytes,
SHA-256 `99fe856562d62fbd138e7529080dc00baf65c647ce11cd8419b1fc63d070e8b9`;
ELF 20,025,164 bytes, SHA-256
`ef0a9553cc90d887b51099e598eb2b90b85d141525eccfc469829234f489c094`;
61,180 bytes static RAM and 5,779,808 bytes exact-image slot headroom.
This checkpoint is offline only; no device was accessed or flashed.

## 2026-10-26 - Full Control targeted popup/reconnect fixes and Browser diagnostics

Corrected two hardware-confirmed REMOTE defects without changing the approved
Full Control behavior. Shared compact popup text now uses a bounded multiline
label instead of the one-line helper, preserving the popup, spinner and button
layout. Catalog PLAY now retains a fixed confirmation fence for up to 15
seconds and, after RTCP reconnect, performs read-only catalog reconciliation.
It returns HOME only after fresh authoritative evidence confirms that the
selected item is active and playing. It never replays PLAY or retries a
mutation; different, stopped, stale or expired evidence fails closed.

Added fixed 16-byte Browser diagnostics to the existing REMOTE diagnostic
snapshot and `/api/status`: START results, page queries sent, page responses,
pages accepted, generation mismatches, the last mismatched generation/revision
low 16 bits, UI state and pending operation. Counters saturate, storage remains
exactly 1,024/8,192 bytes, and no RTCP framing, security or Browser policy was
changed. This is diagnostic-only for the empty Country panel; its cause is not
claimed fixed.

Focused popup (6/6), catalog reconciliation, Browser diagnostic policy (10/10),
diagnostic schema (9/9; 9,098 assertions), Full Control, navigation, D-009, BTX,
HOME metadata and volume tests pass. The broad runner retains its reproduced
pre-existing `utf8_snapshot_bounds` failure and no related code was changed.
Clean `remote01` build at `65923ad` passes: BIN 2,608,160 bytes, SHA-256
`38e165da601756d0b4a2f56be97d8d44a0d42fdf16cb7c871d37a29bbbc53774`;
ELF 20,022,008 bytes, SHA-256
`c08f3fa41e05b887138e6c38a560dbf1ba875c67becfa1f744a8605ea41a293b`;
61,100 bytes static RAM and 5,780,448 bytes exact-image slot headroom. This is
offline verification only; no device was accessed or flashed.

## 2026-10-26 - Full Control Milestone 5 Radio Browser offline complete

Implemented the binding Country (Any/A-Z), Name (Popular/A-Z), Genre
(Any/list) and paged Results workflow without coupling REMOTE navigation to
the physical TUNER menu. Full-Control 1.3 (`+fc4`) adds authenticated bounded
browser IDs 26-28, five-item pages, session generation/revision fencing,
short-tap play, two-second add-to-Favorites, active highlighting, and the
specified loading, empty, error, BACK/EXIT, disconnect and HOME behaviors.
Stale work is cancelled or ignored and mutations remain single-in-flight with
no retry or replay.

The TUNER path is a separate cooperative asynchronous HTTPS service rather
than the legacy synchronous physical-browser path. It uses verified TLS, a
30-second absolute deadline, two mirrors, fixed result limits and PSRAM-only
bounded body/JSON/catalog storage. It does not stop playback and is polled by
the normal application loop so RTCP and physical controls continue to receive
service. Hardware timing and memory coexistence remain to be measured.

Focused browser, Full Control, navigation, D-009, BTX and UI tests pass. The
full REMOTE runner's sole `utf8_snapshot_bounds` failure was reproduced at
pre-Milestone-5 commit `fefaae6` and is not caused by this change. Production
REMOTE build `fdcbdac` passes: 2,606,192-byte BIN, SHA-256
`dc80ba560a3327a069c911de0af31d20eeece77d8f7681ab4d54cbce3ed1c270`;
20,006,100-byte ELF, SHA-256
`3ee7c94c4a43b61ea72ca778501585a9d231ebb303aeb405b161d6a0932f3056`;
61,052 bytes static RAM and 5,782,416 bytes exact-image slot headroom. This is
an offline result only; no firmware was installed and hardware validation is
pending.

## 2026-10-26 - Full Control independent navigation approved

Recorded the owner-approved architectural clarification in the binding
specification: REMOTE navigation is local and never moves the physical TUNER
cursor or e-paper menu. Both interfaces remain usable simultaneously. Shared
source, active station, metadata, volume and station-list mutations remain
TUNER-authoritative; mutations retain revision/generation fencing and are never
replayed automatically. Existing touchscreen behavior is unchanged. Review of
the outstanding diff found no unsafe raw-menu-navigation source draft.

## 2026-10-25 - REMOTE Full Control station catalogs; Radio Browser blocked

Implemented the safe offline scope of Milestones 3 and 4 on top of the binding
Full Control specification. Capability-negotiated Full-Control 1.2 (`+fc3`)
adds fixed, authenticated, paged Saved Stations, Favorites and Recent queries
and semantic play/remove/add-favorite commands. The TUNER remains authoritative;
physical control wins through catalog revision fencing, mutations use the
existing one-request lane, and timed-out or disconnected work is never replayed.
No URLs or raw input events cross RTCP and no NVS layout changed.

The portrait REMOTE UI now provides local BACK/EXIT navigation, the HOME
Favorites shortcut, bounded station rows, active highlighting, two-second
press recognition with scroll cancellation, confirmed deletion and the
approved empty/full/already-present results. Station play waits for
authoritative PLAYING and active-station evidence. The global volume encoder,
HOME, D-009, BTX, sleep/wake and disconnect ownership remain intact.

Executable Full-Control, navigation/gesture, D-009 and BTX regressions pass on
both repositories. The shared result parser intentionally accepts new bounded
catalog errors 8 and 9; the stale D-009 test expectation was corrected without
changing any D-009 record. Final production builds pass. REMOTE is 2,598,176
bytes (SHA-256 `041d112a4b676956ed5265cd32f89fc423a2892dcc58b9d523901df29bcd7a8f`),
uses 59,468 bytes static RAM and retains 5,790,432 bytes in the 8 MiB factory
application partition.

Radio Browser Milestone 5 is **BLOCKED**. Its existing TUNER implementation is
a synchronous main-loop HTTPS operation with 20-second connect/read timeouts,
whole-response `StreamString` buffering and dynamic JSON parsing, while RTCP
uses 5-second response deadlines and the platform has verified very low
internal-memory watermarks. No safe cooperative/cancellable job exists. The UI
entry remains disabled rather than bridging that blocking path or weakening
the approved specification. No hardware was accessed and this is not a full
or hardware-validated candidate.

## 2026-10-25 - REMOTE Full Control milestone 2 protocol projection (offline)

Added the bounded authenticated read-only menu projection needed by later
station-list UI. The TUNER supplies its canonical mode, selection, count,
revision and at most five names; REMOTE validates and copies that fixed window
through its owner boundary. No URL, raw input, mutation, heap allocation or
automatic command was introduced. `+fc2` isolates the new capability mask
from Milestone 1 clients while current REMOTE remains compatible with both
extension generations. Focused TUNER and REMOTE model tests pass. Approved
list rendering and semantic actions remain pending and no hardware result is
claimed.

## 2026-10-25 - REMOTE Full Control permanent specification established

Added the complete owner-approved behavior as the binding
`docs/REMOTE_FULL_CONTROL_SPEC.md`, plus a separate milestone, protocol, test,
build and limitation tracker in `docs/REMOTE_FULL_CONTROL_IMPLEMENTATION.md`.
README now requires future UI/navigation/source/station work to consult the
specification. Unimplemented and hardware-untested behavior remains explicitly
marked; no approved requirement was weakened to match the current partial
implementation.

## 2026-10-25 - REMOTE Full Control milestone 1 (offline)

Added a legacy-safe, capability-negotiated source-control extension and the
approved portrait TUNER CONTROL entry point. HOME now labels its third row
`TUNER CONTROL`; DEVICES remains reserved. Radio/Bluetooth selection uses the
existing authenticated single-request lane with generation fencing, no
optimistic source change, no automatic retry and authoritative completion.
Compact progress/error/success popups, three-second HOME return, disconnected
handling, wake-to-HOME behavior, and the Bluetooth waiting presentation are
implemented. List entries are deliberately disabled until their canonical
TUNER projections are implemented. Focused wire tests cover record bounds,
malformed input, discovery compatibility and source vectors. Hardware remains
untouched and validation is pending.

## 2026-10-25 - BTX Radio playback ownership hardware validation PASS

Installed the playback-ownership correction from implementation commit
`596cb71be867e2b5ff6eb3f6c0dde52d6dd8a770` on the verified REMOTE ESP32-S3.
The application-only write at `0x10000` and independent exact 2,583,008-byte
readback both passed SHA-256
`fad87bb2f50142f3758945c5a2276a2535008aec4f32a5ec29f021faa95f3b3f`.
Runtime reported embedded build `596cb71` and the expected ELF identity. Stored
Wi-Fi, pairing, HOME, display, touch, authenticated RTCP and authoritative
volume were retained.

With Radio ABF HTTPS MP3 playing, REMOTE preserved `PLAYING`, enabled Pause,
correct Radio metadata and authoritative volume across three RTCP cycles. The
same state survived display-idle and touchscreen wake. One Radio Pause and one
Play were accepted and reconciled by TUNER without `UNKNOWN`, reconnect or
volume regression. Bluetooth then showed the phone metadata and `PLAYING`;
one REMOTE Pause/Play controlled the phone, and one clockwise encoder detent
changed authoritative phone volume from 80% to 82%. After Bluetooth-to-Radio,
three further cycles retained Radio ABF metadata, `PLAYING`, enabled Pause and
82% volume with no stale Bluetooth metadata. REMOTE RTCP session 2 remained
READY and its close/attempt counters stayed at 255 throughout the bounded
test. TUNER remained on build `dbf9896876a7`, with continuous uptime, zero
Wi-Fi disconnects and no reset; COM5 was not accessed.

Motion wake did not operate during the display-idle check. Read-only evidence
showed `IMU=0`, zero motion events and otherwise healthy RTCP; touchscreen wake
passed. This preserves the previously documented intermittent IMU
initialization/motion-wake limitation as a separate open issue and is not
attributed to the playback-ownership correction. TUNER internal free/largest
heap varied with source transition, reaching an observed 6,640/2,292 bytes
before recovering to 9,152/6,644 bytes while Radio played. No reset, Wi-Fi
disconnect, HTTP/RTCP failure or audio interruption occurred, but the current
HTTP diagnostics do not expose allocation-failure or Wi-Fi-TX-refusal counts.

## 2026-10-25 - BTX radio playback ownership correction

Corrected the REMOTE adapter's mixed D-009/BTX ownership boundary. Authenticated
BTX state records whose source is Radio no longer publish absent Bluetooth
playback or metadata over fresh D-009 observations, and their independent
generation is no longer admitted into the D-009 media fence. Bluetooth remains
BTX-authoritative. Separate bounded D-009 and BTX fences preserve command
generation fencing, while the latest D-009 snapshot supports a deterministic
Bluetooth-to-Radio handover without an `UNKNOWN` overwrite. Disconnect and
session replacement still clear both fences, cached state and pending work.

The executable real-adapter regression passed 4/4 cases with 92 assertions,
covering playing/paused/stopped Radio, repeated mixed responses, Bluetooth
authority, bidirectional handover, independent generation domains,
display-idle rendering continuity, reconnect invalidation and volume
synchronization. Existing BTX wire tests passed 7/7 (52 assertions), D-009 wire
tests 8/8 (44), HOME/Core 42/42 (3,565), BTX policy 15/15, D-009 policy 16/16,
HOME metadata policy 15/15, UI volume binding 1/1 (11), and RTCP
freshness/volume admission 2/2 (221). The broader hardening suite retained one
unrelated UTF-8 snapshot-length assertion failure; an isolated build from the
previous `349c379` commit reproduced the identical failure (25/26 cases), so no
unrelated source was changed.

The clean `remote01` build from implementation commit
`596cb71be867e2b5ff6eb3f6c0dde52d6dd8a770` passed with no compiler/CMake
warning or error lines. The application is 2,583,008 bytes, SHA-256
`fad87bb2f50142f3758945c5a2276a2535008aec4f32a5ec29f021faa95f3b3f`;
the 19,820,532-byte ELF SHA-256 is
`1bbb0cdc2eb1035c30d46e985369f855f7a10ee98d5bdefdfb592af10a08bd02`.
Static RAM remains 56,652 bytes. Each 3 MiB application slot retains 562,720
bytes; the installed 8 MiB factory partition would retain 5,805,600 bytes.
Embedded build identity is `596cb71`. This is an offline candidate only;
hardware installation and the bounded Radio/Bluetooth/display-idle retest remain
pending separate authorization.

## 2026-10-25 - BTX hardware installation verified; radio state/control regression

Mandatory preflight identified REMOTE as the ESP32-S3 QFN56 revision 0.2
native USB Serial/JTAG device on COM3 with 16 MiB flash. The live partition
table retained the 8 MiB factory application at `0x10000`. A fresh 16 MiB
full-flash backup was captured before writing; its exact Round 5 application
span matched SHA-256
`6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`.
Separate NVS, OTA metadata and `remote_ctl` copies were preserved privately.

The exact 2,582,432-byte BTX application from source head
`2200bc8df02262e076ecdbd3c9fd66eac4c5d3b3`, SHA-256
`5d84f83b8e9e6f860e7cd807a270eeee697ed42dc361bbb1e1022f639dc4206b`,
was written once at `0x10000`. No other partition was written. Independent
exact-span readback matched the candidate SHA-256. Build `5b32cbb` booted with
retained Wi-Fi, pairing and configuration. RTCP reached authenticated READY,
authoritative volume and `volume.set` were available, and no command was
outstanding. Display, touch and HOME behavior passed owner inspection.

During Radio ABF HTTPS MP3 playback, TUNER continued playing and RTCP remained
authenticated/READY, but after REMOTE woke from display-idle the green HOME
playback label showed `UNKNOWN` instead of `PLAYING`. The Play/Pause control
was disabled/unavailable, so the authorized radio-control check could not be
performed. This is an OPEN REMOTE UI/state-synchronization defect, not a proven
RTCP protocol failure. The commissioning loss-of-existing-function stop
condition was applied. Bluetooth UI/metadata/AVRCP and further mode-transition
tests were not run. No source investigation, rebuild, additional flash,
rollback or TUNER serial/flash access occurred. Hardware verdict: **FAIL**.

## 2026-10-23 - BTX/1.0 REMOTE Phase 6B installation

Installed the exact 2,582,432-byte BTX/1.0 application at `0x10000` on the
verified REMOTE ESP32-S3. The single application-only write completed and an
independent 2,582,432-byte readback matched SHA-256
`5d84f83b8e9e6f860e7cd807a270eeee697ed42dc361bbb1e1022f639dc4206b`.
Boot diagnostics reported embedded build `5b32cbb` and the expected ELF
SHA-256. Stored Wi-Fi and pairing were retained; Wi-Fi connected without a
reconnect or error, and RTCP completed authentication and identity acceptance.

The bounded automated checkpoint did not obtain a fresh authoritative volume
observation: RTCP was authenticated and transport-connected, but
`volume_known` and `volume_set` remained unavailable. BTX negotiation is not
separately exposed by the existing status interface. Installation/readback is
verified; control, Bluetooth negotiation/metadata, physical controls and
coexistence remain pending owner validation. No TUNER access or write occurred.

## 2026-10-23 - BTX/1.0 REMOTE software implementation

Implemented the approved authenticated BTX/1.0 extension on the verified
A-REMOTE-013 D-009 Round 5 baseline. The existing identity and first legacy
capability exchange remain unchanged; REMOTE immediately performs the specified
second authenticated capability probe. Exact ID 12 enables BTX for that
session, a second legacy ID 4 cleanly retains D-009-only behavior, and malformed,
unexpected or interrupted negotiation fails closed. Negotiated authority is
cleared on disconnect, session replacement and sleep teardown.

The RTCP codec now strictly validates IDs 12-15, including exact lengths,
versions, feature and reserved bits, enums, capability/current-availability
separation, canonical source/connection/playback combinations, bounded UTF-8
metadata, sequence and generation fields. Bluetooth commands use ID 15 and the
existing correlated ID 8 result. Existing authentication, replay protection,
single request in flight, no automatic retry and D-009 radio semantics remain
unchanged.

HOME reuses its source and StreamTitle widgets. Bluetooth presents the literal
`Bluetooth` and the approved artist/title mapping in the existing bidirectional
scroll path; authoritative state changes and lifecycle invalidation clear stale
metadata. Play, Pause, Previous and Next use fresh authenticated BTX authority.
The existing source-independent volume path, fixed 2% detents, UI geometry,
power behavior and radio controls are unchanged.

Focused verification passed: BTX codec/negotiation 7/7 with 52 assertions;
BTX integration policy 15/15; reference codec 17/17; HOME metadata policy
15/15; D-009 policy 15/15; HOME/Core 42/42 with 3,565 assertions; encoder and
volume routing 10/10 with 98 assertions; RTCP D-009 8/8 with 44 assertions;
volume admission 12/12 with 323 assertions; RTCP freshness/admission 2/2 with
221 assertions; POWER 20/20 with 19,244 assertions; and physical input 19/19
with 24,627 assertions. Approved coordination and TUNER vectors were
byte-identical, SHA-256
`b09f93919e5f91a25531e40352e29043a0dd3f31461e3dd152be6b9bf10aaf34`.

The exact clean `remote01` production build from source
`5b32cbbeb554cd8a4d49094a5bd7c4797fd0b403` passed. The private application is
2,582,432 bytes with SHA-256
`5d84f83b8e9e6f860e7cd807a270eeee697ed42dc361bbb1e1022f639dc4206b`;
ELF SHA-256 is
`e4139e2a50d3b6a439c19ade8792a98c2fc67ff9417e57e02df0a9977100ee57`.
Static RAM is 56,652 bytes and the 3 MiB slot retains 563,296 bytes. Measured
bounded persistent-state growth is 232 bytes against the 320-byte limit; the
temporary HOME snapshot grows 192 bytes against the 195-byte limit. No new
task, transport buffer, unbounded queue or metadata allocation path was added.
No hardware access or flash operation occurred; BTX hardware commissioning is
still required. See [the BTX handoff](docs/BTX_1_0_REMOTE_IMPLEMENTATION.md).
## 2026-10-23 - A-REMOTE-013 D-009 Round 5 partial hardware pass

Installed the exact 2,578,352-byte Round 5 application once at `0x10000` on
the verified 16 MiB REMOTE ESP32-S3. Independent exact-span readback matched
SHA-256
`6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`.
Stored Wi-Fi, secure pairing and authenticated RTCP were retained; authoritative
volume and the existing HOME UI recovered normally.

Owner-observed hardware PASS covers carousel navigation, brightness, touch,
HOME index 2, encoder volume, bidirectional radio Play/Pause state/control,
station and combined StreamTitle presentation/scrolling/invalidation, discovered
context Previous/Next withdrawal, TUNER-restart invalidation/reconnect, A2DP
coexistence and return to radio. Bluetooth metadata and AVRCP are outside D-009.
Fixed saved-list Previous/Next/wrap, forced invalid-command Favorites proof,
REMOTE sleep/wake/restart and representative resource/stability acceptance
remain untested. The session is closed as PARTIAL HARDWARE PASS; A-REMOTE-013
hardware acceptance remains pending. See
[the Round 5 report](docs/A_REMOTE_013_D009_ROUND5_HARDWARE_VALIDATION.md).

## 2026-10-22 - A-REMOTE-013 D-009 hardware-regression round 5

Round 4 was installed application-only and independently read back with the
expected 2,578,304-byte SHA-256 identity. Boot, retained Wi-Fi and authenticated
RTCP synchronization passed. Subsequent owner observations found that manually
counted rotations sometimes produced nine decoded detents from approximately
ten physical detents, requested a fixed two-percentage-point volume step, and
found encoder use did not refresh display inactivity.

**A - encoder reliability:** source and deterministic tests do not establish a
decoder defect. GPIO2/GPIO5 remain input-only with internal pull-ups and a 2 ms
polling task. The decoder preserves partial quarter cycles, emits only after a
complete four-transition cycle, cancels opposing partial motion, and rejects a
diagonal transition without inventing a detent. The measured 39 transitions,
9 detents and 1 invalid transition are reproducible as nine complete cycles,
one ambiguous diagonal and a two-quarter incomplete cycle. Manually counted
mechanical detents are not waveform evidence, so the decoder, threshold,
sampling rate and direction mapping are unchanged. A logic-analyzer capture of
GPIO2/GPIO5 is still required to distinguish missed electrical transitions
from an incomplete or ambiguous physical cycle.

**B - fixed volume step:** `control_home::Channel` now applies exactly two
percentage points per accepted `VOLUME_UP`/`VOLUME_DOWN` intent at the local
desired-target accumulator. It clamps to 0..100 while preserving absolute RTCP
volume semantics, bounded coalescing, one command in flight, command readiness,
session/capability fences, reconciliation and no automatic retry. Slow and
rapid sequences with identical detent counts converge on the same target.

**C - display activity:** each valid decoded encoder cycle now increments the
same thread-safe physical-activity counter used by touch. The POWER owner
refreshes the existing 30-second display-idle timer or follows the existing
display-idle wake path; invalid transitions do neither. No LVGL call is made
from the encoder task, and no new timer, queue, allocation or wake source was
added. GPIO2/GPIO5 are not SYSTEM_SLEEP wake sources, so encoder rotation is
not claimed to wake a fully sleeping ESP32-S3.

Focused verification passed: encoder routing/decoder/activity and RTCP
coalescing 10/10 with 98 assertions; volume admission, fixed-step bounds,
reconciliation and lifecycle fencing 12/12 with 323 assertions; physical input
19/19 with 24,627 assertions and 1,000 cycles; POWER 20/20 with 19,244
assertions and 1,000 cycles; HOME/Core 42/42 with 3,565 assertions; RTCP D-009
8/8 with 44 assertions; D-009 policy 15/15; encoder activity policy 9/9; HOME
metadata policy 15/15; and UI volume binding 1/1 with 11 assertions. The single
`remote01` production build passed at source
`bfce47816a8b2b8a92f41d76c6a4bc4975d25361`.

The private Round 5 application is 2,578,352 bytes with SHA-256
`6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`;
ELF SHA-256 is
`9499b13b9c4f6fc3ece4c19d75b4303c13ab06639e4b66f0e4e1cfd0128d4acf`.
Static RAM remains 56,460 bytes, the 3 MiB application slot retains 567,376
bytes, and diagnostics snapshot/storage remain 1,008/8,176 bytes. No hardware
access or flash operation occurred in Round 5. Encoder reliability, the fixed
step and display activity remain hardware pending.

## 2026-10-22 - A-REMOTE-013 D-009 hardware-regression round 4

Round 3 hardware testing decoded and admitted all ten rapid encoder detents,
but completed only four volume operations; six ended as definite failures and
the authoritative volume moved only four points. The read-only diagnostic
event ring retained only `VOLUME_FAILURE` detail 13
(`GUARANTEED_NO_EXECUTION`) for this failure class. No queue rejection,
session/capability/authority/divergence cancellation, authenticated TUNER
refusal, or RTCP session loss accompanied the rapid-test delta.

Source tracing demonstrated the remaining deterministic race. After an
authoritative snapshot reached one coalesced target, `Channel::publish()`
queued the accumulated successor. Before the owner popped that successor,
`Client::poll()` could occupy the single authenticated record lane with its
automatic snapshot or D-009 now-playing request. `RtcpDevice::dispatch()` then
called `Client::set_volume()` while `awaiting_` was true, received `false`, and
reported `RTCP_NOT_SENT`; Core correctly closed the attempt as failed with
guaranteed no execution. The operation therefore failed locally before any
volume command reached TUNER.

The owner loop now retains its bounded intent while the RTCP client is not
command-ready. A waiting control also suppresses creation of another
background snapshot/metadata request until the current authenticated exchange
finishes, after which the original intent crosses the existing Facade/Core
admission path once. This is pre-dispatch scheduling, not command retry. The
one-command-in-flight invariant, three-second command deadline after capture,
session and authority fences, disconnect/sleep cancellation, external-change
reconciliation, correlated results, and RTCP wire/security behavior are
unchanged.

Focused verification passed: encoder/Facade/transport scheduling 9/9 with 94
assertions; volume admission and lifecycle fencing 11/11 with 290 assertions;
D-009 policy 15/15; and HOME metadata policy 15/15. The single `remote01`
production build passed at source
`bd15cc8906c244152ccadc1a6734bb9c1d4bf998`.

The private Round 4 application is 2,578,304 bytes with SHA-256
`1fa0432b8785bbfce8aecb22e3f5b747080898fdfd850eb4d93ac965b49a30b7`;
ELF SHA-256 is
`7fe9aaaebd2b61dddfc3a414fd41f89fc8f89d1500ae3bcef9ee7b9425cd9cd7`.
Static RAM remains 56,460 bytes, the 3 MiB application slot retains 567,424
bytes, `diagnostics::Snapshots` remains 1,008 bytes, and total diagnostics
storage remains 8,176 bytes. No hardware access or flash operation occurred.
The owner-verified HOME scrolling results remain PASS and that source was not
changed. Rapid-volume hardware acceptance of this candidate remains pending;
no hardware fix is claimed.

## 2026-10-22 - A-REMOTE-013 D-009 hardware-regression round 3

Round 2 hardware counters resolved the remaining volume-loss boundary. Across
26 queued targets, all 14 requests that crossed the RTCP boundary completed;
the other 12 failed locally before dispatch. No authenticated TUNER refusal or
volume cancellation was recorded. Source tracing reproduced a race in which an
unrelated D-009 playback-context update advanced the target-wide capability
revision after `Channel::pop()` but before `Facade::handle()`. The Facade then
rejected a still-valid absolute volume target even though the authenticated
session and fresh volume authority were unchanged.

The Facade now revalidates same-session absolute volume targets against current
volume authority and the current observation precondition. Non-volume commands
retain exact capability-revision matching, and session fencing, strict Core
admission, authenticated target refusals, one-command-in-flight behavior, and
no automatic retry are unchanged. A bounded event detail distinguishes local
admission/capture failures from authenticated target refusals and later
terminal failures without enlarging `diagnostics::Snapshots`; added snapshot
storage overhead is zero bytes.

The existing 148-pixel station and StreamTitle labels now use LVGL's clipped,
bidirectional `LV_LABEL_LONG_SCROLL` mode at 25 pixels/second with one-second
endpoint pauses. Replacement metadata restarts at the first character, and
scrolling stops outside HOME and while display commands are unavailable. The
existing widgets, fonts, coordinates, assignment, buffers, UTF-8 validation,
touch behavior, and carousel remain unchanged.

Focused verification passed: encoder/Facade 8/8 with 47 assertions; volume
admission 11/11 with 290 assertions; diagnostics 9/9 with 9,096 assertions and
1,000 cycles; HOME/Core 42/42 with 3,565 assertions; RTCP D-009 8/8 with 44
assertions; D-009 policy 15/15; HOME metadata policy 15/15; and UI volume
binding 1/1 with 11 assertions. The single `remote01` production build passed
at source `3a2fda5887df8671467cf36917f607e6d1b69b11`.

The private Round 3 application is 2,578,128 bytes with SHA-256
`6ba73c80b774727cc137f5606b408f6cfc3ff0ea486ef879869453a5debb0383`;
ELF SHA-256 is
`e85165c88f1ec3410c1e9f304a4cd224a466bb3f95225045a2aeccb37d074ead`.
Static RAM is 56,460 bytes and the 3 MiB application slot retains 567,600
bytes. This is +624 application bytes and -624 bytes of headroom versus the
preserved Round 2 candidate, with unchanged static RAM. No hardware access or
flash operation occurred. Hardware verification of rapid volume input,
metadata scrolling, lifecycle behavior, and the remaining D-009 functions is
still required; no hardware PASS is claimed.

## 2026-10-21 - A-REMOTE-013 D-009 hardware-regression round 2

Owner testing of the first correction superseded its encoder conclusion. Rapid
rotation remained intermittent: many physical detents sometimes produced only
one or two displayed volume steps. Direction reversal, Play/Pause availability,
Play/Pause execution, and station/StreamTitle assignment passed. Long combined
StreamTitle text still exceeded the physical presentation boundary.

The encoder decoder, GPIO configuration, 2 ms sampling task, and command path
are unchanged from the owner-accepted A-REMOTE-012 implementation. Existing
diagnostics could count sampled transitions, decoded detents, and admitted
intents, but could not distinguish skipped/invalid quadrature transitions,
scheduler gaps, local admission rejection, or later command cancellation. No
new decoder or CCM behavior change is justified by the available evidence.
Bounded diagnostics now expose invalid transitions, direction counts, rejected
emissions, last/maximum sample intervals, and the queued/completed/failed/
cancelled/authority volume pipeline. The counters allocate no hot-path memory,
perform no per-transition logging, and retain the 1,024-byte snapshot limit.

The lower HOME metadata widget used clip-only long-text handling while the
upper station widget used bounded ellipsis. The existing 148-pixel lower widget
now uses `LV_LABEL_LONG_DOT`; its coordinates, font, assignment, carousel,
artwork, navigation, buffers, and validated UTF-8 ingestion remain unchanged.

Focused verification passed: encoder 6/6 with 30 assertions; volume admission
11/11 with 290 assertions; diagnostics 9/9 with 9,096 assertions and 1,000
cycles; HOME 42/42 with 3,565 assertions; ingress/UTF-8 18/18 with 22,189
assertions and 1,000 stress iterations; D-009 policy 15/15; metadata UI bounds
8/8. One `remote01` production build passed at source
`434b1359ba78ebe517f7dfe47423067a99b745f8`.

The private diagnostic candidate is 2,577,504 bytes with SHA-256
`8c529cc5c16ca3e0a3487f02b975187285eddcdd348ba7ae703d5ccc93b2b4c2`;
ELF SHA-256 is
`978a36934c5106d01a7d61870ec92cf355fec6222f8d2556fd9c18b82ea35d7e`.
Static RAM is 56,460 bytes, the diagnostics snapshot is 1,008 bytes, and 3 MiB
slot headroom is 568,224 bytes. This is +2,096 application bytes, +104 static
RAM bytes, and -2,096 bytes headroom versus the installed correction. Hardware
installation and reproduction remain separately authorized and pending; the
encoder regression is not declared solved.

## 2026-10-21 - A-REMOTE-013 D-009 hardware-regression correction

The first D-009 REMOTE application was installed and read back exactly at
2,575,168 bytes with SHA-256
`d9c13a6ce030b9ab2a4db02fad1c5afcf80eee364f0046665a9dcb1652698d76`.
Hardware confirmed authenticated TUNER connection, station and combined ICY
StreamTitle updates, and HOME Play/Pause. It also exposed three REMOTE
regressions: rapid encoder input lost accumulated volume targets, every volume
request temporarily dimmed the playback controls, and the station/StreamTitle
labels were presented in the opposite visual positions.

The bounded correction at source
`5b3c7c08d06fe0395409090221594f9e5bbd8ace` preserves an admitted volume
target across unrelated target-wide D-009 capability revisions while retaining
connection, session, authority, external-divergence and no-replay fences.
Queued volume intent is refreshed to the current capability revision before it
crosses the owner boundary. RTCP pending state no longer masquerades as loss
of fresh playback capability, and volume/playback snapshot observations are
published independently. The existing HOME widgets now present station name
in the large upper label and the combined StreamTitle in the smaller lower
label; CCM field semantics, geometry and generated UI resources are unchanged.

Focused verification passed: volume admission 10/10 with 267 assertions,
HOME/Core 42/42 with 3,565 assertions, RTCP D-009 8/8 with 44 assertions, and
D-009 source-boundary policy 14/14. This includes rapid in-flight accumulation,
successor delivery, direction reversal, external divergence, session/lifecycle
cancellation, pending playback presentation, independent snapshot admission,
metadata mapping, and existing malformed/over-limit UTF-8 handling. The single
authorized `remote01` production build passed.

The private corrected application is 2,575,408 bytes with SHA-256
`250f55bd4ce0edab9c77e59b227c2c7c9d5b3bfbd2780795dc3dd654203b80a7`;
the ELF SHA-256 is
`1e55ee1e6f06ab9ef38a20a7f05b5f53feb2e7e49ec93375f077ba070b2da508`.
Static RAM remains 56,356 bytes and 3 MiB slot headroom is 570,320 bytes.
Relative to the installed D-009 candidate this is +240 application bytes,
unchanged static RAM, and -240 bytes headroom. Hardware retesting of the
corrected application remains required; it has not been flashed or claimed as
hardware validated. The installed candidate and the accepted A-REMOTE-012
rollback (`b6053c04c03114a24a59f178da4eb03d4bada77faded43b6009667579c21d5da`)
remain preserved.

## 2026-10-21 - A-REMOTE-013 D-009 REMOTE software candidate

Implemented the frozen RTCP/1 D-009 playback and now-playing profile on top of
the A-REMOTE-014 discovery candidate. RTCP message IDs 9-11, capability
revision 2 feature gating, authenticated play/pause/previous/next commands,
strict result correlation, fixed-list restrictions, and authoritative station
and now-playing ingestion are integrated through the existing CCM Core and
HOME screen at carousel index 2. Existing HOME controls and text widgets are
reused without layout or generated-resource changes. Physical REMOTE buttons
remain unassigned.

The implementation preserves the 512-byte plaintext limit, 556-byte record
buffers, one command in flight, session/generation fencing, no automatic
command replay, existing volume responsiveness, and LVGL task ownership.
Malformed or over-limit metadata is rejected, presentation remains bounded and
UTF-8 safe, and stale metadata/pending commands are cleared across connection
and session replacement.

Focused verification passed: RTCP D-009 8/8 with 44 assertions; D-009 policy
12/12; HOME 41/41 with 3,562 assertions; volume admission 9/9 with 238
assertions; UI volume binding 1/1 with 11 assertions; power 19/19 with 19,236
assertions and 1,000 stress iterations; ingress 18/18 with 22,189 assertions
and 1,000 stress iterations. The single authorized `remote01` production build
passed at source `1a29c4e3652fb3aeb7ef496b674691e78aca2d67`.

The private application is 2,575,168 bytes with SHA-256
`d9c13a6ce030b9ab2a4db02fad1c5afcf80eee364f0046665a9dcb1652698d76`,
uses 56,356 bytes of static RAM, and leaves 570,560 bytes in the 3 MiB
application slot. Relative to the accepted A-REMOTE-012 baseline, this is
+48,320 application bytes, +8,968 static RAM bytes, and -48,320 bytes of
headroom. The generated partition table remains the accepted seven-entry image
with SHA-256
`a144343580dbdadace66fccc5969d7557de3d4c4f15a61e76db205cc26348184`.

No firmware was flashed and no hardware, NVS, pairing, partition, credential,
TUNER source, or protocol-security state changed. Hardware interoperability and
rollback validation remain required. The coordination documents that still
describe D-009 implementation as unstarted predate the owner's explicit REMOTE
implementation authorization and are preserved as historical status.

## 2026-10-20 - A-REMOTE-014 authenticated RTCP endpoint discovery

REMOTE now performs bounded native ESP-IDF mDNS/DNS-SD discovery for
`_rtcp._tcp` after Wi-Fi becomes available. It accepts at most four IPv4
candidates per 750 ms query, queries no more often than every five seconds,
and retains the saved association endpoint as a fallback. A candidate becomes
the runtime endpoint only after the existing RTCP mutual proof and encrypted
identity exchange succeed. Discovered addresses are never written to pairing
NVS, and discovery/session state is cleared on confirmed network loss,
generation change, or sleep.

Endpoint-selection tests passed 10/10, discovery-policy checks passed 10/10,
network-status checks passed 15 checks/142 assertions, RTCP association/wire
tests passed 11/11, and lifecycle-policy checks passed 13/13. The pinned
`remote01` clean production build passed at source
`9c805e5444587bc3d8952a57f44ec526c615e74a`, producing a 2,568,992-byte
private candidate with SHA-256
`cdfccacdf07d0e4ca0a29b2c92f3cf29ee5e1b65df109ca730c3915185c82e2d`.
Static RAM is 56,356 bytes and OTA-slot headroom is 577,056 bytes; relative to
the accepted A-REMOTE-012 application this is +42,144 binary bytes, +8,968
static RAM bytes, and -42,144 headroom bytes. The fixed application discovery
worker reserves 6,144 stack bytes; the official mDNS component owns its
bounded query-result allocations. Hardware discovery, DHCP-change, multicast
loss, and multiple-advertiser behavior remain NOT RUN.

## 2026-10-19 - Public baseline audit correction and A-REMOTE-012 reconciliation

The initial public-history audit conservatively classified the literals in
`components/esp_wifi_bsp/esp_wifi_bsp.c` as possible operational Wi-Fi
credentials. That classification is **SUPERSEDED**: Waveshare's official
[`06_WIFI_STA` documentation](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49) publishes the exact literals as vendor sample
values and instructs users to replace them for their environment. The
component remains excluded from the effective production build. No credential
rotation, source removal or Git-history rewrite is required by this finding.

The complete reachable-history review found no operational RTCP association
secret, private key, certificate key, API token, NVS/flash dump, device backup
or REMOTE application binary. Local absolute paths and historical private-LAN
addresses remain documentation-hygiene items, not credential exposures.

Current public source and the active local REMOTE source have zero substantive
file differences after line-ending normalization. The preserved local build,
private candidate and installed readback all identify the accepted 2,526,848-
byte A-REMOTE-012 application by SHA-256
`b6053c04c03114a24a59f178da4eb03d4bada77faded43b6009667579c21d5da`.
This proves artifact identity and current source equivalence separately; it
does not create a historical source-commit-to-binary reproducibility claim.
A-REMOTE-012 remains CLOSED / OWNER ACCEPTED. No source, build, firmware,
hardware, NVS, pairing, partition or pending D-009 contract changed.

## 2026-10-17 - A-REMOTE-012 bounded low-latency volume candidate

REMOTE now keeps one bounded absolute desired-volume target and coalesces rapid
detents while retaining one RTCP command in flight. A confirmed command result
requests fresh authority immediately; definite refusal cancels, uncertain
delivery is never retried, and disconnect, sleep, session or capability changes
invalidate pending intent. HOME can show a clearly provisional desired value
while its bar remains authoritative. TCP no-delay, a 5 ms control cadence and a
20 ms UI cadence reduce scheduling delay without changing CCM 1.1.0 or RTCP/1.
Focused and full relevant host regression passed and the private production
build passed. TUNER-side scheduling and local encoder latency are coordinated in
the public A-REMOTE-012 report. Hardware latency is NOT RUN and no device was
accessed or flashed.

## 2026-10-06 - ADC optimization residency FAIL / A/B rollback control

ADC-optimized BIN `b4580e6a87049b84222bd43d973321b60996a09711c8eef1b6bdd97f53af01f1`: **OWNER HARDWARE FAIL for SYSTEM_SLEEP residency**. Battery-only/external fully disconnected: enters normal sleep approximately300s, briefly toward20mA, wakes almost immediately without owner touch/motion, display ON/current approximately100mA. Previous accepted firmware remained near23mA for about5minutes on the same hardware. **Causality NOT YET PROVEN**; brief20mA is not an accepted power saving.

Revert only ADC sleep-lifecycle cleanup to a hash-verified pre-optimization checkpoint: ADC1/calibrations retained across light sleep; original initialization and runtime sleep ordering restored; lifecycle-only helpers/test and harness entry removed. GPIO1/GPIO4, thresholds/filtering/icon/diagnostics, sleep policy, GPIO8/wake, NETWORK/Wi-Fi, SYS_EN, UI/brightness, partitions/CCM and all other fixes unchanged. No LEDC/deep sleep or spontaneous-wake fix. Original optimization host/build PASS and pending records preserved below, superseded for hardware status by this failure.

**A/B DIAGNOSTIC CONTROL BUILD / OWNER HARDWARE TEST PENDING.** Focused5 PASS/0 FAIL/82723 assertions; normal300 PASS/0 FAIL/550686 assertions; soak300 PASS/0 FAIL/5283936 assertions. Clean remote01 build SUCCESS531.89s; zero warnings/errors. Control BIN2485888(-720 vs failed optimized), linked flash2485393(-716), DRAM47084(unchanged), OTA headroom659840(+720). POWER task560/publication880 unchanged; initializer48->96 restored and lifecycle-only frames removed; configured stacks unchanged. Control BIN SHA-256 `d29383357bfc5ad801bdaaaf191907a347fbef0c1e14acdac39c86091d4fa733`; ELF SHA-256 `7fece2e89c10b3146bab8cce11504cfd42bc86c031a57dd14d2db3a3d4273104`; partition BIN byte-identical. Exact6 modified/1 lifecycle-test deleted; every non-documentation file matches pre-optimization manifest. No unrelated functional change. [Failure evidence, exact rollback and control owner procedure](docs/SYSTEM_SLEEP_CURRENT_AUDIT.md#ab-diagnostic-control---adc-lifecycle-rollback---2026-10-06). Owner test: flash without erasing NVS, battery-only/external fully disconnected, leave untouched/unmoved, normal approximately300s sleep then observe approximately5minutes near prior23mA. Stable control strongly implicates ADC lifecycle (not proof); persistent immediate wake requires independent GPIO8/TCA/touch/QMI attribution investigation next. No upload or unrelated fix.

## 2026-10-06 - First conservative SYSTEM_SLEEP optimization: ADC1 lifecycle

Implemented POWER-owned ADC1 unit teardown immediately before the existing GPIO8 light-sleep cycle and restoration on every exit, after IRQ restoration and before owner/display/NETWORK resume. Pinned IDF5.5.3 oneshot lifetime holds a SAR forced-power reference; public unit deletion releases that reference. Immutable independent GPIO4/GPIO1 curve-fitting handles are retained. Channel configuration/conversions, thresholds/filter/debounce/cadence, hold/wake/sleep timing, NETWORK coordination, UI and partitions/CCM unchanged. Teardown failure refuses sleep; restoration failure invalidates affected telemetry, reports existing counters plus concise error markers and retries on existing2s cadence without reboot/hard-off. No WoM reinitialization, deep sleep, dormant audio/SD activation or intrusive probing.

LEDC unchanged: existing default NO_ALIVE_NO_PD does not request KEEP_ALIVE, and pinned timer deconfiguration has an unimplemented clock-source release TODO. No supported additional sleep-power benefit established, so no speculative pause/recreation or brightness change. Focused7 PASS/0 FAIL/7039 assertions; normal307 PASS/0 FAIL/557725 assertions; soak307 PASS/0 FAIL/5353975 assertions, including10000 actual extracted ADC lifecycle/fault-seam cycles. Clean remote01 build SUCCESS525.85s; zero warnings/errors. BIN2486608(+1040), linked flash2486109(+1036), DRAM47084(unchanged), OTA-slot headroom659120. POWER task560/publication880 frames unchanged; new ADC init80/suspend32/restore32/fault48; Board.initialize96->48 with additional nested helper/SDK usage, not physical stack improvement. Configured stacks unchanged. BIN SHA-256 `b4580e6a87049b84222bd43d973321b60996a09711c8eef1b6bdd97f53af01f1`; ELF SHA-256 `b335f42a1d093b9d8838064d8e68637caae28d9a547924ab57c81531768e074b`; partition BIN byte-identical. Exact7-file scope and final diff/resource evidence in report. [Exact APIs, source evidence, recovery, verification and owner procedure](docs/SYSTEM_SLEEP_CURRENT_AUDIT.md#first-conservative-light-sleep-optimization---2026-10-06).

OWNER HARDWARE MEASUREMENT / FUNCTIONAL REGRESSION PENDING for this optimization artifact. Prior accepted GPIO1/approximately23mA whole-device baseline preserved; no saving claimed. Acceptance requires physical wake/display/brightness/ADC/network regression PASS and comparable normal SYSTEM_SLEEP current equal to or below approximately23mA using the same battery-series method. Equal current is non-regression, not a demonstrated saving. No upload.

## 2026-10-06 - GPIO1 and SYSTEM_SLEEP baseline: owner hardware acceptance PASS

**GPIO1 ADC / thresholds / screen-2 battery/USB icon: OWNER HARDWARE VALIDATED / PASS** for the installed100kOhm/100kOhm GPIO1 circuit and current firmware. USB connected, disconnected and reconnect cycles PASS; charging remains UNKNOWN. **Sleep regression on GPIO1 firmware: OWNER HARDWARE VALIDATED / PASS**: automatic SYSTEM_SLEEP entry approximately302s, touch wake, display recovery, Wi-Fi reconnect and browser diagnostics recovery; no observed reset/WDT/blocking regression.

Accepted whole-device hardware power baseline at battery4.075V: display ON/100%120mA; ON/50%90mA; display OFF/system active approximately70mA, rare approximately80mA peaks; normal SYSTEM_SLEEP approximately23mA, approximately stable over about5minutes. No SD card installed. This PASS does not identify component/rail currents or prove the source of23mA. Fine-pitch/per-rail probing: **NOT REQUIRED / OWNER DECLINED for this milestone**, not failed validation. Deep sleep remains a future optimization/design task, NOT IMPLEMENTED / NOT ACCEPTED.

Supersedes earlier GPIO1 ADC/threshold/icon and corresponding sleep-regression pending status without deleting historical records. Firmware's PROVISIONAL diagnostic label is unchanged; owner acceptance applies to the tested circuit/firmware, not universal ADC accuracy. [GPIO1 closure](docs/GPIO1_EXTERNAL_POWER_SENSING.md#current-owner-acceptance---2026-10-06); [accepted current baseline and audit limits](docs/SYSTEM_SLEEP_CURRENT_AUDIT.md#current-owner-acceptance---2026-10-06). Documentation only: no implementation/tests/configuration/artifact changes, build or upload; historical Stage1 and unreported acceptance evidence remain separate.

## 2026-10-06 - Owner current baseline / read-only SYSTEM_SLEEP audit

Owner measured whole-device battery current at4.075 V: display100%120 mA; display50%90 mA; displayOFF/system active approximately70 mA with rare80 mA peaks; current SYSTEM_SLEEP23 mA. Recorded as physical owner evidence, without assigning individual load currents or claiming uninterrupted sleep residency from a meter reading. Current source verifies NETWORK HTTP/DNS/Wi-Fi stop/fence before manual light sleep, Core/LVGL park, GPIO8 physical wake and no2 s normal timer (development Z only). SYS_EN retains board rails; LCD logic/unused audio/SD remain physically supplied, actual standby currents unknown. Deep sleep is RTC-GPIO feasible but requires hold-preserving expander boot, wake-contact consumption, fresh Core/network reconstruction and measured latency/rail validation. No speculative fix or current target asserted. [Component evidence table, measurements, ranked proposals and deep-sleep assessment](docs/SYSTEM_SLEEP_CURRENT_AUDIT.md).

Documentation only: current sleep policy, GPIO1/GPIO4/GPIO8, wake/NETWORK behavior, hold/hard-off, CCM and partitions preserved. No source/tests/config/artifact change, build or upload; broader acceptance and GPIO1 hardware-validation status unchanged.

## 2026-10-06 - GPIO1 external-power ADC and screen2 USB-presence icon

Owner installed100kOhm PTH5V->node/100kOhm node->PTHG, node->GPIO1 on V2 Rev1.1: disconnected PTH5V/node0.00V; connectedapproximately4.95V/2.42V. Separate ADC1 ch0/12dB/12bit curve-fitting calibration and awake2s acquisition added; GPIO4 ch3 battery acquisition unchanged. Provisional600/1800mV hysteresis, three consecutive samples; invalid/init/read/calibration/range errors UNKNOWN, stale post-sleep history reconfirmed. B/browser expose external raw/node mV/valid/calibrated/error/counter/state; charging UNKNOWN. Owner clarified existing battery outline should switch to a standard plugged-in symbol: PRESENT shows enabled Montserrat12 LV_SYMBOL_USB, ABSENT/UNKNOWN retain battery outline. No unrelated layout/carousel/touch change. SYSTEM_SLEEP/NETWORK coordination, wake/GPIO8, battery thresholds, hold/hard-shutdown, partitions/frozenCCM unchanged. [Exact evidence/implementation/verification/first hardware procedure](docs/GPIO1_EXTERNAL_POWER_SENSING.md). Divider measurements are owner evidence; ADC/threshold/icon hardware validation PENDING. No upload.

Focused14 PASS/0 FAIL/4096 assertions; full normal300 PASS/0 FAIL/550686 assertions; soak300 PASS/0 FAIL/5283936 assertions. Final clean remote01 -j1 SUCCESS; zero warnings/errors. BIN2485568(+1664), linked flash2485073(+1668), DRAM47084(+104), OTA-slot headroom660160. Configured stacks unchanged; POWER frame544->560/publication848->880; HOME refresh304->336; HTTP status1072->1104. New physical watermark/current behavior validation pending. BIN SHA-256 `8a16cc8db6fdbe53b493bb34515a068d3fcf684c4e6f4d93178d8ed3e058eefa`; ELF SHA-256 `e0f55b8e65a508b6501e7c46d927796ec81215cf9d73e779cdf48cd3f0253d19`; partition artifact byte-identical. Exact20-file scope and dynamic bounded allocation/frame evidence in report.

## 2026-10-06 - SYSTEM_SLEEP touch wake / NETWORK recovery: owner hardware PASS

Owner reports: **SYSTEM_SLEEP -> TOUCH WAKE -> FIRST TOUCH CONSUMED -> SECOND TOUCH NORMAL -> NETWORK/DIAGNOSTICS RECOVERY: OWNER HARDWARE VALIDATION PASS.** Physical owner acceptance of the complete reported touch-wake/recovery path, complementing the earlier motion-wake PASS. Current/endurance, new stack margins, unreported supply conditions and remaining historical Stage1 gates are not inferred. Historical Stage1 remains OPEN. [Exact owner evidence](docs/SYSTEM_SLEEP_NETWORK_IMPLEMENTATION.md#owner-touch-wake-evidence---2026-10-06). Documentation/status only; no firmware/tests/configuration/artifacts/build/upload change. Earlier pending and motion-only records preserved as history.

## 2026-10-05 - SYSTEM_SLEEP + NETWORK quiesce/resume: owner motion-wake PASS

Owner reports: **SYSTEM_SLEEP + NETWORK QUIESCE/RESUME - OWNER HARDWARE VALIDATION PASS for motion wake.** Physical owner evidence, not host prediction. This acceptance covers the reported motion-wake path only; touch wake and remaining current/endurance/stack/other-condition checks are not newly validated by this message. Broader historical Stage1 remains OPEN. Documentation/status only; no implementation, test, artifact, build or upload change. [Owner evidence](docs/SYSTEM_SLEEP_NETWORK_IMPLEMENTATION.md#owner-hardware-evidence---2026-10-05). Prior pending entries retained as history.

## 2026-10-05 ? automatic SYSTEM_SLEEP / NETWORK owner lifecycle

Owner-authorized implementation of external-independent30s display-idle/300s total-inactivity system sleep. Existing NETWORK owner stops/checks HTTP/DNS/Wi-Fi, fences callbacks and acknowledges; POWER bounded10s timeout/refusal safely cancels before Core/LVGL park. Physical wake preserves first-touch/motion-only contract and Core fresh-state-before-UI order; NETWORK restores saved STA/backoff and healthy diagnostics without credential/NVS changes or wake-triggered provisioning. Active setup/candidate/scanning/confirmation/reboot and genuine safety/owner failures block sleep. Normal GPIO-only wake removes the prior2s battery/WoM monitoring fallback; development Z retains it. No hard-off, battery threshold, stack/WoM/publication, hardware/GPIO/UI/partition/divider change. Diagnostics now reports enabled policy. Earlier audit STOP and inhibited-policy records retained as prior-source history. [Exact lifecycle/verification/resources/owner procedure](docs/SYSTEM_SLEEP_NETWORK_IMPLEMENTATION.md). New hardware validation PENDING; historical Stage1 OPEN. No upload.

Normal286 PASS/0 FAIL/544590 assertions; soak286 PASS/0 FAIL/5223840 assertions. Clean remote01 -j1 SUCCESS, zero warnings/errors. BIN2483904(+3392), linked flash2483405(+3388), static DRAM46980(+32), OTA-slot headroom661824. POWER task frame528->544; NETWORK2176->2256; publication helper848 unchanged, configured stacks unchanged. BIN SHA-256 `4f2542151766d50aaae0c887daf6d97dc2c252da7ade3a555ce57db31fc0ef03`; ELF SHA-256 `0bc16f10af5638baf9be56ed9e702a71b3e2ebab6efc3c511a586cdcc86e99e3`. Partition artifact byte-identical. Compiler reservations do not prove physical headroom; owner wake/network/current/endurance/stack validation pending. Exact22-file scope in linked report.

## Unreleased

### New owner SYSTEM_SLEEP decision / pre-edit audit STOP - 2026-10-05

- DECIDED: normal automatic SYSTEM_SLEEP independent of external PRESENT/ABSENT/UNKNOWN, superseding former unknown-source prohibition. Owner reports overnight deep discharge apparently below approximately3.5V; exact voltage/cause/current not proven. Existing motion/first-touch/no-PWR contract, battery thresholds/SYS_EN hold and planned divider unchanged; no automatic hard-off or charging claim.
- Audit STOP before source changes: existing explicit esp_light_sleep_start parks Core/LVGL but not NETWORK/Wi-Fi/HTTP/DNS. Pinned IDF5.5.3 requires Wi-Fi stopped for this path; current PM_ENABLE disabled. Active2s RTC safety timer persists every round, returning POWER to input/ADC work then sleep again; actual savings unmeasured. Task delays are not themselves explicit sleep wake sources.
- Updated canonical register item22, current roadmap and appended Stage1 audit with source/SDK/service evidence and bounded lifecycle/timer next scope. Policy decided; implementation blocked/not prepared. Existing firmware still inhibits automatic sleep with real external UNKNOWN. No source/tests/config/artifact changes, tests/build/upload; resource/frame deltas0 by unchanged artifacts, no new compiler or hardware result.


### Owner acceptance - Today's Step2 CLOSED / PASS - 2026-10-05

- **PASUL 2 - RECUPERARE + AUDIT + RECONCILIERE DECIZII: CLOSED / PASS**, explicitly confirmed by owner. Canonical register and roadmap record closure of documentation recovery/audit/reconciliation only; future implementation/design/hardware gates and historical Stage1 OPEN remain unchanged. No source/tests/config/artifact changes, build or upload.


### Today's Step2 - canonical owner product reconciliation - 2026-10-05

- Created docs/REMOTE01_PRODUCT_DECISIONS.md with all38 reviewed owner items and six normalized statuses. Supersedes earlier generic-first/compatibility-first product positioning: REMOTE TUNER-primary, protocol-neutral boundaries intact; separate devices, TUNER audio owner, optional HA/openHAB compatibility, subordinate Matter/Smart-TV/IR direction; Zigbee EXCLUDED. No active REMOTE local-radio pipeline.
- Discovery mDNS DECIDED; actual TUNER protocol UNKNOWN / NOT RECOVERED, transport DESIGN REQUIRED; API candidate still unapproved/unfrozen. No WebSocket/HTTPS freeze or support claim.
- SD backup before flashing/updating DECIDED; prior SD-purpose OWNER DECISION REQUIRED/UNKNOWN and optional staging-as-purpose gates superseded. SD hardware/bounded service IMPLEMENTATION + HARDWARE VALIDATION REQUIRED. Required backup enters update dependencies; no backup implementation or prior-upload backup claim.
- Persistence DECIDED: TUNER favorites/recent stations; REMOTE Wi-Fi/UI settings/last TUNER/artwork cache. No REMOTE station/favorites persistence requirement. REMOTE active-URL artwork lookup/retrieval/cache/thumbnail and stale-result rejection DECIDED; detailed cache/retrieval/reset policy DESIGN REQUIRED. Frozen model semantics unchanged.
- Encoder+three buttons DECIDED; electrical facts IMPLEMENTATION + HARDWARE VALIDATION REQUIRED and actions DESIGN REQUIRED, no never-approved claim. External5V divider sensing hardware direction DECIDED; source pad/voltage/resource/pin/resistors must be verified, none selected; not charging detection. Existing wake/no-PWR/automatic-sleep inhibition preserved.
- OTA required; accepted partitions unchanged. Owner same-LAN OTA without password supersedes previous mandatory-authentication/LAN-insufficient access policy; image trust/compatibility/downgrade/interruption/trial/rollback/SD recovery DESIGN REQUIRED. Wi-Fi provisioning and TUNER authorization not changed.
- Actively replaced current roadmap/radio/audio product gates; preserved complete earlier versions as explicitly superseded history. SD/controls current classifications corrected with historical wording notes; original overnight reports appended only. Architecture/native-candidate/OTA status overlays preserve their complete prior text. Earlier CHANGELOG claim "No new ... SD purpose/playback role selected" describes that session, not today's canonical decisions.
- Today's Step1 CLOSED/PASS, Stage2B.2 covered closure and historical Stage1 OPEN remain distinct. Network Settings physical validation blocked by missing UI/harness, not FAIL. Documentation only; no source/tests/config/artifact changes, build or upload. No direct frozen CCM contradiction found; detailed evidence/scope verification in register/roadmap.


### Today's Step1 / overnight-work owner closure - 2026-10-05

- **2026-10-05 - Overnight work / Today's Step 1: OWNER HARDWARE VALIDATION PASS. CLOSED / PASS.** Verification of overnight work: HOME/IP, carousel0->1->2->1->0, touch/controls, display idle, touch/lift wake and browser diagnostics PASS on current instrumented firmware.
- NETWORK CONNECTED/provisioning0/connected1/stored1/storage_ready1/IP192.168.1.83; reconnects/reason/errors/allocation_errors0, minimum1692 B. POWER IMU1/motions220/bus_errors0/errors0/wakes20/sleep_requests0/rounds0/minimum1256 B/wake-ready240ms; battery REAL/calibrated1/valid1/NORMAL. G/Y, K/!, Z not used. POWER provisional1024 B review target is not a safety guarantee; zero HTTP/DIAGNOSTICS watermark fields in a single W snapshot not classified as defects, browser diagnostics physically PASS.
- S/B interference remains DISPROVEN on current firmware; current WoM/lift wake PASS. Prior IMU_WOM0 incident retained as REAL HISTORICAL OBSERVATION / CURRENTLY NOT REPRODUCED / ROOT CAUSE UNKNOWN; no functional fix.
- Today's Step1 is distinct from broader historical Stage1, which remains OPEN for existing mandatory evidence gaps. Network Settings backend: HARDWARE VALIDATION BLOCKED BY DESIGN / MISSING USER-ACCESSIBLE UI OR HARNESS, not FAIL. OPEN_SETUP/CANCEL/confirmed Wi-Fi reset not claimed physically owner-tested.
- Exact owner evidence in docs/REMOTE01_OVERNIGHT_EXECUTION_REPORT.md; latest overnight report and canonical roadmap linked/status-updated. Previous history retained. Documentation only; no source/tests/config/artifact change, build or upload.


### Stage2B.2 / WoM owner hardware closure - 2026-10-05

- **Stage2B.2 CLOSED** within the authorized measured/tested scope. Publication-frame correction: **OWNER HARDWARE VALIDATION PASS for measured/covered paths**. Latest baseline approximately2840 B, STARTUP1416, REPORT1304, repeated B/COMMAND minimum1256; all above provisional>=1024 B review target, not a safety guarantee. Earlier1248 B measurement retained.
- **S/B -> WoM interference hypothesis DISPROVEN on current hardware/firmware**: lift wake passes before S/B, after each S, and after Bx10; no R used during this sequence. IMU remains1, motions>0, bus_errors0. Initial/final lift tests PASS3/3, wake2; approximately220-247ms initial and239-246ms final readiness.
- **Current instrumented WoM/lift-to-wake owner test PASS**: upload boot reset11/ok1/stage16/failed0/err0/actual00/polls8/total29ms/cause0; IMU_WOM1/sleep_ready1. Owner reports prior instrumented software reboot also ok1.
- Prior IMU_WOM0 incident retained as **REAL HISTORICAL OBSERVATION / CURRENTLY NOT REPRODUCED / ROOT CAUSE UNKNOWN**. No functional fix justified/applied; no retry/delay/check/register/stack change. Instrumentation/publication correction preserved.
- Exact evidence/classifications appended to docs/STAGE_2B_2_POWER_STACK_PATH_MEASUREMENT.md; canonical roadmap status updated. Overall Stage1 acceptance remains open for its existing mandatory evidence gaps. Documentation only; no source/test/artifact changes, build or upload.


### Stage2B.2 owner POWER remeasurement / DEVELOPMENT_ONLY WoM initialization diagnostics - 2026-10-05

- Owner hardware POWER minima after publication correction: baseline2832, STARTUP1520, normal/ADC/input/publication1488, REPORT1376, Bx10 minimum1248; later S1248. All measured intervals above provisional>=1024 B review target; no general safety guarantee. POWER stack/publication correction unchanged.
- Owner reproduces IMU_WOM0/sleep_ready0 and failed lift wake on USB/battery after clean/upload boot; same firmware restores physical lift wake after serial R. Post-R IMU_WOM was not captured; value/root cause unproven. No speculative fix.
- Added temporary guarded44-byte fixed POWER-owned WoM initialization record and one post-config summary on success/failure: reset reason, stage/failure/op/register, actual transport error, expected/mask/actual/validity, poll counts, elapsed/total time, outcome/cause. Same transactions/order/values/checks/timeouts/5ms poll delays; no retries, per-poll logs, heap/task/queue/RTC changes.
- Focused enabled7 PASS/27555 assertions and disabled7/21509; full normal276 PASS/0 FAIL/559486 logged assertions; soak276/0/5193736. Zero allocation checks; original-vs-instrumented trace oracle. Test-fixture Werror indentation failure corrected before final passing runs. Clean2.50s/build546.07s exit0, zero final compiler/CMake warnings/errors. POWER frame528/publication848 unchanged; reporting helper112 B. BIN2480512 (+864), linked flash2480017 (+872), staticDRAM46948 (+40), OTA-slot headroom665216 B; partition unchanged.
- Instrumented BIN SHA256 `2cad36cac5504ee539825d1dc3b65b5ef43fadc3bcf900635ae7c29d1f2fba8b`; ELF `0cb61c112b2696243f61cc9be560624fb6526f26819188449a3ad710d988e570`. Exact stage/cause table, summary templates, seven-file scope and paired upload-boot/R-reboot retest: docs/STAGE_2B_2_POWER_STACK_PATH_MEASUREMENT.md. Historical evidence retained; no upload. **ROOT CAUSE UNPROVEN / DIAGNOSTIC HARDWARE RETEST PENDING**.


### Stage2B.2 owner measurement / minimal POWER publication-frame correction - 2026-10-05

- Owner log reproduces608 B: baseline2112, STARTUP800; S/REPORT800->608 independently; after NETWORK R, COMMAND800->768 then B768->608, B count10; subsequent S no further reduction. Deepest measured intervals established, not an unexplained transient; exact nested subcall/interrupt contribution remains unisolated. No overflow/corruption/WDT inferred;>=1024 B remains a provisional review target.
- Only implementation change is POWER runtime.cpp: synchronous explicitly non-inlined publication helper holds Power448 B / ResetSummary256 B locals instead of outer task. Validated RTC accessor, snapshot fields/copy order, publish/events/bookkeeping and same-owner synchronization preserved; no heap/task/queue/stack-size/policy/API change. IMU/WoM untouched.
- Final ELF task frame1248->528 B (-720); helper848 B. Combined publication frames1376 B versus prior1248 before unchanged child calls: publication's nested peak also requires physical retest.4096 B stack/priority2/core1 unchanged; no physical improvement or production headroom acceptance claimed.
- Focused actual-helper test1 PASS/80668 assertions/zero allocator calls; full normal262 PASS/0 FAIL/510422 logged assertions, soak262/0/4730672. Additional3 source-boundary assertions pass. Clean2.35s/build572.03s exit0, zero compiler/CMake warnings/errors. BIN2479648 (-832), linked2479145 (-836), staticDRAM46908 unchanged;OTA headroom666080 B. Partition unchanged; no upload.
- Appended owner evidence/correction/build hashes and fresh-boot S/S then R/Bx10/S/S procedure to docs/STAGE_2B_2_POWER_STACK_PATH_MEASUREMENT.md, preserving historical evidence. Five-file authorized implementation/test/documentation scope; **OWNER HARDWARE REMEASUREMENT REQUIRED**. Initial host compiler PATH invocation issue corrected without weakening tests.


### Overnight #2 hardware/resources and neutral controls - 2026-10-05

- **AUDIT COMPLETE / HOST TESTED / CLEAN BUILD VERIFIED; NO NEW HARDWARE PASS.** Exact previous1,650-file checkpoint verified before edits. Added GPIO/bus/conflict map, physical-controls/SD/audio audits, radio architecture decision gate and UI/backlog completeness inventory. No new pins/actions/SD purpose/playback role selected; real TUNER source absent, API candidate unapproved.
- Added standalone unlinked sampled-input model:3 neutral buttons, signed logical quadrature cycles, caller debounce, fixed8-event queue, validated timestamps/ownership, loss discard/resync and bounded counters. No GPIO/ISR/task/UI/Core integration or heap. Host400 B/target392 B;19 tests, zero allocator calls. Dormant SD/audio remain excluded. Production source/config/frozen CCM/history unchanged.
- Full normal **261 PASS/0 FAIL/429754 assertions**; soak **261/0/3924004**. Clean1.37s/build545.03s exit0, zero compiler/CMake warnings/errors. BIN2480480 (-16), linked2479981 (-12), staticDRAM46908 unchanged;3MiB OTA headroom665248 B. Clean relink differs beyond timestamp metadata; no byte-equivalence claim. Partition unchanged, no upload.
- Exact BIN/ELF hashes, retained fixture failures,11-file scope/protected-file verification, unknowns/owner gates and next actions: `docs/REMOTE01_OVERNIGHT_2_REPORT.md`. Stage2B PASS retained; Stage1, Stage2B.2 measurement and prior overnight physical acceptance remain pending. Roadmap preserves all controls/SD/audio/TUNER/UI/OTA/power/release gates.


### Reconciled overnight foundations - 2026-10-04

- **HOST TESTED / CLEAN BUILD VERIFIED; OWNER HARDWARE VALIDATION PENDING.** Reconciled full backlog; added authoritative zero-wait STA-IP presentation to factory Screen0's blank row and HOME footer, preserving controls/carousel/battery path. Failed NETWORK health checks clear copied stale IP; no Wi-Fi policy change or inferred owner-error cause.
- Added local-only NETWORK-owned settings/recovery API: retain credentials during secure setup re-entry, cancel to saved STA, existing healthy-connection replacement, one-use generation-fenced10s confirmed Wi-Fi-only reset. Factory reset refused. No new UI wizard, HTTP mutation route or serial key; existing provisioning/security/R preserved.
- Added bounded host-only OTA image integrity preflight against installed IDF5.5.3 format; no authenticity/activation claim, writer, boot/rollback or upload endpoint. OTA security/recovery, Stage1 evidence, POWER measurement, hardware controls/SD, production UI and real TUNER dependencies remain open.
- Final normal **242 PASS/0 FAIL/408133 assertions**, soak **242/0/3722383**; clean/build SUCCESS exit0 (2.23s/557.88s), zero compiler/CMake warnings/errors. BIN2480496B (+2144), linked2479993B, staticDRAM46908B (+128), OTA headroom665232B/649.640625KiB. NETWORK frame+96/HOME refresh+160; POWER frame/stacks unchanged. Partition hash unchanged; no upload. Earlier MSYS PATH and successful-unittest stderr wrapper issues retained in report; final runners exit0.
- Exact hashes,17-file inventory, protected-file verification, blocked decisions and owner procedure: `docs/REMOTE01_OVERNIGHT_EXECUTION_REPORT.md`; current dependency/status consolidation: `docs/REMOTE01_COMPLETION_ROADMAP.md`. Historical stage evidence and frozen CCM preserved; no new hardware PASS.

### REMOTE//01 completion audit and roadmap - 2026-10-04

- Created `docs/REMOTE01_COMPLETION_ROADMAP.md`: repository-backed baseline, hardware/feature status, current artifact/resource budgets, dependency-ordered milestones, physical/release gates and unattended-work limits. Retains Stage 2B hardware PASS, open Stage 1 acceptance evidence and pending Stage 2B.2 POWER measurement; no inferred hardware acceptance.
- Documentation only; no source/config/test/historical stage changes, build or upload. Checkout has no Git metadata; SHA-256 inventory comparison verifies the two-document scope.

### Stage 2B.2 DEVELOPMENT_ONLY POWER path stack measurement - 2026-10-04

- **IMPLEMENTED / NORMAL AND SOAK PASS / CLEAN BUILD VERIFIED; OWNER PHYSICAL MEASUREMENT PENDING. No upload or corrective stack change.** Stage2B physical PASS and Stage2B.1 owner608/800-byte observations retained. Pre-edit frozen baseline/artifact hashes verified.
- Guarded POWER-owned fixed948 B state,15 paths/16 new-minimum events; before/after byte watermarks, strict new-minimum flag, last/lowest pairs, epoch/sequence, counts/overflow/validity. No heap, new task/queue, RTC write or HTTP/API change. S reports on demand; T clears records only, explicitly historical_HWM_not_reset=1. Existing UC->POWER serialized dispatch and all old keys preserved. Reporting minima isolated as REPORT; nested ERROR/ADC evidence is inclusive, not independent usage.
- Prototype and final target generated-code review: POWER frame1248 ->1248 B, **delta0**; configured stack4096 B/priority2/core1 unchanged. Disabled runtime's39 code/literal sections byte-identical to accepted-source object. Probe wrappers32 B each; record bookkeeping32/48 B; report/clear48 B, formatting64 B. No exact per-path consumption or physical608 reproduction claimed.
- Full normal **229 PASS/0 FAIL/394390 assertions**; soak **229/0/3591640**, preserving all215 existing tests. Seven new tests in each enabled/disabled build cover attribution/inherited floors, clear, nested intervals, bounded10000-cycle history, invalid brackets, saturation and keys. Final runs zero warnings/errors. Initial new test indentation error and TEMP prototype/disabled-code checks corrected and retained in report; tests not weakened.
- Clean/build SUCCESS exit0,1.56s/522.20s, zero warnings/errors. BIN2478352 B (+1920), linked2477849 B (+1916), static internalRAM46780 B (+952=948 state+4 alignment), no additional PSRAM/runtime application allocation; unchanged3MiB OTA-slot headroom667376 B/651.734375KiB. Partition/config/hardware/reset/diagnostics/provisioning/frozenCore inputs unchanged; no OTA or sleep-policy implementation.
- BIN SHA-256 `b34ad9a8f319532aa7ad898b54d7cd70c08f944452e0090e0c6504a4da6dbd67`; ELF `ee6d6e57ac2aa595674e5281ddfb3897c4b0836e88e80b883da1886e7572fff1`; partition unchanged `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`.
- Procedure/report: docs/STAGE_2B_2_POWER_STACK_PATH_MEASUREMENT.md. Fresh boot before S/T/B establishes baseline paths; controlledR then repeatedB compares minima; S twice captures REPORT effects, safeC/N dry-run logging optional. No K/!; Z omitted absent established safe physical recovery validation. Stop awaiting owner measurements; no Stage2B.2 hardware PASS, production-margin guarantee, stack enlargement or publication restructuring.

### Stage 2B.1 POWER stack investigation - 2026-10-04

- Investigation only; Stage 2B owner hardware PASS preserved. POWER is 4096 bytes / priority 2 / core 1. Pinned ESP-IDF 5.5.3 confirms watermark units are bytes, with no conversion: owner minima 608 and 800 bytes retained without safety/corruption attribution.
- Accepted ELF versus retained pre-Stage-2B ELF and TEMP-only exact-flag compiler reports establish task entry frame 496 -> 1248 bytes (+752). Removing only publication/bookkeeping in a TEMP counterfactual restores 496. New local Power448 + ResetSummary256 remain within the outer frame across unrelated B/logging/startup paths. B/reset-report/formatter is a leading deep-path candidate, not a proven physical worst path.
- Complete unchanged soak: 215 PASS / 0 FAIL / 3471554 assertions, zero warnings/errors. Six target investigation compilations succeeded without warnings/errors. Host tests cannot reproduce physical FreeRTOS watermark or actual B/ADC/ISR/HTTP combinations; 608 not reproduced. No source/test/instrumentation changes, PlatformIO rebuild or upload; accepted artifacts/resource usage unchanged.
- Production headroom remains open. Provisional >=1024-byte measured review target is not a safety guarantee; recommend per-path development measurement before corrective implementation, then evaluate bounded out-of-line publication if warranted, rather than speculative stack enlargement. Full task paths, target object sizes, evidence, limits and potential file scope: docs/STAGE_2B_1_POWER_STACK_INVESTIGATION.md.

### Stage 2B diagnostics / controlled serial reboot OWNER HARDWARE PASS - 2026-10-04

- **OWNER HARDWARE VALIDATED / PASS; hardware validation complete for implemented scope.** Physical ESP32-S3 retained existing credentials across migration/upload, reconnectedSTA without provisioning, served read-onlyLAN diagnostics at assignedIPv4 and remained stable under repeated5s refresh. DEVELOPMENT_ONLY serialR accepted/executed with reset_reason3; NVS/Wi-Fi survived, diagnostics recovered automatically, RTC history persisted/advanced boot21->22. No reported failed allocations, accounting corruption, network allocation errors, POWER errors or bus errors. No unreported test PASS inferred.
- Retain **POWER minimum stack watermark608B** as open investigation/headroom review before production freeze; no sufficiency or reset-cause claim. Two initial transient diagnostic_errors have no established cause; after software reboot counter0. HTTP socket receive error during deliberate reboot interruption is an expected test observation, not a demonstrated defect. Full physical evidence in docs/STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md.
- Roadmap only: Development Screen0 Wi-Fi state/STAIPv4; final Network Settings currentIP and user-accessible Wi-Fi setup/reset wizard for router changes; full factory reset separate and deliberate/confirmed from Wi-Fi credential reset. Not implemented here.
- Documentation only; firmware/source/tests/partition unchanged, no build/upload. Automatic deep sleep remainsINHIBITED/NOT VALIDATED; OTA transfer not implemented. Historical build/pending-validation/partition records preserved.

### Stage 2B connected diagnostics / controlled serial reboot - 2026-10-04

- **IMPLEMENTED / HOST TESTED / CLEAN BUILD VERIFIED; OWNER HARDWARE RETEST PENDING. No firmware upload.** Separate normalSTA HTTP service GET /, /api/status, /api/events?after=sequence; healthy currentSTA/subnet/canonicalHost checks with validated native/mappedIPv4, no provisioning exposure, no writable endpoint/web reboot/auth secrets. Other LAN devices can read allowlisted diagnostics over plaintextHTTP; subnet guards are not user authentication or an Internet-proxy guarantee. Captive routes/DNS/security unchanged.
- Fixed sanitized owner copies for B/m/W-equivalent data and4 copied validated RTC rows; typed32-entry event history,<=16/request, overflow/gap/drop counters, JSON escaping/size checks and5s polling. No raw log interception. Arena8056B PSRAM, control48B internal, HTTP stack6144B/priority1/max2clients; no per-event application allocation or new owner/log/reboot queue/task. SDK live heap and lowered publication-stack margins require owner validation.
- UppercaseR (uniqueness source-audited) uses existing UC->NETWORK inbox; explicit DEVELOPMENT_ONLY queued/accepted/coalesced/refused/executing markers,>=500ms scheduled delay and bounded20ms USB TX opportunity, NETWORK task esp_restart. Pending power/future writer policy refuses; no current OTA writer. No NVS/partition erase, no protocol/UI/power-policy change; lowercaser and existing commands preserved.
- Owner-approved scope amendment: reset_diagnostic.hpp/cpp add only bounded validated by-value accessor/sanitizer. POWER is the sole production export caller; RTC640B/four-boot format/retention/save/capacity and serialB unchanged. Amendment reason and exact20-file scope documented in docs/STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md.
- Final complete normal **215 PASS/0 FAIL/382304 assertions**; soak **215/0/3471554**, including10000 added diagnostic cycles and actual-source namespace audit. JavaScript syntax PASS. Final clean/build **SUCCESS exit0,2.24s/553.00s; zero compiler/CMake warnings/errors**. Initial cached-CMake incremental missing-component failure, resolved tool PATH probe and earlier clean run retained in report; final build matches settled firmware inputs.
- FullBIN2476432B (+15408), linked application2475933B (+15412), staticRAM45828B (+32). Each unchangedOTA slot3145728B; headroom **669296B/653.609375KiB**; BIN78.723653% of slot. PlatformIO29.5% is factory8MiB. Partition/config/captive/hardware/UI/frozenCore protected inputs byte-identical; partition SHA unchanged.
- BIN SHA-256 `a61a303aa23b3f02bb419831080f6d81ecb7e006f8ef11ece1a457ce1bc481e8`; ELF `e50d4847af85bc96bd8719b44e798a71eec8edc892e91b3398432bc253b7271f`. One owner procedure in result report; await owner approval before manual flash. Partition migration's earlier physical PASS remains historical, not new diagnostic/reboot validation. Automatic deep sleep inhibited/unvalidated; no OTA transfer/HTTPS/auth/SSE/WebSocket/persistence/SD/TUNER/encoder/button/UI redesign.

### Stage 2B physical partition migration ACCEPTED / diagnostics design only - 2026-10-04

- **OWNER HARDWARE VALIDATED / ACCEPTED: approved16MiB partition migration.** New table boots retained factory, NVS/Wi-Fi credentials survived with no full-chip erase. One initial failed authentication recovered automatically: reconnects1/finalCONNECTED/stored1/provisioning0/IP192.168.1.83; captive HTTP/DNS remained off. No deliberate router-outage/reprovision PASS inferred.
- Battery withoutUSB, power hold, buttons/UI, display idle, motion wake and touchscreen wake **OWNER PASS**. No reboot during disconnected interval: boot19, uptime~161->429s; no observed brownout/WDT. POWER hold1/wakes3/errors0/bus_errors0/batteryREAL/NORMAL; UC failed_allocations0/accounting_corrupt0/owner_ok1/queue_rejected0; final networkCONNECTED/stored1/provisioning0/allocation_errors0. Exact evidence retained in Stage2B migration report.
- **Automatic deep sleep NOT TESTED / NOT ACCEPTED**; automatic_system_sleep remainsINHIBITED reasonEXTERNAL_POWER_UNKNOWN. No charging/VBUS detection inferred, no policy or hardware change.
- Next scope inspected/planned only: separate connectedSTA read-only HTTP status/event page, sanitized owner snapshots, bounded typed event ring, existing NETWORK-owned deferred DEVELOPMENT_ONLY serialR reboot. Proposed unauthenticated read-only LAN access is explicitly not user authentication; no browser reboot or plaintext auth secrets. Full limits/endpoints/access controls/file list/test plan in docs/STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md for owner review before source edits.
- Documentation only; no source/config/table change, tests/build/upload, OTA update or new runtime capability. Existing BIN/ELF/partition hashes preserved. Physical acceptance does not claim OTA transfer, slot switching, rollback or unlimited endurance. Historical preflight failure and earlier owner tests preserved.

### Stage 2B approved OTA partition migration - 2026-10-04

- **OTA PARTITION LAYOUT IMPLEMENTED / BUILD VERIFIED; OTA UPDATE MECHANISM NOT YET IMPLEMENTED; HARDWARE MIGRATION OWNER TEST REQUIRED.** Scope limited to partitions/documentation. Physical esptool flash_id on freed COM3 confirmed16MiB (manufacturer0x20/device0x4018); initial busy-port failure resolved without writes. No firmware upload occurred.
- Preserved NVS0x9000/0x6000, PHY0xF000/0x1000 and factory0x10000/0x800000. Added otadata0x810000/0x2000, ota_0=0x820000/0x300000, ota_1=0xB20000/0x300000; final end0xE20000 within0x1000000. Pinned IDF5.5.3 generated-table round-trip/bounds/alignment/no-overlap/metadata checks PASS. Factory retained for migration boot/upload address compatibility, not because future A/B OTA requires it; blank metadata selects factory.
- Full regression **205 PASS/0 FAIL/373221 assertions**; full soak **205/0/3381471** unchanged tests. Clean/build **SUCCESS exit0,2.30s/521.33s; zero compiler/CMake warnings/errors**. Shell initially lacked pio; actual commands rerun successfully with installed PATH. BIN **2461024B**, each slot3145728B, exact headroom **684704B/668.65625KiB**; BIN78.233846% of slot. Link usage2460521B/staticRAM45796B unchanged. PlatformIO29.3% refers to factory8MiB, not OTA3MiB. BIN -320B versus entry artifact is recorded, not attributed to code changes.
- USB migration uses normal PlatformIO Upload: bootloader0x0/table0x8000/app0x10000/blank8KiB otadata0x810000. No full-chip erase; NVS is unchanged and outside affected write sectors. Credentials expected to survive, require physical reconnect evidence. One owner procedure and complete hex/byte/KiB tables in docs/STAGE_2B_OTA_PARTITION_MIGRATION.md; old preflight STOP retained. Existing serial software-reboot key is absent; no new command added under partition-only scope, esptool USB reset remains available and is not labeled software-reboot PASS.
- BIN SHA-256 `ab63bda4dbd961c342e8974d5a6a077fa1e4fffd65193dad158676e18be0d302`; ELF `f85c605e361e7b9348a900388c759944ba4a1a4cdf5f4f2dc115cf0a4c8cec31`. No app source/config/test change beyond partition CSV; no OTA transfer/auth/UI/download/SD/cloud, diagnostics service, TUNER or power/HOME redesign. No physical OTA/slot-switch/rollback claims.

### Stage 2B OTA preflight STOP / Stage 2A reboot hardware PASS - 2026-10-04

- Owner designates Stage2A as accepted baseline for Stage2B. Criterion I reboot persistence **OWNER HARDWARE PASS**: actual esptool MCU reset and later normal firmware upload/reset independently restored stored WPA2 credentials -> CONNECTING -> DHCP/CONNECTED at192.168.1.83, no provisioning/HTTP/DNS, errors0/reason0/allocation_errors0. Earlier bad-password and two-client HTTP failure/correction records retained. Router outage/recovery and deliberate reprovision remain genuinely deferred; no new physical PASS inferred.
- **STAGE2B BLOCKED BEFORE IMPLEMENTATION / REPARTITIONING**, per explicit owner stop condition. Source and IDF5.5.3-decoded built table contain only NVS24KiB/PHY4KiB/factory8MiB, no otadata/OTA slots. Current BIN2461344B; inactive slot capacity0B. Pinned SDK next-update lookup returns NULL; OTA boot selection requires missing metadata. Existing factory slack/unpartitioned space is not an OTA partition.
- Proposed for review only: preserve existing partitions, add otadata0x810000/8KiB, ota_0=0x820000/3MiB and ota_1=0xB20000/3MiB, each with684384B current-BIN headroom; future implementation growth/migration remain unverified. Exact sizes, addresses, SDK evidence and identity caveats in docs/STAGE_2B_LOCAL_OTA.md.
- Documentation only; no source/config/partition/firmware change, test/soak/rebuild/upload, LAN service, authentication or reboot-key implementation. Current audit-entry BIN SHA-256 `99de28bbc1607bd1ad0f967d579e58f454a3a271ca0c504666de9e9dacc5ac8b`, ELF `17fa6f213ac108d8e4aefb88d470bbd7a040e439ce4cd3fbfadf02496913e155`, preserved untouched; not inferred flashed identities. No Stage2B hardware validation or TUNER work.

### Stage 2A corrected owner hardware PASS / closure review - 2026-10-04

- Corrected provisioning path **OWNER HARDWARE PASS**: protected Android AP association automatically opened captive page without typing IP; nearby scan, protected home-network selection/submission, safe accidental wrong-password recovery, successful STA connection and HTTP/DNS shutdown observed. No reported crash/watchdog/reset/freeze, HOME/touch failure, unrelated corruption or allocation failure during this run. No exhaustive controls or timed endurance test inferred.
- W: CONNECTED/provisioning0/connected1/stored1/storage_ready1, IP192.168.1.83/RSSI-61, scan_count12, reconnects0, errors1/reason2 retained from incorrect password, allocation_errors0; stack minima NETWORK1796B/HTTP2472B, INT_free62239/PSRAM_free7636068. Portal: accepted11/mapped_v4=11/rejected0, requests97/redirects6/HTTP_errors0, Android6/Apple0/Windows0; DNS55 queries/55 answers/zero rejects/errors, HTTP/DNS running0. Physical mapped traffic supports the diagnosed fix; Apple/Windows physical PASS is not claimed. Original two-client CONNECTION RESET failure and correction preserved below.
- **STAGE 2A ACCEPTANCE / CLOSURE DEFERRED**, not fully OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED: original section15 I reboot persistence, J temporary router loss, K recovery and L deliberate Wi-Fi-only reprovision are unperformed, with no original policy permitting deferred closure. I awaits next natural upload/reset; J/K cannot require production-network disruption; G/Y deletion is not requested now and remains automated-only evidence. Independent manual direct-IP fallback from correction procedure also untested. Full A?M matrix and evidence limits in docs/STAGE_2A_WIFI_PROVISIONING.md. No invented new acceptance gates.
- Documentation only: no firmware/source/configuration changes, regression/soak/build/upload, or Stage2B. Non-document inputs match correction final manifest; prior 205/0 regression and soak plus clean build evidence retained. Documented corrected BIN SHA-256 **d8398089b56133cd382c12d888c7a29bd7b4f3386559ebd101f2900f0b5e7b14** preserved. Existing review-entry BIN differs (**e716b4cf03c526041a85bd572098e3e8ca749e044971dfc6e5db93fc205756b5**) and is untouched; ELF hashes/identity caveat recorded in report. Owner-flashed hash was not supplied, so flash identity is not inferred.

### Stage 2A owner HTTP failure / captive portal correction - 2026-10-04

- **FIRST STAGE2A OWNER HARDWARE ATTEMPT FAILED.** Normal boot and protected SoftAP association worked independently on phone/tablet, but both got CONNECTION RESET at http://192.168.4.1/. W showed PROVISIONING/provisioning1/connected0/stored0/storage_ready1, zero reconnect/reason/scan/rejected/errors/allocation-errors, NETWORK minimum1868B, HTTP minimum0, INT_free46855 and PSRAM_free7635900. Exact evidence retained in Stage2A report; no client-blame or hardware PASS inferred.
- Exact source defect: actual CONFIG_LWIP_IPV6=y makes IDF5.5.3 HTTPD listen PF_INET6; lwIP returns IPv4-mapped IPv6 sockaddr values even for native IPv4 clients. Our IPv4-sized getsockname buffer plus AF_INET requirement rejected those connections in custom open_fn before URI handlers, causing session closure with no counted failure. Fix uses full local/peer sockaddr_storage and validated native/mapped IPv4 normalization; true IPv6/truncation, wrong AP/peer, STA access and identical STA/AP addresses remain rejected. Canonical Host/token/Origin/body bounds and secret scrubbing retained.
- Captive portal now implements owner's explicit product requirement: NETWORK-owned nonblocking AP-bound UDP53 socket, checked DHCP DNS offer, fixed513/512B buffers, four queries per turn, temporary100ms owner wait; no new task/queue. Bounded one-question/EDNS0 parser resolves arbitrary A names to AP/TTL0, returns no data for AAAA/other types and rejects malformed/oversized/compressed input. DNS stops with provisioning. Five HTTP routes including read-only GET probe redirect: Android/Apple/Windows and future probes receive302 to constant local IP with nonempty body. Host exception allows redirects only; UI/token/status/POST remain canonical/AP-local, with no credentials in logs, URLs or secret endpoint. Protected random AP key, NVS isolation and AP shutdown preserved.
- W now reports HTTP/DNS running/accept/mapped/reject/request/redirect/error/probe/query/answer counters and rejection reason. Concise accepted/failed socket markers expose pre-handler failures. HTTP watermark now samples open callbacks as well as handlers; requests separately proves handler entry. SDK parser/protocol failures are counted. No continuous DNS/query/secret logs. Captive OS presentation and physical margins remain PENDING; direct-IP fallback retained, no TLS/DoH interception/CAPPORT114 claim.
- Final full regression **205 PASS/0 FAIL/373221 assertions**; soak **205/0/3381471**, including10000 captive lifecycle and10000 bounded parser-fuzz cases. Fifteen additive tests; prior190 tests/assertions unchanged. Final clean/build **SUCCESS exit0,1.90s/510.14s; zero compiler/CMake warnings/errors**. Initial cached-CMake incremental preflight link failure and subsequent clean regeneration recorded; final artifact includes maximum DNS wire-name boundary regression.
- Flash **2460521B (+7824)**; static RAM **45796B (+1144)** versus initial Stage2A source. RTC640 and NETWORK/HTTP stacks6144B each unchanged. DNS adds one transient socket, no task/stack. Source/build input manifest matches; all unrelated firmware/configuration/frozen model/HOME/Core/power/GPIO8/display/touch/brightness/carousel files byte-identical to correction entry.
- Corrected BIN SHA-256 `d8398089b56133cd382c12d888c7a29bd7b4f3386559ebd101f2900f0b5e7b14`; ELF `f7621ded0ac8d041d6eca1f16cb4422956c6d3fe42fb8a65381b8492b701a413`. Exact12-file scope, source proof, security/resource limits and **one corrected owner procedure** in docs/STAGE_2A_WIFI_PROVISIONING.md: upload once, enter provisioning, join protected AP, test automatic captive presentation AND direct IP, then complete existing functional/persistence/router/re-provision/stability acceptance. **OWNER RETEST PENDING; NOT ACCEPTED/CLOSED.** No upload, Stage2B, OTA, web serial or device/power/UI redesign.

### Stage 2A Wi-Fi connectivity / secure local provisioning - 2026-10-04

- Explicit owner authorization starts Stage2A; Stage1 corrected-firmware physical retest PASS and outstanding closure evidence remain as recorded below. PhaseD/Core/frozen CCM, power/GPIO8 correction, hold/idle/wake, display/touch/brightness/fonts/carousel/HOME content are preserved. No OTA, web serial, device integration, power-policy redesign or upload.
- Audited pinned IDF5.5.3/espressif32 6.13.0, existing NVS0x6000 partition and unused factory esp_wifi_bsp; no active network/provisioning infrastructure existed. New network_manager uses SDK Wi-Fi/netif/event loop/NVS/HTTP, with one delayed asynchronous owner and eight-slot nonblocking bounded ingress. SDK callbacks never touch Core/LVGL; serial commands enqueue to NETWORK; LVGL owner alone renders a temporary dismissible local key card.
- First boot/corrupt records open device-specific WPA2 AP with fresh RF-entropy-generated16-hex key displayed locally only. Direct local http://192.168.4.1 scan/select/manual hidden SSID/password POST;16 scan entries, strict512-byte submissions, CSRF/Origin/Host checks, nonce CSP, AP-address/socket gate including fail-closed STA/AP address collision. No password logs, credential URLs, secret-returning endpoint, cloud or captive-DNS dependency. WPA2 Personal/mixed and compiled WPA3 SAE supported in software; physical interoperability PENDING.
- Versioned/checksummed108-byte credential blob in remote_wifi/credentials; SDK driver RAM-only. Save after verified association/IP; failed connections/router loss retain credentials. Thirty-second attempt deadline,5/10/20/40/60s capped reconnect backoff. Success closes protected provisioning AP/server after2s; failure remains provisionable. DEVELOPMENT_ONLY W reports state/resources without secrets and redisplays local card; G then Y within10s deliberately erases only this Wi-Fi key and starts a fresh AP session. NVS partition/fonts/configuration unchanged. NVS/flash encryption OFF: no claim against physical flash extraction; provisioning HTTP is protected by the generated WPA2 link, not TLS.
- Native font bounds identified and corrected setup-card clipping before release: explicit SSID prefix/suffix and two8-character AP-key lines, joined without spaces. Card x6/y148/160x268 fits172x640; fonts12/14 already enabled/qualified. DISPLAY_IDLE leaves radio/owner active; touch/motion behavior unchanged. Automatic system sleep/hard-off remain inhibited while external/charging UNKNOWN; future deeper-sleep Wi-Fi coordination is documented, not implemented.
- Complete final regression **190 PASS/0 FAIL/368111 assertions**; soak **190/0/3331361**,75000 combined cycles including10000 new network cycles. Thirteen additive network tests cover state/retry, corruption/bounds, replacement/preservation/namespace-only deletion, strict forms, actual diagnostic redaction, bounded inbox failures/scrub and lifecycle. Existing177 tests/assertions unchanged. Portal JavaScript syntax and native text bounds PASS; SDK/RF/real HTTP/flash interruption remain hardware tests.
- Final clean/build commands pio run -e remote01 -t clean -j 1 then pio run -e remote01 -j 1: **SUCCESS, exit0,1.85s/515.86s; zero warnings/errors**. Flash **2452697B, +646992**; static RAM **44652B, +24380** versus corrected Stage1 source baseline; RTC640 unchanged. NETWORK/temporary HTTP stacks6144B each; SDK dynamic task/buffer/netif/NVS/socket costs and physical margin checks documented. Final build-source manifest unchanged; all unrelated existing files byte-identical.
- BIN SHA-256 `b1561e47e1698eddc572ec7edf83e44dc66191df5a5d5f2dc1b14ddf43347a96`; ELF `f68ad30eef1f27ecf35fc71eb28e0a0bbca8c6310f5ee29a0946ec370db0b9af`. Exact16-file scope, architecture/security limitations and **one consolidated owner procedure** in docs/STAGE_2A_WIFI_PROVISIONING.md. **STAGE2A IMPLEMENTED / AUTOMATED VERIFICATION PASS / OWNER HARDWARE VALIDATION PENDING.** No Stage2A acceptance or Stage1 closure inferred.

### Stage 1 corrected firmware owner retest PASS / closure review - 2026-10-04

- Owner physically observed GPIO8 awake_irq_type2/expected_negedge2, gpio_low0, worker/ISR service core1 on V2 PCB V1.1. Corrected run reached75 dispatches/601 motion events/8 wakes, both idle/active transitions and202â€“235ms wake-to-ready. USB stable; no-PWR USB removal maintained battery power, UI/touch/controls/idle/motion wake with no observed reset. Reconnecting USB affected serial only: boot stayed4, reset11/USB, uptime1105644ms. Retained pre-fix INT_WDT is historical, not a new corrected-firmware reset.
- Post-battery: ACTIVE/hold1, sleep_requests0/rounds0/errors0, IMU1/motions601/bus_errors0, POWER stack minimum1368, latency202ms. REAL calibrated4146/filter4152mV, valid NORMAL, read_errors0, external/charging UNKNOWN. Core INT free169263/min94907/largest53248; PSRAM free/min7636184/largest7602176; failed_allocations0/accounting_corrupt0; stack minimum15364/owner_ok1/queue_rejected0. Initial battery approximately4.09V, hold1 and clean counters; exact owner evidence retained in Stage1 report.
- **OWNER CORRECTED-FIRMWARE STABILITY RETEST PASS; OVERALL STAGE1 ACCEPTANCE PENDING, NOT CLOSED.** Physical GPIO correction/hold/handover/idle/wake and reported-interval resource/stability checks accepted. No sole-cause claim for all historical restarts; failure/investigation history preserved. Automatic sleep/hard shutdown remain inhibited; protected Li-Po1000mAh specification unchanged. Stack/memory observations require no invented threshold or firmware adjustment.
- Original mandatory gaps remain: independent physical voltage comparison; safe LOW/CRITICAL/transient/recovery simulation; full normal regression/gesture-isolation confirmation; documented20 cycles/30min battery-only and10min untouched per source; tested BIN identity confirmation. Eight reported wakes and18min25.644s total boot uptime do not establish those endurance criteria. Minimum combined remaining evidence procedure documented; already-performed results can satisfy gaps without repetition.
- Documentation only: CHANGELOG.md and docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md. Source/tests/configuration match the correction-build manifest; all non-documentation files unchanged in this review. Existing BIN/ELF preserved, no rebuild/test/upload. Current BIN `d7d4a8fa305752baa36a114cfa7ccc2533c666799308d9fbdc61a7f2061f6532`, ELF `9584a69f0edbf471b10e5f88ec2436616e5afd191c9e2c3a564562e3404cba98`; original corrected-build hashes retained separately, flashed-byte identity not inferred. Stage2 not started.

### Stage 1 INT_WDT captured / GPIO wake conflict correction - 2026-10-04

- Owner captured boot2 **reset5/INT_WDT, brownout0**. Prior boot1 reached ACTIVE/NORMAL, hold1, voltage4071/filter4064mV, last ADC_OK236721ms, sleep_requests0/rounds0. Post-reboot COMMAND66 is B, not the stalled operation. ADC_OK is a completed periodic sample, not ADC causality proof. A spontaneous reset also occurred with USB continuously connected: handover is not the established cause; its cold-start/hold/immediate-operation PASS remains.
- Owner current telemetry: ACTIVE/hold1, requests0/rounds0/errors0, IMU1/motions2/bus_errors0, POWER stack minimum1368; calibrated REAL4161/filter4155 NORMAL/read_errors0/source UNKNOWN. Core INT free180275/min177331/largest92160; PSRAM free7636184; failed_allocations0/accounting_corrupt0; stack minimum15364/owner_ok1/queue_rejected0. Full retained rows in Stage1 report.
- Confirmed source defect: startup gpio_wakeup_enable LOW_LEVEL overwrote enabled GPIO8 NEGEDGE with awake level-low in pinned IDF5.5.3. A held-low shared expander IRQ can retrigger continuously and starve ticks. Fixed the demonstrated configuration conflict: awake remains edge; only explicit forced sleep masks ISR, arms level wake, sleeps, disarms/restores edge then unmasks. Failures clean up safely and retain polling. Specific owner-reset attribution remains PENDING CPU/backtrace/stability evidence; no speculative ADC or stack fix.
- Boot/B now report actual GPIO trigger and worker/ISR-service routing CPU; SDK reset reason cannot identify the historical watchdog CPU. Existing USB panic output can capture CPU/backtrace if failure recurs. POWER stack1368/4096 observed margin retained; no overflow evidence or stack/configuration increase. Sleep policy/Core/PhaseD/UI/hardware pins unchanged.
- Full regression **177 PASS/0 FAIL/345778 assertions**; soak **177/0/3111028**,65000 existing combined cycles. Two added held-low shared-trigger and cleanup/failure regressions; existing assertions unchanged. Exact clean/build commands succeeded **2.14s/508.33s**, exit0, **zero warnings/errors**. Flash **1805705 bytes, +996** versus previous diagnostic; static RAM **20272, delta0**, RTC640/task stacks unchanged.
- Correction/diagnostic BIN SHA-256 `b37619c4db96ffba5a6c73fdd275ef67725282b6d67ba4395907812798a1f594`; ELF `f2b1935f456686562efa821e1748afc55885ab7b7d708f9a4b2b588abf4639e3`. Exactly five files: power runtime, new wake_irq.hpp, power tests, Stage1 report and CHANGELOG. All other files/configuration byte-identical to entry.
- **STAGE1 HARDWARE VALIDATION FAILED â€” SPONTANEOUS INT_WDT; VERIFIED GPIO SOFTWARE DEFECT FIXED / OWNER STABILITY RETEST PENDING.** Normal acceptance remains stopped; one USB-then-battery stability/capture procedure documented. No upload or Stage2.

### Stage 1 hardware failure / reset investigation - 2026-10-04

- **STAGE 1 HARDWARE VALIDATION FAILED â€” RESET AFTER USB->BATTERY HANDOVER; ROOT CAUSE UNDER INVESTIGATION. Normal acceptance STOPPED.** Owner physically confirms USB cold start, early SYS_EN hold and immediate display/touch/UI after USB removal PASS; sustained battery operation FAIL with reproducible repeating restarts approximately5â€“10s after USB removal. PWR inaccessible/unused; battery-only event log unavailable after USB reconnect. No cause attributed.
- Owner USB-powered display idle/wake works: DISPLAY_IDLE wake0 hold1 display0 -> ACTIVE wake2 hold1 display1, manager_wake_to_ready_ms202. These partial physical passes do not establish sustained-battery acceptance.
- Audited restart/panic, expander/direction/hold, sleep, watchdog, battery/ADC/IMU fault and5â€“10s paths against actual IDF5.5.3 and entry ELF. No explicit Stage1 restart, delayed TCA reset/EXIO6 reconfiguration or normal automatic-sleep entry found. Existing startup panic checks and SDK faults remain possible; UNKNOWN real source still inhibits sleep. K/! and Z remain explicit development overrides. No demonstrated software cause or behavioral fix claimed; accepted PhaseD/Core/policy/hardware configuration unchanged.
- Added DEVELOPMENT_ONLY POWER_RESET history:640 RTC bytes, four boots with two validated80-byte checkpoint copies each;80-byte internal working record. Stores supported SDK reset/wake reasons, RTC-epoch boot count, POWER-runtime flag, real battery/state/hold/event/uptime and sleep counters. Boot and existing B reprint bounded history after USB reconnect. Brownout explicitly marked only when SDK reports it; unavailable retention is not inferred evidence. No heap/NVS/filesystem churn, new task or periodic serial spam.
- Complete default regression **175 PASS/0 FAIL/343748 assertions**; soak **175/0/3108998**,65000 combined cycles. Two additive record tests cover validation/corruption/interrupted commit, prior-runtime retention, bounded four-boot history and counter wrap. Existing assertions unmodified.
- Exact clean/build commands `pio run -e remote01 -t clean -j 1` then `pio run -e remote01 -j 1`: **SUCCESS1.96s/510.47s**, exit0, **zero warnings/errors**. Flash **1804709 bytes, +3020** versus documented amended Stage1; static RAM **20272, +80**; RTC history **640 bytes separately**. BIN SHA-256 `d6fd7ed04c1ae1dac1d26da882391697c03039df4cdf3d422ac95a867be2207d`; ELF `f9cb9baf0d8a322a7c70fcea58e83c08baba8001b6314762add72e1afe97642c`.
- Source/diagnostic design, exact changed files/artifact verification and a one-cycle reset-capture retest are recorded in `docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md`. Diagnostic firmware is not hardware PASS or a confirmed fix. No upload or Stage2 work.

### Stage 1 owner corrections / final requirements - 2026-10-04

- Corrected all battery-capacity references to **Li-Po, 1000 mAh nominal, protection PCB present; protection specifications/thresholds undocumented**. Capacity existed only in documentation: no runtime/test/configuration calculation, SOC, safety threshold, cutoff, charger setting, timer or policy depends on it. Existing voltage conversion/filter/thresholds unchanged.
- Enclosure PWR is inaccessible. Verified unchanged unconditional startup asserts EXIO6/SYS_EN before LCD initialization on USB boot, without a battery/PWR boot condition. One corrected owner procedure in `docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md`: USB cold start -> hold assertion -> USB removal -> sustained battery operation is the PRIMARY acceptance test; USB provides recovery. No acceptance step requires PWR.
- Re-audited exact pinned V2 PCB Rev1.1 schematic and official battery/power example: ETA6098 STAT drives charging LED only; VBUS has no MCU sense route; SYS_OUT/GPIO16 is affected by the hold/button network and its official one-time boot classification cannot establish continuous external-power absence. External/charging remain UNKNOWN. **Normal automatic SYSTEM_SLEEP is inhibited entirely**: policy requires verified ABSENT, runtime supplies real UNKNOWN. Normal saving stops at DISPLAY_IDLE/backlight-off with existing touch/motion wake. Explicit guarded Z remains DEVELOPMENT_ONLY, outside normal policy/required acceptance; simulation cannot authorize real sleep.
- Only affected power regressions rerun: default **15 PASS / 0 FAIL / 28158 assertions**, soak **15 / 0 / 100158**, including 1000/10000 synthetic guarded sleep/resume cycles. New UNKNOWN/PRESENT test checks 10000 turns each (>5h simulated), zero automatic sleep requests and retained idle/wake. Original full-suite results remain historical, not a new complete-suite claim.
- Exact clean/build commands `pio run -e remote01 -t clean -j 1` then `pio run -e remote01 -j 1`: **SUCCESS**, 1.84s/516.14s, **zero warnings/errors**. Flash **1801689 bytes /21.5%, +224** versus initial Stage1; static RAM **20192 /6.2%, delta0**. BIN SHA-256 `26b53bef47bef55bdb9b17f18ea6d391bb72c12490bc5dde20cea17ea2ab3d04`; ELF `c90dbddbede88a48ca6cc11b337928b0a7b3d2957962f8cad3be6703708a5dc4`. Initial Stage1 artifacts are superseded.
- Exactly six amended files: power `model.hpp`, `model.cpp`, `runtime.cpp`; `tests/control/power_tests.cpp`; Stage1 report; CHANGELOG. No added files. Protected startup/hardware, display/touch/carousel/brightness, Core/Mock/frozen CCM and configuration unchanged. **HOST TESTED / CLEAN BUILD VERIFIED / OWNER HARDWARE VALIDATION PENDING**; physical handover/telemetry/idle/wake/no-auto-sleep acceptance not inferred. No upload or Stage2 work.

### Phase D owner hardware acceptance / closure - 2026-10-04

- Owner reports **Phase D OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED**. Physical owner acceptance supersedes the Phase D pending-retest labels below and closes its hardware gate. Phase C remains accepted and closed.
- No additional per-key results, runtime duration, artifact hash confirmation or physical telemetry were supplied; none are inferred. Existing profile/durability limitations and the historical Phase A internal-memory watch remain for future real adapters/significant loads.
- Documentation only: updated this changelog, Phase D report, Core implementation report and HOME integration report. Historical evidence preserved; firmware/configuration/frozen contracts unchanged. No build, upload, Phase E or next milestone.

### Phase D generic adapter boundary / lifecycle - 2026-10-04

- Entry confirmed: Phase C **OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED**. No Git metadata/commit available; pre-change 1596-file checksum manifest and complete runtime map recorded in `docs/PHASE_D_GENERIC_ADAPTER_BOUNDARY.md`. Baseline default **140 PASS / 0 FAIL / 284374 assertions**; soak **140 / 0 / 2779624**, 45000 iterations. Fresh clean build 1751253 flash/19776 RAM, zero warnings/errors; 36 fewer flash bytes than accepted Phase C despite identical source, with cause unassigned.
- Added one bounded generic owner boundary reusing existing Core/Adapter/Dispatch/Journal contracts: eight inline copied event slots, captured epoch/session/mapping/association/revision/media, explicit FIFO/full/contention failure and route invalidation, reconnect/reattach/shutdown fencing, owner-only decoding and no hidden retry. Full schemas remain owner-published; asynchronous profile limitations are explicit. No persistence backend; real execution still requires frozen durable identity/counter/effect/barrier recovery guarantees.
- Mock now uses the boundary for owner mutations and copied USB-triggered observations/completion; periodic acquisition retains the checked owner fast path. Existing keys/USB configuration/HOME semantics unchanged. Fixed new-ingress delayed receipt/age normalization, precise FIFO acknowledgement, repeated-fault mutation and closed-boundary pending execution; eliminated a 6096-byte temporary pack stack frame. No existing assertion weakened.
- Final default tests **158 PASS / 0 FAIL / 306563 assertions**; soak **158 / 0 / 2999813**, 45000 existing iterations plus 10000 ingress cycles (220000 posts/20000 stale drops). Failure injection/cold-start/producer ownership and full diff/checksum review documented. Target PSRAM wrapper **83656 bytes, +78016**; producer posts and unchanged service allocate zero. Periodic/idle Phase C allocation costs unchanged; serial transition stress adds 30500 measured allocation/free pairs. New on-demand m `UC_INGRESS` counters; no per-tick logging.
- Final `pio run -e remote01 -t clean -j 1` **SUCCESS (1.69 s)** then `pio run -e remote01 -j 1` **SUCCESS (507.61 s)**. Flash **1763953 bytes / 21.0%, +12664** versus accepted Phase C (**+12700** versus fresh baseline); static RAM **19776 bytes / 6.0%, delta 0**; **0 target warnings/errors**. Firmware/ELF SHA-256 and complete measurements in the Phase D report. Optional host sanitizer unavailable: MSYS GCC lacks libubsan; no sanitizer PASS claimed.
- **Phase D HOST TESTED / CLEAN BUILD VERIFIED / HARDWARE VALIDATION PENDING OWNER RETEST**. Exact manual COM3 retest and remaining durability/profile/physical-headroom gates documented. All preexisting files except the six listed modifications are byte-identical, including frozen CCM/Core/model, existing tests, HOME/facade/API, UI/drivers/hardware/configuration and historical A/B/C reports. No TUNER/yoRadio access, networking/real adapter/Phase E work or upload.

### Phase C owner hardware PASS / closure - 2026-10-04

- Owner reports **REMOTE//01 Phase C hardware validation: PASS; OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED** after approximately 960 seconds. Prescribed functional retests passed, including **v=25**, capability/session behavior, disabled-action behavior, no buffered replay, delayed completion and >3 s late completion. The earlier v=50 discrepancy is closed by owner confirmation of the current v=25 behavior; no firmware value changed.
- Final physical telemetry: `key=m recognized=1 accepted=1 owner_ok=1 session=3 capability_revision=45 dispatches=14`; internal free/min/largest **167519 / 166491 / 94208** bytes; PSRAM free/min/largest **7714504 / 7714504 / 7602176** bytes; payload **85765**, allocations **30465**, frees **30114**, failed allocations **0**, accounting corruption **0**, stack minimum **15888** bytes, owner_ok **1**, queue_rejected **0**.
- No crash, panic, reset, watchdog or UI freeze observed. Owner physical evidence and acceptance supersede previous Phase C pending-retest labels. No Phase C hardware gate remains pending; the historical Phase A internal-memory watch remains for future significant loads.
- Documentation-only closure: updated this changelog, Phase C report, HOME integration report and Core implementation report. Firmware unchanged; no build, upload, Phase D or next milestone.

### Phase C HOME/runtime hardening - 2026-10-04

- Entry: Phase B HOME/Core/Mock OWNER ACCEPTED / CLOSED / PASS, including corrected physical USB RX. Recorded newly supplied owner telemetry at startup and approximately 994 s, complete accepted behavior and no observed crash/panic/reset/watchdog/freeze in `docs/PHASE_C_RUNTIME_HARDENING.md`. PSRAM free unchanged; live allocation delta is not classified as a leak. Phase A's 34,515-byte internal-memory watch retained.
- Reproduced/fixed buffered presses rebasing after reconnect/revision restoration, buffered/pending-refused presses reviving after completion, duplicate delayed execution after completion-evidence allocation failure, and accepted=1 acknowledgement of a Core admission refusal. Same fixed eight-slot FIFO, serialized owner, normal Core envelope/dedup/result/state path and static HOME geometry. No new user feature.
- Local measured allocation correction: fixed selection pointer scratch and exact-evidence equality guard. Unchanged tick allocation pairs 8000->0 per 1000 ticks; 30-second deterministic runtime model 6750->870, identical outcomes. No broad allocator rewrite or physical memory improvement claim.
- v source/tests/docs consistently define 25; physical reported 50 remains an explicit artifact/result discrepancy for owner retest. Keys, USB driver/read/log path, Mock normal semantics, Phase A opt-in instrumentation and profile=0 boot preserved; no frozen/UI/hardware/configuration changes or real networking/TUNER work.
- Final complete default host suites: **140 PASS / 0 FAIL / 284374 assertions**, including 26 new hardening tests and 4500 stress iterations. Opt-in finite soak: **140 PASS / 0 FAIL / 2779624 assertions**, 45000 stress iterations. Strict compiler checks pass; target stack/ABI and per-test allocator balance reviewed. Exact thirteen-file scope, audit, measurements, limitations and manual owner retest are recorded in the Phase C report. Historical failure/review logs preserved.
- Final `pio run -e remote01 -t clean -j 1` **SUCCESS (1.95 s)** then `pio run -e remote01 -j 1` **SUCCESS (479.16 s)**. Flash **1751289 bytes / 20.9%, +1640** versus fresh baseline 1749649 (+1664 versus documented Phase B 1749625); static RAM **19776 bytes / 6.0%, +128**. **0 warnings / 0 errors**. **Phase C HOST TESTED / CLEAN BUILD VERIFIED; hardware validation PENDING OWNER RETEST**. No upload, Phase D or real adapter work.

### Phase B HOME/Core/Mock owner hardware PASS / closure â€” 2026-10-03

- Owner reports **HOME/Core/Mock hardware validation: PASS** following the development serial correction. This is physical owner acceptance, not a host-test prediction. Serial input retest and overall Phase B hardware validation gates are closed: **OWNER ACCEPTED / HARDWARE PASS; PHASE B CLOSED / PASS**.
- Previous serial failure and pending-retest entries below are historical and superseded by this acceptance. No additional per-key logs or memory measurements were supplied with the acceptance.
- Acceptance covers the synthetic HOME/Core/Mock integration only; it does not validate a real TUNER adapter, networking or an end-to-end real-device protocol. The existing 34,515-byte internal-memory watch remains open for future significant loads.
- Documentation only: updated this changelog, `docs/HOME_CORE_INTEGRATION_V1.md` and `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md`. Firmware/configuration/frozen CCM unchanged; no build, upload or new milestone.

### Phase B development serial input defect / correction â€” 2026-10-03

- Owner hardware PASS: normal boot/profile=0, display orientation, carousel 0->1->2 and 2->1->0, snapping, brightness, initial Mock HOME, PREVIOUS/PLAY-PAUSE/NEXT, Mock track/playback/icon changes, HOME intents accepted by Core; no crash/reset/watchdog observed. These owner results do not complete overall HOME-CORE-MOCK hardware validation.
- Owner reproducible FAIL: documented Mock keys sent on interactive COM3/115200, with/without Enter, produced neither acknowledgement nor HOME change despite `input=USB_SERIAL_JTAG`. **Serial correction PENDING OWNER RETEST; overall hardware validation remains incomplete.**
- Actual COM3 PNP identity verified read-only: Espressif USB Serial/JTAG VID_303A/PID_1001/MI_00. Resolved SDK is versioned `framework-espidf@3.50503.0`, ESP-IDF 5.5.3. Standard console writes duplicate to primary UART0 and secondary USB; stdin reads only primary UART0. Old reader used the correct secondary USB VFS endpoint, but its nonblocking available-byte gate reads the driver RX ring buffer. No USB driver had been installed, so that count was always zero; direct FIFO logging still worked. VFS registration/open success did not prove RX readiness. Neither Arduino/UART API mixing nor CR/LF was the root cause.
- Small fix: existing Core owner installs/reuses native USB Serial/JTAG driver (default RX/TX 256 bytes each), pairs secondary VFS logging with it, reads at most eight bytes via zero-wait driver API, and invokes the unchanged Mock command path only on that owner. No HOME/UI/Core/Mock semantic or hardware-configuration change. Each recognized key including telemetry acknowledges; unknown printable ASCII logs deterministic rejection; CR/LF/nonprintable bytes remain silent. Explicit initialization success/error replaces the old open-success claim. Dynamic USB driver memory is outside Core tracked payload; existing 34515-byte internal-memory watch item retained.
- Exact files modified (6): `components/control_home/runtime.cpp`, `components/control_home/include/control_home/development.hpp`, `components/control_home/CMakeLists.txt`, `tests/control/home_tests.cpp`, `docs/HOME_CORE_INTEGRATION_V1.md`, `CHANGELOG.md`. No added/deleted files. Core/facade/API, Device semantics, HOME/buttons, display/touch/carousel/brightness, GPIO/backlight, frozen CCM, configuration/package versions remain unchanged; no networking/TUNER work/upload.
- Complete host suite: **69 foundation PASS / 0 FAIL / 2425 assertions; 6 profile PASS / 0 FAIL / 40; 39 HOME PASS / 0 FAIL / 3541; total 114 PASS / 0 FAIL / 6006 assertions**. Five new regressions: single key/telemetry, CR, LF, unknown printable, serialized owner gate/path. Host/source tests do not validate physical USB input. Exact clean build measurements and manual COM3 owner retest procedure are recorded in `docs/HOME_CORE_INTEGRATION_V1.md`.
- Final `pio run -e remote01 -t clean -j 1` **SUCCESS (1.97 s)**, then `pio run -e remote01 -j 1` **SUCCESS (505.07 s)**. Flash **1,749,625 bytes / 20.9%, +7,972** from prior Phase B 1,741,653; static RAM **19,648 bytes / 6.0%, delta 0**. **0 compiler warnings/errors**. USB driver runtime allocations are additional dynamic memory, not represented by static RAM delta. Exact COM3 upload/interactive monitor and single-key/CR/LF/unknown/state/telemetry retest are prepared in the HOME report. **Serial input PENDING OWNER RETEST; overall HOME-CORE-MOCK hardware validation incomplete. No upload performed.**

### Phase B HOME -> Core -> synthetic Mock integration â€” 2026-10-03

- Persistent serialized `UC_owner` on CPU 1, priority 2, 24576-byte stack; Session explicitly in PSRAM. Normal boot no longer runs Phase A stress/retention/cleanup. Profiling sources/historical evidence remain unchanged and opt-in. Same Core, Store, process-local Journal and synthetic adapter; no second Core implementation.
- Neutral eight-entry intent FIFO and fixed copied/coalesced snapshot; LVGL callback only enqueues, Core owner performs all reads/mutations and dispatch, existing LVGL owner updates widgets. No HOME->Mock or raw Core pointer boundary. Current-token retries use the identical Core envelope; older tokens and queue-full newest presses reject deterministically. HOME timer removed/references cleared when page is deleted.
- Dynamic connectivity, media title/subtitle, playback label/symbol, volume/bar and control availability. PREVIOUS/NEXT route through Core; PLAY/PAUSE derives from fresh accepted PLAYING/PAUSED, with field preconditions. UNKNOWN/conflicted/unsupported/unavailable/pending state disables appropriate controls; 40% disabled opacity is the only added treatment. Independent synthetic acquisitions supply observations; no optimistic UI truth. Static battery/Wi-Fi/product/artwork/quick-nav literals remain decorative; volume is display-only. Accepted geometry/palette/fonts/carousel/pages/snapping/spacer/brightness/rotation/touch/GPIO/drivers/backlight/toolchain unchanged.
- Small DEVELOPMENT ONLY / MOCK ONLY nonblocking hook on existing secondary USB console: external playback/volume, disconnect/reconnect, session markers, actual NEXT descriptor withdrawal/restore, readiness revision, UNKNOWN, older-sequence injection, delayed completion and on-demand `UC_RUNTIME` telemetry. No shell/hidden gestures. Telemetry includes internal/PSRAM free/minimum/largest, tracked payload/counters/failures/corruption, stack minimum, owner and queue rejection checks.
- Exact new files (9): `components/control_home/CMakeLists.txt`, `components/control_home/facade.cpp`, `components/control_home/development.cpp`, `components/control_home/runtime.cpp`, `components/control_home/include/control_home/api.hpp`, `components/control_home/include/control_home/facade.hpp`, `components/control_home/include/control_home/development.hpp`, `tests/control/home_tests.cpp`, `docs/HOME_CORE_INTEGRATION_V1.md`. Exact modified files (10): `components/control/core.cpp`, `components/control/include/control/core.hpp`, `components/control/mock_adapter.cpp`, `components/user_app/user_app.cpp`, `components/user_app/CMakeLists.txt`, `main/main.cpp`, `main/CMakeLists.txt`, `tests/control/run_tests.py`, `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md`, `CHANGELOG.md`. No deletions or unrelated source/config edits.
- Full final host suites: foundation **69 PASS / 0 FAIL / 2425 assertions**, profile **6 PASS / 0 FAIL / 40**, HOME **34 PASS / 0 FAIL / 3507**; total **109 PASS / 0 FAIL / 5972 assertions**. Native font bounds separately PASS for 16 new dynamic string/symbol samples. Host tests are not physical evidence.
- Review corrections: owner-side observation ingestion instead of invalid outside-dispatch publish; coherent media-context invalidation on changed tracks; media schema limited to the requested two text fields; explicit session/revision diagnostics. Fixed the test expectation for deliberately rejected conflicting sequence. All complete suites passed after corrections; no frozen CCM contradiction found.
- Final reviewed-source `pio run -e remote01 -t clean -j 1` **SUCCESS (1.99 s)**, then `pio run -e remote01 -j 1` **SUCCESS (519.46 s)**. Flash **1,741,653 bytes / 20.8%**, exact Phase A delta **-884** from 1,742,537. Static RAM **19,648 bytes / 6.0%**, exact delta **+256** from 19,392. **0 compiler warnings/errors**. The earlier clean build passed too; complete suites and clean/build repeated after final review corrections. Dynamic runtime memory/physical headroom remains for owner validation.
- **Phase A memory watch item retained: internal minimum 34515 bytes**, owner-accepted nonblocking; substantial future networking/adapters/discovery/TLS/catalog/artwork loads require renewed physical profiling. Phase B combined headroom/stability and all physical UI/control tests remain **PENDING OWNER TEST**. Exact clean build measurements and the complete owner upload/interactive serial/test procedure are recorded in `docs/HOME_CORE_INTEGRATION_V1.md`. No upload, TUNER repository access, TUNER adapter/networking or frozen-model edit. Stop after Phase B preparation; next action owner physical validation.

### Phase A owner acceptance / closure â€” 2026-10-03

- Automated/local checks PASS; real ESP32 execution COMPLETED; owner review COMPLETED; owner acceptance ACCEPTED. **CORE ESP32 RUNTIME PROFILED â€” OWNER ACCEPTED / PASS; PHASE A â€” CLOSED / PASS.** Historical `phase_a=OWNER_REVIEW_REQUIRED` and preparation records remain unchanged. Acceptance applies only to Phase A; Phase B implementation is now authorized, hardware validation remains PENDING OWNER TEST.
- Exact owner-provided checkpoint measurements are recorded in `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md`, including wrapper 5112 bytes in PSRAM, internal minimum 34515, both stress/purge waves, 32 iterations and exact initial heap recovery: internal 267191 and PSRAM 7720268 bytes. Cleanup payload 0, allocations/frees 23976/23976, failures/corruption 0, owner_ok=1; final local_checks=PASS, balance=1, heap_integrity=1. No captured leak, watchdog, crash or ownership failure.
- **Memory watch item remains open: 34515-byte minimum internal free heap**, accepted nonblocking. Substantial future runtime loads require renewed physical profiling; no premature optimization solely for this watch item.

### Universal Control Phase A runtime/memory preparation - 2026-10-03

- Implemented only **Phase A diagnostics**. **Hardware gate PENDING; Phase B NOT STARTED**: no serial devices enumerated and no real-board execution/heap/stack/stability measurements obtained. No CORE ESP32 RUNTIME PROFILED or HOME-CORE hardware validation claimed; no upload performed.
- One dedicated CPU 1 / priority 2 / 24,576-byte-stack owner creates, exercises and destroys Store/Core/Mock/Mock Journal after UI startup. It never calls LVGL or HOME. Explicit PSRAM Session container; unchanged Core malloc policy instrumented by optional non-allocating observer. Logs actual pointer classification, per-region live/peak payload and allocated-block bytes, allocation/free/failure counts, internal/PSRAM free/minimum/largest blocks, owner checks, reset reason and stack minimum. Entire Session is reclaimed after a finite profile; task/TCB remains until its final self-deletion.
- Runtime profile: synthetic one device/target, two bindings, 28 descriptors, five source samples, one shared resource. Exercises commands/results/separate state, duplicates, stale sequence/session, delayed/no result, expiry, resource conflict/no-execution proof and resync. **32 stress iterations**, two waves of 16, with real 60,001-ms retention waits/purge, session/capability/state/conflict updates; 69 synthetic dispatches on success. No capacity/policy enlargement or frozen CCM semantic change. Mock installation now accepts current monotonic time, keeping its prior default for existing callers.
- Exact new files: `components/control_profile/CMakeLists.txt`, `components/control_profile/profile.cpp`, `components/control_profile/runtime.cpp`, `components/control_profile/include/control_profile/profile.hpp`, `components/control_profile/include/control_profile/runtime.hpp`, `tests/control/profile_tests.cpp`. Exact modified files: `main/main.cpp`, `main/CMakeLists.txt`, `components/control/include/control/storage.hpp`, `components/control/include/control/mock_adapter.hpp`, `components/control/mock_adapter.cpp`, `tests/control/run_tests.py`, `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md`, `CHANGELOG.md`.
- Actual host tests: `python tests/control/run_tests.py` -> **69 foundation PASS / 0 FAIL (2,425 assertions)** plus **6 profile PASS / 0 FAIL (40 assertions)**; total **75 PASS / 0 FAIL**. Full/advancing-time driver, owner/wait rejection and observer balance/failure tests use virtual time and are not hardware measurements.
- Actual `pio run -e remote01 -t clean -j 1` then `pio run -e remote01 -j 1` -> **SUCCESS**; final diagnostic rebuild same command -> **SUCCESS**. Flash **1,742,537 bytes (+18,996)**; static RAM **19,392 bytes (+104)** versus Core foundation. **0 compiler warnings**. Dynamic stack/container/heap footprint remains physically UNKNOWN. Target compiler frame data and exact manual owner flash/serial procedure recorded in [implementation report](docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md).
- Adversarial review: single Core owner/lifetimes, no LVGL/Mock calls from HOME, no command-created state, timestamp-consistent observations, bounded yielding/stress, purge/cleanup accounting, pointer-based memory placement, failure stop and explicit hardware gate. Existing user_app/LVGL/drivers, fonts/palette/HOME geometry, carousel/pages/spacer/snapping, rotation/touch/GPIO/backlight and PlatformIO/SDK/package configuration unchanged. Main adds only the profile include/start after LVGL unlock.
- No TUNER/yoRadio repository access, real adapter or networking added. Next action is **owner manual upload + full Phase A serial/memory/stability evidence review**; only actual Phase A PASS permits deferred HOME->Core->Mock integration. No later milestone started.

### Universal Control Core v1 foundation + local Mock Adapter - 2026-10-03

- Implemented a portable ESP-IDF `components/control` foundation against **CCM v1.0.0 OWNER APPROVED / FROZEN**. Frozen model, architecture, Native API candidate and historical reviews remain byte-identical. No CCM contradiction discovered. Implementation is a documented subset, not full CCM conformance.
- Added scoped CCM records/registries, tagged absent values, capability/context captures, verified-sequence state reduction and persistent conflict roster, monotonic deadlines, one-route/one-attempt command lifecycle/cancellation, bounded dedup with exact purge/compact pairing metadata, qualified resource ordering and independent historical per-key proof/barrier state. Commands do not manufacture state. Late proof preserves delivery history and cannot reopen the old parent; independent queued work follows its normal gate. Added synthetic catalog merge/stale-reference and two bounded rational numeric profiles.
- Mock identity `adapter.mock` and synthetic device/target labels: power set/toggle, volume set/step/mute, playback play/pause/stop/previous/next; four state fields. Catalog tests additionally publish source.select. Deterministic confirmed/open-loop/refusal/unavailable/delayed/no-result/partial/ambiguous/no-execution modes; synthetic stale/duplicate/session/revision/media/restart cases. Core uses the generic Adapter/Sink/Journal boundary, with no Mock-specific calls. Mock Journal is process-local, not durable NVS recovery.
- Exact new files: `components/control/CMakeLists.txt`, `components/control/core.cpp`, `components/control/mock_adapter.cpp`, `components/control/memory_metrics.cpp`, `components/control/include/control/storage.hpp`, `components/control/include/control/model.hpp`, `components/control/include/control/core.hpp`, `components/control/include/control/mock_adapter.hpp`, `tests/control/core_tests.cpp`, `tests/control/run_tests.py`, `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md`. This `CHANGELOG.md` is the only pre-existing file changed.
- Actual final host run: `python tests/control/run_tests.py` -> **69 PASS / 0 FAIL, 2,425 assertions**, strict warnings as errors, per-test allocation balance, 32-budget allocation-failure sweep. FR01-FR16 applicability/regression mapping is recorded in the implementation report; recipe/retry/general-numeric findings are explicitly partial/deferred. Optional undefined-behavior sanitizer linking failed because installed GCC lacks `libubsan`; no sanitizer PASS claimed and no toolchain upgrade.
- Adversarial corrections cover complete nested Mock schemas, descriptor domains, state invalidation, whole-binding sequence resets/recovery, resource revisions/media scope, weaker delivery/progress evidence, partial completion proofs, post-purge immutable unit extent, deadline revision counting and atomic OOM reservation rollback. Corrected code was retested and rebuilt.
- Actual `pio run -e remote01 -t clean -j 1`, then `pio run -e remote01 -j 1`: **SUCCESS**; final correction rebuild with the same build command **SUCCESS**. Retained code/sizeof anchors prevent linker GC from disguising detached-module cost. Final flash **1,723,541 bytes (+111,000)**; static RAM **19,288 bytes (+72)** versus actual pre-change build. **0 compiler warnings** in clean/final builds. Host peak owning payload **3,027,218 bytes**; target record sizes/capacities and allocation overhead limits documented separately. No ESP32 runtime memory PASS claimed.
- Validation: **CORE IMPLEMENTATION TESTED / REMOTE FIRMWARE COMPILE VERIFIED**, limited to this slice. No TUNER/yoRadio repository access, real adapter/network transport, HOME integration, UI/display/touch/carousel/GPIO/driver/backlight/configuration/library/toolchain changes or firmware upload. Recipe/retry/fallback, general payload/numeric profiles, durable persistence and real asynchronous runtime remain deferred.
- Recommended next REMOTE milestone: **HOME UI -> Universal Control Core integration using Mock Adapter for hardware validation**, after foundation review; not started. See [implementation report](docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md) for boundaries, exact files, test coverage, costs and limitations.

### TUNER//01 Native API v1 specification candidate - 2026-10-03

- Phase A verified first: Common Control Model `1.0.0` is **OWNER APPROVED / FROZEN**, with zero normative semantic changes. Phase B does not modify the frozen model or historical reviews.
- Created [`docs/TUNER_NATIVE_API_V1.md`](docs/TUNER_NATIVE_API_V1.md), **`1.0.0-candidate.1` â€” UNAPPROVED / UNFROZEN**, covering all 25 requested topics: local scope/layers, ESP32 constraints, transport evaluation, discovery/identity/version/session, independent capabilities, typed commands/dedup/guards/results, coherent sequenced/fresh state/media/catalogs, bounded separate artwork, connectivity/errors/security/multiple controllers, JSON framing/mapping register, lifecycle/handoff/validation/open decisions.
- Repository inspection found no qualifying verified TUNER implementation evidence; architecture design entries and static HOME literals do not prove support. Every proposed behavior is classified DESIGN REQUIREMENT, with actual radio facts UNKNOWN / REQUIRES TUNER PROJECT VERIFICATION.
- JSON selected for candidate serialization; intended discovery is mDNS with manual endpoint fallback. No broadcast fallback selected. **Final transport OPEN**: authenticated WebSocket + HTTPS is the leading evaluated option, pending actual stack/TLS/peak-resource evidence. Security/pairing, endpoint/port/framing, identities, capability subset, units, stable catalogs/atomic guards, completion/fences/persistent ledger, media/acquisition, other writers, artwork and bounded resource/timing profile remain explicit gates.
- Self-review checked duplicate/delayed work, stale reconnect state, both reboots, multiple writers, capability change, stale station selection, late artwork, timeout/late completion, unsupported operation/version mismatch, sequence gaps, unauthorized controllers, DHCP, partial counts/recipes, partial/future-only proofs, capacity/history and freshness anchors. Corrected initial candidate field names and clarified private ref/guard schemas, native-key scope, progress, counter exhaustion and atomic server checks before final review. No internal CCM representation blocker found; OPEN binding/security/profile decisions prevent API freeze/implementation readiness.
- Validation: exact freeze byte comparison with only two version substitutions; 96 unchanged normative vectors; 25 API sections, local links, common operation/field names, JSON example and documentation-only file scope checked. Document reasoning only; no executable adapter/protocol/control-path tests claimed.
- Phase B exact files changed: `docs/TUNER_NATIVE_API_V1.md`, `CHANGELOG.md`. Combined task scope: these plus `docs/COMMON_CONTROL_MODEL_V1.md` and `docs/CONTROL_ARCHITECTURE.md` from Phase A. Historical review files unchanged.
- No separate radio/yoRadio/TUNER repository opened, read or touched; no firmware/Core/adapter/network/UI/hardware/PlatformIO/library implementation/change; no build/upload. Next milestone **B: separate TUNER repository READ-ONLY protocol/implementation verification**; not started.

### Common Control Model v1 OWNER FREEZE - 2026-10-03

- Explicit owner approval promotes source `1.0.0-candidate.4` to **`1.0.0` â€” OWNER APPROVED / FROZEN**. FR01-FR16 PASS; R01-R04, A-T/E01-E35/F01-F22 and G01-G15 document review PASS. Final candidate.4 verdict: **NO BLOCKERS FOUND â€” CANDIDATE READY FOR OWNER FREEZE REVIEW**.
- Frozen scope: identifiers/generations, values, devices/targets/bindings, descriptors, operations/fields/observations, requests/results/lifecycle, retry/fallback/deduplication/order/deadlines, numeric mapping/quantization, catalogs/media context, identity/association, conflicts, shared resources, recipes and conformance.
- Zero normative semantic changes. Exact byte comparison of sections 1-20 permits only the current version tuple `{1,0,0,4}` -> `{1,0,0}` and exact version check `candidate.4` -> `1.0.0`; every other byte and all 96 vector rows remain unchanged. Header/section 21 and current architecture status references only; historical reports and earlier changelog entries preserved.
- Phase A exact files changed: `docs/COMMON_CONTROL_MODEL_V1.md`, `docs/CONTROL_ARCHITECTURE.md`, `CHANGELOG.md`. Verification completed before beginning Native API specification.
- Freeze establishes no Core/API/adapter/server, actual TUNER control, Matter/HA/Smart-TV/IR support, tested external mapping or end-to-end hardware control. DESIGNED FOR COMPATIBILITY / PROTOCOL TESTED / HARDWARE VALIDATED remain separate; HOME physical PASS remains UI evidence.
- No firmware/API/adapter/networking/UI/PlatformIO/library/hardware changes or implementation; no build/upload required or performed; no release/tag.


### Candidate.4 Common Control Model FR15/FR16 correction and freeze-gate review - 2026-10-03

- Advanced `1.0.0-candidate.3` to `1.0.0-candidate.4` after correcting both residual blockers. Candidate.4 remains **UNAPPROVED / UNFROZEN**. FR01-FR14 corrections retained; no model redesign, new operation, schema record, enum, error code or protocol scope.
- FR15 PASS: distinguish historical delivery from physical-execution uncertainty; whole-attempt no-execution proof requires both no past and no future execution. Release only proved per-attempt/resource keys; retain other owners/unresolved barriers and delivery history. Historical AMBIGUOUS alone cannot veto an otherwise eligible pre-terminal captured fallback. Terminal proof never reopens scheduling. Cross-check completed the guarded FAILED/AMBIGUOUS and aggregate covering-emission cases without changing prior Attempt.delivery.
- FR16 PASS: qualify uncertainty closure and deadline wording for genuine extent/declared correlated obligations. Full known feedback-NONE work stays ACCEPTED/SENT/UNCONFIRMED; known partial work stays FAILED/PARTIALLY_SENT/UNCONFIRMED with PARTIAL_EXECUTION and underlying cause. STRICT/LOCAL_ONLY ordering remains independent; no synthetic confirmation or partial resumption.
- Exact files changed: `docs/COMMON_CONTROL_MODEL_V1.md`, `docs/COMMON_CONTROL_MODEL_V1_CANDIDATE_4_FREEZE_REVIEW.md`, `CHANGELOG.md`. Historical reviews and architecture unchanged. Normative edits limited to sections 11/12/13/17, new section 19.3 vectors and readiness/version references.
- Re-evaluated R01-R04, all unchanged A-T/E01-E35/F01-F22 rows (77 vectors), FR01-FR16 interactions and 15 new adversarial G01-G15 vectors. Checked late proof, future-only/partial proof, terminal no-rescheduling, retry/deadline/supersession, per-key release, open-loop/partial recipe and count folds, history and contradiction handling. Local propagation issues corrected during the pass; no new unresolved finding or remaining internal blocker. Structural/link/version/section-scope and workspace hash checks verify documentation scope; no executable conformance tests claimed.
- Verdict: **NO BLOCKERS FOUND â€” CANDIDATE READY FOR OWNER FREEZE REVIEW**. Actual protocol/proof/fencing/control-path verification remains a separate future gate; physical UI/display/navigation PASS is not protocol validation.
- No firmware, Universal Control Core, TUNER Native API, adapter, networking, UI, hardware/configuration, PlatformIO or library implementation/change; no build, upload, release, tag, approval or freeze.


### Candidate.3 Common Control Model freeze review - 2026-10-03

- Repeated freeze-gate recheck: candidate.3 is unchanged since the previous review; FR15/FR16 remain open and the blocked verdict stands. Clarified that R02 requires no-execution proof before terminal closure; later evidence cannot reopen scheduling. Recheck changes only the candidate.3 review record and this changelog; protected files unchanged. No correction, approval, freeze, build or upload.

- Reviewed the complete candidate and all 77 A-T/E01-E35/F01-F22 vectors against lifecycle, retry, deadline, barrier, schema, catalog and conflict rules. Added `docs/COMMON_CONTROL_MODEL_V1_CANDIDATE_3_FREEZE_REVIEW.md` with exact contradictory rules, four counterexample traces R01-R04 and proposed local repairs only.
- Found two residual BLOCKER FOR V1 FREEZE findings: FR15 no-execution proof versus blanket ambiguous/incomplete barrier retention and non-idempotent fallback restrictions; FR16 the closed-attempt uncertainty rule versus full feedback-NONE acceptance and known partial open-loop failure. The prior correction-review readiness verdict is withdrawn.
- Exact changed files: the new candidate.3 freeze review, `docs/COMMON_CONTROL_MODEL_V1.md` (header/readiness only), `docs/COMMON_CONTROL_MODEL_V1_CORRECTION_REVIEW.md` (historical supersession notice only), and `CHANGELOG.md`. Normative sections 1-20, existing vectors, model version `1.0.0-candidate.3`, architecture and historical freeze review are unchanged. Structural/link and file-hash checks verify documentation scope; no executable tests claimed.
- Verdict: **BLOCKERS REMAIN â€” NOT READY FOR V1 FREEZE**. Candidate remains unapproved/unfrozen; no repair or Native API work started. Actual protocol/control-path verification remains separate from owner physical UI/display/navigation PASS.
- No firmware, UI, hardware/configuration, GPIO/driver, PlatformIO, library, API, adapter, networking or protocol edits; no build, upload or tag.

### Common Control Model v1 blocker correction pass - 2026-10-03

- Advanced the normative candidate to `1.0.0-candidate.3`; it remains unapproved and unfrozen. Corrected FR01-FR12 blockers, FR13 conflict-array/diagnostic ordering and FR14 type/wording ambiguities. Corrections cover lifecycle/late evidence, expiry precedence, deterministic dedup purge, qualified all-or-none FIFO acquisition, immutable recipe captures/parent folding, absent-state/conflict resolution, TABLE quantization, partial catalog merge and total validation/error mapping.
- Exact changed files: `docs/COMMON_CONTROL_MODEL_V1.md`, `docs/COMMON_CONTROL_MODEL_V1_CORRECTION_REVIEW.md`, and `CHANGELOG.md`. Architecture and historical freeze review are unchanged.
- Reproduced all original findings; cross-checked schema/transition/retry/deadline/dedup/recipe/reservation/barrier/observation/numeric/catalog/validation interactions. Re-evaluated all A-T and E01-E35 vectors, added normative repaired outcomes and 22 fresh boundary vectors, and performed a fresh adversarial review. Clarified retry-wait transitions, purged-result versus retained-barrier evidence, proved refusal progress, late open-loop failure, barrier-proof ordering, derived attempt-history bounds and proved target refusal versus historical byte ambiguity. FR01-FR14 PASS in document review; no internal blocker found. Structural/link and file-hash checks verify coverage and documentation-only scope; no executable conformance/protocol/hardware-control tests claimed.
- Verdict: **NO BLOCKERS FOUND â€” CANDIDATE READY FOR OWNER FREEZE REVIEW**. This is not owner approval or freeze. Actual mapping/protocol guarantees remain unverified; physical HOME/display/navigation PASS remains separate.
- No firmware, API, adapter, networking, UI, hardware configuration, PlatformIO, protocol or library work; no build, upload, tag or freeze.

### Final adversarial Common Control Model freeze review - 2026-10-03

- Read the complete normative candidate and reviewed architecture. Added `docs/COMMON_CONTROL_MODEL_V1_FREEZE_REVIEW.md`: lifecycle transition matrix/trace audit, every A-T vector, 35 additional ordered edge-case vectors, exact counterexamples/rules and smallest local repair proposals. This is documentation reasoning, not executed adapter/conformance tests.
- Found 12 BLOCKER FOR V1 FREEZE issues, all still open: FR01 lifecycle aggregate/transition contradictions; FR02 discretionary/contradictory late terminal evidence; FR03 expiry/invalidation precedence and closed-result deadline flags; FR04 discretionary dedup eviction; FR05 inconsistent resource reservation rules; FR06 recipe capture/envelope/step dedup ambiguity; FR07 interrupted recipe parent outcomes; FR08 selected absence reduction; FR09 conflict participant/clearing ambiguity; FR10 TABLE quantization; FR11 incomplete catalog generation handling; FR12 incomplete validation/ticket/error mapping.
- Secondary findings: FR13 SHOULD FIX BEFORE V1 for ordered conflict versus diagnostic/history output; FR14 EDITORIAL / CLARIFICATION for type/field shorthand and normative negation. Existing future protocol verification/optional extensions remain separate; missing optional functionality was not counted as a blocker. Intentional permanent STRICT safety blocks were distinguished from accidental reservation cycles.
- Withdrawn the prior readiness claim. `docs/COMMON_CONTROL_MODEL_V1.md` is now `1.0.0-candidate.2` with matching logical candidate version and corrected readiness/reference only; no proposed schema, lifecycle or policy repair was silently adopted. Verdict: BLOCKERS REMAIN â€” NOT READY FOR V1 FREEZE. Candidate remains unapproved/unfrozen; no tag or Native API work.
- Exact changed files: `docs/COMMON_CONTROL_MODEL_V1_FREEZE_REVIEW.md`, `docs/COMMON_CONTROL_MODEL_V1.md`, `CHANGELOG.md`. Architecture is inspected but unchanged. Firmware, validated UI, GPIOs/drivers, PlatformIO, networking, adapters, APIs and libraries unchanged. No build or upload performed; stopped after documentation/review.

### Normative Common Control Model v1 candidate - 2026-10-03

- Created `docs/COMMON_CONTROL_MODEL_V1.md`, revision `1.0.0-candidate.1`, as a protocol-neutral logical specification only. Defined normative language; scoped persistent IDs/revisions/generations; exact primitive/value/schema types; device/target/binding/connectivity and capability records; all requested operation/field contracts and independent catalog queries; observation/request/result/attempt/progress schemas and valid lifecycle combinations.
- Resolved the previous schema/conformance blocker with mandatory capture/revalidation, closed argument types, separate delivery/target outcomes, partial progress and normative A-T vectors. Resolved the policy blocker with bounded-age freshness, session/sequence protection, deterministic conflict selection/latching, monotonic deadlines, disabled-by-default retries, bounded deduplication/restart handling, exact rational quantization and strict shared-resource barriers. These limits and restrictions are candidate project decisions, not external protocol facts.
- Completed the bounded bidirectional compatibility review from the architecture's existing official evidence. Documented mapping candidates and exclusions for TUNER Native, Matter, HA, one-way IR and Smart-TV; actual TUNER API/adapter support and unverified identity/units/catalog/context/fencing guarantees remain UNKNOWN / REQUIRES PROTOCOL VERIFICATION. No protocol or hardware control-path success is claimed.
- No remaining candidate semantic freeze blocker was identified by the documentation consistency review. Verdict: READY FOR OWNER REVIEW FOR V1 FREEZE; owner approval and freeze have not occurred. Fixed limits, strict barriers and guarded-context restrictions remain owner-review items. Actual protocol verification and later adapter/control-path tests remain separate gates.
- Updated `docs/CONTROL_ARCHITECTURE.md` only to reference the normative candidate/precedence and identify the prior NOT READY verdict as historical, preserving the architecture, review findings and source evidence. Exact files changed: `docs/COMMON_CONTROL_MODEL_V1.md`, `docs/CONTROL_ARCHITECTURE.md`, `CHANGELOG.md`.
- Documentation/specification only: no firmware, networking, native API, adapters, UI, libraries or PlatformIO changes. No firmware build or upload performed. Stopped after documentation validation.

### Adversarial Universal Control architecture review and HOME hardware PASS - 2026-10-03

- Reviewed all twelve requested scenarios against `docs/CONTROL_ARCHITECTURE.md`, preserving the protocol-neutral layers and already-correct rules. Section 13 records each concrete failure, original rule, severity, smallest correction, scenario coverage and remaining freeze gates. This is a documentation reasoning review, not executed protocol tests.
- Recorded eight BLOCKER FOR V1 FREEZE findings: mandatory revision/authorization/association revalidation (R1), reversible merge consequences (R2), cache-age/session/late-report protection (R3), coherent media contexts and station/content semantics (R4), delivery/outcome/retry/shared-actuator ordering (R5), catalog-token execution races (R7), deterministic numeric/quantization/seek domains (R8), and unsafe read-then-toggle power emulation (R9). Added the exact neutral corrections in sections 2-6; external guarantees remain UNKNOWN / REQUIRES PROTOCOL VERIFICATION where unproven.
- Corrected two SHOULD FIX BEFORE V1 gaps: distinguish intentional repeated keys from retries and report partial compound execution/deduplication limits (R6); separate selected observations, conflicts and optimistic overlays and prohibit emulation from unresolved conflicting state (R10).
- Existing rules already handle semantic navigation mismatches and read-only/partial devices; preserved these without redesign. Also retained independent target capabilities, unknown one-way IR state, identity independent of names/IPs, and the no-duplicate cross-route fallback rule.
- Owner-confirmed static REMOTE//01 HOME screen physical validation: hardware PASS (R11). Corrected stale pending HOME statements and README/hardware documentation to describe the current HOME page rather than retained qualification content. DISPLAY QUALIFICATION and navigation remain separately owner-validated PASS; none of these establishes protocol, adapter or end-to-end control-path validation.
- Common Control Model v1 remains unfrozen and NOT READY for freeze: complete/approve normative schemas and conformance examples; fix policy/descriptor contracts for freshness, conflicts, deadlines, deduplication/restart, quantization and resource barriers; complete compatibility review with explicit scoped support/exclusions. Actual protocol guarantees and all adapter/control-path testing remain future evidence obligations, not invented behavior.
- Changed documentation only: architecture, changelog, README and hardware evidence notes. No firmware, networking, adapters, APIs, UI, configuration or libraries changed. No build or upload performed; stopped after documentation/review.

### Universal Control Layer architecture proposal - 2026-10-03

- Added `docs/CONTROL_ARCHITECTURE.md` as a documentation-only proposal for review. REMOTE//01 is protocol-neutral; TUNER//01 is the planned first native target, not the owner of the common architecture. Common Control Model v1 remains unfrozen.
- Defined all nine layers, stable device/target identities, descriptive per-operation capabilities, typed neutral commands/units, per-field state confidence/provenance/freshness, multi-adapter routing, cautious fallback after uncertain delivery, separate discovery/merging, independent model/adapter versioning, and review gates.
- Added a preliminary TUNER Native/Matter/Home Assistant/IR/Smart-TV matrix and official-source mapping candidates. Smart-TV behavior remains UNKNOWN without verified official project documentation. No existing adapter support, protocol-test success, or Matter/Home Assistant hardware validation is claimed; the owner has no current HA/Matter validation infrastructure.
- Distinguished DESIGNED FOR COMPATIBILITY, PROTOCOL TESTED and HARDWARE VALIDATED, and documented the requested next-milestone order. Display/navigation and subsequently owner-confirmed HOME UI hardware PASS do not establish an end-to-end control path.
- Changed only this changelog and the new architecture document. No firmware, PlatformIO configuration, UI, networking or libraries changed; no build or upload performed. Stopped at documentation.

### First static REMOTE//01 HOME iteration - 2026-10-03

- Before this UI work, the owner physically validated DISPLAY QUALIFICATION successfully on the real 172x640 hardware. The hardware PASS and size/detail/contrast observations are recorded below and in `docs/HARDWARE.md`; they are physical results, not host-render predictions.
- Removed only page 2's temporary qualification content (font test rows, detail patterns/shapes, swatches/grayscale and symbol tests) and replaced it with the specified first static HOME screen. The previous FM/101.7 mockup remains discarded.
- HOME top/status: `REMOTE//01` at (12,10), right-aligned `87%` in a 38 px box at (122,10), primitive battery outline at (104,13) with a small terminal immediately to its right, 148x1 divider at (12,32); `TUNER//01` at (12,46) in 16 px and accent `CONNECTED` at (12,68) in 12 px.
- Artwork: 148x148 secondary-background placeholder at (12,96), radius 6, centered 20 px `TUNER//01` with 12 px secondary `ARTWORK` below. No image asset.
- Now playing: 20 px `Radio Paradise` at (12,262) in a fixed 148x22 single-line ellipsis box; 14 px `Mellow Mix` at (12,290) in a 148x16 single-line box; 12 px accent PLAY symbol + `PLAYING` at (12,316). The native LVGL render displays `Radio...` using the explicitly permitted ellipsis behavior.
- Playback: three clickable but action-free 44x52 targets at X=12/64/116, Y=350, secondary backgrounds/radius 6/no border; existing PREV/PAUSE/NEXT symbols in 20 px, with PAUSE accented to match the static PLAYING state. No callbacks or playback logic.
- Volume: secondary 12 px `VOLUME` at (12,424); right-aligned 20 px `68` at (124,418) in a 36 px box; 148x6 track at (12,450) with 101x6 accent level and radius 3. Level is a child of the track at (0,0), preserving the requested page-relative position. No slider behavior.
- Quick navigation: action-free 148x36 FAVORITES/DEVICES/REMOTES rows at (12,486/526/566), radius 4, 14 px primary labels with 10 px left padding and existing RIGHT symbols at 10 px from the right edge. Footer: secondary 12 px `TUNER//01` at (12,618), existing 12 px WIFI symbol at (145,618).
- Exact specified palette retained: background `#11191D`, primary `#F3F5F2`, secondary `#91A3A6`, panels `#1C292E`, accent `#C2FF00`, divider/track `#344247`. Only already enabled and physically qualified Montserrat 12/14/16/20 px are used; no new fonts, libraries or assets. No Wi-Fi, discovery, capability API, IR or power-management implementation.
- Before firmware build: vendored LVGL host checks PASS for all 34 content objects and 22 labels: page/parent bounds, no unintended sibling/label overlaps, no text wrapping, and symbol glyph availability without fallback in the already enabled fonts. Inspected native 172x640 HOME render. Page 2 remains the same 172x640, non-scrollable, snappable carousel element at logical X=344; spacer stays non-snappable at X=516. Simulated LEFT 0 -> 1 -> 2 and RIGHT 2 -> 1 -> 0 passed with exact snap positions and end-stop directions, including swiping back from a navigation-row touch target.
- Firmware changes are restricted to page 2's content/helpers in `components/user_app/user_app.cpp`. Factory/brightness pages, initialization/task code, carousel behavior, page order, display rotation, touch mapping/protocol, GPIOs, controllers and backlight are unchanged. Subsequent owner physical validation of the static HOME UI: hardware PASS; this does not validate a universal-control path. No automatic upload performed.
- Build: `pio run -e remote01 -j 1` SUCCESS (23.59 s); reported program flash use 1,612,541 bytes, RAM use 19,216 bytes. Stopped after the successful build; no upload performed.

### DISPLAY QUALIFICATION hardware PASS - 2026-10-03

- Owner validated the qualification page on the physical 172x640 LCD: DISPLAY QUALIFICATION hardware PASS. These results are physical owner observations, not host-render predictions.
- Montserrat 12 px: readable; reserve for metadata, minor status and low-priority information.
- Montserrat 14 px: clearly readable; suitable for secondary UI.
- Montserrat 16 px: comfortably readable; provisional minimum for normal primary UI text.
- Montserrat 20 px: very clear; suitable for important labels/controls.
- Montserrat 28 px: very clear; suitable for prominent values/headlines.
- Physical detail PASS: 1 px lines and borders are clearly distinguishable; fine checker/stripe patterns render distinctly; grayscale progression is distinguishable; color samples render clearly.
- Physical symbol PASS: LVGL symbols at 12 px remain readable; symbols at 20 px are very clear.
- Physical contrast PASS: dark background + white text + `#C2FF00` accent works well on the panel.
- Montserrat 18 and 24 px remain disabled and were not physically qualified. No result is inferred for those sizes.
- Documentation-only update: recorded the results in project documentation. Qualification page retained; firmware, fonts/LVGL configuration, carousel, touch mapping, display rotation, GPIOs, drivers, hardware configuration and backlight unchanged. No build or upload performed; further UI work stopped.

### Physical navigation PASS and temporary display qualification - 2026-10-03

- Owner physical hardware validation PASS: 180-degree LCD orientation, touch mapping, carousel snapping, and previous label clipping fixes. Physical LEFT advances factory 0 -> brightness 1 -> qualification 2, with the next page entering from the RIGHT; physical RIGHT returns 2 -> 1 -> 0, with the previous page entering from the LEFT. This confirms the preceding navigation correction on the actual board.
- Replaced only page 2's temporary radio mockup content/helpers with a fixed native 172x640 DISPLAY QUALIFY card on the existing `#11191d` dark background. This is a temporary rendering/readability test, not a proposed product design. Carousel, page IDs/order, snapping/spacer, display rotation, touch mapping/protocol, hardware configuration/GPIOs, and backlight are unchanged.
- Font availability verified before building using active generated `sdkconfig.h`, `sdkconfig.remote01`, and font symbols in the compiled LVGL archive: Montserrat 12, 14, 16, 20, 28 px ENABLED; 18 and 24 px DISABLED (source files exist, but no compiled font symbols). No fonts were enabled or substituted. The 18/24 rows explicitly say `not enabled` in status text; they contain no readability sample at another size. Available font samples: 12 `TUNER//01`, 14 `Connected`, 16 `Volume 68`, 20 `Favorites`, 28 `Aa68`; each has a separate actual-size tag. Also includes 14 px `Now Playing`.
- Fine detail: exact 64x1 and 64x2 horizontal bars, 1x20 and 2x20 vertical bars, three 16x16 shapes with 1 px borders (circle, square, rounded square), and three deterministic unscaled 16x16 canvas patterns (1 px checkerboard, 2 px checkerboard, alternating 1 px vertical stripes). Canvas RGB565 buffers have persistent storage.
- Color/contrast: nine swatches, tagged K/D/M/L/W/R/G/B/A: black `#000000`, dark gray `#202020`, medium gray `#808080`, light gray `#c0c0c0`, white `#ffffff`, red `#ff0000`, green `#00ff00`, blue `#0000ff`, current accent `#c2ff00`; plus a 16-step grayscale ramp (0..255 in steps of 17).
- Symbol readability: existing LVGL PLAY, PAUSE, AUDIO, WIFI symbols at 12 and 20 px; no icon library added. Labels use natural single-line content bounds; detail objects and labels are non-scrollable/non-interactive, and the existing carousel element remains non-scrollable. Physical qualification results are pending; no upload performed.
- Before firmware build: host validation with the vendored LVGL and matching enabled font sizes PASS. All 37 labels fit a single font line and their parent bounds; all 72 page children fit within 172x640 with no sibling overlap. All four symbol glyphs exist directly in both selected fonts, without fallback. Inspected the native 172x640 render; pixel patterns, shapes, lines, swatches and text render correctly. Application initialization and factory task code were checked unchanged.
- Build: `pio run -e remote01 -j 1` SUCCESS (22.95 s); reported program flash use 1,651,609 bytes, RAM use 20,760 bytes. No upload performed.

### Navigation direction correction - 2026-10-03

- Owner physical observation: the previous reversed page IDs made a right swipe advance through factory -> brightness -> REMOTE//01, with the next page entering from the left. This supersedes the earlier assumption that reversing IDs would yield physical left-swipe advancement.
- Arranged the real pages in normal LVGL order: factory ID 0 at X=0, brightness ID 1 at X=172, REMOTE//01 ID 2 at X=344; each remains 172x640. Startup now selects ID 0. These are the only three firmware source changes.
- With this layout, leftward pointer movement advances 0 -> 1 -> 2 and pulls the next page in from the right; rightward movement returns 2 -> 1 -> 0 with the previous page entering from the left. Preserved center snapping, one-page scrolling, animations, and existing logical end-stop code: RIGHT at X=0, horizontal at X=172, LEFT at X=344. No end-stop directions were swapped.
- Preserved the non-snappable spacer at X=516 and its exclusion from active-page lookup. Working LCD rotation, touch coordinate mapping/read protocol, GPIOs, controller initialization, backlight, and mockup label layout are unchanged.
- Vendored LVGL host validation PASS: page IDs/positions/sizes/flags, factory and brightness active-page identities, all 24 mockup label bounds, left swipes to X=172 and 344, right swipes to X=172 and 0, and attempts to swipe beyond either endpoint. Physical retest remains pending; no upload performed.
- Build: `pio run -e remote01 -j 1` SUCCESS (24.35 s); reported program flash use 1,628,369 bytes, RAM use 19,216 bytes.

### Second hardware test and mockup/carousel corrections - 2026-10-03

- Owner hardware test PASS: LCD 180-degree rotation, touch input, factory-to-brightness swipe, and REMOTE//01 page access. FAIL: clipped mockup labels and different snapping from brightness to REMOTE//01.
- Carousel comparison: factory ID 2 at X=344, brightness ID 1 at X=172, REMOTE//01 ID 0 at X=0. All use the same carousel parent, 172x640 element geometry, `SNAPPABLE` flag, non-scrollable/non-clickable element flags, center snapping and one-page scrolling. Factory element styles change background/focus appearance; the mockup overrides background, border and padding without changing page geometry. The working logical end stops remain LEFT at ID 2, horizontal at ID 1, RIGHT at ID 0.
- Exact snap regression: `lv_carousel_add_element` moved the zero-width sentinel spacer to `172 * (last_added_id + 1)`. Reverse creation order 2, 1, 0 left it at X=172, and its inherited `SNAPPABLE` flag produced an extra center-snap position at scroll X=86, halfway between brightness (172) and REMOTE//01 (0). It was also included in active-element lookup. The spacer is now non-snappable/non-interactive, remains beyond the highest page ID at X=516 regardless of insertion order, and is skipped by active-element lookup. Real pages retain the existing snap, animation and end-stop behavior.
- Label audit with vendored font metrics: `USB  /  READY` is 86 px wide but had a 69 px box; the footer wraps to two lines but had a fixed 18 px height. Moved the two header labels to separate rows, right-aligned the status in a 148 px box, and moved the divider to Y=46. All helper-created labels now wrap with content-sized height. Cleared control/source container padding so icon, caption and source labels have their full available area. Text and font sizes are unchanged.
- Removed temporary touch serial diagnostics after the successful hardware test. Touch read protocol and coordinate transform, working LCD rotation/initialization, GPIOs, controller configuration, backlight, and disabled factory producer tasks are unchanged.
- Host validation using the vendored LVGL/font code: all 24 mockup labels fit their text boxes and parent bounds; all three pages have matching dimensions/flags and correct logical positions/end stops. Reproduced the old spacer configuration: a simulated brightness-to-mockup swipe stopped at X=86 instead of 0. With the fix, simulated swipes 2 -> 1 -> 0 -> 1 -> 2 snapped exactly to X=172, 0, 172, 344. Physical retest of the new layout and transitions remains pending. No upload performed.
- Build: `pio run -e remote01 -j 1` SUCCESS (25.77 s); reported program flash use 1,628,369 bytes, RAM use 19,216 bytes.

### Hardware test and touch/navigation correction â€” 2026-10-03

- First successful flash (owner test): LCD initialization PASS; 180-degree display rotation with USB at the top PASS. Touch/swipe FAIL; REMOTE//01 third page unreachable. Factory values are static because producer tasks are disabled; those tasks remain disabled.
- Compared our callback and I2C helper with official V2 `11_FactoryProgram` at commit `1c157e6e8e68b89fd4dc400f46bf1724cb64a57e`: touch command, response length, raw coordinate decoding, accepted touch counts (`1..4`), and I2C transaction match. Our callback adds the 180-degree coordinate transform and omits the unused factory touch-test queue send.
- Confirmed navigation regression: carousel end-stop directions had been reversed. LVGL permits positive pointer delta X with `LV_DIR_LEFT` and negative delta X with `LV_DIR_RIGHT`. Startup is factory page ID 2 at logical X=344; the modified `LV_DIR_RIGHT` blocked movement toward IDs 1 and 0. Restored the official end stops: `LV_DIR_LEFT` at the highest ID and `LV_DIR_RIGHT` at ID 0. With the existing mirrored coordinate transform, physical left swipes can advance 2 â†’ 1 â†’ 0 (factory â†’ brightness â†’ REMOTE//01).
- Kept the existing factory touch orientation mapping plus 180-degree inverse transform. LCD initialization, working panel rotation, LCD/touch GPIOs, backlight, I2C/controller configuration, and producer task state are unchanged.
- Added temporary `Touch` serial diagnostics: read result/error name, response header/count, raw X/Y before clamping, mapped LVGL point/state, press/release transitions, active samples at most every 100 ms, and idle/error heartbeat every 2 s. A released point is the retained last LVGL position.
- Validation: `pio run -e remote01 -j 1` SUCCESS (44.37 s); reported program flash use 1,628,837 bytes, RAM use 19,240 bytes. No upload performed.
- Hardware retest pending: the end-stop regression explains blocked swipes, but loss of all raw touch input cannot be established from source inspection. After a manual flash, serial output must confirm valid points, physical coordinate alignment, brightness slider response, and left-swipe access to the third page. No hardware success is claimed.

- Selected Waveshare's official V2 ESP-IDF `11_FactoryProgram` as the baseline; retained its main demo, swipe carousel, backlight page, and brightness slider.
- Added the third 172x640 REMOTE//01 radio-control readability page with comparison fonts, status text, icons, buttons, and static controls.
- Added 180-degree AXS15231B display mirroring and the corresponding touch-coordinate rotation; reversed carousel page IDs for physical left-swipe navigation with USB at the top. The initial end-stop direction reversal was corrected as recorded above.
- Disabled starting factory-only Wi-Fi/BLE, audio, SD, battery/button, and sensor tasks. Removed active use of V1.1-derived GPIO assignments from the factory config.
- Added PlatformIO project configuration, README, and V2 source-cited hardware notes.

### Build

- Result: `pio run -e remote01 -j 1` completed successfully; firmware image `1,628,736` bytes.
- Build stack: PlatformIO Core 6.1.19, Espressif platform 6.13.0, ESP-IDF 5.5.3, Xtensa GCC 14.2.0+20251107.
- Resolved board libraries: Waveshare-vendored LVGL 8.3.11, AXS15231B 1.0.1, TCA9554 expander 2.0.3, and ESP LCD touch 1.2.1.
- Version note: Waveshare's official V2 `dependencies.lock` pins ESP-IDF 5.5.4. PlatformIO does not publish that exact framework package; the successful build uses its available 5.5.3 package, one patch release lower. No display/touch driver replacement was made.
- Source snapshot: Waveshare V2 repository commit `1c157e6e8e68b89fd4dc400f46bf1724cb64a57e`.


## 2026-10-04 ? Stage 1 battery and power management

**HOST TESTED / CLEAN BUILD VERIFIED / OWNER HARDWARE VALIDATION PENDING.** Entry Phase D remains OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED. No Stage2/network/TUNER work; no upload.

- Audited exact Waveshare3.49B V2/PCB V1.1 official schematic and battery/factory sources pinned1c157e6e8e68b89fd4dc400f46bf1724cb64a57e. Owner battery: Li-Po, 1000 mAh nominal with undocumented protection board; no guessed protection/charge specification.
- Corrected battery-only PWR-release power loss: omitted active-high SYS_EN/EXIO6 now asserted immediately after expander initialization, before LCD delays, using verified direction-first/reset-high latch behavior.
- Added calibrated GPIO4/ADC1-channel3 x3 BAT-node telemetry, bounded2s sampling, median/EMA with exact-threshold convergence, invalid-read safety, LOW/CRITICAL hysteresis and30s qualification. Existing HOME battery text now voltage/--/SIM at the same coordinates/font; no false87% or SOC estimate.
- Added30s backlight display idle retaining PWM brightness; QMI8658 low-power WoM200mg/21Hz viaEXIO2/shared GPIO8, touch wake and first-contact consumption. Eligible300s total idle uses RAM-retaining light sleep with2s safety timer. Core owner/LVGL park handshakes, new session/incarnation, fresh acquisition, preserved capability/disconnect state, input reset/redraw, buffered intent/ingress refusal; pending/uncertain work refuses sleep. Fault backoff and safe output/hold guards; GPIO/LCD/touch mapping/rotation/carousel/brightness implementation unchanged.
- External-power/charging UNKNOWN: hardware STAT is LED-only and no independent MCU VBUS sense is available. **Automatic critical hard-off INHIBITED**, not hardware validated or complete. Real critical warning; separate simulation cannot release hold. Actual controlled terminal release requires separate DEVELOPMENT_ONLY K then !. Installed cell/protection specifications and acceptance of the source-observation limitation remain owner evidence/decision items.
- DEVELOPMENT_ONLY uppercase B/I/Z/L/C/H/N/K/! retain all prior Mock keys/m; fixed one-slot mailbox, no shell/replay/networking. Diagnostics report real/simulated condition, wake/sleep/hold/errors, stack/heap/latency. Full one-session acceptance procedure and hardware references: [Stage 1 report](docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md).
- Pre-change158PASS/0FAIL/306563 assertions; soak158/0/2999813; clean/build success. Final172PASS/0FAIL/314717 assertions; soak172/0/3079967,65000 combined stress iterations including10000 actual owner-helper sleep/resume cycles. New fault matrix/output guards, stale session/UI/ingress, disabled/withdrawn/disconnected preservation, >3s completion, exact thresholds and allocation plateau pass. Existing tests unchanged. UBSan unavailable (missing libubsan), not claimed PASS.
- Final exact clean/build SUCCESS; build501.97s; zero warnings/errors. Flash1801465/21.5%, staticRAM20192/6.2%; accepted PhaseD deltas+37512/+416 (fresh entry baseline deltas+37488/+416; prior+24 unexplained). Target Core Session PSRAM83656 unchanged; POWER task4096 internal stack,496-byte static task frame. Actual total runtime heap/headroom/current/endurance OWNER PENDING.
- Frozen CCM SHA-2567a46f9fffe706476de458aac078d0af1c94844df339248d3f9f4b5cf033011d7 and accepted SDK configuration unchanged; no Core/model/PhaseD boundary/Mock semantic edits. Historical entries preserved.
- Final BIN SHA-256 `269f10faa4a3be7fb435dcf5f229f287612a0529b7a35111bc4f8d938ec1d896`; ELF SHA-256 `153d6071f95a050ee760cf71040321f1d092c46d49ea7b2d79d6b725e571d452`.


## 2026-10-07 - Canonical REMOTE-side TUNER requirements handoff

Documentation/specification only. Separate TUNER v1.1.0 implementation audit is now available at main `371cdebce7ba2648ea71636d5bdb5d738b42e680` (read-only `TUNER01-audit` checkout). Created [TUNER requirements from REMOTE](docs/TUNER_REQUIREMENTS_FROM_REMOTE.md): 24 sections, categorized evidence/requirements/changes/open decisions/deferred work and section 22 numbered implementation acceptance checklist. Direct REMOTE/TUNER operation must work without HA/openHAB/MQTT/Matter/cloud; optional HA/openHAB are additional Native clients. TUNER owns audio and native station/Favorites/Recent persistence; REMOTE owns protocol-neutral Core and context-safe artwork lookup/cache/display.

Added minimal current-evidence overlays to Native API candidate, control architecture and product decisions, preserving historical text. Existing TUNER radio/catalog/HTTP functionality is verified by source; Native API remains NOT IMPLEMENTED / UNAPPROVED / UNFROZEN. Transport/security/identity encoding, association, synchronization and measured resource limits remain open. Frozen CCM 1.0.0 byte-unchanged. No firmware/network/API/adapter implementation, TUNER checkout modification, build or upload. REMOTE Git metadata unavailable; documentation-only scope verified against entry SHA-256 inventory. Next REMOTE milestone: Native binding/security/resource-profile reconciliation and CCM mapping review before any API freeze or adapter implementation.


## 2026-10-08 ? A-REMOTE-001 inspection results prepared for shared review

Executed owner-invoked REMOTE inspection/documentation assignment against coordination769c4dc1f50acebbf855b06d4bcf9cfc5eba7e16. Recovered actual frozenCCM1.0.0 and original owner approval attachment excerpt; sections1?20 exact byte-match historical candidate after two documented version substitutions. Published live source/artifact inventory, bidirectional minimum-Native mapping, durable recovery/ingress/operation/numeric prerequisites and separate sleep evidence ledger. Current local BIN/ELF differs from recorded rollback; flashed image and control residency remain UNKNOWN/PENDING, no inherited PASS. O01?O17 remain OPEN; Native unapproved/unfrozen/not implemented. Archived prior REMOTE handoff byte-for-byte; new handoff READY FOR REVIEW. No PM/TUNER status edits, source/tests/config/library/artifact changes, new tests/build/upload/hardware or common-gate approval. See docs/A_REMOTE_001_BASELINE_RECONCILIATION.md; coordination publication recorded in the task handoff.


## 2026-10-08 - A-REMOTE-002 bounded durable recovery design

READY FOR REVIEW, DESIGN ONLY. docs/A_REMOTE_002_DURABLE_RECOVERY_DESIGN.md records durable commit-before-dispatch identity/effect/conflict recovery, NVS alternatives and qualification gates, reference/catalog/query/Favorite/content and exact rational-volume prerequisites. No firmware/test/config changes, build, upload or hardware test. CCM unchanged; Native unapproved/unfrozen/not implemented; O01-O17 OPEN. Results UNPUBLISHED, handed to separate PM session. Already integrated: A-001 PR2 and assignment PR3; artifact/rollback residency gaps preserved.


### 2026-10-08 - A-REMOTE-002 direct publication authorization

Latest owner instruction supersedes coordination publication delegation for this result. Completed design/source evidence rechecked against coordination main8e1ab9d87c4647964bbc48d0e50982472a0b9cca; result prepared on remote/a-remote-002-publication-20261008 for PR review. Previous unpublished package retained as history. No firmware, tests, configuration, artifacts, build/upload or hardware activity. Final publication identity reported separately.

Publication verified: https://github.com/OviOneKenoby/remote_tuner_system/pull/5 ; commit bfaf9a579a41b45d7c3e7ded0ac1af5c2d956fec. READY FOR REVIEW for design only; no firmware/build/upload or Native approval.


## 2026-10-08 - A-REMOTE-003 isolated D0 qualification

- Added standalone host-only tools/qualification/a_remote_003 and docs/A_REMOTE_003_D0_QUALIFICATION.md; production Core/Journal/NVS integration NOT IMPLEMENTED. No live NVS/credential reads or writes; no firmware build/upload/hardware test.
- Host tests PASS:13 tests,3859 counted checks,87 fault scenarios;100 fake update/recovery cycles. Exact canonical fixtures:2746/3228 typical pending/resolved bytes;35324/44284 maximum bytes;maximum resolution reserve8960.
- Production qualification FAIL/BLOCKED:3072-byte bank cannot hold complete resolution;maximum profile exceeds current NVS. Valid-old-anchor counterexample exposes missing trusted newest-intent guarantee;physical SDK rollback is NOT demonstrated. Capacity/atomicity/endurance/target resources remain unverified.
- CCM frozen semantics unchanged;Native unapproved/unfrozen/not implemented,O01-O17 open,D1-D5 unassigned. PM review and separate redesign/physical qualification required. Prior hardware/sleep history preserved.


## 2026-10-08 - A-REMOTE-004 bounded recovery redesign / isolated qualification
- Added standalone tools/qualification/a_remote_004 and docs/A_REMOTE_004_STORAGE_RECOVERY_QUALIFICATION.md;no production Core integration,NVS/credentials access,build/upload/hardware or partition/configuration changes. A003 evidence/pending PR8 preserved.
- Complete narrowed profile uses exact lossless interning;full typed BASE/ALLOC/PROOF records preserve mandatory history/barriers/conflict/non-reuse. Intermediate proofs and future growth reserved;compaction preserves all state,cleans interrupted inactive tail before accumulating another tail.
- Host PASS12tests/7805counted checks/562scenarios;max complete state6383bytes,future growth796;raw record reserve15510,measured maximal fixture compaction14390. Existing24KiB only optimistically plausible for small other-occupancy cases;fails2-4KiB assumptions,actual free/root overhead UNKNOWN.
- Actual newest-intent backend integrity unqualified:production BLOCKED. A003 counterexample and whole trusted-service rollback reproduced;hash/readback not anti-rollback proof. Default unqualified mode GLOBAL_BLOCK;hardware NOT RUN. No Native approval/common-gate pass;separate physical/backend qualification required.

## 2026-10-11T18:03:56Z - A-REMOTE-005 isolated concrete backend qualification
- Read-only current selected SDK5.5.3/Core/NVS analysis; new tools/qualification/a_remote_005 and docs/A_REMOTE_005_BACKEND_QUALIFICATION.md only. Host PASS; concrete required G1 FAIL, actual capacity/durability UNKNOWN; overall BLOCKED. No firmware/config/SDK/NVS/credential/flash/hardware/Native changes.
# A-REMOTE-010 private implementation — 2026-10-13

- Added a dormant `remote_persistence` component for the installed `remote_ctl` NVS partition. No boot path opens or initializes the partition.
- Added focused host tests for versioning, commit ordering, interruption, corruption, ambiguity, capacity, restart and replay rejection.
- Updated the private build copy partition CSV to the already installed seven-entry A009 layout. No physical hardware operation was performed.


## A-REMOTE-010 diagnostic validation candidate — 2026-10-13
Added an explicitly build-gated `!A010` USB-serial diagnostic interface for later owner-authorized hardware validation of the dedicated `remote_ctl` backend. The default `remote01` environment remains disabled; only `remote01_a010_diag` defines `A010_REMOTE_PERSISTENCE_DIAGNOSTIC=1`. Boot starts no persistence operation. Read-only inspection and every mutation require explicit serial commands; mutations also require exact per-operation confirmation tokens and the fixed partition identity. No common API, TUNER, UI, default-NVS, or partition-layout change.

## A-REMOTE-010 Stage 1 read-only hardware preflight — 2026-10-13
Executed only the owner-authorized ROM/USB identity and flash reads. COM3 identified the expected ESP32-S3 revision 0.2 with 16 MiB physical flash. The installed partition-table sector matched the accepted A-009 seven-entry image, `remote_ctl` remained 65,536 bytes of `0xFF`, and the installed factory-image read matched the accepted pre-A010 identity. No firmware was flashed, no NVS partition was initialized, and no flash write, erase, or diagnostic command was issued. Stages 2 and later remain blocked pending explicit owner approval.
## A-REMOTE-010 Stage 2 diagnostic firmware validation — 2026-10-13
Under exact owner authorization, flashed only the validated diagnostic application image at `0x10000`. The 2,503,936-byte readback was byte-identical to the candidate. One controlled boot emitted the expected A010 diagnostic identity with persistence disabled on boot and no observed panic, watchdog, partition error or boot loop. The complete `remote_ctl` region remained 65,536 bytes of `0xFF`; the owner confirmed normal UI and retained observable configuration. No diagnostic command, NVS initialization, commit or erase was executed. Stages 3 and later remain unexecuted.
## A-REMOTE-010 Stage 3 read-only UART validation — 2026-10-13
Sent exactly `!A010 INFO` and `!A010 STATUS` to the installed diagnostic candidate. Each produced one accepted response with the exact dedicated partition identity, erased state, schema 1, generation and next generation 0, no incomplete transaction, zero reported entry counts and `payload_logged=0`. No unexpected error or payload disclosure was observed. No source code, firmware, NVS initialization, commit or erase changed. Stage 4 initialization remains unexecuted pending separate owner authorization.
## A-REMOTE-010 Stage 4 controlled initialization partial result — 2026-10-13
The exact `INIT` command was transmitted once and passed: erased generation 0/next 0 transitioned to initialized-empty generation 0/next 1 with no incomplete transaction and no payload logging. The immediately following `STATUS` independently confirmed the same state and reported 10 used, 2,006 free and 1,880 available NVS entries. One requested serial reset signal then produced no ROM boot banner or A010 identity within the 20-second observation window. The protocol STOP fired; `RECOVER` was not transmitted, `INIT` was not retried, and no COMMIT or ERASE command occurred. Reboot persistence and post-reboot UI/configuration remain unverified pending a new owner decision. No source-code or firmware change occurred.
## A-REMOTE-010 Stage 4 recovery continuation stopped — 2026-10-13
The prior reset remained unverified after a successful passive COM3 open produced no historical boot output. Under the limited continuation authorization, one USB-JTAG reset sequence was issued while capturing serial output. It produced exactly one ESP32-S3 ROM banner with reset reason USB_UART_CHIP_RESET and boot mode DOWNLOAD(USB/UART0), followed by `waiting for download`; the normal A010 application identity did not appear. The required guard stopped before `RECOVER`. INIT was not repeated; no COMMIT, ERASE, flash write or source-code change occurred. The device was left in ROM download mode and normal UI/configuration were not verified. A new owner decision is required for one normal hard reset out of ROM followed by read-only RECOVER.
## A-REMOTE-010 Stage 4 completed — 2026-10-13
After the owner performed a normal hardware reset out of ROM download mode and confirmed the UI was functional, exactly one read-only `!A010 RECOVER` command was sent. The installed A010 diagnostic firmware returned one accepted response with the exact `remote_ctl` partition identity, initialized-empty schema-1 state, generation 0, next 1, no incomplete transaction, 10 used/2,006 free/1,880 available entries and `payload_logged=0`. This verifies that initialization survived the confirmed reboot. INIT was not repeated; no COMMIT, ERASE, flash, additional reset or source-code change occurred. Stage 4 PASS; Stage 5 remains unexecuted pending separate authorization.
## A-REMOTE-010 Stage 5 first-record validation — 2026-10-13
Sent the exact confirmed COMMIT1 command once. It transitioned initialized-empty generation 0/next 1 to valid-recoverable generation 1/next 2 with no incomplete transaction, reporting 14 used, 2,002 free and 1,876 available entries and no payload logging. After the owner performed the proven normal hardware reset, one read-only RECOVER returned the same valid generation 1/next 2 state and counts. The owner confirmed normal UI and retained observable configuration. INIT and COMMIT1 were not repeated; no COMMIT2, ERASE, flash, additional reset or source-code change occurred. Stage 5 PASS; Stage 6 remains unexecuted pending separate authorization.
# A-REMOTE-011 Phase 3 private candidate — 2026-10-13

- Promoted the byte-verified A-010 source baseline after creating and verifying a reversible private backup of the active workspace.
- Added the bounded RTCP/1 client, exact application codec and read-only association-envelope loader for the dedicated `remote_ctl` backend.
- An unconfigured/non-association payload causes no write. A valid commissioned association must durably reserve the next CCM session generation through the accepted A-010 commit ordering before connecting; the current generation-2 validation payload fails closed.
- Added focused host tests. No operational PSK, hardware access, flash operation, partition change, UI change or TUNER private material is included.

