# SYSTEM_SLEEP current-consumption audit - 2026-10-06

Documentation-only inspection of the current repository, installed ESP-IDF 5.5.3 and exact Waveshare V2/Rev1.1 schematic. No firmware, policy, thresholds, tests, configuration or artifacts changed; no build/upload. This is an optimization assessment, not authorization to implement deep sleep. Existing motion/touch/NETWORK recovery acceptance remains intact.

## Current owner acceptance - 2026-10-06

**Accepted whole-device hardware power baseline / OWNER HARDWARE VALIDATED / PASS** at battery4.075V:120mA display ON/100%,90mA display ON/50%, approximately70mA display OFF/system active (rare approximately80mA peaks), and approximately23mA normal SYSTEM_SLEEP. The sleep current remained approximately stable during about5minutes of observation. **No SD card was installed** during these measurements.

**GPIO1 ADC / thresholds / icon: OWNER HARDWARE VALIDATED / PASS** for the installed100kOhm/100kOhm GPIO1 circuit and current firmware: USB connected/disconnected and reconnect cycles PASS; screen-2 battery/USB icon switching PASS; charging remains UNKNOWN as designed. [GPIO1 owner closure](GPIO1_EXTERNAL_POWER_SENSING.md#current-owner-acceptance---2026-10-06).

**Sleep regression on GPIO1 firmware: OWNER HARDWARE VALIDATED / PASS**: automatic entry approximately302s; touch wake, display recovery, Wi-Fi reconnect and browser diagnostics recovery PASS; no reset/WDT/blocking regression observed.

This PASS establishes tested whole-device behavior/current, **not individual component or rail currents, nor the source of23mA**. Stable meter readings do not independently resolve chip-domain residency or brief waveform events. Fine-pitch/per-rail physical probing is **NOT REQUIRED / OWNER DECLINED for this milestone**, not a failed or incomplete acceptance test. No intrusive probing or micro-component measurement is authorized. Unknown current attribution remains an audit limitation, not a closure blocker. Deep sleep remains **FUTURE OPTIMIZATION / DESIGN TASK - NOT IMPLEMENTED / NOT ACCEPTED**.

The earlier audit below is retained as historical source analysis. Its requests for SD population/observation duration are now resolved by owner evidence; intrusive measurement proposals are declined for this milestone. Historical Stage1 gaps and unreported stack/endurance evidence are not implicitly closed.

## A/B diagnostic control - ADC lifecycle rollback - 2026-10-06

**ADC-optimized artifact: OWNER HARDWARE FAIL for SYSTEM_SLEEP residency. Causality NOT YET PROVEN.** Owner reproduced on battery with external power fully disconnected: normal sleep entry at approximately300s, whole-board current briefly toward approximately20mA, almost immediate uncommanded wake without owner touch/motion, display ON and approximately100mA. Previous accepted pre-optimization firmware held normal sleep near23mA for about5minutes on the same hardware. A brief current dip is not an accepted reduction or proof of the wake cause.

Failed optimized artifact BIN SHA-256: `b4580e6a87049b84222bd43d973321b60996a09711c8eef1b6bdd97f53af01f1`; ELF SHA-256: `b335f42a1d093b9d8838064d8e68637caae28d9a547924ab57c81531768e074b`. The ADC lifecycle was the only new functional change, so it is isolated first; this does not establish causality.

**Current rollback: A/B DIAGNOSTIC CONTROL BUILD / OWNER HARDWARE TEST PENDING.** Restore only the ADC-lifecycle change: shared ADC1 oneshot unit and both calibration handles remain allocated across light sleep. Remove unit teardown/recreation, restoration retry/fault helper and lifecycle-only test/harness invocation. `runtime.cpp`, `hardware.cpp`, `hardware.hpp` and test harness restored byte-for-byte against the saved, hash-verified pre-optimization checkpoint. GPIO1/GPIO4 conversions, thresholds/filtering, diagnostics/icon, sleep policy/timing, GPIO8, touch/motion, NETWORK, hold, UI/brightness, Wi-Fi, partitions and frozenCCM unchanged. No LEDC/deep-sleep/other fix. The original component table's retained-ADC description again applies to the control source.

The historical optimization implementation, host PASS, pending procedure and artifact evidence below are retained, explicitly superseded by this OWNER HARDWARE FAIL and rollback. No functional fix for spontaneous wake is attempted.

### Control owner procedure

1. Owner-controlled flash of the control BIN without NVS erase or partition change; no PWR access required. Keep the battery connected; disconnect external power/USB fully before the inactivity test. No SD card fitted, same battery-series method as accepted baseline. Record control BIN hash and battery voltage; stop browser polling/other test commands and do not touch or move the device.
2. Wait for normal approximately300s SYSTEM_SLEEP, not development Z. Record current drop, whether display stays OFF, and whether current remains near the previous approximately23mA baseline for about5minutes. Do not wake it merely to capture USB logs during the residency interval. Record any spontaneous display-on/current rise and timing.
3. If sleep remains stable, the ADC lifecycle optimization is **strongly implicated**, not yet causally proven; keep it reverted pending root-cause analysis. This is isolation evidence, not a new optimization PASS.
4. If immediate wake persists, ADC cleanup is not required to reproduce the regression in this control test. Next investigation is GPIO8/TCA/touch/QMI wake attribution; do not assign one of those as the cause or implement a workaround from this result alone.
5. After the observation interval, intentionally wake and check touch/motion, battery/GPIO1 telemetry, saved Wi-Fi and browser diagnostics recovery; capture existing B/S/W as useful, without intrusive probing. Report observed current/voltage/duration, wake timing and recovery. No automatic upload.

### Control verification / resources

Focused ADC/UI/source + sleep-boundary + POWER publication:5 PASS /0 FAIL /82723 assertions. Full normal300 PASS /0 FAIL /550686 assertions; full soak300 PASS /0 FAIL /5283936 assertions. Clean `pio run -e remote01 -t clean -j 1`, then `pio run -e remote01 -j 1`: SUCCESS (build531.89s); zero compiler/CMake warnings/errors. Full suites exited0. No upload. Hardware control residency remains PENDING OWNER TEST.

| Resource | Failed ADC-optimized artifact | A/B control | Delta |
| --- | ---: | ---: | ---: |
| BIN bytes |2486608|2485888|-720|
| Linked flash bytes (PIO) |2486109|2485393|-716|
| Static DRAM bytes |47084|47084|0|
| OTA-slot headroom (3145728-byte slot) |659120|659840|+720|
| ELF file bytes |17789660|17777512|-12148|

Control BIN SHA-256: `d29383357bfc5ad801bdaaaf191907a347fbef0c1e14acdac39c86091d4fa733`.

Control ELF SHA-256: `7fece2e89c10b3146bab8cce11504cfd42bc86c031a57dd14d2db3a3d4273104`.

Partitions BIN unchanged: `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`.

Target entry-frame reservations: POWER task560->560; publication880->880; Board.initialize48->96 (original implementation restored); initialize_external64->64, battery48->48, external48->48. Optimization-only initialize_adc80/suspend_adc32/restore_adc32/adc_restore_fault48 removed. These are compiler frames, not physical watermark predictions. Configured POWER4096/NETWORK6144/UC_owner24576/LVGL4000 unchanged; no new state/task/queue.

Exact7-file scope: modify `components/power_management/hardware.cpp`, `components/power_management/hardware.hpp`, `components/power_management/runtime.cpp`, `tests/control/run_tests.py`, `docs/SYSTEM_SLEEP_CURRENT_AUDIT.md`, `CHANGELOG.md`; delete only `tests/control/adc_sleep_lifecycle_tests.py`. Six modified, one deleted, zero added. Every non-documentation file matches the saved pre-optimization hash manifest, not merely selected snippets. Control binaries are a fresh build with newly reported hashes, not claimed byte-identical to a historical artifact. No unrelated functional source/config/test change. Current generated build artifacts are expected outputs. Optimized and control artifact/hash/frame/test evidence retained in session TEMP/remote01-adc-ab-control; saved original checkpoint in TEMP/remote01-adc-sleep.


## First conservative light-sleep optimization - 2026-10-06

**HISTORICAL / REVERTED: OWNER HARDWARE FAIL for SYSTEM_SLEEP residency; causality NOT YET PROVEN.** The original host/build PASS and pre-test pending record below remain history; they do not describe current control implementation or acceptance.

IMPLEMENTED / FOCUSED + FULL NORMAL + SOAK + CLEAN BUILD PASS / OWNER HARDWARE MEASUREMENT AND REGRESSION PENDING. The accepted approximately23mA GPIO1 baseline above is the pre-optimization checkpoint. Its PASS history is preserved; no mA saving or physical PASS is claimed for this new artifact. The original component table/checklist below describes the audited pre-optimization source; the ADC lifetime row is superseded by this section only.

### Exact SDK evidence and selected change

Pinned IDF5.5.3 `esp_adc/adc_oneshot.c`: normal-mode `adc_oneshot_new_unit()` acquires a SAR power reference; `adc_oneshot_del_unit()` releases it and frees the unit. S3 `sar_periph_ctrl.c` selects SAR POWER_ON on first acquire, POWER_FSM on last release; S3 `sar_ctrl_ll.h` maps these to `force_xpd_sar=3` and0. This is a demonstrated retained forced-power reference, not proof of its whole-board mA contribution. No private power-register/control API is called by application code. [Supported oneshot resource lifecycle](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/peripherals/adc_oneshot.html).

POWER alone deletes its shared ADC1 unit after NETWORK/Core/LVGL readiness, display gate OFF and existing wake-source/timer checks. A teardown error refuses sleep and retains the handle. The unchanged GPIO8 wake cycle runs, then ADC restores on every exit, including rejected/failed sleep, before owners/display/network resume. Public APIs: `adc_oneshot_del_unit`, `adc_oneshot_new_unit`, `adc_oneshot_config_channel`; existing `adc_cali_create_scheme_curve_fitting` is used only when a channel's calibration handle is missing. GPIO4/ch3 and GPIO1/ch0 retain12dB/12bit configuration and independent coefficients. Curve-fitting handles hold immutable conversion characteristics, not the unit pointer/power reference, so they remain allocated: no unnecessary coefficient teardown/reallocation.

Battery four-read averaging/x3 conversion, external four-read node conversion, thresholds/filter/debounce,2s sampling cadence and diagnostics contents are preserved. WoM is not reconfigured. After a restoration failure, the affected channel immediately loses validity in POWER/UI diagnostics (external UNKNOWN/error); successful independent channel remains usable. Missing unit/channel/calibration retries on the existing awake2s sample cadence. POWER logs `ADC_suspend=FAILED error=<err> hold_retained=1` or `ADC_restore=FAILED error=<err> battery_ready=<0|1> external_ready=<0|1> hold_retained=1`; existing sleep/measurement/external counters expose errors. Hold remains asserted; no forced reboot/hard-off. Numeric historical battery values may remain but cannot be interpreted as valid when validity is false.

### LEDC finding - intentionally unchanged

GPIO42 timer3/channel1 remains50kHz/8bit RC_FAST, with the existing user duty. Its omitted `sleep_mode` is IDF's default NO_ALIVE_NO_PD, not KEEP_ALIVE. Pinned `ledc.c` requests sleep-time clock power only for KEEP_ALIVE. `ledc_timer_del()` explicitly has a TODO to release timer/global clock sources; pause/deconfigure therefore is not evidence of releasing RC_FAST. No supported additional sleep-power benefit was established for this configuration, so PWM shutdown/recreation is not introduced. Existing brightness, default brightness, timer/channel, UI and restore behavior remain byte-identical. Tests guard that no new pause/deconfigure/KEEP_ALIVE policy was added rather than claiming an unimplemented LEDC lifecycle. [Pinned LEDC behavior](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/peripherals/ledc.html).

### Historical optimization owner procedure - superseded by failed test and control procedure

1. Owner-controlled upload of the verified new BIN only, preserving partitions/NVS; no erase/PWR access. Capture boot, B/S/W, confirm real/calibrated/valid battery telemetry, GPIO1 state and screen2 icon, saved Wi-Fi and diagnostics. No deliberate hardware fault injection.
2. Set brightness50%, let normal inactivity reach SYSTEM_SLEEP (approximately300s), wake by touch: first contact wakes only, release and second contact acts. Verify display returns at the selected brightness, Wi-Fi reconnect and browser diagnostics recover. After>=8s, battery remains valid/calibrated and GPIO1 reconfirms correctly. Repeat at100% to verify the user-selected brightness is preserved; do not alter default brightness policy.
3. Repeat normal sleep/wake at least3cycles including motion-only wake (no control intent), USB connected and battery-only operation. Confirm PRESENT/ABSENT and icon reconfirmation, charging UNKNOWN, hold1, IMU1 and no ADC_restore/ADC_suspend error, reset/WDT or allocation/accounting regression. Existing S/B/W stack/error telemetry is useful; compiler frames are not physical headroom guarantees.
4. With no SD card and the same battery-series method/conditions as the accepted4.075V baseline, leave normal SYSTEM_SLEEP untouched approximately5minutes. Record contemporaneous battery voltage, average/stability/peaks and current; do not use development Z (it retains2s timer wake). USB must match the original measurement conditions; document its attachment so USB cannot bypass the battery-series measurement. No fine-pitch/per-rail probing required or authorized.
5. Acceptance requires all physical functional/ADC/brightness/network tests PASS and measured SYSTEM_SLEEP current **equal to or below approximately23mA** under comparable conditions. Report actual mA and battery voltage. A higher current, invalid telemetry, brightness change, reset or failed recovery is FAIL/investigation; no source/host-derived savings claim. Equal current validates non-regression but does not demonstrate a reduction. Deep sleep remains future/unimplemented/unaccepted.

### Verification / artifacts

Focused lifecycle/fault/source tests:7 PASS /0 FAIL /7039 assertions; soak-focused7/0/70039. Complete normal307 PASS /0 FAIL /557725 assertions; complete soak307 PASS /0 FAIL /5353975 assertions. Actual Board methods and immediate telemetry-invalidation helper executed against fault-injected host SDK seams;1000/10000 lifecycle cycles, teardown refusal, allocation/configuration/calibration failure/recovery, independent channels, retained calibration/no per-cycle coefficient allocation and balanced unit creation/deletion. GPIO8/NETWORK/display/owner ordering guards and unchanged PWM behavior covered. Host results do not model ADC analog current or physical wake latency.

Commands: `python tests/control/adc_sleep_lifecycle_tests.py` (also `--soak`); `python tests/control/run_tests.py`; `python tests/control/run_tests.py --soak`; `pio run -e remote01 -t clean -j 1`; `pio run -e remote01 -j 1`. All succeeded. Clean target build525.85s; zero compiler/CMake warnings/errors. No upload.

| Resource | Accepted GPIO1 artifact | Optimization artifact | Delta |
| --- | ---: | ---: | ---: |
| BIN bytes |2485568|2486608|+1040|
| Linked flash bytes |2485073|2486109|+1036|
| Static DRAM bytes |47084|47084|0|
| .dram0.data / .dram0.bss |24540 /22544|24540 /22544|0 /0|
| OTA-slot headroom,3145728-byte slot |660160|659120|-1040|
| ELF file bytes |17777532|17789660|+12128|

BIN SHA-256: `b4580e6a87049b84222bd43d973321b60996a09711c8eef1b6bdd97f53af01f1`.

ELF SHA-256: `b335f42a1d093b9d8838064d8e68637caae28d9a547924ab57c81531768e074b`.

Partitions BIN unchanged: `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`.

Target entry-frame reservations (not complete nested-call stack bounds): POWER task560->560; publication880->880; Board.initialize object96->48, now calls non-inlined initialize_adc80; new suspend_adc32, restore_adc32, adc_restore_fault48; initialize_external64->64, battery48->48, external48->48. New nested restoration/SDK frames require hardware watermark remeasurement; do not infer physical improvement from a smaller initializer frame. POWER4096, NETWORK6144, UC_owner24576 and LVGL4000 configured stacks unchanged. Board fields/size and static state unchanged. ADC unit SDK allocation is released/recreated per actual sleep attempt; calibration allocations are retained and reused, with no new application task/queue/heap-backed state. Dynamic allocator plateau must be checked physically; no host claim of an SDK heap leak bound beyond the verified lifetime discipline.

Exact7-file scope: `components/power_management/hardware.cpp`, `components/power_management/hardware.hpp`, `components/power_management/runtime.cpp`, `tests/control/adc_sleep_lifecycle_tests.py` (new), `tests/control/run_tests.py`, `docs/SYSTEM_SLEEP_CURRENT_AUDIT.md`, `CHANGELOG.md`. Six existing files changed, one added; no deletions. Manifest/diff checks preserve every other non-build file, plus byte-identical battery/external conversion bodies, GPIO8 WakePort and NETWORK coordinator region. Build artifacts are the expected clean-build outputs. Evidence retained in session TEMP/remote01-adc-sleep. No GPIO8/SYS_EN/NETWORK/WoM/partition/CCM/UI/PWM/SD/audio change; no deep sleep or stack increase.


## Owner-measured baseline

Battery voltage: **4.075 V**. Whole-device battery current reported by owner:

| State | Current |
| --- | ---: |
| Display ON, brightness 100% |120 mA|
| Display ON, brightness 50% |90 mA|
| Display OFF, system active |approximately70 mA; rare approximately80 mA peaks|
| Normal SYSTEM_SLEEP |approximately23 mA; approximately stable for about5minutes|

These are accepted physical owner measurements, not host predictions. No SD card was fitted; observation duration was about5minutes. Meter method/burden voltage, detailed waveform, USB attachment during current measurement, speaker population and flashed artifact hash were not supplied. The23mA result does not establish any individual component's current or its source. At the supplied voltage it represents approximately93.7mW of total battery power. Display-OFF active to SYSTEM_SLEEP reduces measured total current by approximately47mA; that difference is not an isolated ESP32 current measurement.

## Exact current sleep path

`Machine::tick`: DISPLAY_IDLE after30 s; SYSTEM_SLEEP after300 s total physical inactivity, independent of external PRESENT/ABSENT/UNKNOWN. Wake hardware, valid battery measurement and asserted hold are prerequisites. NETWORK/owner unsafe work can refuse sleep. No automatic hard-off is added by GPIO1 detection; battery `external` remains separate from the new external-presence view.

POWER requests NETWORK prepare (10 s coordinator bound). NETWORK seals its inbox, gates callbacks, changes generation, clears connection presentation, revokes/stops connected diagnostics, stops provisioning HTTP/DNS, calls/checks `esp_wifi_stop()`, then fences the existing event loop. Only successful completion acknowledges READY. Active provisioning/scanning/candidate/reset/reboot work and unavailable credentials can block eligibility. Wi-Fi is stopped, not deinitialized: driver/netif allocation remains in retained RAM. HTTP stop completion can block its owner; POWER refuses on timeout rather than sleeping with an unacknowledged service.

POWER then parks Core (500 ms) and LVGL (500 ms). Core closes ingress and invalidates the old binding; LVGL stops its5 ms tick timer and completes its current handler/flush. Backlight is gated off through EXIO1. POWER checks TCA inputs and GPIO8 high before sleep, masks the edge ISR, enables GPIO8 LOW-level wake and calls **`esp_light_sleep_start()`**. It restores normal edge interrupts after return. Sleep errors/refusals wake safely rather than demonstrating a successful low-power interval.

**Normal automatic sleep has no2 s timer.** Only DEVELOPMENT_ONLY forced `Z` enables the2,000,000 us fallback. Task delays/polls, ADC2 s cadence and software timers do not continue executing inside manual light sleep and are not configured RTC wake sources. The SDK stops CPUs/the scheduler; retained task objects do not imply running tasks. Normal sleep has no enabled UART, USB, Wi-Fi, ULP or RTC-alarm wake path in application code. Wake is through the shared expander interrupt on GPIO8.

After GPIO wake, POWER acknowledges/decodes touch and QMI motion, consumes the first touch contact until release and applies its settle period. Core reacquires/publishes a fresh session before LVGL resumes. NETWORK resumes its existing owner, starts STA and reconnects using saved credentials; diagnostics restarts on healthy IP. No credential erase/reprovisioning is caused by wake. Existing owner tests verified this recovery, but the current measurement alone does not prove uninterrupted sleep residency: inspect `rounds`, `errors`, `last_sleep_cause` and a current/IRQ waveform to distinguish true residency from repeated exits.

## Components and likely power state

VERIFIED means a source/configuration or schematic fact, not a measured component current. INFERRED means the expected electrical result of that fact. UNKNOWN identifies a missing physical or device-specific observation.

| Component | State during a successful current SYSTEM_SLEEP interval | Classification / evidence |
| --- | --- | --- |
| POWER, UC_owner, LVGL, NETWORK; SDK event/TCPIP/HTTP tasks |No application execution inside light sleep. Core/LVGL explicitly parked; HTTP tasks stopped; NETWORK paused. Remaining SDK tasks/RAM retained, not executing.|VERIFIED: runtime/main/SDK sleep path. Actual continuous residency UNKNOWN.|
| ESP32 CPUs/APB peripherals/PLL/40 MHz system clock |Light-sleep gating/power reduction selected by SDK, not an active240 MHz polling loop. RTC slow clock remains internal RC; no application forced XTAL-on request.|VERIFIED configuration/API intent; exact chip-domain current UNKNOWN.|
| Wi-Fi RF / connection |`esp_wifi_stop()` checked before READY; association lost; no radio connection kept alive. Wi-Fi allocation/netifs retained.|VERIFIED NETWORK implementation. Physical RF-off current not separately measured.|
| Bluetooth |No active application BLE stack; BSP excluded.|VERIFIED build scope.|
| SYS_EN / battery switch / VSYS /3V3 |EXIO6 stays HIGH; Q2 battery path, VSYS and MP1605 regulator kept available. SYS_OUT is GPIO16's hold/button feedback net, not a separately switchable peripheral rail.|VERIFIED schematic and software hold; continuing rail voltage INFERRED until measured.|
| LCD controller / panel logic |VCI/VDDI supplied by3V3; no POWER sleep-in/display-off controller command or separate LCD logic supply switch. Controller remains initialized even with a dark panel.|VERIFIED wiring/absence of command; continuing powered controller INFERRED; panel standby current UNKNOWN.|
| Backlight AP3032 boost / LEDs |EXIO1 BL_EN LOW. Boost VIN remains VSYS. No whole-rail disconnection; LEDC configuration/duty remains allocated.|VERIFIED control/wiring; boost shutdown, residual VLED voltage and leakage UNKNOWN physically.|
| PWM / RC_FAST |GPIO42 LEDC timer3/channel1 uses50 kHz RC_FAST. Channel default is NO_ALIVE_NO_PD, not KEEP_ALIVE. No application timer stop/deconfiguration at idle; selecting RC_FAST alone is not proof it runs in sleep.|VERIFIED BSP/pinned LEDC source. Actual sleep clocks/output waveform UNKNOWN; do not attribute23 mA to PWM.|
| Touch / AXS15231B panel interface |No low-power touch command; panel supply retained. TP_INT -> EXIO0 required to retain touch wake.|VERIFIED wiring/software; touch scan power/mode UNKNOWN. This is external touch, not ESP capacitive-touch wake.|
| QMI8658 |Still configured WoM: accelerometer21 Hz, threshold200 mg, blanking4 samples, CTRL7=1; INT1 -> EXIO2. No gyro stream or application polling during sleep.|VERIFIED register sequence/wiring; sensor current UNKNOWN.|
| TCA9554 |3V3 supplied; output latch maintains hold/backlight state; INT -> GPIO8 with10 kOhm pull-up R6. Must remain powered for either wake source.|VERIFIED schematic; rail retention INFERRED/current UNKNOWN. EXIO3 is IMU INT2, EXIO4 RTC_INT.|
| System and touch I2C |Drivers/devices retained; no transactions inside sleep. External pull-ups remain connected to3V3. Idle-high buses should not draw pull-up DC current; stuck-low buses would.|VERIFIED configuration; actual levels/leakage UNKNOWN.|
| Battery GPIO4 / external GPIO1 ADC |ADC1 oneshot handle/calibration retained; no conversions during sleep, no ULP monitoring. SDK acquires ADC power for handle lifetime; no application ADC teardown before sleep.|VERIFIED code/SDK; actual analog-domain sleep current UNKNOWN. Dividers remain physical loads: battery200k/100k approximately13.6 uA at4.075 V; new100k/100k approximately24.8 uA when PTH5V=4.95 V, ideally0 without USB.|
| Flash / integrated PSRAM |VDDSDIO explicitly forced ON for light sleep; flash and PSRAM leakage workarounds enabled. State retained; no application memory power-off. External flash U6 powered by VDD_SPI.|VERIFIED configuration/wiring; actual chip standby/leakage currents UNKNOWN.|
| USB Serial/JTAG / UART |Primary UART0, secondary USB Serial/JTAG configured. No serial wake enabled. S3 SDK backs up/disables USB pads for light sleep and restores afterward; USB debugging/enumeration is not retained like active operation.|VERIFIED SDK/config; attached-cable effects/current UNKNOWN.|
| PCF85063 RTC |3V3/VBAT diode-backed supply; external32.768 kHz oscillator remains. No RTC alarm wake configured by current application.|VERIFIED schematic/software; current UNKNOWN. This crystal is not the MCU's selected internal RTC clock.|
| ES8311 / ES7210 / NS4150B amplifier / microphones |Codec/ADC supplies wired to3V3/A3V3, amp to VSYS. Audio BSP excluded and no audio clock stream, but there is no active sleep/shutdown transaction. EXIO7 NS_MODE remains reset-configured input; schematic has10 kOhm pull-down.|VERIFIED wiring/software; actual default codec/amp/mic operating mode and quiescent draw UNKNOWN. Dormant software does not prove unpowered hardware.|
| SD card |Socket VDD wired to3V3; SD BSP excluded. No card fitted during accepted measurements, so an installed SD-card load is absent.|VERIFIED schematic/build and owner population evidence; socket/pull-up leakage not separately measured.|
| Charger/protection/regulator/LED losses |ETA6098, board battery protection, MP1605 and switching/hold network remain attached; no MCU control of charger STAT. Charging remains UNKNOWN.|VERIFIED circuit; quiescent current, conversion efficiency, attached-USB behavior and LED current UNKNOWN.|

The schematic does **not** show independently switchable logic rails for LCD/touch/audio/SD. SYS_EN holds the battery supply for the board. This does not prevent MCU deep sleep, but prevents treating deep sleep as a whole-board power cut. Releasing SYS_EN is hard-off with different recovery consequences, outside this audit's scope.

## Original proposed checks - owner disposition supersedes milestone requirements

The original checklist is preserved below, not a required remaining acceptance procedure. Owner has supplied stable5minute current observation and no-SD population. Fine-pitch/per-rail probing, branch isolation and micro-component measurements in items3/4/6 (and fine-pitch parts of2/5) are **NOT REQUIRED / OWNER DECLINED for this milestone**. Do not perform or request them as a PASS condition. Non-intrusive whole-device measurements, existing diagnostics and source/datasheet research remain possible future optimization evidence; any intrusive future work requires separate authorization.

1. Repeat battery-input current logging at4.075 V with USB physically absent, known BIN hash, no connected speaker/known SD population, meter burden/range documented. Wait for normal300 s inactivity, not `Z`; record a stable interval and transient peaks.
2. Use existing `B` before sleep/after one physical wake: compare `sleep_requests`, `rounds`, `errors`, `last_sleep_cause`, hold and wake reason; use `W`/browser after wake for recovery. Observe GPIO8 and current together over the idle interval. Repeated sleep exits/IRQ assertions must be distinguished from steady23 mA residency. USB/serial measurement itself changes conditions; use a scope/independent UART if needed.
3. Measure VSYS,3V3, BL_EN, VLED+/VLED-, LCD supply and GPIO42 during sleep; verify boost shutdown and no backlight glow/periodic pulses. Observe both I2C lines and GPIO8 high between wake events.
4. Measure/regulator-account the MCU3V3 branch versus panel, codec/ADC/amp, SD and boost branches. Shared rails require reversible bench isolation or current probes/test points by a competent hardware operator; no total-current subtraction can identify those branches. Compare SD inserted/removed if fitted.
5. Obtain exact panel/controller low-power and touch-retention command documentation, and actual fitted codec/amp defaults/current specifications. Read relevant status/registers only in an authorized future diagnostic build. Verify NS_MODE voltage; do not blindly drive it or initialize dormant codecs.
6. For an authorized future MCU deep-sleep prototype, scope EXIO6/Q2 gate/VSYS through sleep and reset/startup, verify latched GPIO8 low wake from each source and subsequent deassertion, and measure boot-to-local-UI versus IP/diagnostics-ready latency separately.

## Ranked optimization plan (proposals only)

Current disposition: no optimization is implemented or authorized by this acceptance. Per-rail/intrusive checks in the historical proposals are owner-declined for this milestone; they do not block this PASS. No SD card is present to remove. Future proposals must use available non-intrusive evidence or retain unknown component attribution.

1. **Lowest risk:** establish residency and per-rail current first; check backlight boost actually disabled and unwanted low bus lines. No behavior change needed to collect existing diagnostics.
2. **Measured active-use benefit:** user-selected lower brightness already saves30 mA between supplied100%/50% observations. Preserve brightness behavior; do not impose a new default in this audit.
3. **Target unused physical loads:** if measured significant, use documented codec/ADC/amp shutdown modes and remove an unnecessary fitted SD card. Power-down sequences must preserve shared I2C/SYS_EN and electrical defaults; no expected mA saving established yet.
4. **Panel standby with wake preserved:** investigate documented LCD sleep-in while keeping touch/IRQ functional; measure before/after and wake latency. Combined display/touch controller makes this higher risk than backlight gating. No unverified register commands.
5. **MCU light-sleep refinements:** review ADC lifetime/power control, LEDC clock registration and forced VDD_SPI retention only after branch measurements. Flash power-off with asynchronous GPIO wake has SDK safety risks; enabled leakage workarounds are already present. Do not remove VDDSDIO retention blindly.
6. **Highest architectural impact:** optional long-idle deep sleep, after proving hold/wake/bootstrap/session restoration below. This may reduce MCU/memory consumption but leaves board loads powered; no whole-board target current is justified yet.
7. **Hardware revision if external loads dominate:** separately switch non-wake peripheral rails while preserving TCA/touch/IMU/MCU RTC supply. Board modification/redesign and SYS_EN release are not authorized here.

## Deep-sleep feasibility and UX

**GPIO-level feasible, not currently implemented or ready for a drop-in switch.** GPIO8 is an ESP32-S3 RTC GPIO. IDF5.5.3 supports EXT0 low-level or EXT1 ANY_LOW on this pin; existing `esp_sleep_enable_gpio_wakeup()` is light-sleep-only. Touch and motion can share this external interrupt if TCA/touch/QMI stay supplied, INT remains latched long enough and the bus is cleared before rearming. No ESP touchpad wake API substitutes for this board's external touch controller. RTC pin ownership must be restored after wake.

**Critical battery-only boot risk:** `esp_io_expander_new_i2c_tca9554()` synchronously resets direction to0xff (all inputs), then output to0xff. `example_lcd_exio_init()` only subsequently restores EXIO6 output/HIGH. Re-running this on deep wake can temporarily release the independent SYS_EN latch. Whether circuit capacitance masks this is UNKNOWN; no PWR-access-dependent recovery is acceptable. Deep-wake-aware hold-preserving expander initialization and physical validation are prerequisites, not changes made by this audit.

Deep sleep restarts the CPU through boot rather than returning to POWER's sleeping call. FreeRTOS tasks, normal RAM, PSRAM contents, Core/session objects, queued input, diagnostics events, UI objects, timer state and network handles cannot be treated as retained. NVS credentials/partition data persist; supported RTC memory may retain an explicitly validated small record. Existing reset-history RTC record is not permission to alter its format or to persist all runtime state. Current Mock state restoration/persistent product state require an explicit contract, not accidental replay of saved intents.

The current `PowerSuspend` saves availability/reachability in normal RAM and attaches a fresh session before UI resumes. Deep wake needs a cold reconstruction path with stale work invalidated and fresh session/incarnation boundaries; old callbacks/commands must never be restored as accepted work. NETWORK stop-before-sleep can be reused conceptually, but RAM-based resume epoch/owner cannot survive: normal boot initializes Wi-Fi again using NVS, reconnects and restarts diagnostics. The current NETWORK task deliberately waits6 s before initialization. HTTP clients/sockets are lost in either mode; deep sleep additionally loses in-memory event history, so clients must handle a new boot identity/event sequence.

**First touch wakes; second touch acts is achievable but not automatic.** Current boot defaults `active=true`, `consume_contact=false`, and a fresh TouchGate. Deep wake must establish a wake-only contact gate before any LVGL input, acknowledge shared IRQ safely, and consume the wake contact until release. Motion must produce no control intent. Any other pending interrupt on the shared expander must be accounted for to prevent immediate wake loops.

**Latency:** current owner light-wake measurements were approximately220-247 ms in covered tests (not a new measurement of this ADC build). Deep wake incurs ROM/bootloader, PSRAM initialization/memtest, main startup, a310 ms explicit LCD reset sequence plus panel initialization, then UI/Core reconstruction. The NETWORK6 s delay precedes STA startup/association/IP. Exact total deep-wake latency is UNKNOWN; immediate-touch UX will be slower and must be measured independently from network recovery. The2000 us deep-wake SDK setting is not an application-ready latency prediction.

**Two-stage short light/long deep sleep is feasible as a future design.** Current normal light sleep waits indefinitely for physical GPIO wake; a longer-inactivity promotion cannot execute while CPUs are asleep. It would require a single purpose-specific RTC deadline to wake into a coordinated deep transition, not reinstating the2 s periodic fallback. Cancellation on touch/motion/pending unsafe work, elapsed time across boots and the hold/wake/fresh-state contract need tests and owner acceptance. No new timing thresholds or automatic policy changes are made here.

## Evidence and limits

Repository: `components/power_management/{runtime.cpp,hardware.cpp,model.cpp}`, headers `model.hpp`, `qmi_wom.hpp`, `sleep_timer.hpp`, `wake_irq.hpp`; `components/network_manager/runtime.cpp` and `include/network_manager/sleep.hpp`; `components/control_home/runtime.cpp` and `include/control_home/power_suspend.hpp`; `main/{main.cpp,user_config.h}`; `components/{i2c_bsp,lcd_bl_pwm_bsp}`; root CMake exclusions and generated `sdkconfig.remote01`.

Installed pinned SDK: `esp_hw_support/sleep_modes.c`, `port/esp32s3/rtc_sleep.c`, `esp_driver_ledc/src/ledc.c`/`include/driver/ledc.h`, `esp_adc/adc_oneshot.c`, S3 `soc_caps.h`; managed TCA9554 driver's constructor/reset. PlatformIO pins framework3.50503.0 / IDF5.5.3. The old5.3.2 comment in sdkconfig.defaults is not the running framework version.

Primary references: [exact pinned Waveshare V2 schematic, PCB title V1.1](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/blob/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/schematic/ESP32-S3-Touch-LCD-3.49%20V2.pdf), full sheet and enlarged KEY&POWER/codec regions visually inspected; [IDF5.5.3 S3 sleep APIs](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html). Schematic population/net connectivity is not measured rail current. MCU reference low-power figures cannot be substituted for this complete board's current.

Original audit validation: read-only source/SDK/schematic cross-check, documentation inspection and protected-file hash comparison; no tests/build. At that checkpoint ADC/threshold/icon acceptance was not yet supplied. The subsequent owner closure above now validates GPIO1 and its sleep regression plus the whole-device current baseline. Historical Stage1 closure, deep-sleep acceptance and a demonstrated23mA source/root cause remain unclaimed. Documentation-only acceptance update; no firmware/build/upload.
