# REMOTE BTX hardware validation - 2026-10-25

Status: **HARDWARE FAIL - OPEN UI/state-synchronization defect**

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
| Radio playback label after display-idle wake | FAIL (`UNKNOWN`) |
| REMOTE Radio Play/Pause availability | FAIL (disabled) |
| Radio metadata presentation | NOT COMPLETED |
| Bluetooth source/metadata/playback | NOT TESTED |
| Bluetooth AVRCP controls | NOT TESTED |
| Radio/Bluetooth transition synchronization | NOT TESTED |
| Sleep/reconnect stale-state checks | NOT TESTED |

## Remaining work

A separate source task must inspect authoritative playback-state mapping, RTCP
snapshot/update handling and display-idle wake republishing. The corrected
behavior must be software-verified and separately authorized for installation
before hardware commissioning resumes.
