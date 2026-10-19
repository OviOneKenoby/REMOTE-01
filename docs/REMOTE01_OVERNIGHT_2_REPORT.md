# REMOTE//01 overnight #2 execution report

Completed 2026-10-05. **AUDIT COMPLETE / HARDWARE-NEUTRAL CONTROLS HOST TESTED / CLEAN BUILD VERIFIED. NO UPLOAD OR NEW HARDWARE PASS.** Owner-unavailable session continued from the exact previous checkpoint. No Git metadata is present; file inventories provide scope evidence, not a commit/worktree claim.

## Entry freeze and protected baseline

All1,650 non-`.pio`/`.git` files matched the previous overnight final inventory before editing. Entry inventory SHA-256: `d179930776c0bbf9273a598b2dea56124012bab520ee7c9bf515c4e53a8048a2`. Evidence/logs and retained entry artifacts are in `%TEMP%/remote01-overnight2`; previous checkpoint is [the original report](REMOTE01_OVERNIGHT_EXECUTION_REPORT.md).

| Artifact/resource | Exact entry |
| --- | --- |
| BIN | 2,480,496 B; `b3356538890ec91249c4d80cb55bf765e36c408ba2c65f5574aded030d005b40` |
| ELF | 17,722,712 B; `aa8d4c0c49be04887b5dc9bf4bec7ef2fd0a4e05d9b2cb9605d78aad96fcabb6` |
| Partition BIN | 3,072 B; `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835` |
| Linked flash / static DRAM | 2,479,993 / 46,908 B |
| OTA slot / entry headroom | 3,145,728 / 665,232 B |
| Normal / soak baseline | 242 PASS/0 FAIL/408133 assertions;242/0/3722383 |

Frozen: all existing firmware source, GPIO/drivers, LVGL/assets/UI/carousel, display rotation/touch mapping, power policy/stack sizes/RTC records, network/provisioning/security/serial dispatch, NVS format, partitions, dependencies/framework configuration, frozen CCM and historical evidence. Previous Screen0/HOME IP, local recovery backend and offline OTA checker are preserved. Stage2B owner hardware PASS remains accepted; previous overnight additions still await owner hardware evidence. Stage1 pending gates and Stage2B.2 physical POWER measurement remain pending.

## Hardware resources and conflicts

[Hardware map](REMOTE01_HARDWARE_RESOURCE_MAP.md) records source/owner evidence separately for GPIO0-48, invalid GPIO22-25, expander bits0-7, buses, IRQs, DMA, memory and USB/console implications. Active LCD SPI3 9-14, touch I2C1 17/18, board I2C0 47/48, backlight42, shared IRQ8, batteryADC4 and SYS_EN EXIO6 are preserved. USB19/20 and MSPI resources are reserved. SD/audio/factory button macros are dormant references, not spare-pin authority. Candidate1/2/3/5 connectivity is UNKNOWN.

Explicit matrix separates VERIFIED USED, RESERVED, DORMANT, POSSIBLY AVAILABLE and UNKNOWN / MUST VERIFY. Alternate codec-board profiles collide with LCD/touch/POWER/USB/memory references if copied; SD39-41 overlaps alternate external JTAG, not current native USB JTAG. No active hardware conflict or new pin assignment was inferred. Actual expansion routing, control wiring, codec/amp population, card detect/power and physical allocation remain owner gates.

## Controls findings and implementation

[Controls plan](REMOTE01_PHYSICAL_CONTROLS_PLAN.md) separates requirements, current capabilities, unknown hardware, owner decisions and safe software. Encoder plus three physical buttons are verified owner requirements; push is unconfirmed. Dormant two-button BOOT/PWR debounce and LVGL examples do not implement these controls. Existing facade/Core serialized ownership supports neutral input before separately approved action mapping.

