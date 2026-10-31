# REMOTE Full Control — Implementation Record

This record tracks implementation against the binding
[owner-approved specification](REMOTE_FULL_CONTROL_SPEC.md). It describes
engineering state; it does not replace or relax the specification.

The ordered owner-executable hardware procedure is
[`FULL_CONTROL_HARDWARE_VALIDATION.md`](FULL_CONTROL_HARDWARE_VALIDATION.md).
Its checkboxes are intentionally unmarked until physical evidence is recorded.

## 2026-10-31 Radio Browser paired hardware checkpoint

REMOTE `8fdabc6` and TUNER `12b1b24` were installed application-only and both
exact-span readbacks matched their approved SHA-256 identities. A complete
Country -> Name -> Genre -> Results flow and one station selection passed on
hardware: overlays cleared, no disconnect card appeared, audio started once,
and metadata, PLAYING and volume were authoritative. REMOTE stayed at RTCP
stage 13 and its retained close history did not change. TUNER recorded one
Browser start/result, nine produced pages, three completed HTTPS jobs, HTTP
200, zero Browser job failures, and authoritative MP3 playback.

This closes the specific blank-page, Country-overlay and synchronous Browser
PLAY regressions. Extended mixed-use soak, exhaustive cancellation/error/empty
results, and unrelated historical UTF-8 snapshot bounds remain outside this
checkpoint.

## 2026-10-29 focused hardware follow-up

The `0ed0ffb` TUNER / `9142401` REMOTE pair passed startup, idle RTCP baseline,
Radio/Bluetooth source switching, Bluetooth audio/metadata, phone-, REMOTE- and
physical-TUNER Play/Pause, Radio gain recovery, authoritative volume and
SYSTEM_SLEEP/touch-wake restoration. Audio continued without interruption
while REMOTE slept. TUNER does not render playback state; only audible behavior
and REMOTE's authoritative label were verified.

Favorite playback succeeds, but action-time transport is not accepted: both
trials produced a brief disconnect overlay and retained response-timeout/
`ECONNRESET` evidence. The TUNER response maximum reached 3,011,296 us while
its e-paper redraw was in the deferred-completion path. Browser START tried two
endpoints, each ending before HTTP status with `ESP_ERR_HTTP_CONNECT`; audio,
BACK/EXIT and the underlying Country list remained usable.

The minimum follow-up is TUNER-only source
`579f412691df6ffc4d03513d1c64f7a65ea18320`: authoritative deferred-play state
is published before a delayed e-paper redraw, and Browser transport begins at
the official DNS hostname with two resolving fallbacks and a five-second
per-operation timeout under the unchanged 30-second job fence. Focused tests
and production build pass. Candidate BIN: 2,135,920 bytes, SHA-256
`663983a35e060eb1fee6943512dc60d22ef7414255725a46f329b7c5915aed3c`;
static RAM 80,792 bytes; exact-image headroom 1,009,808 bytes. It is not
installed. REMOTE source and installed firmware are unchanged.

## 2026-10-28 post-hardware correction checkpoint

The first joint `+fc5` hardware run proved several action-time failures after
the clean five-minute idle interval. The minimum paired correction is now
offline verified; it is not installed and has no hardware PASS yet.

| Observed failure | Correction | Verification state |
| --- | --- | --- |
| Accepted Favorite PLAY was reported unavailable before deferred startup completed; a repeat then coincided with a three-second RTCP timeout. | REMOTE retains the accepted confirmation fence and issues bounded read-only catalog re-queries every 500 ms under the existing 15-second deadline. TUNER temporarily lowers only the persistent audio task's priority during the blocking stream-open call, then restores it. No mutation retry/replay was added. | Focused confirmation/deferred-start and protocol regressions PASS; hardware retest required. |
| REMOTE and physical TUNER Bluetooth Play/Pause produced no phone action. | TUNER submits checked AVRCP press/release commands with rotating transaction labels that do not collide with the pinned library's label-0 capability request. Link state and both API results are checked. | Authority, BTX and integration tests PASS; phone hardware response required. |
| Phone Pause could be reported as STOPPED. | Once A2DP reports `REMOTE_SUSPEND`, a simultaneous AVRCP STOPPED report preserves authoritative PAUSED for phones using that representation. | Behavioral authority tests PASS; hardware retest required. |
| Both Browser mirrors failed connection establishment within 618 ms using a 250 ms per-transport timeout. | Asynchronous per-transport timeout is 1,000 ms; the two-mirror limit and one 30-second absolute deadline remain unchanged. Error dismissal renders a real Browser page instead of an empty grey backing view. | Policy/UI tests PASS; HTTPS success remains hardware-pending. |
| Radio could report Playing but remain silent after Bluetooth until a physical volume action. | TUNER reapplies the authoritative gain after decoder startup following the Bluetooth I2S handoff. | Source/audio hardware retest required. |

