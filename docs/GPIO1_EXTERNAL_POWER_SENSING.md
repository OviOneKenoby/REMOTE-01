# GPIO1 external-power sensing - 2026-10-06

## Current owner acceptance - 2026-10-06

**OWNER HARDWARE VALIDATED / PASS**: GPIO1 ADC / binary thresholds / screen-2 battery-to-USB icon switching for the installed100kOhm/100kOhm GPIO1 circuit and current firmware. USB connected, USB disconnected and reconnect cycles all PASS. Charging remains UNKNOWN as designed. The600/1800mV thresholds and existing debounce are accepted for this tested circuit/firmware; this is not a universal ADC accuracy/noise specification. No new numeric runtime raw/mV values were supplied in this closure. Firmware diagnostics still say `thresholds=PROVISIONAL`; this is the unchanged implementation label, superseded as an acceptance status by this owner decision. No firmware label or threshold was changed.

**Sleep regression on the GPIO1 firmware: OWNER HARDWARE VALIDATED / PASS.** Automatic SYSTEM_SLEEP entered at approximately302s; touch wake, display recovery, Wi-Fi reconnect and browser diagnostics recovery PASS. No reset/WDT/blocking regression was observed.

**Accepted whole-device hardware power baseline at battery4.075V:** display ON/100%120mA; display ON/50%90mA; display OFF/system active approximately70mA with rare approximately80mA peaks; normal SYSTEM_SLEEP approximately23mA, approximately stable during about5minutes of observation. No SD card was fitted. These owner measurements do not identify individual component/rail currents or prove the source of23mA. Fine-pitch/per-rail physical probing: **NOT REQUIRED / OWNER DECLINED for this milestone**, not failed validation. [Current baseline/audit and future optimization limits](SYSTEM_SLEEP_CURRENT_AUDIT.md).

This supersedes the ADC/threshold/icon and sleep-regression pending wording in the original implementation record/procedure below; those records are preserved as history. Unreported physical stack-watermark/dynamic-memory measurements and broader historical Stage1 evidence gaps are not promoted by this acceptance. Deep sleep remains a future design/optimization task, NOT IMPLEMENTED / NOT ACCEPTED. Documentation only; no firmware/build/upload.

## Historical implementation checkpoint - pending status superseded above

IMPLEMENTED / FULL HOST REGRESSION + SOAK + CLEAN BUILD PASS / OWNER ADC + THRESHOLD + ICON HARDWARE VALIDATION PENDING. Owner divider wiring/voltage measurements are evidence; firmware acquisition/classification/rendering are not yet hardware-validated. No upload. Historical Stage1 remains OPEN; accepted SYSTEM_SLEEP motion/touch/NETWORK-recovery evidence is preserved.

## Exact owner hardware evidence

Board: Waveshare ESP32-S3-Touch-LCD-3.49 V2 Rev1.1. Source is PTH `5V`, with100kOhm from PTH5V to ADC node and100kOhm from node to PTH `G`; node wired to GPIO1.

| Measurement | USB disconnected | USB connected |
| --- | ---: | ---: |
| PTH5V to PTHG |0.00V|approximately4.95V|
| GPIO1/node to PTHG |0.00V|approximately2.42V|

Owner measurements, not calculated divider values or host predictions. GPIO4 remains the existing battery ADC. An ideal equal-resistor ratio does not replace the measured node voltage. Firmware reports node millivolts without multiplying by2 or declaring a calibrated VBUS voltage. GPIO1 availability/wiring is now owner-evidenced; the prior unallocated-pin inventory is superseded for GPIO1 only.

## Audit and implementation

Pinned ESP-IDF5.5.3 ESP32-S3 mapping: GPIO1=ADC1 channel0, GPIO4=ADC1 channel3, checked through SDK soc/adc_channel.h and target static assertion. ADC1 is already allocated once by POWER; use a second independently configured channel on that unit, not ADC2 or a second ADC1 owner. [Pinned oneshot documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/peripherals/adc_oneshot.html) supports separately configured channels. [Pinned calibration documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/peripherals/adc_calibration.html) and installed curve-fitting implementation establish calibration support/failure behavior.

GPIO1 explicitly uses ADC_ATTEN_DB_12, ADC_BITWIDTH_12 and its own channel0 curve-fitting handle. The S3's nominal12dB measurement range extends to3100mV, enclosing the measured2420mV; lower attenuation does not provide that same coverage. [Espressif range guidance](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/adc.html). This is range suitability, not proof of runtime accuracy on the installed100k/100k divider. Calibration is required; no guessed reference/default linear conversion fallback. The additional initialization runs after unchanged WoM configuration/summary, so no new pre-WoM timing/delay/retry is introduced.

