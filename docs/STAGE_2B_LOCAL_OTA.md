# Current canonical OTA product decisions - 2026-10-05

> **2026-11-01 superseding authorization:** the owner subsequently required an
> authenticated local-browser updater. The implemented candidate uses a
> short-lived PIN challenge and single-use session as documented in
> [REMOTE local Wi-Fi OTA](REMOTE_LOCAL_WIFI_OTA.md). The historical
> unauthenticated-LAN decision below is preserved as design history, not the
> current implementation policy.

[Owner register](REMOTE01_PRODUCT_DECISIONS.md), items24/35-38: OTA required; accepted16MiB factory/otadata/ota_0/ota_1 unchanged. Same-local-network OTA access without password authentication DECIDED, superseding older authentication-required/LAN-insufficient access language. This affects future OTA access only, not current read-only diagnostics, Wi-Fi provisioning credentials/security, or TUNER control authorization.

Before flashing/updating, firmware backup written to SD is a DECIDED requirement. Current SD backup/writer/transfer/trial/rollback implementation and acceptance are not claimed. No prior owner upload is retroactively represented as backed up. Image identity/compatibility/downgrade/interruption/trial-health/rollback/factory-slots-SD relationship DESIGN REQUIRED; no signature/trust mechanism selected from access policy alone. Current roadmap M4/M8/M5/M6/M7 reflects backup dependency, not optional update-source staging.

Earlier status/build/preflight/policy reports below are preserved historical evidence. Superseded OTA password/authentication gate is not current policy. POWER covered headroom closure/current owner acceptance is recorded in the canonical roadmap; prior608/800 pending notes remain historical. Automatic sleep remains inhibited pending trustworthy external-power hardware. No source/config/build/upload.

---

# Current Stage 2B diagnostics / serial reboot hardware status

2026-10-04. **OWNER HARDWARE VALIDATED / PASS; owner reports validation complete** for implemented read-onlySTA diagnostics and DEVELOPMENT_ONLY serialR reboot. Credentials persisted across upload/reboot, STA reconnected without provisioning,5s refresh was stable, software reset3 occurred, diagnostics recovered, RTC boot21->22 persisted, and no reported allocation/accounting/network-allocation/POWER/bus failures. See [complete owner evidence, retained observations and roadmap](STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md).

POWER minimum608B remains an open production-freeze headroom review. Two initial transient diagnostic errors have no established cause; post-reboot counter0 is a separate observation. Socket receive error during deliberate reboot interruption is retained as expected test observation, not a demonstrated defect. OTA transfer remainsNOT IMPLEMENTED; automatic deep sleep remainsINHIBITED/NOT VALIDATED. Documentation only; no build/upload or behavior change. Historical pre-owner reports below are superseded for current hardware status.

---

# Current Stage 2B diagnostics / serial reboot status

2026-10-04. Connected-STA read-only diagnostics and DEVELOPMENT_ONLY NETWORK-owned serialR reboot **IMPLEMENTED / FULL TEST-SOAK PASS / CLEAN BUILD VERIFIED / OWNER RETEST PENDING**. Physical partition acceptance remains intact; approved table/config/NVS/provisioning/hardware/power/CCM/Core semantics unchanged. OTA transfer remainsNOT IMPLEMENTED; automatic deep sleep remainsINHIBITED/NOT VALIDATED. No firmware upload. See [approved plan, amended scope, final artifact/resources and owner procedure](STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md). Historical preflight and design-only reports are preserved below.

---

# Current Stage 2B owner acceptance / next design status

2026-10-04. **Physical partition migration OWNER HARDWARE VALIDATED / ACCEPTED.** Retained factory boot, preserved NVS/Wi-Fi without full erase, automatic recovery toSTA, battery hold/UI and idle/motion/touch wake passed. No observed reset with boot19/uptime161->429s during USB-disconnected interval. Automatic deep sleep is **NOT TESTED / NOT ACCEPTED** and remainsINHIBITED because external power isUNKNOWN. See [physical evidence](STAGE_2B_OTA_PARTITION_MIGRATION.md).

Next connected-STA diagnostics and controlled serial reboot are **DESIGN ONLY / NOT IMPLEMENTED**, per owner stop-before-source-edit instruction. See [proposed architecture/endpoints/access control/resources/file scope](STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md). OTA updating remainsNOT IMPLEMENTED. No source/build/upload accompanies this review; historical preflight and build reports retained below.

---

# Historical pre-owner-migration status

# Current Stage 2B partition status

2026-10-04. Owner approved the preflight layout direction for **partition migration only**. Reverified physical16MiB flash; approved layout now **IMPLEMENTED / BUILD VERIFIED**, with both3MiB slots fitting final2,461,024B BIN. Actual OTA mechanism remains **NOT YET IMPLEMENTED**; hardware migration **OWNER TEST REQUIRED**. See [exact migration report and owner procedure](STAGE_2B_OTA_PARTITION_MIGRATION.md). Original preflight STOP below is historical, not the current layout status.

---

# Historical preflight STOP ? before owner partition authorization

# Stage 2B ? OTA partition preflight blocker