Implementation commits:

- REMOTE `91424013726cf9ede72de68d579842445141c2f2`.
- TUNER `0ed0ffb366b8b5f12e773bc7bfb658a09f8523a4`.

Production artifacts:

- REMOTE `remote01`: BIN 2,610,928 bytes, SHA-256
  `e7105144fcca1faf0d6a998b39603262d459132394d36da03cf833cb129508ef`;
  ELF 20,057,180 bytes, SHA-256
  `0380d8b56bd417e2bc15c1496ec1a0515d1693c075a6dd296331aafa02e1a206`;
  static RAM 61,180 / 327,680 bytes; linked flash 2,610,433 / 8,388,608
  bytes; exact-image slot headroom 5,777,680 bytes; embedded ID `9142401`.
- TUNER `esp32-lwip-psram`: BIN 2,135,904 bytes, SHA-256
  `129f7c9e276e4e06c3c007eef7290409720f899845a244d97b704768a40d321e`;
  ELF 31,252,700 bytes, SHA-256
  `746786cae402b1e96231b8db1d14b3178aa75ba522cef5786d04867b926276ed`;
  static RAM 80,784 / 327,680 bytes; linked flash 2,129,321 / 3,145,728
  bytes; exact-image slot headroom 1,009,824 bytes; embedded ID `0ed0ffb`.

Hardware acceptance still requires one Favorite start, outbound Bluetooth
Pause/Play from both controls, phone-originated Pause mapping, one complete
Browser flow plus error dismissal, Bluetooth-to-Radio audible startup without
a gain nudge, and action-time RTCP counter correlation. AAC/AAC+ and a longer
mixed-use soak remain untested. The unrelated pre-existing
`utf8_snapshot_bounds` failure remains open.

## 2026-10-28 stability-correction checkpoint

| Item | Offline status | Hardware requirement |
| --- | --- | --- |
| FC-RTCP-01 retained socket-close evidence | PASS, diagnostic-only | Capture both four-entry rings during one reproduced disconnect; do not call transport fixed without correlated evidence. |
| FC-FAV-02 deferred station startup | PASS offline | Verify bounded admission, physical TUNER preemption, exactly-one playback and no replay after disconnect/revision change. |
| FC-BT-01 AVRCP playback authority | PASS offline on TUNER | Pause/resume from phone and REMOTE; verify state and button across reconnect/source transitions. |
| FC-RB-01 TLS failure evidence | PASS, diagnostic-only | Reproduce one Browser request and retain first/final ESP-TLS error categories; HTTPS retrieval is not declared fixed. |

The optional capability is now version 1.4, revision 5, identity `+fc5`.
Feature bit 32 explicitly advertises deferred catalog PLAY. Older peers fail
closed for this operation; RTCP security and existing IDs remain compatible.

Final offline identities:

- REMOTE source `2ff3c9b0a0cfa07b2bd7282b1e41ef10f7b4c5c7`; BIN
  2,610,752 bytes, SHA-256
  `81986adf59e75b6df1f9af8ca6397c15ca1deb0cccc240146bc68b0e03a9b8dd`;
  ELF SHA-256
  `fa76d949cd9e7933f7e2783dbf9bffc5cba0337404864cadc8c800c2a151de20`;
  static RAM 61,180 bytes; exact-image headroom 5,777,856 bytes; app version `2ff3c9b`.
