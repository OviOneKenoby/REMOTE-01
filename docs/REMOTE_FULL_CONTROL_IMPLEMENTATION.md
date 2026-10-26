# REMOTE Full Control — Implementation Record

This record tracks implementation against the binding
[owner-approved specification](REMOTE_FULL_CONTROL_SPEC.md). It describes
engineering state; it does not replace or relax the specification.

## Baselines

- REMOTE branch: `remote/btx-1.0-implementation`
- Initial verified REMOTE HEAD: `ea7ae801803f7285e9b3d1549080cc3035087595`
- Installed REMOTE implementation: `596cb71be867e2b5ff6eb3f6c0dde52d6dd8a770`
- TUNER branch: `tuner/btx-tls-psram-remediation`
- Initial verified TUNER HEAD: `39a3705b2c9a85824cf995782cd44abee3a1408a`
- Installed TUNER implementation: `dbf9896876a7fe8c9b421bb1608dc9c2782e9cfe`
- No hardware access, flash, NVS change, push, merge, or release is part of
  this offline project.

## Milestone matrix

| # | Milestone | Software | Hardware | Remaining work |
| --- | --- | --- | --- | --- |
| 1 | Source switching | PASS; TUNER `5d10615`, REMOTE `9293a99` | NOT TESTED | Hardware install/Radio-Bluetooth validation separately authorized. |
| 2 | Read-only canonical menu/state projection | PASS; bounded TUNER-owned menu projection is rendered as read-only status | NOT TESTED | Hardware rendering/physical-priority validation. |
| 3 | Semantic navigation | PASS for the safe local REMOTE navigation model; no raw TUNER input injection | NOT TESTED | Hardware touch, BACK/EXIT, sleep/wake and simultaneous-control validation. |
| 4 | Saved/Favorites/Recent | PASS offline; bounded queries, play, delete confirmation, add-favorite, paging, HOME shortcut and gestures implemented | NOT TESTED | Hardware persistence, playback-confirmation and physical-priority validation. |
| 5 | Radio Browser | **BLOCKED** | NOT TESTED | Existing synchronous 20-second HTTPS/JSON path cannot safely meet the 5-second RTCP lifecycle or proven internal-memory constraints. |
| 6 | UI/lifecycle | PARTIAL; all non-Browser approved flows implemented and built | NOT TESTED | Browser-dependent flow blocked; all UI/lifecycle behavior still needs hardware validation. |
| 7 | Final offline verification | PARTIAL; focused compatibility tests and both clean production builds PASS; partial-candidate manifests produced | N/A | A complete candidate cannot be declared while Milestone 5 is blocked. |

## Milestone 1 implementation

### Protocol

Existing authenticated RTCP/1 framing and IDs 1-15 are unchanged. A bounded
`+fc1` identity-text suffix advertises that the optional capability request is
safe before a new REMOTE sends any new ID. Legacy clients accept and ignore
identity text, so existing BTX feature bits remain byte-for-byte compatible.
Current records are:

| ID | Direction | Purpose | Payload bytes |
| --- | --- | --- | --- |
| 16 | REMOTE -> TUNER | Full-Control capability request | 1 |
| 17 | TUNER -> REMOTE | Version/features/freshness/revision | 20 |
| 18 | REMOTE -> TUNER | Source-state request | 1 |
| 19 | TUNER -> REMOTE | Source/transition/BT connection/error/sequence/generation | 24 |
| 20 | REMOTE -> TUNER | Correlated source-select with expected generation | 18 |

Command completion uses existing result ID 8. The TUNER one-slot source
mailbox is main-loop-owned and cleared on disconnect. Same-source selection is
idempotent; stale, busy, invalid, unavailable, submission, and internal errors
remain bounded sanitized codes. Switching to Radio uses restartable Bluetooth
cleanup and leaves Station Select stopped.

### REMOTE UI and ownership

HOME preserves its accepted geometry and changes only `REMOTES` to
`TUNER CONTROL`. The dedicated 172x640 overlay has BACK/EXIT, source selectors,
and later-milestone entries. Those list entries are intentionally disabled at
this checkpoint rather than presenting false functionality. Source progress,
error, and three-second success popups are compact and centered. Source state
does not move optimistically. The owner task holds a fixed source request/state
channel and uses the existing RTCP single-request lane. Disconnect and suspend
drop queued/in-flight source work without replay. The encoder remains enabled
while the control overlay is visible. Wake closes the overlay and resumes HOME.

