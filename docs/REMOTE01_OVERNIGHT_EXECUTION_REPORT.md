# REMOTE//01 overnight execution — 2026-10-04

**IMPLEMENTED / HOST TESTED / CLEAN BUILD VERIFIED; OWNER HARDWARE VALIDATION PENDING. No upload. No physical acceptance claimed.**

## Entry freeze and reconciliation

Entry inventory:1,644 non-build files, SHA-256 retained in TEMP/remote01-overnight/entry.json. Git metadata absent (`git status` fails); no Git diff/history claim. Relevant source, test runner, stage records and roadmap re-inspected before source changes. No AGENTS.md found in the checkout or its parent chain. Root configuration/component exclusions and previous audit remain the evidence baseline.

| Entry artifact/resource | Verified value |
| --- | --- |
| BIN |2,478,352 bytes; `b34ad9a8f319532aa7ad898b54d7cd70c08f944452e0090e0c6504a4da6dbd67` |
| ELF |17,693,016 bytes; `ee6d6e57ac2aa595674e5281ddfb3897c4b0836e88e80b883da1886e7572fff1` |
| Partition BIN |3,072 bytes; `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835` |
| Linked flash/static DRAM |2,477,849 /46,780 bytes |
| OTA capacity/headroom |3,145,728 /667,376 bytes (651.734375 KiB) |
| POWER/NETWORK stacks |4,096 /6,144 bytes, unchanged configured sizes |

Entry artifacts and task objects were copied to TEMP before clean. Existing Stage2B OWNER HARDWARE PASS is preserved; latest Stage2B.2 measurement firmware has no new owner physical results. Stage1 remains open for missing mandatory acceptance evidence.608/800-byte POWER observations are not diagnosed failures.

### Backlog discrepancies and corrections

The roadmap already listed the principal product branches, but its M2 specified factory Screen0 only and missed the explicitly requested **HOME local IP**. Actual factory page0 is not HOME page2. Both are now tracked and implemented as bounded address presentation. HOME source already uses authoritative POWER `battery_view()` for rounded voltage/SIM/invalid; factory's generated Voltage is still a static demo. Neither establishes SOC or independent voltage accuracy. The report does not upgrade that factory value into telemetry.

Roadmap reconciliation now explicitly distinguishes Wi-Fi-only reset from general factory reset; lists backend versus final UI dependencies; records OTA integrity versus authenticity, disabled rollback, and verified-source blocking for real TUNER. Battery endurance/accuracy/LOW-CRITICAL recovery, POWER physical measurement, encoder/push/three buttons, SD use/hardware, production UI, final acceptance and release remain separately tracked. Historical stage documents were not rewritten.

## Completed software slices

### 1. Screen0 and HOME local STA IP

`NETWORK::read_status` copies only existing secret-free Status under the existing mutex with **zero wait**. LVGL's existing100ms HOME timer formats this copy, never querying Wi-Fi/HTTP or holding owner pointers. A connected canonical unicast IPv4 is shown unchanged; bounded fallback is SETUP, CONNECTING, OFFLINE, NO IP or --. Unterminated/malformed/multicast/loopback/zero-leading addresses are refused. No SSID/password/token copied to labels.

Factory page0 reuses its blank label at x5/y70/width162. HOME reuses the footer at x12/y618/width124, replacing the repeated TUNER//01 footer identity; identity/artwork labels and Wi-Fi symbol remain. No carousel element/geometry, controls, battery display, rotation, touch mapping or backlight changes. Font is already-enabled Montserrat12, line height15; labels clip without wrapping. Actual font-data conservative advance bound is107px for223.255.255.255 and99px for CONNECTING, within124px. HOME footer ends at y633 and x136, leaving the right symbol separate. Factory row ends y85, before the next row at104. Physical readability remains pending.

NETWORK republishes existing copied status at its diagnostic cadence (at most normal1s interval), including IP changes discovered by its existing health check. Failed health checks clear the copied IP/RSSI instead of retaining a stale address. This is a source-supported stale-presentation correction, not attribution of any owner HTTP error or network failure. State ownership/connect/retry/security policy is unchanged. UI lock contention displays -- rather than waiting.

### 2. Local-only Network Settings/recovery backend