- TUNER source `014185ab1e2e25ce590dc67833350a4de98fc77e`; BIN
  2,135,376 bytes, SHA-256
  `c33ec3b137f2ba21b8dbdead00dd962efe21bd5b343273f4c50d62ed133e273f`;
  ELF SHA-256
  `16f3659d88a60065df00a7c687d841fc2f0d3eae07f7432e71cee656bdb1d1e4`;
  static RAM 80,784 bytes; exact-image headroom 1,010,352 bytes; build ID `014185ab1e2e`.

This checkpoint is offline only. Installation and physical acceptance remain
pending separate authorization.

## 2026-10-27 targeted-correction checkpoint

Classification and evidence for the post-deployment defects:

| Issue | Status | Offline result | Hardware requirement |
| --- | --- | --- | --- |
| FC-RB-02 Browser touch-through | **FIXED OFFLINE - hardware retest pending** | Browser `list_panel` alone is pointer-intercepting; executable/policy coverage verifies blank-area isolation and visible controls. | Exercise loading, error, empty and results overlays while ACTIVE; confirm no covered menu activation. |
| FC-UI-02 source-switch HOME return | **FIXED OFFLINE - hardware retest pending** | Bounded `SourceConfirmation` handles ACK/state ordering, rejection, wrong/stale state, expiry and reconnect without replay; HOME is armed only by authoritative success. | Switch both directions and verify confirmed feedback followed by HOME after three seconds. |
| FC-RB-01 whole-job deadline | **FIXED OFFLINE - hardware retest pending** | One 30-second deadline now spans both mirrors and cannot be extended by fallback. | Verify one physical Browser job respects the bound. |
| FC-RB-01 HTTPS `error 8` cause | **DIAGNOSTIC ONLY - more evidence required** | TUNER retains first/final mirror, transport stage, `esp_err_t`, HTTP status, elapsed time and deadline result. Pinned client API has no safe TLS last-error accessor. | Capture diagnostics before and after exactly one Browser attempt. Do not call Browser fixed until retrieval succeeds. |
| FC-FAV-02 blank disconnect popup | **DIAGNOSTIC ONLY - more evidence required** | REMOTE retains popup reason/lifecycle/session/close reason/command elapsed time; TUNER retains source/catalog/Browser response last/max duration. Behavior is not suppressed. | Reproduce one Favorite PLAY and correlate both endpoints. |
| REMOTE `/api/status` 503 | **FIXED OFFLINE - hardware retest pending** | Worst-case serialization proved the old fixed buffer overflow. The bounded response area is 6,384 bytes inside unchanged 8,192-byte storage; 503 categories are distinct. | Confirm normal and long-running `/api/status`; if 503 occurs, retain its explicit body. |
| Silent audio report | **UNCHANGED - deliberately outside scope** | Controlled Pause/Resume had not reproduced it; audio/I2S paths were untouched. | Report only if reproducible. |
| `utf8_snapshot_bounds` | **UNRESOLVED - pre-existing unrelated test** | Reproduced before these corrections; broad runner stops there. | Track separately; no speculative change in this checkpoint. |

Source/build checkpoints:

- REMOTE implementation: `42c31034c64235d0625f226978e6d8900dc18195`.
- TUNER implementation: `67f610a383dfca3b51f80ee4d734375578c197ed`.
- REMOTE `remote01`: BIN 2,608,800 bytes, SHA-256
  `99fe856562d62fbd138e7529080dc00baf65c647ce11cd8419b1fc63d070e8b9`;
  ELF 20,025,164 bytes, SHA-256
  `ef0a9553cc90d887b51099e598eb2b90b85d141525eccfc469829234f489c094`;
  static RAM 61,180 bytes; exact-image slot headroom 5,779,808 bytes.
- TUNER `esp32-lwip-psram`: BIN 2,130,896 bytes, SHA-256
  `6a89b8388393ac2a9c962c6bcc9e3ad3ee980f6a42c9e73bf3166df54e9d4756`;
  ELF 31,186,376 bytes, SHA-256
  `5f64eef42f9b0d464ec7327af27673cd2c9d64d094a396882b97249effaba8d2`;
  static RAM 80,032 bytes; exact-image slot headroom 1,014,832 bytes.