### Changed source at this checkpoint

TUNER:

- `src/rtcp_full_control.h`, `src/rtcp_full_control.cpp`
- `src/rtcp_btx.h`
- `src/rtcp_server.h`, `src/rtcp_server.cpp`
- `src/main.cpp`
- `tests/test_rtcp_full_control.cpp`, `tests/test_rtcp_btx.cpp`

REMOTE:

- `components/rtcp_client/include/rtcp_client/wire.hpp`
- `components/rtcp_client/include/rtcp_client/client.hpp`
- `components/rtcp_client/wire.cpp`, `components/rtcp_client/client.cpp`
- `components/control_home/include/control_home/full_control.hpp`
- `components/control_home/runtime.cpp`
- `components/user_app/user_app.cpp`
- `tests/control/full_control_tests.cpp`

Implementation commits are TUNER
`5d1061516175d3ef6471b07b94e8ba164404220b` and REMOTE
`9293a99d05b6218dde0206a05d73a56f4590ade5`. The permanent documentation
checkpoint is REMOTE `401709c`.

### Automated verification

- TUNER Full-Control source model: PASS (parse, bounds, generation stale,
  busy, idempotent, bounded mailbox/no replay).
- TUNER existing BTX regression: PASS, 46/46 assertions.
- REMOTE Full-Control wire regression: PASS, 17 assertions.
- REMOTE existing BTX regression: PASS, 7 tests / 52 assertions.
- TUNER `esp32-lwip-psram` post-commit build: PASS. Image 2,074,096 bytes,
  SHA-256 `e715bd517623e6a4168a26995dbb507fa8589a08344c41aecb56158461270c7a`;
  ELF 30,347,892 bytes, SHA-256
  `634a90164918bb591f77c98631d6775886aa26a985391ed4009b0b7823f22a63`;
  link usage 2,067,521 / 3,145,728 bytes; static RAM 79,128 / 327,680.
- REMOTE `remote01` post-commit build: PASS. Image 2,587,088 bytes, SHA-256
  `f244f7ac9a618cb1136421e0268bf022fef9cef3e081a605c24fe801d01df0ae`;
  ELF 19,851,880 bytes, SHA-256
  `7423a8d424459fc9c11e879f4ba40440e205a4aa4d20305669913f075109c01a`;
  link usage 2,586,593 / 8,388,608 bytes; static RAM 56,796 / 327,680.

These are interim compile checkpoints, not final candidates: the embedded
build identity still reflects the pre-commit HEAD and final clean builds and
manifests are required after all safe milestones.

## Milestone 2 checkpoint

Full-Control 1.1 adds a fixed read-only menu projection. The TUNER serializes
its actual main-loop-owned mode, selection, total count, error/loading flags,
stable revision and at most five visible items. Each item carries only an
opaque 16-bit index, active flag and at most 48 bytes of validated UTF-8 text;
URLs never cross the protocol. Empty lists are valid and the maximum response
is 288 bytes, below the existing 512-byte plaintext limit. TUNER storage adds
only a 16-byte revision/signature tracker and uses the existing response
buffer; no dynamic allocation or input injection was added.

Compatibility uses `+fc2` for the 1.1 feature mask. Milestone 1 REMOTE clients
therefore remain on D-009/BTX when paired with this TUNER, while the current
REMOTE accepts both `+fc1` and `+fc2`. The REMOTE parser stores one bounded
five-item window and publishes it through a copied owner/UI channel. Focused
TUNER source/menu tests pass; focused REMOTE wire tests pass 25 assertions.
The projection is now rendered as bounded read-only TUNER menu status without
injecting navigation or forcing a REMOTE page change. Physical TUNER controls
remain authoritative, completing the software scope of Milestone 2.

Milestone 2 implementation commits are TUNER
`4cf9b65e00e0a6c5a28f6833ec06a672a046a461` and REMOTE
`e032ae324fab9627c6071a4848a976f92c81df51`. Both production builds pass:

- TUNER: 2,076,272-byte BIN, SHA-256
  `d308d56a52f46241f0c2eae432eccf0bf2e542f4279cc34c57c9d614b292a982`;
  30,380,224-byte ELF, SHA-256
  `3816b9cfcbdb14d0a7648c6a6e53f8914a1d2374fa5f99fbc2a8b88b6198a933`;
  79,208 bytes static RAM and 1,069,456 bytes exact-image slot headroom.