New `settings_request(action,generation,confirmation)` enqueues a bounded message through the existing NETWORK inbox. No HTTP route, new serial key or final UI is added. NETWORK alone performs transitions/NVS operations. `read_settings` is a zero-wait copied result/ticket/deadline accessor.

- OPEN_SETUP re-enters the existing secure portal without erasing stored credentials; old driver messages are generation-fenced. Repeated open in existing idle setup is idempotent.
- Existing submit validation/security and save-only-after-healthy-DHCP path handle replacement. Failed candidate leaves the old NVS blob intact.
- CANCEL_SETUP reloads the stored blob through NETWORK and resumes STA. With no valid saved credentials or no active setup it refuses, retaining a setup path rather than inventing credentials.
- ARM_WIFI_RESET publishes a bounded, one-use confirmation ticket with the existing10s convention; CONFIRM_WIFI_RESET requires matching generation/ticket and an unexpired window. Only the existing remote_wifi/credentials key is erased/committed. Failure is reported; unrelated NVS is untouched.
- FACTORY_RESET and unknown actions are refused. No erase-domain/confirmation contract has been selected for general reset.
- Owner processing refuses settings while unready or reboot/power action is pending. Service/storage failures have distinct results; logs contain result/generation, no credentials. A ticket is confirmation state for trusted local firmware, **not network authentication**.

Existing G/Y behavior, R scheduling, captive HTTP/DNS/Host/Origin/CSRF checks, credentials schema and partition geometry remain intact. Existing saved-credential connection failures still retry; setup re-entry is deliberate, not automatic credential deletion. Backend has no user-facing wizard yet. Host model and source-boundary tests do not validate actual SDK/NVS/HTTP lifecycle on a board. Later approved local UI/harness is needed to exercise these new methods physically; existing G/Y is not a substitute for that validation.

### 3. Offline OTA integrity preflight

`tools/ota_preflight.py` reads the actual partition CSV and conservatively uses the smaller positive ota0/ota1 capacity. It checks current unsigned IDF app-profile bounds,24-byte image header,1–16 segments, ESP32-S3 chipID9, bounded/aligned segment lengths, first app-descriptor magic0xABCD5432, SDK XOR checksum and appended simple SHA-256; rejects truncation, mismatch and unsupported trailing/signature profiles. Reads are at most1,024 bytes, no whole-image buffering.

Constants/layout/checksum padding were inspected in the **installed pinned IDF5.5.3** `components/bootloader_support/include/esp_app_format.h`, `src/esp_image_format.c`, `components/esp_app_format/include/esp_app_desc.h`, and installed esptool `bin_image.py`. The helper is host-only, not in firmware. It is not the complete SDK verifier: load addresses, chip revisions/eFuses, project compatibility, signature/authenticity, replay/downgrade and boot health are not certified. Output explicitly has `activation_authorized=false`, authenticity UNCONFIRMED and compatibility PARTIAL_CHIP_ONLY. A recomputed unsigned hash does not authenticate an image.

This closes a decision-independent artifact-check gap only. No inactive-slot writer, ESP OTA calls, update endpoint, trial boot, rollback or transfer was implemented. Preflight PASS is neither OTA security nor physical OTA PASS.

## Partially completed and blocked branches

| Branch | Status / evidence needed |
| --- | --- |
| Network Settings | Backend implemented; final UI OWNER DECISION REQUIRED; SDK lifecycle/credential replacement/reset OWNER HARDWARE VALIDATION PENDING. |
| OTA | Offline preflight tested; update initiator authentication, image trust/signing/key provisioning, anti-replay/downgrade/version compatibility and safe recovery decisions missing. Rollback config remains disabled. Writer/activation/transfer blocked; no writable service exposed. |
| Battery/Stage1 | Existing authoritative ADC/calibration/filter/validity/hysteresis/UI already present; no evidenced reason to change policy/thresholds. Independent physical voltage, specified endurance/wake/gesture/simulation tests and artifact evidence still required.1000mAh metadata unchanged; external/charging UNKNOWN, SOC unavailable. |
| POWER/Stage2B.2 | Await full owner S/T path results per existing procedure. No speculative stack increase, heap relocation or publication restructuring. |
| Encoder/push/three buttons | OWNER HARDWARE INPUT REQUIRED: exact wiring/levels and intended actions absent. No GPIO changes; generic action/debounce semantics not invented. |
| SD | OWNER DECISION/HARDWARE VALIDATION REQUIRED: dormant interface references are not working integration or an approved use. No card activation, filesystem/staging change. |
| Production UI | OWNER DECISION REQUIRED; no final design approved. Existing accepted HOME actions/geometry preserved. |
| Real TUNER/radio | BLOCKED BY VERIFIED SOURCE/PROTOCOL AVAILABILITY. Unapproved Native API candidate alone is insufficient; no target source/server established locally. No Mock-to-real substitution or yoRadio assumptions. |
| Full factory reset | OWNER DECISION REQUIRED: erase-domain, explicit confirmation, accessible recovery and preservation policy. Refusal boundary only. |