No hardware was accessed. Both candidates require separate installation
authorization and the focused retests above.

## Baselines

- REMOTE branch: `remote/btx-1.0-implementation`
- Initial verified REMOTE HEAD: `ea7ae801803f7285e9b3d1549080cc3035087595`
- Previous REMOTE rollback implementation: `596cb71be867e2b5ff6eb3f6c0dde52d6dd8a770`
- TUNER branch: `tuner/btx-tls-psram-remediation`
- Initial verified TUNER HEAD: `39a3705b2c9a85824cf995782cd44abee3a1408a`
- Previous TUNER rollback implementation: `dbf9896876a7fe8c9b421bb1608dc9c2782e9cfe`
- Current Full Control applications were installed application-only and
  independently read back on 2026-10-26: REMOTE source `fdcbdac`, 2,606,192
  bytes, SHA-256
  `dc80ba560a3327a069c911de0af31d20eeece77d8f7681ab4d54cbce3ed1c270`;
  TUNER source `e51e39d`, 2,127,616 bytes, SHA-256
  `4d68d01fd3ccaffbd463c6ff310e54d16bb42d2e9e102426384cf1d4b63e8e6d`.
  Full raw rollback backups are preserved on `D:`. Installation/readback is
  PASS; functional hardware validation remains NOT TESTED.

## Milestone matrix

| # | Milestone | Software | Hardware | Remaining work |
| --- | --- | --- | --- | --- |
| 1 | Source switching | PASS; TUNER `5d10615`, REMOTE `9293a99` | NOT TESTED | Run the Radio/Bluetooth checks in `FULL_CONTROL_HARDWARE_VALIDATION.md`. |
| 2 | Read-only canonical menu/state projection | PASS; bounded TUNER-owned menu projection is rendered as read-only status | NOT TESTED | Hardware rendering/physical-priority validation. |
| 3 | Semantic navigation | PASS for the safe local REMOTE navigation model; no raw TUNER input injection | NOT TESTED | Hardware touch, BACK/EXIT, sleep/wake and simultaneous-control validation. |
| 4 | Saved/Favorites/Recent | PASS offline; bounded queries, play, delete confirmation, add-favorite, paging, HOME shortcut and gestures implemented | NOT TESTED | Hardware persistence, playback-confirmation and physical-priority validation. |
| 5 | Radio Browser | PASS offline; bounded cooperative TUNER service and complete REMOTE flow implemented | NOT TESTED | Execute `FULL_CONTROL_HARDWARE_VALIDATION.md`; do not infer hardware behavior from installation/readback. |
| 6 | UI/lifecycle | PASS offline for all approved flows | NOT TESTED | Touch, cancellation, disconnect, sleep/wake and simultaneous physical-control validation. |
| 7 | Final offline verification | IN PROGRESS; Milestone 5 focused tests and both production builds PASS | N/A | Remaining Full Control milestones and complete hardware acceptance. |

## Full Control 1.3 targeted correction checkpoint

Hardware validation established two REMOTE defects and one unresolved Browser
failure. The approved specification is unchanged.

- **FC-UI-01 — FIXED offline; hardware retest pending.** The shared popup used
  `remote01_single_line()` and then enabled wrapping, so a one-line object
  clipped multiline progress and result messages. `make_popup()` now creates a
  bounded 124 x 52 multiline label directly, centered above the existing
  spinner/button area. It does not change the global helper or popup lifetime.
- **FC-FAV-01 — FIXED offline; hardware retest pending.** A fixed
  `CatalogPlaybackConfirmation` retains collection, token, prior sequence,
  minimum revision and a 15-second deadline. Disconnect invalidates the old
  session but preserves the selected identity for read-only reconciliation.
  Reconnection sends only a catalog query. Fresh same-item plus authoritative
  PLAYING completes the existing feedback and three-second HOME return;
  different/stopped/stale/expired evidence clears the lifecycle without
  claiming success. PLAY is never replayed or duplicated.