Added standalone `include/physical_input/model.hpp`, **not included by firmware**. One serialized task supplies normalized samples and nonzero debounce interval. Three generic IDs emit PRESS/RELEASE after sampled stability; Gray AB transitions emit signed complete logical cycles. Physical direction/detents/polarity/sampling interval and product actions are deliberately undecided. Eight-event FIFO has timestamps/epoch/sequence, deterministic ties, bounds checking, explicit loss/discard and baseline recovery. Invalid two-bit encoder jumps are counted, never reconstructed. No held-at-baseline press/replay. Counters refuse wrap or saturate visibly. Model uses fixed arrays, no heap/task/ISR/callback/transport/Core/UI calls. Caller identity is ownership discipline, not synchronization; never invoke model from ISR.

Host size400 B; standalone Xtensa target compile proves392 B Model/32 B Event, with strict warnings and no exceptions/RTTI. Probe object is TEMP-only, not linked into firmware. No actual GPIO capture shim, debounce calibration, hardware wake handling or mapper is implemented. Host traces cannot prove detents or physical clockwise direction, missed short pulses or electrical correctness.

## SD, audio and radio

[SD audit](REMOTE01_SD_AUDIT.md): no active mount/service. Dormant one-bit SDMMC expects39/40/41; card-detect38 unused; FATFS `/sdcard`, format-on-failure false. Source hazards include format-string write and capacity/short-read/lifecycle checks. These are excluded-code hazards, not active defects. **SD PRODUCT USE = OWNER DECISION REQUIRED.** No abstraction added: no approved purpose/caller/ownership/quota/removal contract. No formatting/media operation or activation.

[Audio audit](REMOTE01_AUDIO_RADIO_AUDIT.md): excluded PCM/I2S examples and S3 codec profile do not provide local streaming/decoding/output. Profile ES8311/ES7210 and6/7/15/45/46 references are not owner-confirmed audio wiring. esp_codec_dev declaration is absent from current active build/source set. No application MP3/AAC/AAC+, stream acquisition, station management, Radio Browser or real target transport. SDK HTTP/I2S libraries alone do not establish these features. Expander probe is not a blanket reset; dependency address encoding and ownership need review before activation.

[Radio architecture decision](REMOTE01_RADIO_ARCHITECTURE_DECISION.md) compares documented external control, dormant local playback possibility and documented future automation directions across reuse, network/security, CCM, CPU/RAM, audio/UI, OTA/recovery and testing. No final architecture selected; no yoRadio assumptions. **REAL TUNER SOURCE NOT AVAILABLE IN CURRENT WORKSPACE.** Frozen CCM1.0.0 is unchanged; Native API candidate1 is UNAPPROVED/UNFROZEN, documented only. Mock/Core host tests and accepted HOME/Mock hardware do not prove protocol interoperability or audio playback.

## UI and backlog reconciliation

No production design implemented. The architecture document includes a bounded classified UI inventory; [roadmap](REMOTE01_COMPLETION_ROADMAP.md) includes every requested adversarial completion item. Screen0/HOME authoritative IP remains software-complete, owner readability pending. Network Settings/wizard needs approved UI; Wi-Fi reset backend is separate from refused general reset. Battery accuracy/endurance/LOW/CRITICAL/recovery acceptance remains open;1000mAh is not SOC or threshold evidence. External-power/charging UNKNOWN; automatic sleep stays inhibited.608/800-byte POWER observations await per-path measurement, not speculation.

Current additional gaps: static factory producers/demo telemetry, obsolete/inaccessible factory instructions, static Wi-Fi glyph, read-only volume/artwork and inert navigation rows, final error/recovery/service feedback. Startup demo's LVGL mutex discipline is a source review concern for future approved demo removal, not demonstrated failure. Stage2A deferred recovery/client gates and unexplained transient Stage2B diagnostic errors remain tracked. Historical fixed touch/carousel/serial defects and vendor-library TODOs were inspected in context and not converted into new product requirements. Storage/radio functionality remains purpose/architecture-gated; OTA trust/writer/rollback/transfer and final release remain pending.

## Unknowns and required owner input

**UNKNOWN / UNCONFIRMED:** actual controls/header routing, active levels/pulls, encoder push/detents, reliable sample timing, assembled SD detect/supply/bus integrity, codec/amplifier/output wiring, local playback role, actual external TUNER source/protocol, physical multimeter accuracy/endurance, charging sense, POWER worst path/headroom, production UI and OTA trust/recovery policy.