No remaining independently justified runtime work was identified without crossing these evidence/decision gates. Display idle remains the safe normal power policy; automatic system sleep and critical hold release remain inhibited for UNKNOWN external power. Inaccessible PWR is never a required acceptance/recovery action.

### End-of-session adversarial backlog review

All requested areas were checked for tracking, not marked implemented by checklist: Screen0/HOME IP; Network Settings; wizard/recovery; Wi-Fi credential reset; distinct general/factory reset; battery completion; independent voltage accuracy; battery endurance/overnight runtime; encoder/push; all three buttons; SD; OTA security; validator/writer; rollback; transfer; production UI; real radio/TUNER; final hardware acceptance; release readiness. Encoder/button debounce, optional short/long-press semantics, touch coexistence and wake mapping still require owner wiring/action evidence. No generic unconnected abstraction was added merely to claim progress. Overnight battery-runtime duration/recovery needs owner-approved physical procedure; no capacity-based runtime estimate.

Adversarial source review: no new web mutation/OTA route or credential logging; existing credential schema/security source unchanged; factory reset denied; tickets are not authentication; incomplete/corrupt images rejected by the host checker but cannot activate anything; old Mock remains Mock; no automatic sleep or GPIO changes. Protected-file hashes and unchanged partition BIN were checked. Owner evidence is neither inferred from tests nor erased by new software status.

## Tests, failures and build evidence

Focused network/presentation/recovery:17 PASS,0 FAIL,35,386 assertions; focused network soak:17/0/350,386. Offline preflight:8/0/685, including every truncation and every single-byte corruption of its336-byte synthetic fixture, bounds, malformed lengths/trailers and bounded reads. Existing entry BIN passes preflight without activation authorization.

A direct ad-hoc g++ invocation lacked the MSYS runtime PATH and did not produce the requested executable; the established runner supplies that PATH, and corrected focused invocation passes. One normal and one soak wrapper reported PowerShell NativeCommandError from successful unittest progress on stderr; all suite result lines were PASS. The additive Python suite now writes stdout and final runs capture explicit native exit codes. These are retained verification-runner issues, not hidden firmware/test failures. No failing test weakened or removed.

Final settled complete normal: **242 PASS /0 FAIL /408,133 assertions**, native exit0. Complete soak: **242 PASS /0 FAIL /3,722,383 assertions**, native exit0. The original229 cases are retained; additions are4 network cases,1 source-boundary audit and8 offline cases. Source-boundary audit checks actual SETTINGS enqueue/factory refusal, zero-wait status copying, and absence of settings mutation calls from both HTTP services. Logs are retained as TEMP/remote01-overnight/normal-settled.log and soak-settled.log. Focused and earlier run logs are retained as well.

Clean `pio run -e remote01 -t clean -j 1` succeeded (2.23s), followed by `pio run -e remote01 -j 1` (557.88s), native exit0. Installed `C:/Users/RYZEN/.platformio/penv/Scripts/platformio.exe` used. Zero compiler/CMake warning/error lines in both final logs. No incremental build substituted for final evidence. No test proves physical stack safety, Wi-Fi/IP lifecycle, UI readability, actual NVS mutation or OTA behavior on hardware.

### Target resource measurements