Existing GPIO4 battery acquisition is byte-preserved: ADC1 ch3,12dB,12bits, independent original curve-fitting handle, four reads/rounded mean, calibrated node voltage multiplied by3 for its original schematic divider. No battery threshold/filter/qualification change. External setup/calibration failures do not change battery readiness, WoM readiness or sleep eligibility. If the shared ADC1 unit itself is unavailable, both measurements naturally cannot read; external reports UNKNOWN/unit-unavailable error.

POWER's existing awake2s ADC cadence acquires four bounded GPIO1 reads and rounded mean, then converts with channel0 calibration. Raw saturation4095, invalid raw values, negative/out-of-range conversion or SDK failure produce invalid/UNKNOWN. Zero raw and zero calibrated millivolts are valid external measurements, distinct from the battery path's positive-voltage requirement. Initialization error/read/calibration/range error exposed; on failed conversion mv0 denotes unavailable, not a confirmed ABSENT. Saturated offending raw is retained for diagnostics. No repeated sampling task, queue, new interrupt, GPIO8 change or I2C/WoM change.

### Original provisional binary policy - hardware acceptance superseded above

Thresholds are node voltage: ABSENT candidate<=600mV; PRESENT candidate>=1800mV. They approximate the lower/upper quarters of the measured0..2420mV separation, allowing a broad1200mV hysteresis band and about620mV margin below the measured connected node. They are not a5V supply compliance/battery/charging threshold. Both are explicitly PROVISIONAL pending actual firmware ADC distributions and physical comparison.

Three consecutive qualifying samples are needed at the existing2s cadence:4s between first and third, normally within6s of a physical change. A contrary sample resets confirmation. Within600..1800mV, retain the last confirmed state; boot/invalid history with only intermediate readings stays UNKNOWN. Any invalid/calibration failure immediately sets UNKNOWN and resets confirmation; recovery needs three fresh qualifying samples. A sampling gap>4s or time regression invalidates the historical decision before new confirmation, so sleep/wake cannot indefinitely retain a stale plugged-in indication. Sampling is paused by existing SYSTEM_SLEEP and resumes on wake; this is cached sampled telemetry, not USB-event/wake-source sensing.

State/error counter storage is bounded, counter saturates. Detector state is fixed POWER-owned storage, not a persistent outer-task stack-local copy or heap object. Copied read-only ExternalView is published through the existing short POWER view critical section; UI performs no ADC/SDK conversion. Separate external state is not copied into Battery.external and does not alter critical-shutdown semantics. Existing SYSTEM_SLEEP eligibility call and NETWORK prepare/resume implementation remain unchanged. Charging remains UNKNOWN for every state, including PRESENT. Existing automatic hard-off prohibition/hold behavior and manual development controls remain unchanged; the battery transition log now labels inhibition POLICY_UNCHANGED instead of attributing it to missing external sensing.

### Diagnostics

Serial `B` retains existing reports and adds:

`EXTERNAL_POWER: gpio=1 raw=<ADC> mv=<node_mV> valid=<0|1> calibrated=<0|1> error=<esp_err_t> read_errors=<count> external=<PRESENT|ABSENT|UNKNOWN> charging=UNKNOWN thresholds=PROVISIONAL`

One boot summary identifies GPIO1/ADC1_CH0/12dB/12bits, calibration initialization/error and provisional thresholds/cadence. No per-read log spam. Browser `/api/status` adds to `power`: external_raw, external_mv, external_valid, external_calibrated, external_error, external_read_errors; existing external becomes the published state string. Existing charging UNKNOWN, battery fields,23-entry value ordering, RTC reset history, routes/access control and bounded response remain unchanged. Invalid/error/uncalibrated snapshots cannot display PRESENT. No new persistent writes.

### Screen2 icon

Inspection found a battery outline at104,13 with tip117,15 and no pre-existing plugged-in object. Owner clarified: switch that battery icon to a usual plugged-in icon when USB is present. Implemented existing LV_SYMBOL_USB/U+F287 from already-enabled Montserrat12; no new library/font/asset. The glyph's existing bitmap is16px wide/11px high, advance15px, font line-height15px: plug label104,10,width16,height15. Fits wholly inside172x640, ends at120 with2px clearance before unchanged voltage text at122; ends at25 above divider32. No glyph clipping; battery outline/tip geometry retained.