**OWNER HARDWARE INPUT REQUIRED:** board/enclosure wiring or continuity evidence for A/B and each button; levels/pulls and optional push; header availability; SD wiring/detect/supply and chosen test media; populated codec/amp/load and audio routing. Collect pending Stage1 multimeter/endurance/wake/battery simulation evidence and Stage2B.2 S/T/B measurements using existing documented safe procedures. No inaccessible PWR recovery requirement.

**OWNER DECISIONS REQUIRED:** control action/focus/wake/hold mapping and physical direction; SD product purpose/lifecycle/quotas; local versus external playback and verified target source; station/artwork/format ownership; final network/reset/UI flows; OTA trust/version/recovery. Choose only after evidence; no fabricated defaults.

## Tests, failures and verification

Focused physical-input final:19 PASS/0 FAIL,21621 normal assertions/201621 soak; zero allocator calls. Covers clean/bounce/release, simultaneous IDs, duplicate/malformed/time-regressed samples, saturation/resync, positive/reverse Gray traces/noise, rapid input, ordering, initialization, owner refusal, bounds/ring wrap and1000/10000-cycle endurance. Numerical debounce values are synthetic fixture settings.

Retained development failures: first strict compile rejected two unbraced loops as misleading indentation; braces fixed. First endurance fixture generated initial button presses while expecting only an encoder event (18 PASS/1 FAIL). Fixture corrected to start released and assert subsequent alternating press/release events. Implementation/assertions were not weakened to obtain PASS. No baseline test removed.

Complete `python tests/control/run_tests.py`: **261 PASS/0 FAIL/429754 assertions**, exit0. Complete `python tests/control/run_tests.py --soak`: **261 PASS/0 FAIL/3924004 assertions**, exit0. All15 summary groups counted, including unchanged OTA preflight. Final logs contain no compiler/test warnings/errors. Additional standalone target compile: SUCCESS, no warnings/errors; no target runtime evidence.

Clean `pio run -e remote01 -t clean -j 1`: SUCCESS, exit0,1.37s. `pio run -e remote01 -j 1`: SUCCESS, exit0,545.03s. Executed installed PlatformIO executable with these arguments. Zero compiler/CMake warnings/errors. No build/upload command targeted hardware.

## Final artifacts and resources

| Resource | Final | Delta from entry |
| --- | --- | --- |
| BIN bytes | 2,480,480 | -16 |
| ELF bytes | 17,722,712 | 0 |
| Linked flash | 2,479,981 | -12 |
| Static DRAM | 46,908 (data24,540+bss22,368) | 0 |
| OTA-slot headroom | 665,248 B /649.65625KiB | +16 B |
| Tasks/stacks/sockets/queues/runtime heap allocations | Existing implementation unchanged | No runtime integration added |

PlatformIO's displayed8MiB program denominator is not the approved3MiB OTA slot. No runtime RAM/PSRAM/headroom or physical stack improvement is claimed. Model target392 B is a preparation object size, not a firmware allocation.

Final BIN SHA-256: `19b6ffa3138c735c682eb1aec7c57f42ae7cb211d48be286dc0cb3b3ef294c46`.

Final ELF SHA-256: `23fcc0825f45c8e7391b7dfe2dc8e0e29890aa633247adf14de53aee23399e01`.

Final partition SHA-256 unchanged: `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`.

Clean rebuilt hashes differ although all existing firmware/build inputs match entry. Effective CONFIG_APP_COMPILE_TIME_DATE is enabled; app description date/time changed. ELF comparison also shows changed code/constant placement and some instruction-size differences after clean relinking; differences are **not claimed to be metadata-only or executable byte equivalence**. The unreferenced model is absent from firmware include/dependency/symbol paths. Original artifacts are retained. This session changes no production source behavior and provides no physical acceptance of rebuilt artifacts.

## Exact file scope and final state

Added8 files:

- `include/physical_input/model.hpp`
- `tests/control/physical_input_tests.cpp`
- `docs/REMOTE01_HARDWARE_RESOURCE_MAP.md`
- `docs/REMOTE01_PHYSICAL_CONTROLS_PLAN.md`
- `docs/REMOTE01_SD_AUDIT.md`
- `docs/REMOTE01_AUDIO_RADIO_AUDIT.md`
- `docs/REMOTE01_RADIO_ARCHITECTURE_DECISION.md`
- `docs/REMOTE01_OVERNIGHT_2_REPORT.md`

Modified3 files: `tests/control/run_tests.py` (additive harness), `docs/REMOTE01_COMPLETION_ROADMAP.md`, `CHANGELOG.md`. Deleted0. All other1,647 entry files SHA-256 unchanged, including every existing production/config/frozen/historical file. Final1,658-file manifest retained in session TEMP. No AGENTS.md found in applicable checkout/parent chain. No hidden firmware integration, generated UI edit, persistence, GPIO assignment, network/POWER/security change or OTA extension.

## Dependency-ordered next actions

1. Owner review audit/architecture and supply verified wiring/role decisions; collect original pending hardware evidence. No flash is necessary to validate this disconnected input model.
2. POWER path physical measurements and Stage1/2A evidence precede corrective safety or release claims. Previous IP/recovery/preflight hardware acceptance remains separate.
3. Approve platform controls capture/sampling and action mapper; implement bounded owned integration, host/fault/resource tests, then real controls/wake acceptance.
4. Define SD purpose before service design; choose radio role and provide actual target source or verified audio hardware before protocol/pipeline implementation.
5. Approve Network Settings/production UI and OTA trust/version/recovery independently; only then writer/trial/rollback/authenticated transfer and integrated endurance/release.

Stopped after documentation/verification. No upload, new physical PASS or speculative implementation.


## 2026-10-05 - Today's Step1 owner validation status

**2026-10-05 - Overnight work / Today's Step 1: OWNER HARDWARE VALIDATION PASS. CLOSED / PASS.** This is verification of the overnight work on current instrumented hardware/software, limited to the owner-reported running checks, not hardware acceptance of dormant controls/SD/audio/OTA foundations. Exact final HOME/IP/carousel/touch/idle/wake/browser results and W/B telemetry are recorded in [the overnight execution owner-closure section](REMOTE01_OVERNIGHT_EXECUTION_REPORT.md#2026-10-05---overnight-work--todays-step-1-owner-closure).

Broader historical Stage1 remains OPEN for its existing mandatory evidence gaps. Network Settings backend remains **HARDWARE VALIDATION BLOCKED BY DESIGN / MISSING USER-ACCESSIBLE UI OR HARNESS**, not FAIL; OPEN_SETUP/CANCEL/confirmed Wi-Fi reset were not physically owner-tested. Covered POWER minimum1256 B exceeds the provisional1024 B review target, not a safety guarantee. Current WoM/lift wake PASS; S/B interference DISPROVEN; prior IMU_WOM0 incident real, currently not reproduced, root cause unknown. Preserve earlier pending/failure history and current implementation. Documentation only; no build/upload.


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

- Controls findings: "product actions are deliberately undecided" -> action design/freeze now required; no never-approved claim.
- SD, audio and radio: "SD PRODUCT USE = OWNER DECISION REQUIRED" / "no approved purpose" -> SD backup purpose DECIDED.
- SD, audio and radio: "dormant local playback possibility" / "No final architecture selected" -> playback ownership DECIDED; comparison historical.
- Unknowns and required owner input: "local playback role" UNKNOWN -> external TUNER role DECIDED.
- Unknowns and required owner input: "OWNER DECISIONS REQUIRED ... SD product purpose ... local versus external playback ... station/artwork/format ownership" -> purpose/playback/persistence/artwork ownership DECIDED; detailed designs separately required.
- Dependency-ordered next actions: "Define SD purpose ... choose radio role" -> implement verified backup service and verify actual target contract; no role/purpose reselection.
