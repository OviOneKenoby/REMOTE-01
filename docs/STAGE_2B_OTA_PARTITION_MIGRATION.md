# Current Stage 2B partition / diagnostics artifact status

2026-10-04. Partition migration remains **OWNER HARDWARE VALIDATED / ACCEPTED**. Approved layout and generated partition binary are unchanged. Subsequent diagnostics/serialR firmware is **HOST TESTED / CLEAN BUILD VERIFIED / OWNER RETEST PENDING**, with fullBIN2,476,432B and each3MiB slot headroom669,296B (653.609375KiB). BIN SHA-256 `a61a303aa23b3f02bb419831080f6d81ecb7e006f8ef11ece1a457ce1bc481e8`. New diagnostics do not establish hardware OTA, slot switching, rollback or deep-sleep acceptance. See [result and single owner procedure](STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md). No upload occurred; accepted migration's original artifact/evidence remain historical below.

---

# Stage 2B physical partition migration ? OWNER HARDWARE VALIDATED / ACCEPTED

2026-10-04. Owner accepts the approved16MiB physical partition migration. This acceptance applies to installed partition layout, retained factory boot, NVS/Wi-Fi retention and observed local/power operation. **OTA transfer, boot-slot switching, rollback and automatic deep sleep are not accepted or claimed.**

Physical evidence supplied by owner:

- New partition table boots successfully from retained factory app. Physical16MiB flash was previously verified with esptool flash_id. No full-chip erase performed; NVS/Wi-Fi credentials survived.
- Initially one authentication attempt failed, then automatic recovery succeeded: reconnects=1, finalCONNECTED, stored=1, provisioning=0, IP192.168.1.83. Captive HTTP/DNS remained disabled after normal STA connection. Do not classify the recovered attempt as an unexplained current failure or infer deliberate router-outage validation.
- Battery use without USB, SYS_EN power hold, buttons/UI on battery, display idle, motion wake and touchscreen wake: **OWNER HARDWARE PASS**.
- No reboot during USB-disconnected test: boot remained19, uptime advanced approximately161s to429s. No observed brownout/WDT. This is evidence for the reported interval, not an unlimited stability claim.
- POWER: hold=1, wakes=3, errors=0, bus_errors=0, batteryREAL/NORMAL. UC runtime: failed_allocations=0, accounting_corrupt=0, owner_ok=1, queue_rejected=0. Final network: CONNECTED, stored=1, provisioning=0, allocation_errors=0.
- **Automatic deep sleep NOT TESTED / NOT ACCEPTED.** Existing policy remains `automatic_system_sleep=INHIBITED reason=EXTERNAL_POWER_UNKNOWN`. No external-power/charging signal is inferred.

Next scope is inspection/design only for connected-STA diagnostics and controlled software reboot; see [implementation plan](STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md). No new firmware change/build/upload accompanies this acceptance record. Owner did not supply a flashed-artifact hash; existing build hashes below remain build identities.

---

# Historical migration implementation / pre-owner acceptance report

# Stage 2B ? approved OTA partition migration

2026-10-04. **OTA partition layout: IMPLEMENTED / BUILD VERIFIED. OTA update mechanism: NOT YET IMPLEMENTED. Hardware migration: OWNER TEST REQUIRED.** Owner authorization is limited to layout migration and verification. No HTTP OTA, authentication, OTA UI, firmware download, SD staging, cloud, LAN diagnostics, reboot-key implementation or TUNER work. The [preflight STOP](STAGE_2B_LOCAL_OTA.md) remains historical evidence: the old table provided no inactive OTA slot or selection metadata.

## Physical geometry and SDK evidence

Read-only esptool 4.11.0 `flash_id` on COM3 succeeded after the owner freed the monitor: ESP32-S3 revision v0.2, USB-Serial/JTAG, manufacturer0x20/device0x4018, **detected flash16MB**. Exact capacity is 0x1000000 / 16,777,216 bytes / 16,384 KiB, matching PlatformIO and sdkconfig. Initial query failed because COM3 was busy; it wrote nothing. Successful query loaded a temporary RAM stub and reset the MCU; no flash write/upload occurred.

Pinned target SDK is installed `framework-espidf@3.50503.0`, version5.5.3, confirmed by build project description. Its partition generator validates app64KiB alignment, data sector alignment, overlap and configured capacity. `esp_ota_get_next_update_partition()` needs an app/OTA subtype; `esp_rewrite_ota_data()` needs data/OTA. Bootloader `bootloader_utility.c:bootloader_utility_get_selected_boot_partition()` defaults to factory when OTA metadata is blank and factory exists; without factory it can start ota_0. Metadata is two4KiB sectors (8KiB). Rollback remains disabled, not implemented or validated here.

## Old table

| Partition | Type/subtype | Offset: hex / bytes / KiB | Size: hex / bytes / KiB | End exclusive |
| --- | --- | --- | --- | --- |
| nvs | data/nvs | 0x9000 / 36,864 / 36 | 0x6000 / 24,576 / 24 | 0xF000 |
| phy_init | data/phy | 0xF000 / 61,440 / 60 | 0x1000 / 4,096 / 4 | 0x10000 |
| factory | app/factory | 0x10000 / 65,536 / 64 | 0x800000 / 8,388,608 / 8192 | 0x810000 |

