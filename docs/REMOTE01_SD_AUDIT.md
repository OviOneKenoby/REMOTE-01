# REMOTE//01 SD audit

Evidence date: 2026-10-05. Audit only; no mount, format, media access or physical PASS. Pin/resource classifications are in [the hardware map](REMOTE01_HARDWARE_RESOURCE_MAP.md).

## Current canonical purpose

2026-10-05: **DECIDED - SD keeps a firmware backup before flashing/updating.** [Register items24-27/37-38](REMOTE01_PRODUCT_DECISIONS.md). SD staging/SD OTA are historical suggestions, not canonical purpose. Backup selection/completeness/verification/retention/failure/recovery contract DESIGN REQUIRED; current backup implementation/hardware acceptance not claimed.

## Current implementation and dormant code

Root `CMakeLists.txt` excludes `sdcard_bsp` and `codec_board`. Active `main/main.cpp` does not initialize SD. SDK SDMMC/FATFS libraries and the factory SD-size label do not establish an active storage service; factory producer tasks remain disabled.

| Question | Verified answer and evidence |
| --- | --- |
| Is SD currently implemented? | No active application SD service. DORMANT code exists in `components/sdcard_bsp/sdcard_bsp.c`. |
| Expected interface | Native SDMMC, one-bit mode, high-speed host setting; not SPI. |
| Expected pins | Header assigns CMD39, D040, CLK41; CD/D3 macro38 is not configured as card detect. These are source expectations, not owner-verified REMOTE wiring. |
| Conflicts | 39/40/41 overlap alternate external JTAG functions; not an active conflict with native USB Serial/JTAG. Consult map before assigning controls/audio. Physical exposure/routing UNKNOWN / UNCONFIRMED. |
| Card detect | No active detect handling; GPIO38 macro is insufficient evidence of a connected switch. OWNER HARDWARE INPUT REQUIRED. |
| Power switching | No verified S3 SD power switch/control. Generic codec helper's ESP32 GPIO13 and ESP32-P4 LDO branches do not apply to this S3 target. |
| Filesystem | ESP-IDF FATFS/VFS `/sdcard`; mount config max_files=5, allocation_unit_size=48KiB, format_if_mount_failed=false. The misleading nearby formatting comment does not override the false setting. Effective SDK config: two FATFS volumes, no long filenames, 4096-byte sector setting, external-RAM allocation preference. No flash filesystem partition. |
| Product purpose | **DECIDED: firmware backup before flashing/updating.** This settles use, not implementation, backup format/verification or physical acceptance. |

Generic `components/codec_board/codec_init.c` also contains mount helpers driven by board configuration. The reviewed `board_cfg.txt` profiles do not establish an actual SD configuration for this product. Neither helper is called by active firmware.

## Error handling and resource findings

Dormant BSP creates an event group without checking allocation, retains global card/status/list variables, and sets mount status after card discovery. It has no complete owner/lifetime, unmount/hot-removal, bounded application retry or concurrent-access contract. SDK transaction/DMA capabilities are not a measured application RAM budget.

Its write helper uses `fprintf(f, data)`: arbitrary data would be interpreted as a format string. Write/close results are not fully checked. The read helper has no caller capacity argument and uses unchecked seek/tell-derived length; failed/short reads and oversized files are not safely bounded. These are **source hazards in excluded code**, not demonstrated running defects. Do not activate this implementation unchanged.

Physical bus integrity, card voltage/pullups, card models/capacity/format, speed, sustained transfers, detect and safe removal are UNKNOWN / UNCONFIRMED. Do not infer these from macros or SDK availability.

## Bounded implementation and hardware-validation gate

No active storage/backup service was added by the earlier audit. Purpose is now DECIDED; routing/card detect/power/mount/unmount/bounded I/O/removal/errors are IMPLEMENTATION + HARDWARE VALIDATION REQUIRED. Define backup identity/completeness/verification/retention, media lifecycle and failure/recovery behavior with OTA design; do not invent policy from the dormant filesystem. Verify physical routing, approve the bounded backup contract, audit the selected driver, test failures/removal/limits and separately validate media. No auto-format, boot mount, partition change or physical write is authorized by this audit.


## Historical classification supersession - 2026-10-05

Earlier audit statement "SD PRODUCT USE = OWNER DECISION REQUIRED" and later recovery classification "UNKNOWN / NOT RECOVERED" are SUPERSEDED by owner-decided firmware backup purpose. Historical SD staging/OTA possibilities do not replace backup. Dormant implementation/resource/error findings above remain preserved; no media operation, source change or new physical PASS.


### Exact superseded audit wording - historical only

These statements are preserved as evidence of the earlier audit, not current decision status. Current classifications are in the register and corrected sections above.

```text
| Product purpose | **SD PRODUCT USE = OWNER DECISION REQUIRED.** No approved content, persistence, cache or export policy was found. |
```

```text
No storage abstraction was added: purpose, ownership, allowed operations, filename/size quotas, media lifecycle and failure semantics are not specified. A generic filesystem wrapper now would silently choose policy without a caller. First obtain owner purpose plus physical routing evidence; then approve a bounded service contract, audit the selected driver, test failure/removal/limits on host, and separately validate real media. No auto-format, boot mount, partition change or physical write is authorized by this audit.
```