2026-10-04. **BLOCKED BEFORE IMPLEMENTATION / REPARTITIONING.** Owner request sections 1 and 17 require stopping if the current layout cannot safely support OTA. No OTA, LAN diagnostics service or development reboot command was implemented. No Stage 2B hardware validation claimed. The accepted Stage 2A baseline, including newly owner-validated reboot persistence, is preserved.

## Verified target and current artifact

`platformio.ini` pins espressif32 6.13.0, ESP-IDF package 3.50503.0 and custom `partitions.csv`, with 16 MiB flash. Existing `project_description.json` identifies `framework-espidf@3.50503.0` and git_revision 5.5.3; that exact installed package's version.txt confirms 5.5.3. Other installed IDF packages are not the target SDK.

The IDF 5.5.3 partition generator decoded and verified existing `.pio/build/remote01/partitions.bin` with:

| Partition | Type/subtype | Offset | Size | End (exclusive) |
| --- | --- | --- | ---: | --- |
| nvs | data/nvs | 0x9000 | 24,576 B | 0xF000 |
| phy_init | data/phy | 0xF000 | 4,096 B | 0x10000 |
| factory | app/factory | 0x10000 | 8,388,608 B | 0x810000 |

This matches source `partitions.csv`; partition table offset is 0x8000. **No otadata, ota_0 or ota_1 exists. Inactive OTA capacity is 0 B.** Current BIN length is **2,461,344 B**; inactive-slot shortfall is at least **2,461,344 B**, before future Stage 2B growth. Current factory headroom is 5,927,264 B, but factory slack is not an inactive app partition. Remaining unpartitioned flash is 8,323,072 B (0x7F0000), also not currently a usable OTA slot. Previously recorded Stage 2A flash link usage is 2,460,521 B and static RAM 45,796 B; link usage differs from the complete BIN length.

Current on-disk BIN SHA-256:
`99de28bbc1607bd1ad0f967d579e58f454a3a271ca0c504666de9e9dacc5ac8b`

Current on-disk ELF SHA-256:
`17fa6f213ac108d8e4aefb88d470bbd7a040e439ce4cd3fbfadf02496913e155`

These are audit-entry artifacts, untouched by this task, not independently verified owner-flashed identities. Prior documented corrected Stage 2A BIN hash `d8398089b56133cd382c12d888c7a29bd7b4f3386559ebd101f2900f0b5e7b14` remains historical; no rebuild was used to replace it.

## Exact SDK blocker

Inspected installed IDF 5.5.3 `components/app_update/esp_ota_ops.c`:

- `esp_ota_get_next_update_partition()` searches APP_OTA subtypes; with this table it returns NULL. It does not turn factory slack/unpartitioned flash into a slot.
- `esp_ota_set_boot_partition()` verifies an image, then the OTA selection path uses `esp_rewrite_ota_data()`. Missing data/OTA partition produces ESP_ERR_NOT_FOUND. The source describes two sectors for OTA selection records.
- Current `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is disabled; no rollback protection is claimed. Secure boot and flash encryption are also disabled. No security configuration was changed.

Thus software endpoint implementation cannot meet inactive-slot write/commit requirements with the verified current partition layout. The stop condition is reached before the remaining implementation/security/key-allocation audit; no unused serial key or authentication design is represented as approved or implemented.

## Proposed layout for owner review ? not applied

One possible layout preserves the existing NVS, PHY and entire factory region, adding OTA metadata and two 3 MiB slots in currently unpartitioned flash:

| Proposed addition | Type/subtype | Offset | Size | End (exclusive) |
| --- | --- | --- | ---: | --- |
| otadata | data/ota | 0x810000 | 8,192 B | 0x812000 |
| ota_0 | app/ota_0 | 0x820000 | 3,145,728 B | 0xB20000 |
| ota_1 | app/ota_1 | 0xB20000 | 3,145,728 B | 0xE20000 |

Both app offsets are 64 KiB aligned; regions do not overlap and end below 16 MiB (0x1000000). Each proposed slot has **684,384 B** headroom over the current BIN; actual Stage 2B image growth must be checked before accepting that capacity. Alignment gap is 57,344 B; remaining flash after the second slot is 1,966,080 B. This is a size/address proposal only, not a validated migration or rollback design.

If approved later, migration needs one deliberate USB partition-table/firmware update with NVS and factory retention explicitly verified, no full-chip erase, and a defined initial otadata state. Existing factory-only firmware cannot install this new table through the proposed app-only OTA API. Do not silently repartition or flash anything. OTA/authentication, LAN diagnostics, bounded log view, reboot-key uniqueness and one consolidated hardware procedure will be designed only after this prerequisite is resolved.

## Verification / scope

Read-only configuration, built partition decoding/verification and pinned SDK inspection completed. No executable source, configuration, partition CSV, firmware or generated artifact changed. No new task, RAM/PSRAM allocation or resource delta. No regression, soak, clean build or upload performed: the mandated early-stop branch was taken before implementation. Existing Stage 2A regression/soak/build evidence remains historical, not rerun or presented as Stage 2B verification.

Documentation changes only: this preflight report, Stage 2A reboot-persistence evidence/status and CHANGELOG. No TUNER integration, MQTT, cloud, HOME redesign, power redesign or charging detection. Stage 2B is not implemented or owner validated; no Stage 2B owner upload/retest is requested while blocked.