## Final approved table

| Partition | Type/subtype | Offset: hex / bytes / KiB | Size: hex / bytes / KiB | End exclusive |
| --- | --- | --- | --- | --- |
| nvs | data/nvs | 0x9000 / 36,864 / 36 | 0x6000 / 24,576 / 24 | 0xF000 |
| phy_init | data/phy | 0xF000 / 61,440 / 60 | 0x1000 / 4,096 / 4 | 0x10000 |
| factory | app/factory | 0x10000 / 65,536 / 64 | 0x800000 / 8,388,608 / 8192 | 0x810000 |
| otadata | data/ota | 0x810000 / 8,454,144 / 8256 | 0x2000 / 8,192 / 8 | 0x812000 |
| ota_0 | app/ota_0 | 0x820000 / 8,519,680 / 8320 | 0x300000 / 3,145,728 / 3072 | 0xB20000 |
| ota_1 | app/ota_1 | 0xB20000 / 11,665,408 / 11392 | 0x300000 / 3,145,728 / 3072 | 0xE20000 |

Partition table remains at 0x8000 / 32,768 bytes / 32KiB. All old partitions retain their exact offsets, sizes and subtypes. Added app offsets are64KiB aligned; metadata offset/size are4KiB aligned. Gap after otadata is0xE000 / 57,344 bytes /56KiB. Final end0xE20000 /14,811,136 bytes /14,464KiB, leaving0x1E0000 /1,966,080 bytes /1,920KiB. No overlap, filesystem or SD partition. No material departure from the approved preflight proposal.

### Factory decision / boot model

Factory is **not required by ESP-IDF for future A/B OTA**. It is retained for this limited migration to keep the current app upload/boot offset at0x10000, preserve the existing factory app region, and avoid an additional application relocation/boot-selection migration. Two independent3MiB OTA slots fit without removing it. This is a migration compatibility decision, not a claim that the old layout mandates factory forever. Removing/shrinking it is outside this authorization and unnecessary to meet the approved capacity.

Initial USB migration writes the new firmware into factory and blank otadata; first boot still runs factory. Later OTA implementation may write ota_0, validate it and select it; subsequent updates alternate ota_0/ota_1. No OTA write, slot switch or rollback has happened in this task. Presence of these partitions alone does not make the current firmware perform OTA.

## USB upload behavior / credential safety

Inspected installed PlatformIO espressif32 builder `frameworks/espidf.py` (`get_partition_info`, `get_app_partition_offset`, `generate_empty_partition_image`, OTA extra-image registration), `builder/main.py` esptool uploader, and generated flash arguments. Ordinary USB **Upload** uses write_flash, including:

| Address | Image |
| --- | --- |
| 0x0 | bootloader.bin |
| 0x8000 | partitions.bin |
| 0x10000 | firmware.bin (factory) |
| 0x810000 | ota_data_initial.bin (8,192 bytes of0xFF) |

PlatformIO wrapper names differ from native CMake flash_args (`12_LVGL_Test.bin`, `partition_table/partition-table.bin`); their generated destination offsets agree. No ota_0/ota_1 app image is uploaded in this migration. Application-only write at0x10000 is insufficient: it would leave the old table in flash. No full-chip erase is required. write_flash necessarily erases only affected sectors before writing; NVS is outside those ranges. Do not use `pio run -t erase` or esptool erase_flash.

NVS remains data/nvs at0x9000 with0x6000 bytes; namespace remote_wifi/key credentials and existing fixed108-byte CRC-validated record are unchanged. Network runtime never erases NVS to repair initialization failure. PHY reserved partition remains unchanged; CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION is disabled, so current PHY initialization data is compiled rather than read from that region. Existing NVS calibration/storage policy is unchanged.

Expected Wi-Fi credential preservation follows unchanged storage geometry/format and upload writes excluding NVS, not proof of physical post-migration contents. Owner must validate reconnect without provisioning. Future normal PlatformIO uploads regenerate blank otadata and select factory again in this retained-factory layout; this must be documented when actual OTA is introduced, not mistaken for preserving a selected OTA boot slot.

## One owner migration procedure ? one firmware upload

1. Close COM3 monitor. Use **PlatformIO Upload** for environment remote01, or `pio run -e remote01 -t upload --upload-port COM3 -j 1`, using this verified source/artifact without edits. It may check build freshness; no separate rebuild/test images required. **Do not erase flash.** Confirm upload log shows the four addresses above and no NVS write.
2. Open `pio device monitor -p COM3 -b 115200`. Capture bootloader partition listing including otadata/ota_0/ota_1 and loaded app offset0x10000, followed by normal runtime boot. Stored credentials should produce CONNECTING -> CONNECTED automatically, with no provisioning AP/HTTP/DNS. Send `W`: capture stored1/storage_ready1/connected1/provisioning0, IP/RSSI and error/HTTP/DNS counters. No password disclosure.
3. Verify HOME/touch/buttons, both carousel directions/snapping, display orientation and brightness. Check display idle and touch/motion wake; confirm hold remains1 and automatic system sleep remainsINHIBITED. Check no-PWR USB->battery handover and continued local use; reconnecting USB should not itself be treated as MCU reboot. Do not invoke forced sleep/hard release commands or erase credentials.
4. Close monitor. Read back the installed partition sector once (read-only; causes a controlled USB reset) and decode it using pinned SDK:

```powershell
$stage2bPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
$stage2bEsptool = "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
$stage2bSdk = "$env:USERPROFILE\.platformio\packages\framework-espidf@3.50503.0"
$stage2bReadback = "$env:TEMP\remote01-owner-partitions.bin"
& $stage2bPython $stage2bEsptool --chip esp32s3 --port COM3 --before usb_reset --after hard_reset read_flash 0x8000 0x1000 $stage2bReadback
& $stage2bPython "$stage2bSdk\components\partition_table\gen_esp32part.py" $stage2bReadback
pio device monitor -p COM3 -b 115200
```

Compare decoded six rows exactly with final table above. Combined flash readback and bootloader listing/loaded factory address prove the new table is installed and used. Capture `W` again after reset: retained credentials, automatic reconnect, no provisioning.
5. **Development serial software-reboot test: NOT AVAILABLE in this layout-only firmware.** No such key exists in accepted baseline; adding one would exceed the explicit partition-only authorization. Step4 exercises the existing documented esptool USB reset/reconnect path, not ESP_RST_SW or a new serial key. Do not send an invented reboot key or claim software-reboot PASS.
6. Migration PASS requires verified installed table/boot, expected retained credentials/reconnect, working local UI and existing power behavior, with no new crash/watchdog/reset/freeze or reported allocation failure. Return secret-free boot/upload/readback/W and power observations. Unexpected provisioning, wrong table/boot address, lost credentials or local regression is FAIL; do not erase to conceal it. OTA transfer, boot switching, rollback and software-reboot command remain unimplemented/unvalidated and are not migration PASS claims.

## Final verification / resource audit

Full existing regression: **205 PASS /0 FAIL /373,221 assertions**, exit0. Full soak: **205 PASS /0 FAIL /3,381,471 assertions**, exit0, retaining Stage1/Core/HOME/PhaseD/network/captive/mapped coverage and95,000 combined stress cycles. No test changes or weakening. Commands: `python tests/control/run_tests.py` and `python tests/control/run_tests.py --soak`.

Clean target: `pio run -e remote01 -t clean -j 1`, **SUCCESS exit0,2.30s**; `pio run -e remote01 -j 1`, **SUCCESS exit0,521.33s**. PATH initially lacked pio; the first invocation never ran PlatformIO, then the installed penv Scripts directory was added and both actual commands succeeded. Zero compiler/CMake warnings/errors in completed tests/clean/build. Initial busy-port capacity query is also retained as a resolved audit failure, not hidden. Read-only `pio run -e remote01 -t envdump -j 1` subsequently confirmed actual upload images, factory address and esptool write_flash command; no upload target was executed.

Generated binary partition table parsed/SDK-verified and round-tripped byte-identically; all six name/offset/size entries matched approved table. Additional checks passed for capacity, alignment, no overlap, bootloader and app sector-write separation from NVS, both slot capacities and8,192-byte all-0xFF metadata. Partition binary is3,072 bytes (fits its4KiB sector), SHA-256 `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`.

| Metric | Final / delta |
| --- | --- |
| Full firmware BIN | 2,461,024 bytes /2,403.34375KiB; -320 B versus audit-entry BIN2,461,344 |
| Each OTA slot | 0x300000 /3,145,728 bytes /3,072KiB |
| Each OTA slot headroom | 684,704 bytes /668.65625KiB |
| BIN percentage of each OTA slot | 78.23384602864583% |
| Linked application usage | 2,460,521 bytes; unchanged from Stage2A recorded link usage |
| PlatformIO reported flash usage | 29.3% of retained factory8,388,608B; not the OTA-slot denominator |
| Static RAM | 45,796 bytes; delta0 |
| RTC allocation | Existing640B record unchanged |
| Tasks / runtime allocation policy | No additions or changes; heap/PSRAM runtime margin not physically remeasured |

BIN size and linked section usage are different measurements. Entry/final BIN identity differs; no source-code optimization or runtime memory change is inferred from the -320B BIN delta. OTA headroom is calculated from the full final upload BIN, not link usage. Source-input manifest comparison found only partition CSV and documentation edits; sdkconfig/PlatformIO/source/test inputs remain byte-identical.

Final firmware.bin SHA-256:
`ab63bda4dbd961c342e8974d5a6a077fa1e4fffd65193dad158676e18be0d302`

Final firmware.elf SHA-256:
`f85c605e361e7b9348a900388c759944ba4a1a4cdf5f4f2dc115cf0a4c8cec31`

No flash write, OTA write, slot switching, rollback validation or owner migration occurred. Only the read-only capacity query reset the board. Exact changed files: partitions.csv, this migration report, STAGE_2B_LOCAL_OTA.md (current status link; historical preflight preserved), and CHANGELOG.md.