- **FC-RB-01 — DIAGNOSTIC ONLY.** The empty Country panel is not corrected
  speculatively. REMOTE adds a fixed 16-byte diagnostic record; TUNER adds a
  fixed 56-byte record. No task, dynamic diagnostic allocation, packet logging,
  timeout change, generation relaxation or wire change was introduced.
- **FC-RB-02 — NOT CHANGED.** Touch-to-HOME may be the specified display-idle
  wake path. The required future test keeps REMOTE continuously ACTIVE, opens
  Browser, and touches an empty area before inactivity.

Implementation commits are REMOTE `9286e31` (popup), `e020640` (catalog
reconciliation), `05abdf5` (Browser diagnostics) and TUNER `4fa198a` (Browser
diagnostics). Focused test commits are REMOTE `65923ad` and TUNER `b3ed9e5`.

Verification:

- popup layout: 6/6 PASS;
- catalog confirmation sequences, including reconnect/different/stopped/
  expiry/stale/rapid operation and no replay: PASS;
- REMOTE diagnostic schema: 9/9, 9,098 assertions; exact Snapshots 1,024 bytes
  and total fixed diagnostic storage 8,192 bytes;
- REMOTE Browser diagnostic policy: 10/10 PASS;
- TUNER Browser diagnostic state/counter coverage and policy: PASS (13/13
  policy checks);
- relevant Full Control, catalog/navigation, Browser, BTX, D-009,
  StationManager, HOME metadata and volume regressions: PASS;
- broad REMOTE runner: Core 69/69, profile 6/6, HOME 42/42, ownership 4/4;
  hardening 25 PASS / 1 FAIL. The sole `utf8_snapshot_bounds` assertion was
  reproduced at pre-Milestone-5 commit `fefaae6` and remains separately
  tracked.

Production candidates:

| Device | Build source/identity | BIN | BIN SHA-256 | Static RAM | Exact slot headroom |
| --- | --- | ---: | --- | ---: | ---: |
| REMOTE | `65923ad` (embedded app version `65923ad`) | 2,608,160 | `38e165da601756d0b4a2f56be97d8d44a0d42fdf16cb7c871d37a29bbbc53774` | 61,100 | 5,780,448 |
| TUNER | implementation/build ID `4fa198a55422` | 2,129,472 | `5cb1ad29a26173d277772d5f798a1be32ff857a322e2f82bea1ce1ec704fbd68` | 79,976 | 1,016,256 |

REMOTE ELF is 20,022,008 bytes, SHA-256
`c08f3fa41e05b887138e6c38a560dbf1ba875c67becfa1f744a8605ea41a293b`.
TUNER ELF is 31,157,412 bytes, SHA-256
`552d732c823044033666ce4d0d631d9288f0532aa14b66bd4b98db8b2c6756e0`.
Versus the installed applications, REMOTE adds 1,968 bytes BIN and 48 bytes
static RAM; TUNER adds 1,856 bytes BIN and 48 bytes static RAM. The known
TUNER dependency warning about AudioTools include ordering remains unchanged;
no new warning was identified.

Private candidates and manifests are preserved under
`C:\Users\RYZEN\Documents\FULL_CONTROL_PRIVATE_CANDIDATES\20261026T214412Z-targeted-fixes`.
Installation and hardware retesting require separate owner authorization.

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

## Milestone 5 checkpoint

Full-Control 1.3 (`+fc4`, feature bit 16, capability revision 4) adds the
approved Radio Browser without exposing the legacy synchronous browser path to
RTCP. Authenticated IDs 26-28 provide bounded state requests, state pages and
correlated semantic commands. Pages contain at most five validated 48-byte
UTF-8 labels and opaque TUNER tokens; stream URLs and credentials never cross
RTCP. Commands are start, select country, select name, select genre, play,
add-to-Favorites and cancel. The existing correlated result ID 8 reports
completion. Mutations retain the single encrypted request lane and are never
retried or replayed.