- REMOTE: 2,588,864-byte BIN, SHA-256
  `eef4c8dad70f8e0817a65458341c88d18ffbee822fd40a4dddd68cde6a745450`;
  19,883,688-byte ELF, SHA-256
  `e76a9998e04cf43e5afafb77f16b4256f79fe1e0b19f80ccc8906bcd2f7eaf9d`;
  57,100 bytes static RAM and 5,799,744 bytes exact-image factory-slot
  headroom.

## Milestones 3 and 4 checkpoint

Full-Control 1.2 adds capability-negotiated, authenticated station catalogs.
Compatibility advances to the identity suffix `+fc3`, feature mask 15 and
capability revision 3. Earlier clients do not send the new records; current
REMOTE still accepts `+fc1` and `+fc2` peers and keeps D-009/BTX behavior.

| ID | Direction | Purpose | Exact/maximum payload |
| --- | --- | --- | --- |
| 23 | REMOTE -> TUNER | Catalog query: collection, offset, limit | 5 bytes |
| 24 | TUNER -> REMOTE | Catalog page: capacity/count/revision and up to five items | 288 bytes maximum |
| 25 | REMOTE -> TUNER | Correlated semantic catalog command | 21 bytes |

Collections are Saved Stations (15), Favorites (10), and Recent (5). Pages
carry only stable positional tokens, active flags, and validated station names
bounded to 48 UTF-8 bytes; stream URLs and credentials never cross this
extension. Semantic operations are play, remove, and add-to-Favorites. Every
mutation includes the authoritative catalog revision and fails closed on stale,
busy, unsupported, unavailable, full, or already-present state. The TUNER uses
a fixed one-slot mailbox, executes work on its application main loop, clears
pending work on disconnect, and never automatically replays a mutation.

REMOTE adds one fixed catalog channel and keeps the existing single request
lane. The dedicated portrait view now provides Saved Stations, Favorites and
Recent, the HOME Favorites shortcut, five-item pagination, active-station
highlighting, two-second press recognition with scroll cancellation, confirmed
deletion, and non-destructive add-to-Favorites results. A station selection
waits for authoritative PLAYING plus the active catalog entry before showing
`Now Playing` and returning HOME. BACK/EXIT, disconnect dismissal, wake-to-HOME,
first-touch handling, no pending-command replay, and the global volume encoder
remain under their existing owners. TUNER physical navigation remains
independent; REMOTE does not inject raw input events.

The owner explicitly approved this independent-navigation architecture on
2026-10-26. REMOTE page, row, scroll and filter focus are local UI state and do
not alter the TUNER cursor or e-paper display. Only source, active station,
metadata, volume and catalog mutations synchronize, and mutations remain
revision-fenced with no replay. Review of the current worktrees found no
uncommitted raw-input/menu-driving source draft to remove; only generated host
test executables were untracked and are not product source.

Implementation commits are TUNER
`c217275262266b709a244870dd0f317ce135fdab` and REMOTE
`df7d804d140b6c13c1be84485a518284024e322b`. REMOTE compatibility-test follow-up
`f2d7c8a25e27541f6b1434dfa9d5201d001a47f5` records the shared result errors 8
and 9 used by catalog operations without changing D-009 records.

Focused executable verification:

- TUNER catalog model: PASS.
- TUNER Full-Control source/menu model: PASS.
- TUNER D-009: PASS, 23/23; BTX: PASS, 46/46.
- TUNER StationManager PSRAM behavior: PASS, 62/62.
- REMOTE Full-Control wire/catalog: PASS, 36 assertions.
- REMOTE navigation/gesture state machine: PASS.
- REMOTE D-009: PASS, 8/8 with 46 assertions; BTX: PASS, 7/7 with 52 assertions.

Final implementation builds (before documentation-only commits):

- TUNER `esp32-lwip-psram`: PASS; static RAM 79,264 / 327,680 bytes;
  linked flash 2,072,557 / 3,145,728 bytes. BIN 2,079,136 bytes, SHA-256
  `7749b1f0f819773c2ee18e964115cbea55ccefa6aa92e388d5e06b31deaf2315`;
  ELF 30,428,856 bytes, SHA-256
  `ab5c1b58bae98baaba0d617fd87ac3a41e8db666b2586572fea1b1df439a19d0`;
  exact-image slot headroom 1,066,592 bytes.
