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
| 1 | Source switching | PASS at current offline working-tree checkpoint | NOT TESTED | Commit implementation after permanent-doc checkpoint; hardware install/Radio-Bluetooth validation separately authorized. |
| 2 | Read-only canonical menu/state projection | NOT IMPLEMENTED | NOT TESTED | Bounded revisioned visible-window projection. |
| 3 | Semantic navigation | NOT IMPLEMENTED | NOT TESTED | Safe open/move/select/back/exit with local-control priority. |
| 4 | Saved/Favorites/Recent | NOT IMPLEMENTED | NOT TESTED | Queries, play, gestures, confirmation, persistence operations, HOME shortcut. |
| 5 | Radio Browser | NOT IMPLEMENTED / RISK GATE | NOT TESTED | Prove bounded cooperative HTTPS job or mark BLOCKED. |
| 6 | UI/lifecycle | PARTIAL | NOT TESTED | Milestone 1 source UI, popup, disconnect, wake-to-HOME and global encoder context exist; list/navigation lifecycle remains. |
| 7 | Final offline verification | NOT STARTED | N/A | Full focused regressions, clean final builds, manifests and rollback plan. |

## Milestone 1 implementation

### Protocol

Existing authenticated RTCP/1 framing and IDs 1-15 are unchanged. BTX feature
bit 6 advertises the optional extension before a new REMOTE sends any new ID.
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

Implementation commit IDs are pending the required documentation-first local
commit and will be recorded here at the next checkpoint.

### Automated verification

- TUNER Full-Control source model: PASS (parse, bounds, generation stale,
  busy, idempotent, bounded mailbox/no replay).
- TUNER existing BTX regression: PASS, 46/46 assertions.
- REMOTE Full-Control wire regression: PASS, 15 assertions.
- REMOTE existing BTX regression: PASS, 7 tests / 52 assertions.
- TUNER `esp32-lwip-psram` integration build: PASS. Interim image 2,074,080
  bytes; link usage 2,067,505 / 3,145,728 bytes; static RAM 79,128 / 327,680.
- REMOTE `remote01` integration build: PASS. Interim image 2,587,024 bytes;
  link usage 2,586,529 / 8,388,608 bytes; static RAM 56,796 / 327,680.

These are interim compile checkpoints, not final candidates: the embedded
build identity still reflects the pre-commit HEAD and final clean builds and
manifests are required after all safe milestones.

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
