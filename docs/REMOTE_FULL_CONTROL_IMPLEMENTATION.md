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
| 2 | Read-only canonical menu/state projection | PARTIAL; wire/server/client/API PASS, dedicated list rendering pending | NOT TESTED | Render through the approved list UI during Milestone 4. |
| 3 | Semantic navigation | NOT IMPLEMENTED | NOT TESTED | Safe open/move/select/back/exit with local-control priority. |
| 4 | Saved/Favorites/Recent | NOT IMPLEMENTED | NOT TESTED | Queries, play, gestures, confirmation, persistence operations, HOME shortcut. |
| 5 | Radio Browser | NOT IMPLEMENTED / RISK GATE | NOT TESTED | Prove bounded cooperative HTTPS job or mark BLOCKED. |
| 6 | UI/lifecycle | PARTIAL | NOT TESTED | Milestone 1 source UI, popup, disconnect, wake-to-HOME and global encoder context exist; list/navigation lifecycle remains. |
| 7 | Final offline verification | NOT STARTED | N/A | Full focused regressions, clean final builds, manifests and rollback plan. |

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
Rendering that projection in the approved station-list UI is intentionally
deferred to the list milestone, so Milestone 2 remains PARTIAL rather than
being overstated as complete.

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

## Known limitations and risk gates

- No Full-Control feature has been installed or hardware-tested.
- Saved/Favorites/Recent and Radio Browser controls are not implemented.
- The existing TUNER Radio Browser performs blocking HTTPS and cannot be
  exposed as a synchronous RTCP operation. Milestone 5 must prove a bounded
  cooperative design within the historically constrained internal/DMA heap or
  be recorded BLOCKED.
- Runtime DMA headroom cannot be established from static build figures.
- Existing intermittent REMOTE QMI8658 motion-wake initialization remains a
  separate known limitation and is not silently folded into this project.
- Existing dependency warnings in the TUNER build are historical; the
  Full-Control sources compile with their configured warning policy.

## Required update discipline

At each milestone, record source/documentation commits, behavioral tests,
build and memory evidence, hardware status, limitations, and remaining work.
Never convert an offline PASS into hardware PASS. Before any future UI,
navigation, source, station, or persistence change, re-read
`REMOTE_FULL_CONTROL_SPEC.md`.