- REMOTE `remote01`: PASS; static RAM 59,468 / 327,680 bytes;
  linked flash 2,597,681 / 8,388,608 bytes. BIN 2,598,176 bytes, SHA-256
  `041d112a4b676956ed5265cd32f89fc423a2892dcc58b9d523901df29bcd7a8f`;
  ELF 19,951,184 bytes, SHA-256
  `b410a027858fa80d33e241b7bf6783997aeb0bfb926cdcab5f329a967fe9b6c9`;
  exact-image factory-slot headroom 5,790,432 bytes.

## Milestone 5 blocker

The approved Radio Browser behavior is not implemented. Source inspection
demonstrates that `StationBrowser::httpsGetJsonOnce()` in `src/browser.cpp`
runs synchronously on the TUNER main loop, gives both connect and read phases
20,000 ms, buffers the complete response in `StreamString`, and deserializes a
dynamic JSON document before returning. The RTCP server arms 5,000 ms response
deadlines in `src/rtcp_server.cpp`. That mismatch can starve RTCP and the
physical/e-paper application loop, and the verified platform has historically
reached 1-2 KiB internal-memory low-water marks.

There is no verified cancellation or bounded cooperative Browser job to reuse.
A new large-stack task is forbidden without memory proof. Bridging the current
call directly to RTCP would therefore violate the binding specification. The
Browser entry remains disabled instead of advertising false functionality.
Completion requires a separately reviewed cooperative HTTPS/JSON job with
bounded ownership, cancellation/stale-result rules, memory measurements and
RTCP coexistence tests.

## Known limitations and risk gates

- No Full-Control feature has been installed or hardware-tested.
- Saved/Favorites/Recent are implemented and offline-tested but not installed
  or hardware-validated. Radio Browser remains blocked as described above.
- Runtime DMA headroom cannot be established from static build figures.
- Existing intermittent REMOTE QMI8658 motion-wake initialization remains a
  separate known limitation and is not silently folded into this project.
- Existing dependency warnings in the TUNER build are historical; the
  Full-Control sources compile with their configured warning policy.

## Separately gated hardware plan and rollback

These partial images are not authorized for installation by this offline task.
If the owner later accepts commissioning with Radio Browser still disabled:

1. Reverify device identity, partition layout, each candidate hash, and all
   local/external rollback copies without altering either device.
2. Install TUNER first, application-only, then perform an independent exact-span
   readback. With the installed REMOTE, verify legacy D-009/BTX, volume, Radio,
   Bluetooth, HTTP and RTCP compatibility before proceeding.
3. Install REMOTE application-only and verify exact readback. Test source
   switching, independent navigation, read-only physical-menu projection,
   Saved/Favorites/Recent paging, active highlighting, mutation fencing,
   playback confirmation, disconnect/reconnect, sleep/wake and simultaneous
   physical TUNER use. Use a controlled temporary record for destructive tests
   and restore owner data exactly.
4. Stop on any authentication, audio, memory, persistence, UI or no-replay
   regression. Never auto-retry a write or mutation.

The preserved installed REMOTE baseline is implementation `596cb71`,
2,583,008 bytes, SHA-256
`fad87bb2f50142f3758945c5a2276a2535008aec4f32a5ec29f021faa95f3b3f`, plus
the verified full 16 MiB backup. The preserved installed TUNER baseline is
implementation `dbf9896`, 2,072,416 bytes, SHA-256
`6f02cdfd1576831a5d4a40f54eff88dcc3960fcdf72545d0909b0525cbceabf6`.
Rollback, if separately authorized, restores REMOTE first and then TUNER so an
older REMOTE never emits optional records to a downgraded TUNER. All writes are
application-only with exact-span verification; NVS, pairing, bootloaders and
partition tables remain untouched.

## Required update discipline

At each milestone, record source/documentation commits, behavioral tests,
build and memory evidence, hardware status, limitations, and remaining work.
Never convert an offline PASS into hardware PASS. Before any future UI,
navigation, source, station, or persistence change, re-read
`REMOTE_FULL_CONTROL_SPEC.md`.