The TUNER uses a distinct cooperative `esp_http_client` service, polled from
the application loop. HTTPS certificate verification uses the pinned Arduino
certificate bundle. Each job has a 30-second absolute deadline, two bounded
mirror attempts and fixed limits of 250 countries, 40 tags and 20 result
stations. The 32 KiB response body, JSON document and retained catalog arrays
must allocate in PSRAM; allocation has no internal-RAM fallback. Response
processing remains separate from audio decoding and never invokes the legacy
physical-browser `StreamString` path. Leaving the view, disconnecting, changing
a filter, or physical TUNER browser entry cancels or invalidates the job.
Session generation and browser revision fencing discard stale pages and stale
commands without automatic replay.

REMOTE implements the binding Country (Any/A-Z), Name (Popular/A-Z), Genre
(Any/list), Results and five-item paging flow. It renders searching, empty,
error and disconnected states; provides local BACK and EXIT; resets the
specified scroll positions; highlights an active result; uses a short tap to
play and a two-second press to add a result to Favorites; and waits for
authoritative playback before its success/HOME transition. REMOTE navigation
remains independent and never drives the physical TUNER cursor or display.

Implementation commits are TUNER
`bfcd2574bdf4d221eb96c26fe1444b82c9bcc096` plus opaque-country-token
validation follow-up `e51e39db9ab4b6e8ba7c6497765393d39a05c594`, and REMOTE
`fdcbdac28e67654a85f38b1b458110f530c1d033`.

Focused verification:

- TUNER browser protocol model: PASS; cooperative Browser policy: PASS, 9/9.
- TUNER D-009 integration policy: PASS, 12/12; BTX integration policy: PASS,
  21/21; StationManager PSRAM policy: PASS, 16/16. Previously passing unchanged
  D-009 (23/23), BTX (46/46), catalog and StationManager model evidence remains
  applicable.
- REMOTE browser/full-control wire model: PASS, 47 assertions; navigation and
  gesture model: PASS; D-009: PASS, 8/8 with 46 assertions; BTX: PASS, 7/7 with
  52 assertions; UI metadata policy: PASS, 15/15.
- The wider REMOTE runner passed Core 69/69 (2,425 assertions), Profile 6/6,
  HOME 42/42 (3,565 assertions), RTCP ownership 4/4 and 25/26 hardening cases.
  Its sole `utf8_snapshot_bounds` expectation failure was reproduced unchanged
  at pre-Milestone-5 commit `fefaae6`; it is recorded as pre-existing and was
  not hidden with an unrelated source change.

Production builds from the implementation commits:

- TUNER `esp32-lwip-psram`: PASS; BIN 2,127,616 bytes, SHA-256
  `4d68d01fd3ccaffbd463c6ff310e54d16bb42d2e9e102426384cf1d4b63e8e6d`;
  ELF 31,124,020 bytes, SHA-256
  `f77bc0dea52af9328d305ec88ab221f3c7ad39d4861fe757e49825b61bca3020`;
  static RAM 79,928 / 327,680 bytes, linked flash 2,121,041 / 3,145,728
  bytes, and 1,018,112 bytes exact-image slot headroom.
- REMOTE `remote01`: PASS; BIN 2,606,192 bytes, SHA-256
  `dc80ba560a3327a069c911de0af31d20eeece77d8f7681ab4d54cbce3ed1c270`;
  ELF 20,006,100 bytes, SHA-256
  `3ee7c94c4a43b61ea72ca778501585a9d231ebb303aeb405b161d6a0932f3056`;
  static RAM 61,052 / 327,680 bytes, linked flash 2,605,693 / 8,388,608
  bytes, and 5,782,416 bytes exact-image slot headroom.

At the Milestone 5 build checkpoint this was offline-only: HTTPS latency,
runtime internal/DMA headroom, PSRAM low-water marks, RTCP/audio coexistence
and all physical UX remained hardware-pending, and no device had been accessed.

## Known limitations and risk gates

- Both Full-Control applications are installed with exact readback PASS, but no
  Full-Control functional hardware test has been credited yet.
- Saved/Favorites/Recent and Radio Browser are implemented and offline-tested
  and installed, but remain hardware-unvalidated.