Target object disassembly: NETWORK task entry frame2,080→2,176 bytes (+96), HOME refresh144→304 (+160), POWER1,248→1,248 (zero). Task stack allocations/priorities/core affinities are unchanged. Frames are not nested peak or hardware watermark evidence. Compiled target sizeof: Status128, Message120, Inbox976 (entry908, +68), SettingsView32, owner-local Recovery32 bytes. Existing WebView868 and LocalSetup52 unchanged. New HOME IP pointer is4 bytes; complete aligned HOME storage64→72 bytes (+8). No added LVGL object, timer, task, socket or queue. No explicit new firmware heap/PSRAM allocation; LVGL label text uses its existing bounded allocation mechanism and may resize for the15-character IP. HTTP limits and diagnostic arena remain unchanged. Offline tooling has no linked firmware cost.

| Final artifact/resource | Value / delta from entry |
| --- | --- |
| BIN |2,480,496 bytes; **+2,144** |
| BIN SHA-256 |`b3356538890ec91249c4d80cb55bf765e36c408ba2c65f5574aded030d005b40` |
| ELF |17,722,712 bytes; +29,696 (includes debug data) |
| ELF SHA-256 |`aa8d4c0c49be04887b5dc9bf4bec7ef2fd0a4e05d9b2cb9605d78aad96fcabb6` |
| Partition BIN |3,072 bytes; unchanged `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835` |
| Linked application flash |2,479,993 bytes; **+2,144** |
| Static internal DRAM |46,908 bytes; **+128** (`.dram0.data`24,540 + `.dram0.bss`22,368); aggregate includes layout/alignment overhead beyond individual object sizes |
| OTA slot / exact BIN headroom |3,145,728 / **665,232 bytes =649.640625 KiB**; headroom -2,144 |
| RTC history / IRAM |RTC noinit640, IRAM vectors1,028/text133,719, unchanged |

Final BIN passes the offline preflight (7 segments), with no authentication/activation claim. Dynamic heap, live socket usage and physical watermarks on this new image are UNKNOWN until owner retest. Existing PSRAM framebuffer/diagnostic/Core allocations and socket/task limits are unchanged by source; future load safety is not inferred from static figures.

## Owner validation procedure and next actions

No upload was performed. After owner approval/manual flash of the final hashes:

1. USB cold-start without PWR; confirm preserved Wi-Fi credentials reconnect, new settings are not automatically invoked and serial W address matches **factory page0 and HOME page2**. Open that address in the existing browser diagnostics. Confirm footer/row readability, no overlap or wrapping.
2. Exercise existing HOME playback, both carousel directions/snapping, brightness and battery voltage/SIM/invalid presentation; idle then motion/touch wake. Compare battery with independent voltage measurement using the existing safe Stage1 procedure.
3. Safely interrupt/restore the router connection: IP must cease being shown once NETWORK reports unavailable/no IP and update after reconnection or changed DHCP assignment. Confirm existing portal card/security on an already approved provisioning test; do not erase credentials merely to test a label.
4. Existing serial R should defer/reboot with software reset, preserve credentials and recover diagnostics/IP display. Capture W/B/m telemetry and existing S reports during5s browser polling; Stage2B.2 acceptance remains its separate physical procedure.
5. Do not use K/!, require PWR, force sleep, perform full erase or attempt OTA. New recovery backend requires a later approved local UI/harness and deliberate owner credential-reset test; it is not reachable from a new browser endpoint.

Next owner decisions in dependency order: POWER/Stage1/2A evidence; approve Network Settings/reset interaction; select OTA trust/compatibility/recovery policy; supply physical-control/SD/audio evidence and playback location; approve real target contract and final UI. Next software: validated local Settings UI/harness, only evidence-justified POWER correction, approved OTA validator/coordinator then boot-trial recovery then authenticated transfer; input/SD/real-adapter branches only after their prerequisites.

## Change scope and final stable checkpoint

Final SHA-256 inventory compared against entry; only the following17 files differ (11 modified,6 added,0 deleted). Protected/frozen files, historical stage documents, CCM/reviews, all `control`/`control_home`/`control_profile`, POWER/RTC history, main/display/touch/GPIO, vendored fonts/drivers, platform/sdk configuration, partition CSV, provisioning `portal.cpp`/`web.cpp`/`dns.cpp` and diagnostics are unchanged. NETWORK's existing credential Blob/schema is unchanged; runtime is extended for local recovery and IP publication. No physical NVS operation occurred, because no firmware was uploaded.

Modified:

- CHANGELOG.md
- docs/REMOTE01_COMPLETION_ROADMAP.md
- components/network_manager/CMakeLists.txt
- components/network_manager/include/network_manager/api.hpp
- components/network_manager/include/network_manager/model.hpp
- components/network_manager/model.cpp
- components/network_manager/runtime.cpp
- components/user_app/CMakeLists.txt
- components/user_app/user_app.cpp
- tests/control/network_tests.cpp
- tests/control/run_tests.py

Added:

- docs/REMOTE01_OVERNIGHT_EXECUTION_REPORT.md
- components/network_manager/include/network_manager/presentation.hpp
- components/network_manager/include/network_manager/recovery.hpp
- components/network_manager/presentation.cpp
- tests/control/ota_preflight_tests.py
- tools/ota_preflight.py

No temporary objects/logs/binaries retained in product source; Python bytecode output disabled in the additive test and the initial generated cache removed. Evidence is in TEMP/remote01-overnight (entry/final inventories, accepted entry artifacts, focused/normal/soak/clean/build logs, sizes and disassembly). Current stable checkpoint is the clean-built software foundation above, not a new owner-accepted release. Stop here pending the listed physical evidence and product/security decisions.


## 2026-10-05 - Overnight work / Today's Step 1 owner closure

**2026-10-05 - Overnight work / Today's Step 1: OWNER HARDWARE VALIDATION PASS. CLOSED / PASS.** Today's Step1 means "Verify the overnight Codex work on real hardware/software"; it is distinct from the broader historical Stage1 acceptance ledger. This dated owner decision supersedes earlier pending entries for the tested scope only; previous evidence remains intact.

### Final owner regression - current instrumented firmware

| Check | Owner hardware result |
| --- | --- |
| HOME/IP |PASS|
| Carousel0 ->1 ->2 ->1 ->0 |PASS|
| Touch/controls |PASS|
| Display idle |PASS|
| Touch-to-wake |PASS|
| Lift-to-wake |PASS|
| Browser diagnostics |PASS|

```text
NETWORK W:
state=CONNECTED provisioning=0 connected=1 stored=1 storage_ready=1
ip=192.168.1.83 reconnects=0 reason=0 errors=0 allocation_errors=0
stack_min_bytes=1692

POWER B:
IMU=1 motions=220 bus_errors=0 errors=0 wakes=20
sleep_requests=0 rounds=0 stack_min_bytes=1256
manager_wake_to_ready_ms=240
battery source=REAL calibrated=1 valid=1 condition=NORMAL
```

G/Y, K/!, Z were not used. Lowest observed covered POWER minimum remains1256 B, above the provisional1024 B review target; the target is not a safety guarantee. No automatic/deep-system-sleep acceptance follows from zero requests/rounds. http_stack_min_bytes=0 or DIAGNOSTICS stack_min_bytes=0 in a single W snapshot is not classified as a defect: browser diagnostics was physically verified PASS, and that snapshot alone does not establish a fault.

### Exact scope/status boundaries

- Today's Step1 / overnight-work verification: **CLOSED / OWNER HARDWARE VALIDATION PASS** for the owner-reported checks.
- Stage2B.2 publication-frame correction: **OWNER HARDWARE VALIDATION PASS for measured/covered paths**; closed at its previously documented scope.
- S/B -> WoM interference hypothesis: **DISPROVEN on current hardware/firmware**.
- Current WoM/lift-to-wake: **PASS in the current owner test**.
- Previous IMU_WOM=0 incident: **REAL HISTORICAL OBSERVATION / CURRENTLY NOT REPRODUCED / ROOT CAUSE UNKNOWN**. No WoM functional fix applied.
- Broader historical Stage1 acceptance ledger: **OPEN** for previously documented mandatory evidence gaps. Today's20 reported wakes and working controls are recorded, but do not invent unreported gesture-isolation details, voltage comparison, source-specific durations, battery simulations or endurance. No historical gate is silently waived.
- Network Settings backend: **HARDWARE VALIDATION BLOCKED BY DESIGN / MISSING USER-ACCESSIBLE UI OR HARNESS**. This is **not a FAIL** or demonstrated defect. OPEN_SETUP/CANCEL/confirmed Wi-Fi reset are **not claimed physically owner-tested**. HOME/IP/browser diagnostics PASS does not validate those mutation/recovery operations.