PRESENT + valid/calibrated/error0 hides battery body/tip and shows USB glyph. ABSENT or UNKNOWN/error hides USB glyph and restores existing battery outline. Same primary color/font, voltage formatting, screen layout/carousel/touch remain unchanged. Binding runs before Core snapshot generation early-return, so external transitions render even without Mock/Core presentation changes. A USB-presence glyph is not a charging claim. Owner must verify its physical appearance and state changes.

## Verification / resources / exact files

Focused tests:14 PASS/0 FAIL/4096 assertions (detector11/4062, actual ADC/UI seam2/22, source/font/layout guards1/12). Includes PRESENT/ABSENT/UNKNOWN, initialization/configuration/calibration/read/range failure, zero/saturation, hysteresis/debounce/bounce/recovery, stale sampling, time regression, saturating errors, independent battery channel/calibration/x3 behavior and real HOME icon branch. No application allocation in detector or publication loop.

Complete normal suite:300 PASS/0 FAIL/550686 assertions. Complete soak:300 PASS/0 FAIL/5283936 assertions, including10000 external transition cycles; prior sleep/network/Core/WoM/publication/physical controls regressions retained. Both enabled/disabled WoM and stack-measurement variants run. Host -Wall -Wextra -Werror. Source tests execute actual extracted Board methods and HOME binding against fake SDK/LVGL seams, not physical ADC/GUI predictions.

Commands: `python tests/control/run_tests.py`; `python tests/control/run_tests.py --soak`; final `pio run -e remote01 -t clean -j 1`; `pio run -e remote01 -j 1`. Final clean/build SUCCESS1.43s/583.61s; zero compiler/CMake warnings/errors. Earlier in-progress build cancelled before final clean rebuild after source/error/layout finalization; it is not the delivered artifact. No upload/erase/serial operation performed.

| Resource | Verified entry artifact | New clean artifact | Delta |
| --- | ---: | ---: | ---: |
| BIN bytes |2483904|2485568|+1664|
| Linked flash |2483405|2485073|+1668|
| Static DRAM |46980|47084|+104|
| .dram0.data |24540|24540|0|
| .dram0.bss |22440|22544|+104|
| OTA slot |3145728|3145728|0|
| OTA-slot headroom (BIN) |661824|660160|-1664|
| ELF file bytes |17765480|17777532|+12052|

Actual3MiB-slot headroom660160bytes; PIO's8MiB board-size display denominator is not the OTA slot. SHA-256:

- BIN: `8a16cc8db6fdbe53b493bb34515a068d3fcf684c4e6f4d93178d8ed3e058eefa`
- ELF: `e0f55b8e65a508b6501e7c46d927796ec81215cf9d73e779cdf48cd3f0253d19`
- partitions.bin: `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835` (byte-identical to entry).

Target compiler entry-frame reservation: POWER task544->560(+16); publish_power848->880(+32); Board.initialize96->96; battery48->48; HOME refresh304->336(+32); diagnostics HTTP status1072->1104(+32). New initialize_external64, Board.external48, sample_external64, report_external64. POWER4096, NETWORK6144, UC_owner24576, LVGL4000 unchanged. Nested SDK/caller usage is additional; these are not physical minimum-watermark predictions. Remeasure POWER S/B and HTTP/NETWORK/LVGL margins; provisional>=1024-byte POWER review target remains a review target, not a safety guarantee.

Fixed POWER storage: ExternalPower56bytes + published ExternalView32bytes; UI retains three additional object pointers. One GPIO1 calibration handle adds two SDK internal allocations (target payload8+32=40bytes plus allocator overhead), once at boot. No second ADC unit. One additional existing-font LVGL label/text allocation, bounded and parent-deleted. Diagnostics PSRAM Storage8056->8088(+32), remains<=8192; Snapshots904<=1024. No per-sample allocation/new task/new queue/NVS write. Runtime dynamic peak/physical watermark and icon rendering remain owner-measurement pending.

GPIO4 battery acquisition function byte-preserved against entry copy; NETWORK sleep/resume, power model.cpp/thresholds, GPIO8/wake/WoM, RTC reset format/storage, partitions/configuration, frozen Core/CCM and other UI geometry unchanged. Source hash/diff audit:16 modified existing files +4 added, zero deleted (20 total); all other non-build files unchanged. .pio outputs are expected clean-build artifacts. Session evidence (entry manifests, source/object copies, logs, frames/hashes) retained in TEMP/remote01-external-power.

Exact changed files:

- `CHANGELOG.md`
- `docs/HARDWARE.md`
- `docs/REMOTE01_COMPLETION_ROADMAP.md`
- `docs/REMOTE01_HARDWARE_RESOURCE_MAP.md`
- `docs/REMOTE01_PRODUCT_DECISIONS.md`
- `docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md`
- `tests/control/power_publication_tests.py`
- `tests/control/run_tests.py`
- `components/diagnostics/model.cpp`
- `components/power_management/hardware.cpp`
- `components/power_management/hardware.hpp`
- `components/power_management/runtime.cpp`
- `components/user_app/user_app.cpp`
- `components/power_management/include/power_management/api.hpp`
- `components/power_management/include/power_management/model.hpp`
- `components/diagnostics/include/diagnostics/model.hpp`
- `docs/GPIO1_EXTERNAL_POWER_SENSING.md`
- `tests/control/external_adc_ui_tests.py`
- `tests/control/external_power_tests.cpp`
- `components/power_management/include/power_management/external_power.hpp`

Host evidence cannot verify high-impedance divider loading, target ADC calibration accuracy/noise, physical node wiring or symbol appearance. No runtime firmware threshold/accuracy acceptance is inferred from the multimeter-only owner measurements. Actual sleep/wake, stack and memory behavior require remeasurement for the new artifact.

## Historical first owner validation procedure - completed owner results above

1. Owner-controlled flash this artifact only, retaining current partitions/NVS/credentials; no erase. Battery stays connected, boot by USB, never inaccessible PWR. Wait>=8s. Capture boot external calibration/error line and `B`. Expected calibrated1/error0; external valid1; measured ADC-node mv approximately2420 (record actual raw/mV and difference from contemporaneous multimeter), external PRESENT after three samples. Screen2 voltage remains battery voltage; USB glyph replaces battery outline. Charging UNKNOWN; automatic_system_sleep=ENABLED/external_gate=NONE. Unexpected error/UNKNOWN, saturation, unstable readings or material measurement discrepancy: report raw/mV/error and do not call thresholds validated.
2. Use `W`/HOME to find current STA IP; open `http://<STA-IP>/api/status` or existing browser diagnostics. While USB-connected record all external fields after>=8s, cross-check contemporaneous GPIO1/node and PTH5V voltages. The formerapproximately2.42/4.95V are owner baseline references, not fabricated runtime readings.
3. Disconnect USB while battery remains attached and SYS_EN holds. Serial will be unavailable, so keep using the same LAN browser. Within8s, expected valid1/calibrated1/error0, node mv approximately0 (a small ADC floor must be recorded, not hidden), external ABSENT after confirmation; screen2 restores battery outline. Re-measure node/PTH5V to PTHG, compare raw/calibrated values with physical0.00V; no reboot/credential loss expected. A0mV value with valid0 is UNKNOWN/error, not ABSENT PASS.
4. Reconnect USB without rebooting: after8s expect stable node near2420mV, PRESENT and USB glyph, charging UNKNOWN. Reattach serial monitor115200 if required and capture `B`/`S`/`W`. Repeat at least5 connect/disconnect cycles; record transitions, node/ADC differences, saturation/read errors, icon correspondence and boot/uptime continuity. Brief/intermediate readings must not rapidly toggle the icon. Do not deliberately short the ADC/5V or alter installed resistors for error injection; UNKNOWN/failure paths are host-tested.
5. Only after both states' actual ADC distributions/physical comparison and transitions are accepted by the owner may600/1800mV thresholds be marked HARDWARE VALIDATED for this circuit. If readings disagree, keep PROVISIONAL and investigate before adjustment; do not merely lower thresholds to hide acquisition problems. Record actual measurements/error/noise and tested conditions; no universal precision/specification claim.
6. Regression: leave untouched300s on USB and again on battery. Normal sleep must remain independent of PRESENT/ABSENT; motion wake and first-touch-consumed/second-touch-normal plus NETWORK/browser recovery must still pass, no2s normal wake cadence/hard-off/PWR requirement. After long sleep, UNKNOWN/battery outline while fresh three-sample confirmation occurs is intentional. Check POWER S/B and NETWORK W stack/allocation/error telemetry. Historical system-sleep PASS is preserved, not substituted for this artifact's retest. No battery deep-discharge experiment is needed.

PASS requires physically consistent calibrated readings/valid states, stable debounced transitions and correct icon in both USB states; charging UNKNOWN and no protected-behavior regression. UNKNOWN/error or disagreement remains a reported failure/investigation, not an invented hardware PASS. Existing independent battery-accuracy/endurance and historical Stage1 evidence gaps remain separate.