- Runtime DMA headroom cannot be established from static build figures.
- Existing intermittent REMOTE QMI8658 motion-wake initialization remains a
  separate known limitation and is not silently folded into this project.
- Existing dependency warnings in the TUNER build are historical; the
  Full-Control sources compile with their configured warning policy.

## Hardware-validation plan and rollback

Application-only installation and exact readback are complete. Functional
acceptance must follow `FULL_CONTROL_HARDWARE_VALIDATION.md`:

1. Preserve the verified application identities, full backups and deployment
   report without altering either device.
2. First verify TUNER boot, saved-station playback and installed physical
   controls; this gate was deferred by explicit owner instruction during
   deployment and remains pending.
3. Test source
   switching, independent navigation, read-only physical-menu projection,
   Saved/Favorites/Recent paging, and the complete Country/Name/Genre/Results
   Browser flow including empty/error states, page boundaries, play and
   add-to-Favorites. Verify stale-result cancellation on BACK/EXIT, filter
   changes, disconnect and physical TUNER browser entry.
4. During deliberately slow HTTPS work, verify uninterrupted audio, responsive
   physical TUNER controls, RTCP/HTTP health, bounded internal/DMA and PSRAM
   use, no resets, no stale mutation replay, sleep/wake behavior and active
   station/metadata synchronization. Use a controlled temporary record for
   destructive tests and restore owner data exactly.
5. Stop on any authentication, audio, memory, persistence, UI or no-replay
   regression. Never auto-retry a write or mutation.

The preserved REMOTE rollback baseline is implementation `596cb71`,
2,583,008 bytes, SHA-256
`fad87bb2f50142f3758945c5a2276a2535008aec4f32a5ec29f021faa95f3b3f`, plus
the verified full 16 MiB backup. The preserved TUNER rollback baseline is
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

## 2026-10-31 Browser disconnect recovery checkpoint

Hardware follow-up on the installed `efed8f0` candidate established that the
Country list loads successfully (HTTP 200, 2,087 ms, one attempt, no RTCP/TLS/
socket error), but an accepted Country selection left `Loading...` visible even
though the Name-filter page had been admitted. Implementation `8fdabc6` closes
that overlay before rendering Name filters. The directly affected executable
overlay-policy test passes. Clean `remote01` build: PASS; BIN 2,610,992 bytes,
SHA-256 `ba37f42ef9bb520ad94ba0b76bd20f7a08bf38c293e6332f2e6d4af0b89b5c9f`;
ELF SHA-256 `01f57c2c2d6c13901498911fd34614f83123ab4ced9491acbcb67ec6f45b4f81`;
static RAM 61,180 bytes; factory-slot headroom 5,777,616 bytes. Hardware
revalidation of Country -> Name -> Genre -> Results remains pending.

- Implementation `efed8f0` adds deterministic single-shot HOME recovery when
  authenticated RTCP loss invalidates an active Browser page.
- The interrupted Browser session and revision are fenced immediately; no
  Browser START or other mutation is replayed after reconnect.
- Executable navigation/recovery and existing overlay-policy tests: PASS.
- Clean `remote01` build: PASS; BIN 2,610,976 bytes, static RAM 61,180 bytes,
  factory-slot headroom 5,777,632 bytes.
- BIN SHA-256:
  `0dcd32a955db8788af064f15bd063290c867eb45f4ff5439d3ef3940403d22f6`.
- ELF SHA-256:
  `d8f45d3191130c8245f0736b5fe9e226fbf6441a314d771430b6b6241b2fefd6`.
- Hardware validation remains pending: interrupt Browser loading with a genuine
  transport loss, verify popup dismissal returns HOME, and verify no replay or
  stale result. The initiating RTCP close remains unproven because the retained
  REMOTE history was lost to a USB/JTAG-open reset during read-only collection.
- TUNER Browser HTTP 503 was verified across all configured mirrors in the
  failed run, while the same IPv4 URLs later returned HTTP 200 from the PC.
  This is tracked as transient upstream availability; no timeout, TLS, endpoint
  or scheduler change is justified by the current evidence.
