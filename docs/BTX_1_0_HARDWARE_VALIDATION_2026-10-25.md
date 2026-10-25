# REMOTE BTX hardware validation - 2026-10-25

Status: **HARDWARE FAIL PRESERVED; SOFTWARE CORRECTION VERIFIED / HARDWARE RETEST PENDING**

## Offline ownership correction checkpoint

Source analysis established that REMOTE accepted the correct D-009 Radio
snapshot, then allowed the following non-Bluetooth BTX state record to publish
an absent playback value and Bluetooth operation unavailability over the shared
HOME model. TUNER data, RTCP authentication and wire parsing were correct.

Implementation commit `596cb71be867e2b5ff6eb3f6c0dde52d6dd8a770`
introduces explicit ownership at the existing adapter boundary:

- Radio retains D-009 authority for playback, metadata and Radio operations.
- Bluetooth retains BTX authority for Bluetooth state, metadata and operations.
- Non-Bluetooth BTX generations never enter the D-009 media fence.
- Separate bounded fences retain current-source command generation fencing.
- A cached latest D-009 snapshot completes Bluetooth-to-Radio handover without
  publishing an `UNKNOWN` BTX absence.
- Disconnect/session replacement clears both fences and all cached/pending
  state as before.

Executable verification drove the real `RtcpDevice` handlers and CCM Facade:
4/4 ownership cases passed with 92 assertions. Existing BTX wire 7/7 (52),
D-009 wire 8/8 (44), HOME/Core 42/42 (3,565), BTX policy 15/15, D-009 policy
16/16, HOME metadata 15/15, UI volume binding 1/1 (11), and RTCP
freshness/volume admission 2/2 (221) also passed.

The broad hardening run had one unrelated `utf8_snapshot_bounds` length failure.
The exact same failure was reproduced in an isolated worktree at pre-correction
commit `349c37982cebd908cd66408afa0d31182c5a8cbf` (25/26 cases passed), proving it
was already present. No unrelated correction was made.

Clean production build `remote01`: PASS, exit 0, zero warning/error lines.

| Candidate item | Verified result |
| --- | ---: |
| Embedded build | `596cb71` |
| Application size | 2,583,008 bytes |
| Application SHA-256 | `fad87bb2f50142f3758945c5a2276a2535008aec4f32a5ec29f021faa95f3b3f` |
| ELF size | 19,820,532 bytes |
| ELF SHA-256 | `1bbb0cdc2eb1035c30d46e985369f855f7a10ee98d5bdefdfb592af10a08bd02` |
| Static RAM | 56,652 / 327,680 bytes |
| Linked flash | 2,582,509 bytes |
| 3 MiB slot headroom | 562,720 bytes |
| 8 MiB factory-partition headroom | 5,805,600 bytes |

Private candidate:
`C:\Users\RYZEN\Documents\BTX_PRIVATE_CANDIDATES\20261025T130324Z-remote-radio-ownership-596cb71`

The previously installed application and fresh 16 MiB rollback backup remain
unchanged and independently hash-verified. No hardware was accessed for this
correction.

## Installed identities

- REMOTE device: ESP32-S3 QFN56 revision 0.2, native USB Serial/JTAG, 16 MiB.
- Source/documentation head: `2200bc8df02262e076ecdbd3c9fd66eac4c5d3b3`.
- Embedded build: `5b32cbb`.
- Application offset: `0x10000`; live factory capacity: `0x800000`.
- Application size: 2,582,432 bytes.
- Application and exact readback SHA-256:
  `5d84f83b8e9e6f860e7cd807a270eeee697ed42dc361bbb1e1022f639dc4206b`.
- TUNER remained on build `dbf9896876a7`; it was not flashed, reset or opened
  through COM5.

## Backup and rollback

A fresh read-only 16 MiB REMOTE backup was captured before installation. Its
SHA-256 is
`bacaac4c60abd1d13b7aca0764d6eff166f6442427fc53304a7ab0c5a4b42d5f`.
The extracted installed Round 5 application is 2,578,352 bytes and matches
SHA-256
`6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`.
The partition-table sector matched the preserved layout SHA-256
`fd62ef669cdbab98ac5372777a686d8dcd9813f7eb5302a67cc5136e96e90f69`.
Private NVS, OTA metadata and `remote_ctl` region backups were also retained.

Private evidence:
`C:\Users\RYZEN\Documents\BTX_PRIVATE_HARDWARE_EVIDENCE\20261025T105633Z-REMOTE-BTX-integration`

## Installation verification

- Single application-only write at `0x10000`: PASS.
- esptool transmitted-data verification: PASS.
- Independent 2,582,432-byte readback at 460800 baud: PASS.
- Boot and embedded build identity: PASS.
- Stored Wi-Fi/configuration/pairing retention: PASS.
- Display, HOME and touch owner check: PASS.
- RTCP authentication and READY state: PASS.
- Authoritative volume and `volume.set`: PASS; no request outstanding.
- Unexpected reset after boot: none observed.

## Functional result and stop condition

Radio ABF HTTPS MP3 played audibly and TUNER remained online. Diagnostics at
the failure checkpoint showed REMOTE RTCP stage 13, unchanged session 2, no
RTCP failure/error, authoritative volume 55%, and no in-flight command. TUNER
reported Radio ABF as `playing`, remained Wi-Fi connected with zero disconnects
and continued producing RTCP responses.

After REMOTE woke from display-idle, the small green HOME playback-status label
displayed `UNKNOWN` rather than `PLAYING`. The Play/Pause control was
disabled/unavailable. This is recorded as an OPEN UI/state-synchronization
defect. Available evidence does not demonstrate an RTCP transport,
authentication or wire-protocol failure.

The loss-of-existing-function stop condition ended commissioning. No source
analysis or correction was attempted in this task.

## Acceptance matrix

| Check | Result |
| --- | --- |
| Candidate write/readback | PASS |
| Boot, Wi-Fi, pairing, display and touch | PASS |
| RTCP authentication/READY | PASS |
| Authoritative volume and volume control availability | PASS |
| Radio ABF TUNER playback | PASS |
| Radio playback label after display-idle wake | HARDWARE FAIL; corrected candidate retest pending |
| REMOTE Radio Play/Pause availability | HARDWARE FAIL; corrected candidate retest pending |
| Radio metadata presentation | NOT COMPLETED |
| Bluetooth source/metadata/playback | NOT TESTED |
| Bluetooth AVRCP controls | NOT TESTED |
| Radio/Bluetooth transition synchronization | NOT TESTED |
| Sleep/reconnect stale-state checks | NOT TESTED |

## Remaining work

Install the exact corrected candidate only after separate owner authorization.
With Radio ABF playing, observe at least three snapshot cycles, wake from
display-idle, and require persistent `PLAYING` plus enabled Pause. Then perform
one controlled Pause/Play and one Radio -> Bluetooth -> Radio transition,
confirming current-source metadata, RTCP READY, volume synchronization and no
stale replay. The historical hardware FAIL remains authoritative until this
bounded retest passes.