Documentation/status closure only: no implementation, tests, configuration, artifacts, stack/publication correction or WoM behavior changed; no build/upload. No new feature or milestone implemented.


## 2026-10-05 - canonical owner product reconciliation / supersession

Authority: [REMOTE01_PRODUCT_DECISIONS.md](REMOTE01_PRODUCT_DECISIONS.md), all38 owner items, and its corrected [current roadmap](REMOTE01_COMPLETION_ROADMAP.md). The original report above remains historical evidence of the session's available information. This note does not rewrite it as if the new decisions were known then.

- DECIDED: separate REMOTE/TUNER; TUNER owns internet-radio streaming/decoding/playback/output. REMOTE is TUNER-primary with protocol-neutral boundaries, Home Assistant/openHAB optional compatibility; Matter/Smart-TV/IR historical/intended below native priority; Zigbee EXCLUDED. No live REMOTE local-radio branch.
- DECIDED: mDNS discovery with optional manual IP/hostname; actual TUNER protocol UNKNOWN / NOT RECOVERED and transport DESIGN REQUIRED. Native API candidate remains unapproved/unfrozen; naming mDNS does not select WebSocket/HTTPS or prove interoperability.
- DECIDED: SD firmware backup before flashing/updating. Earlier SD-purpose OWNER DECISION REQUIRED/UNKNOWN classifications and local/external selection gates are SUPERSEDED. SD staging/SD OTA remain historical suggestions, not canonical purpose. Backup implementation/media verification still required; no earlier upload is retroactively claimed to have made a backup.
- DECIDED: favorites/recent stations persist on TUNER; REMOTE reads/presents/selects, TUNER plays. REMOTE persists Wi-Fi/UI settings/last associated TUNER/artwork cache, not station database/favorites. Reset/survival policy DESIGN REQUIRED. Frozen logical favorite domains/Core durable command history do not select product station persistence.
- DECIDED: REMOTE artwork lookup/download/cache/thumbnail bound to active URL/media, late obsolete results discarded. Sources/formats/dimensions/bytes/timeouts/redirects/errors/decode/cache quota/lifetime/eviction/fallback DESIGN REQUIRED.
- DECIDED: encoder+three buttons. Electrical capture IMPLEMENTATION + HARDWARE VALIDATION REQUIRED; action/focus/press/control-wake mapping DESIGN REQUIRED. Earlier deliberately undecided/no approved action claims do not establish that prior approval never existed; owner now requires explicit design/freeze.
- DECIDED: owner external5V divider to GPIO/ADC direction. Physically verify source pad/voltage; select GPIO through resource review and resistor values after verification. External PRESENT/ABSENT only, not charging. Automatic system sleep remains inhibited until trustworthy state; no new signal implemented.
- DECIDED: OTA required, accepted16MiB partitions preserved; same-local-network access without password authentication and SD backup before flashing/updating. Earlier authentication-required/LAN-insufficient OTA access requirement SUPERSEDED. Image identity/compatibility/downgrade/interruption/trial/health/rollback/factory-slots-backup relationships DESIGN REQUIRED. Wi-Fi provisioning and TUNER authorization unchanged.
- Network Settings/current IP/reprovisioning and separate Wi-Fi versus factory reset DECIDED; complete production/reset/retention flows DESIGN REQUIRED. Temporary Mock UI replaced by approved production UI later. Backend physical validation remains blocked by missing UI/harness, not FAIL; no OPEN_SETUP/CANCEL/confirmed Wi-Fi reset hardware claim.

Today's Step1 CLOSED / OWNER HARDWARE VALIDATION PASS and Stage2B.2 covered-path closure retained; broader historical Stage1 remains OPEN. No firmware/test/config/artifact changes, build or upload. No new feature/hardware acceptance implied.

### Exact historical statements superseded (original text retained above)

- Partially completed and blocked branches: "SD ... OWNER DECISION/HARDWARE VALIDATION REQUIRED ... an approved use" -> decided backup purpose plus separate implementation/hardware gate.
- Owner validation procedure and next actions: "supply ... audio evidence and playback location" -> no REMOTE audio bring-up/playback-location decision prerequisite; target contract remains unverified.
