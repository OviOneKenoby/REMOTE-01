# Stage 1 battery and power management

Current external telemetry override - 2026-10-06: GPIO1 ADC1_CH0 installed100k/100k PTH5V/PTHG divider now has calibrated POWER acquisition and provisional600/1800mV binary debounce. Owner physical disconnected0.00V/0.00V and connectedapproximately4.95V/2.42V source/node evidence recorded in [GPIO1 report](GPIO1_EXTERNAL_POWER_SENSING.md). ADC/state/icon hardware validation PENDING; charging UNKNOWN. GPIO4 battery acquisition/thresholds/hard-shutdown unchanged; external sensing is telemetry/UI only and does not gate accepted SYSTEM_SLEEP or alter wake/NETWORK coordination. Prior external-UNKNOWN/inaccessible-signal records below remain historical before this owner-installed circuit. Historical Stage1 remains OPEN.

Current policy override ? 2026-10-05: separately authorized automatic SYSTEM_SLEEP implementation is independent of external power and coordinates NETWORK/Wi-Fi stop/resume. Normal30s DISPLAY_IDLE/300s total SYSTEM_SLEEP, physical wake only; development Z retains2s battery/WoM fallback. [Full lifecycle, verification and owner retest](SYSTEM_SLEEP_NETWORK_IMPLEMENTATION.md). SYSTEM_SLEEP + NETWORK quiesce/resume: OWNER HARDWARE VALIDATION PASS for motion wake and the2026-10-06 touch-wake/first-contact-consumption/second-contact-normal/NETWORK-diagnostics-recovery path; other unreported checks remain PENDING; historical Stage1 acceptance remains OPEN. Earlier inhibited-policy artifacts and audit STOP below remain historical evidence, not current behavior. No automatic hard-off/new battery threshold/charging claim/external-divider implementation.

2026-10-04. **STAGE 1 / OWNER CORRECTED-FIRMWARE STABILITY RETEST PASS / OVERALL ACCEPTANCE PENDING REMAINING MANDATORY EVIDENCE.** Corrected GPIO8 NEGEDGE, USB/battery operation and display idle/motion/touch wake physically validated. No new watchdog/reset observed in the reported run. Not ACCEPTED/CLOSED: closure review below identifies missing gates. Historical failures/investigations remain intact. PhaseD stays accepted/closed; no firmware change, build, upload or Stage2.

## Entry and verified baseline (before source modification)

No Git metadata/commit. 1600-file SHA-256 baseline in %TEMP%/remote01-stage1-baseline.json, all hashes unchanged through baseline tests/clean build. Default 158 PASS/0 FAIL/306563 assertions; soak 158/0/2999813, existing45000 iterations plus ingress10000 cycles. Clean/build successful; build517.22s, flash1763977/21.0%, RAM19776/6.0%, zero warnings/errors. Accepted Phase D flash1763953: fresh unchanged-source baseline is +24 bytes, cause unassigned; artifact not byte-identical. Fresh BIN23f8fd861dc9ee6cbc089f11b081a958dabc2ba798a7f30b7609aec2f4231b51; ELF822cd7fc9cd7801ef9ae4207d8b5ff53ca9d154cc688b0fdc5dfc75fc562a470. Owner acceptance supplied no additional physical telemetry.

## Hardware audit and design decision recorded before implementation

Exact owner board: Waveshare ESP32-S3-Touch-LCD-3.49B V2, silkscreenV1.1. Official resource page identifies Rev1.1 as V2. Official V2 repository pinned 1c157e6e8e68b89fd4dc400f46bf1724cb64a57e; schematic titleV1.1, battery example ESP-IDF/07_BATT_PWR_Test and FactoryProgram inspected. No V1 board evidence used. Owner battery: Li-Po, 1000 mAh nominal with undocumented protection board. Its charge rating/protection thresholds are not inferred.

SOURCE -> VERIFIED FACT -> CONSEQUENCE:
- V2 schematic KEY&POWER and official battery user_app.cpp: PWR grounds Q2 gate viaD4; EXIO6 SYS_EN high drives T1 to maintain battery path -> assert latch early after I2C/expander availability, before LCD reset/delays; deassert only after controlled shutdown. GPIO16 SYS_OUT is affected by asserted hold, so cannot be treated as an independent PWR/USB detector.
- Schematic power R3=200k/R15=100k, BAT_ADC=GPIO4; official V2 adc_bsp ADC1 channel3, 12-bit12dB curve fitting, x3 -> bounded calibrated measurement, no uncalibrated guessed3.3V conversion/SOC.
- Schematic ETA6098 STAT drives LED only; VBUS has no dedicated MCU sense -> charger/external power UNKNOWN, never infer absent USB from serial disconnect or voltage. Automatic critical hard-off is inhibited without verified external-absence evidence; explicit DEVELOPMENT_ONLY armed release can validate the circuit. Critical decision/filter/hysteresis are still implemented/testable without discharge.
- Schematic HM5411H/HM8205 protection; manufacturer table specifies nominal2.9V discharge detection -> conservative firmware LOW3.6V/recover3.7V and CRITICAL3.4V/recover3.55V warning/decision thresholds above protection, sustained qualification30s; not precise SOC, cell certification or a replacement for protection.
- QMI8658 I2C0 address0x6b, INT1=EXIO2, INT2=EXIO3, TCA9554 INT=GPIO8 viaR39 -> use documented WoM200mg, low-power21Hz, INT1 initiallyhigh/activeLow; event IRQ with bounded register reads, gyro off. IRQ failure inhibits system sleep; touch remains active-safe fallback.
- Touch interrupt=EXIO0; expander low INT represents any input change -> shared GPIO8 light-sleep wake, normal raw touch polling retained, first wake gesture consumed. No inaccessible PWR daily wake.
- Accepted LEDC GPIO42 duty + BL_EN EXIO1 -> gate backlight only; preserve brightness duty. No unverified panel sleep/reset/rotation sequence.
- ESP-IDF5.5.3 light sleep retains RAM and supports GPIO wake -> bounded light-sleep intervals with timer safety fallback; cooperative Core and LVGL quiesce/resume, captured-session fencing, cleared buffered intents/serial input. Deep sleep/hard-off not daily idle modes. No PM/network configuration change.

Implementation uses a small fixed-storage portable policy plus one hardware worker and narrow copied/atomic infrastructure hooks. Physical ACTIVE timeout30s; automatic system sleep is inhibited entirely because external power/charging are UNKNOWN. DISPLAY_IDLE keeps Core running. The300s model gate requires independently verified external ABSENT, which this board cannot supply; Z alone remains an explicit development override. Motion never emits HOME intent. System sleep refused for pending/uncertain work or failed wake prerequisites. The current owner procedure below uses USB cold start/handover and never requires access to PWR.


## Authoritative references

- [Waveshare V2 resources](https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.49/Resources-And-Documents), exact V2 repository snapshot below. The V2 schematic filename identifies the board generation; its V1.1 title identifies PCB revision, not a substitute V1 board. Earlier hardware-document source restrictions predate this verified Stage 1 audit.
- [Pinned V2 schematic](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/blob/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/schematic/ESP32-S3-Touch-LCD-3.49%20V2.pdf): visually inspected full sheet and KEY/POWER, ADC divider, IMU and expander regions.
- [Pinned battery example](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/tree/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/ESP-IDF/07_BATT_PWR_Test): main, user_config, button_bsp, user_app, manifests.
- [Pinned V2 FactoryProgram](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/tree/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/ESP-IDF/11_FactoryProgram): main/config/adc_bsp. Existing factory producer tasks remain disabled.
- [QMI8658C datasheet](https://files.waveshare.com/wiki/common/QMI8658C_datasheet_rev_0.9.pdf), sections 5.9/9, plus existing SensorLib register constants/WoM sequence. Checked implementation avoids the existing helper's read-error-as-command-success behavior.
- [HM5411H manufacturer table](https://hmsemi.com/index.php/pro_v_2_4540.html) and [ETA6098 datasheet](https://www.eta-semi.com/wp-content/uploads/2022/03/ETA6098_V1.1.pdf). Board circuitry does not establish the installed cell/protection-board specification.
- [ESP-IDF 5.5.3 S3 sleep documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html), verified against installed esp_sleep.h, esp_timer and USB Serial/JTAG sources. Firmware uses repository IDF5.5.3, not the reference example's IDF5.5.4.

## Implemented behavior

| State | Entry / operation | Exit |
| --- | --- | --- |
| ACTIVE | Boot or physical touch/motion. Existing HOME, brightness, carousel and owner behavior run normally. | 30s physical inactivity -> DISPLAY_IDLE. |
| DISPLAY_IDLE | BL_EN EXIO1 low; PWM brightness duty retained; panel/controller retained; Core and touch polling continue. | Touch/motion -> ACTIVE; Automatic system sleep inhibited for UNKNOWN/PRESENT source; stays DISPLAY_IDLE indefinitely until physical wake. |
| SYSTEM_SLEEP (DEVELOPMENT_ONLY Z) | Explicit forced test only; never automatic on this board. Valid calibrated measurement, asserted hold, initialized WoM/GPIO wake, owner and LVGL parked; refuse pending or unresolved effect work. Manual light sleep, RAM/PSRAM retained, GPIO8 low wake, 2s timer fallback for battery/IRQ health checks. | Touch/motion -> coherent ACTIVE; source/read/handshake/sleep failure -> safe refusal/resume. Timer alone does not light the screen or refresh user activity. |
| CRITICAL_SHUTDOWN | Separate DEVELOPMENT_ONLY K then ! within 10s. Core boundary closed, UI parked, display off, successful expander release required. | Successful hold release is terminal, even if USB keeps MCU powered; no reassert/reboot loop. Failed quiesce/output retains hold and resumes safely. |

Normal idle never enters SYSTEM_SLEEP, deep sleep or hard-off. Automatic system sleep requires trustworthy external ABSENT; neither real voltage nor simulated voltage supplies that missing observation. Daily wake requires motion/touch, not inaccessible PWR. Timeout constants are centralized in the portable model. Serial diagnostics and Mock keys do not reset physical activity; I explicitly forces display idle; Z explicitly overrides the normal sleep prohibition for controlled development testing only.

Power hold is asserted unconditionally during app_main: I2C initialization -> example_lcd_exio_init -> SYS_EN/EXIO6 output high -> LCD initialization. This executes on USB boot as well; no PWR, battery-boot, GPIO16 or USB-source condition guards it. Required cold start/recovery is off -> connect USB -> hold asserted -> disconnect USB with battery attached -> continue running. Power hold is asserted immediately after expander creation, before LCD reset/delays. TCA9554's verified reset output latch is 0xff; its API rejects set_level on an input, so output direction precedes high set_level as in the working official example. Worker is the sole post-startup expander-output owner; original startup finishes before it starts. GPIO8 initialization configures the verified shared interrupt, not new touch GPIO/controller settings.

### Measurement and safety

Dedicated worker samples every 2s: four bounded ADC reads, rounded mean, eFuse curve-fitting conversion to ADC mV, exact schematic divider x3. Five-sample median then EMA (3 old + median)/4, with1mV convergence when integer rounding would stall at a steady threshold. No guessed reference voltage, SOC percentage, charger state or battery-present claim. Valid range2500..4500mV; failed calibration/read or out-of-range sample invalidates current measurement/filter history/qualification. Last numeric voltage may remain in diagnostics but valid=0 and HOME displays --. HOME displays filtered voltage rounded to0.1V in the existing 38px/font12 status slot, e.g.4.0V. SIM appears during test injection. BAT-node voltage alone cannot prove a cell is attached when USB powers the charger.

LOW <=3600mV, recovery >=3700mV. CRITICAL <=3400mV after a full median window and 30s continuously qualified low median/filter; critical recovery >=3550mV. Invalid/high samples cancel qualification; isolated transients cannot hard-off. Thresholds are conservative warning/decision thresholds above nominal board protection detection, not a cell-specific cutoff or precision SOC. Board divider tolerance and ADC/load variation require owner voltage comparison.

**Automatic critical hold release is INHIBITED: external power is UNKNOWN.** Real critical condition produces an explicit bounded warning; it does not claim external absence or shut down a possibly USB-powered board. Portable policy requests shutdown only for qualified CRITICAL plus verified external ABSENT. Safe simulation uses ABSENT on a separate synthetic model to prove the decision; it cannot reach the physical release path. The actual board currently has no trustworthy external-source observation to enable that automatic branch.

**OWNER DECISION / HARDWARE EVIDENCE REQUIRED:** accept this exact limitation explicitly during Stage 1 acceptance, or provide a verified external-power observation/control before automatic shutdown can be completed. Obtain the installed cell's charge specification/protection thresholds before claiming cell-specific safety or charging compatibility. Owner supplied Li-po,1000 mAh, protection board present but undocumented; capacity is not used to fabricate endurance/SOC. Firmware changes no charger/current setting. The board's analog supply switchover handles USB insertion/removal while firmware maintains SYS_EN; no unreliable serial-disconnect or SYS_OUT heuristic is used. Undervoltage protection remains hardware-backed, with the installed protection board's limits unknown.

### Motion, display and coherent resume

QMI8658 WHOAMI05 verified; bounded reset/CMD acknowledgement and register readback. Accel +/-8g, low-power21Hz, gyro disabled; WoM200mg, INT1 initially high/active low, four-sample blanking. No continuous acceleration stream or sensor-library objects. ISR only notifies worker; status read acknowledges WoM; expander input read refreshes its interrupt comparison before sleep. Stuck GPIO8 or I2C/WoM error refuses/resumes sleep instead of busy-looping. Motion resets activity but never generates a HOME intent.

Worker owns hardware/policy only; it never accesses Core, Mock, LVGL or their allocator. Neutral copied BatteryView and atomic park/activity hooks are the only UI/power interface. Core's existing owner handles all lifecycle operations. Before sleep it checks pending and unresolved effects, captures bounded capability availability/reachability, closes the existing Phase D inbox, invalidates old samples and publishes unavailable snapshot, discarding queued HOME presses. Resume reattaches through the same boundary (new incarnation/session), restores only surviving prior capabilities/availability, reacquires synthetic device state through the existing owner acquisition, drains bounded serial input and publishes before releasing LVGL. Withdrawn/context-disabled/disconnected states are preserved; no 'r' Mock normalization or playback command is invented. Pending/uncertain work refuses sleep until existing semantics resolve it; no effect/history is erased to force sleep.

LVGL parks after its current handler/flush finishes, stops its5ms tick timer, then acknowledges. Resume resets LVGL input gesture state, invalidates screen, restarts the tick and renders on the existing task/lock. Rotation, LCD init, touch coordinates, page IDs/order/sentinels/snapping and PWM implementation remain unchanged. Wake contact is consumed until release; short200ms settling prevents pickup/touch from becoming playback/carousel input. A second deliberate touch works normally. Runtime records manager-handling-to-ready latency; it is not a measurement from physical IRQ edge or photon response.

Automatic system sleep is inhibited on both USB and battery because source presence cannot be established. Only explicit DEVELOPMENT_ONLY Z may enter the existing guarded light-sleep path. Such a forced test may disconnect/re-enumerate USB Serial/JTAG; IDF5.5.3 has no selected USB wake API. Resume using motion/touch; reattach PlatformIO monitor if needed. Normal operation always refuses automatic system sleep, including charging/USB-only operation, and supports display idle plus motion/touch wake. Timer fallback periodically checks battery and WoM health. Full panel sleep/deep sleep deferred for this artifact: no verified panel sequence and inaccessible-PWR recovery are unsuitable.

### Development diagnostics and safe hooks

Same bounded USB Serial/JTAG driver reader/serialized owner as Phase D; old lowercase Mock keys and m unchanged, CR/LF ignored. New uppercase keys are distinct; one-slot worker mailbox reports queued=0 if full, never retries/replays. Send keys separately and wait for acknowledgement.

| Key | DEVELOPMENT_ONLY operation |
| --- | --- |
| B | POWER/BATTERY snapshot: raw/calibrated/filter/valid/condition, UNKNOWN source/charge, real vs SIM, state/hold/wake/idle, sleep/IRQ/read/error counters, last SDK wake cause and manager latency, worker stack minimum and internal/PSRAM free. |
| I | Force DISPLAY_IDLE; still uses normal touch/motion wake. |
| Z | Explicit DEVELOPMENT_ONLY override of automatic-sleep prohibition; guarded hardware/owner/UI prerequisites remain. Controlled test only, not normal product policy or a required acceptance step. |
| L / C / H | Separate simulated input3550 /3200 /3800mV; preserve simulation filter/hysteresis after activation. HOME clearly SIM; real measurements and safety prerequisites remain separate. |
| N | Disable injection; return HOME to real voltage/--. |
| K, then ! | Arm actual terminal hold-release test for10s, then execute separately. Neither LOW/CRITICAL injection nor normal operation can invoke it. Missing/expired arm refuses. |
| m | Original UC_RUNTIME and UC_INGRESS diagnostics unchanged. |

Boot explicitly logs Stage1 DEVELOPMENT_ONLY, injection=0, source/charge UNKNOWN, SOC unavailable, hold control, wake wiring and prerequisites. Transition/condition/explicit snapshot logs only; no per-sample or acceleration spam. Critical simulation logs decision=1 dry_run=1 hold_release=0 once per critical episode.

## Failures found and corrected during implementation

- Owner's battery-only power loss: SYS_EN omitted from initialization; verified EXIO6 high asserted before LCD startup.
- Preliminary latch ordering: expander set_level requires output mode. Verified reset latchff plus official direction-first sequence avoids fatal input-mode error.
- Preliminary hold monitoring mask omitted bit6; corrected read mask4d with actual hold readback.
- Existing SensorLib command helper can misreport read failure; minimal checked reset/ACK/readback instead, with fault matrix tests.
- Mock reconnect shortcut would restore disabled availability; bounded PowerSuspend preserves exact current capabilities/reachability, tests withdrawal/context/disconnect.
- Tick callback suppression alone would leave periodic5ms timer scheduled; stop/restart timer on LVGL owner handshake.
- Early acknowledgement timeout paths could release UI before Core readiness; UI stays parked until fresh owner snapshot acknowledgement.
- Integer EMA could stall1mV short of exact thresholds; converge to steady inputs and test exact3400/3550/3700mV.
- Expander faults could retry on every worker turn;2s input/output fault backoff, first touch consumed on fault resume, and bounded owner-resume failure log. LVGL timer restart failure uses a task tick fallback to retain UI operation.
- Preliminary critical warning could repeat every sample and motion could suppress an active touch; log condition transitions and consume contacts only on wake.
- Target compile probes found uint32_t pointer type mismatch and unconstrained snprintf truncation; exact SDK type and bounded digit formatting fixed, no warning suppression.
- Linking SensorLib unnecessarily exposed new Kconfig options; use only its existing constant header as a private include. Original config regenerated and hash-matched during audit; final configuration is unchanged.
- New completion test incorrectly expected an extra queued event after the existing bridge's synchronous FIFO acknowledgement; corrected to assert confirmed late result/deadline and empty inbox. Existing tests unmodified.

## Verification and resources

| Suite | Default PASS/FAIL/assertions | Soak PASS/FAIL/assertions |
| --- | --- | --- |
| Core | 69/0/2425 | 69/0/2425 |
| Profile | 6/0/40 | 6/0/40 |
| HOME | 39/0/3541 | 39/0/3541 |
| Phase C hardening | 26/0/278368 | 26/0/2773618 |
| Phase D ingress | 18/0/22189 | 18/0/220189 |
| Stage 1 power | 14/0/8154 | 14/0/80154 |
| **Total** | **172/0/314717** | **172/0/3079967** |

Complete existing suites retained, unchanged test assertions/implementation. Default6500 combined stress iterations (4500+1000+1000); soak65000 (45000+10000+10000). Stage1 cycles alternate sleep/motion/touch resume, reject stale queued HOME/ingress, preserve disabled/withdrawn/disconnected state, assert no duplicate dispatch, assert payload plateau and allocation return on test destruction. New14 tests include calibrated validity/out-of-range/read failure; median/EMA/transients; threshold hysteresis/qualification/exact convergence; UNKNOWN/PRESENT external inhibition; physical timer/wake/refusal; first-contact consumption; real suspend helper ownership/pending and >3s late completion; full16-operation WoM read/write failure matrix, wrong identity/reset/ACK timeouts; expander/display failure, quiesce/refused/failed/successful release through the actual output seam. No destructive battery discharge or host hardware claim.

Stage1 host sleep-cycle payload: defaultmaximum/final98497 bytes; soak98501/98501 (text ordinal size changes across runs), <=warmed+256 guard, zero dispatches. Each test releases all tracked allocations. Existing soak acquisition20000 iterations: allocations580000/frees580000, settled98490/largest98506/final98506. Existing allocator-failure and ownership/accounting regressions pass. Hardware-driver heap/ADC setup/USB buffers are outside Core allocator accounting; final B/m owner observations are required for total runtime memory/failed allocation/stack claims. Undefined-behavior sanitizer attempted but unavailable: Windows GCC cannot link -lubsan, as already documented in Phase D; not counted as a passing sanitizer run.

New POWER task4096-byte internal stack, priority2 CPU1; existing UC owner24576 and LVGL4000 unchanged. Target static frame checks compiled final sources with existing flags and -fstack-usage outside artifact build: POWER worker496, Board initialize96, battery sample80; owner task544 and PowerSuspend::resume1280 bytes. These individual frames are not complete call-chain or measured headroom; B/m stack minima remain owner validation. Target Session83656 bytes is unchanged: **wrapper PSRAM delta0**. Fixed PowerSuspend140 bytes on existing owner stack; Battery64+Machine48 bytes, fixed windows/atomics/one-slot mailbox; no power policy heap/PSRAM queue. Additional task TCB, GPIO/ADC calibration driver initialization allocations and actual total PSRAM delta require runtime measurement; no whole-system dynamic-memory number is invented. UI has the same objects and a battery pointer/text update, no new layout/assets/fonts.

### Exact changed files

- main/main.cpp; main/CMakeLists.txt: early hold, worker startup, LVGL park/tick/input/reset/redraw hooks.
- components/control_home/runtime.cpp; components/control_home/CMakeLists.txt; new components/control_home/include/control_home/power_suspend.hpp: same owner/USB reader, bounded suspend/resume, power development keys and inactive intent guard.
- components/user_app/user_app.cpp; components/user_app/CMakeLists.txt: neutral copied voltage presentation in existing battery label.
- New components/power_management/CMakeLists.txt, model.cpp, hardware.cpp, hardware.hpp, runtime.cpp; include/power_management/api.hpp, model.hpp, output.hpp, qmi_wom.hpp: fixed policy, checked hardware and worker.
- tests/control/run_tests.py; new tests/control/power_tests.cpp: additive suite.
- CHANGELOG.md; docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md: this milestone report/history entry. Total20 files, including12 additions.

### Frozen contract and initial Stage 1 build (historical; superseded)

Frozen CCM SHA-256: 7a46f9fffe706476de458aac078d0af1c94844df339248d3f9f4b5cf033011d7 (unchanged). Every existing components/control source/header/CMake, Phase D ingress, Mock/development.cpp and development.hpp, facade/API, LVGL/ui_bsp/brightness, GPIO constants, drivers, SDK/project defaults and platformio.ini match the1600-file entry baseline. sdkconfig.remote01 SHA-2565e1757dd81d2a891c4e8863fa4993b0bc643baeaef766399d644ff27a1441f0d unchanged. Only7 existing code/test integration files plus CHANGELOG intentionally changed; original changelog bytes retained. No Wi-Fi/network/TUNER application code or configuration enabled. Existing SDK network libraries may still compile as in baseline; compilation does not enable networking.

Initial Stage1 exact commands, after the original source/tests/soak (historical; amended artifact below supersedes this build):

```text
pio run -e remote01 -t clean -j 1
pio run -e remote01 -j 1
```

Clean SUCCESS (1.40s); build SUCCESS501.97s, exit0, **zero warnings/errors**. A prior in-progress build was explicitly stopped for final fault-guard corrections; it is not the final artifact. Subsequent full tests/soak and exact clean/build above succeeded. No upload.

| Resource | Final | Delta vs fresh entry baseline | Delta vs accepted Phase D |
| --- | --- | --- | --- |
| Flash | 1801465 bytes /21.5% of8388608 | +37488 | +37512 |
| Static RAM | 20192 bytes /6.2% of327680 | +416 | +416 |
| Core Session PSRAM wrapper | 83656 bytes | 0 | 0 |
| POWER internal task stack | 4096 bytes, plus SDK TCB/driver heap | +4096 stack | +4096 stack |

Dynamic total internal/PSRAM usage, physical stack minima, actual current/endurance/latency remain OWNER PENDING; use B/m. Baseline's unexplained+24flash bytes are preserved in the separate accepted-vs-fresh delta, not attributed to Stage1.

Initial Stage1 artifacts (SUPERSEDED; do not flash for corrected acceptance):
- .pio/build/remote01/firmware.bin SHA-256: `269f10faa4a3be7fb435dcf5f229f287612a0529b7a35111bc4f8d938ec1d896`
- .pio/build/remote01/firmware.elf SHA-256: `153d6071f95a050ee760cf71040321f1d092c46d49ea7b2d79d6b725e571d452`

Initial manifest/freeze audit matched its reviewed source; exact config and all protected baseline sources unchanged. The initial20-file change set is enumerated above; the owner correction touches only the6 files listed in the amendment; no unrelated source edits. Historical CHANGELOG bytes retained. Stage1 is not owner-accepted, and automated critical hard-off remains evidence-inhibited. Hardware acceptance is ONE session below.

Host tests prove portable logic/actual owner suspend helper and fake register/output seams; they do not prove electrical wake, battery voltage accuracy, touch IRQ polarity on owner hardware, current consumption, USB enumeration, stack headroom or endurance. Those remain in the one final physical acceptance session. No current/endurance prediction is invented. Relative intent: DISPLAY_IDLE removes backlight load; SYSTEM_SLEEP additionally pauses UI/Core/CPU in bounded intervals while panel/RAM/PSRAM/IMU retained. This is intentionally less aggressive than an unproven deep-off implementation.

## Adversarial freeze review

Exact V2 Rev1.1 evidence: yes. USB boot/hold/battery handover: unconditional software path verified, physical result PENDING. Conversion: calibrated schematicx3, not guessed. Percentage: removed. Transient critical: filtered/time-qualified and automatic release inhibited. Reboot loop: no release/reassert cycle. Cold start/recovery: USB connection only, no accessible PWR assumed. Normal power saving stops at DISPLAY_IDLE, with touch/motion wake;2s sleep fallback is forced-test-only; failed prerequisites refuse sleep. False motion/touch HOME command: no synthetic intent, first wake contact consumed. Old UI/ingress replay: closed inbox plus new session and unchanged scope validation. Fresh HOME: owner reacquisition before LVGL restart, redraw/input reset. Repeated transitions: bounded storage and tested allocation plateau. Polling: no high-rate IMU stream,2s sampling/50ms notified worker control; parked Core/UI use yielding20ms waits. Queues: existing fixed queues plus one atomic byte slot. HOME/Core hardware knowledge: none added. Wi-Fi/TUNER: no application/config changes. Frozen/UI changes: only truthful battery text and idle/wake infrastructure. Diagnostics: explicit/transition logs; safety failures refuse release/sleep. Automatic critical cutoff remains explicitly incomplete pending hardware evidence/owner decision.

## Owner corrections and source audit ? 2026-10-04

Installed battery: **Li-Po,1000 mAh nominal, protection PCB present; specifications/thresholds undocumented**. Repository-wide case-insensitive capacity-reference search found one CHANGELOG statement and two report statements; corrected all. No battery-capacity value exists in runtime, tests, comments or configuration. Capacity was never used for voltage thresholds, SOC, protection cutoff, charger behavior, safety, timers or policy. Existing ADC conversion/filter, safety thresholds, timeouts and charger settings are unchanged by the capacity correction.

Re-audited the pinned V2/PCB Rev1.1 power, key, MCU and expander schematic plus official07_BATT_PWR_Test. ETA6098 STAT9 connects to charging LED D5/R10, not MCU/expander; VIN connects VBUS. VBUS is on power/header nets, not an MCU sense input. SYS_OUT/GPIO16 follows the diode/button/hold network and is influenced by SYS_EN/T1; official startup power_Test reads it once to classify a boot, not continuous external presence. That flag is not a trustworthy USB/charging observer after firmware holds the circuit. Neither USB enumeration/serial traffic nor BAT voltage proves source absence. No new pin, voltage-divider or charging heuristic added.

Correction: normal Machine::tick requires hardware-safe **and verified External::ABSENT** before SYSTEM_SLEEP. Default UNKNOWN and PRESENT both inhibit it. Runtime passes real.external (always UNKNOWN); simulated ABSENT remains isolated in the simulation model. Normal production reaches DISPLAY_IDLE only, regardless ofUSB/battery. Boot/B explicitly report automatic_system_sleep=INHIBITED reason=EXTERNAL_POWER_UNKNOWN; sleep_ready denotes only forced-test hardware prerequisites. Z remains an explicitly marked DEVELOPMENT_ONLY override with existing safety guards. The original threshold/owner-helper stress fixtures now state their synthetic ABSENT assumption explicitly; they no longer silently model source knowledge available on the real board.

No startup firmware change needed: main/main.cpp confirms early hold on every USB boot, before display delays and before profile selection. Normal recovery uses USB; no enclosure button access is required or assumed.

Affected verification (only the changed power policy and existing power/owner-helper regressions rerun): default15 PASS/0 FAIL/28158 assertions/1000 sleep-resume fixture cycles; soak15/0/100158/10000 cycles. New test simulates10000 maintenance turns (>5hours) for UNKNOWN and PRESENT separately, with zero automatic sleep requests; both remain display-idle and retain touch/motion wake. Synthetic verified-ABSENT fixtures explicitly exercise the existing guarded sleep/resume path. Full original172-test suite results above remain historical; unchanged Core/PhaseD/UI suites were not unnecessarily rerun for this confined correction.

Exact amendment scope: components/power_management/include/power_management/model.hpp; components/power_management/model.cpp; components/power_management/runtime.cpp; tests/control/power_tests.cpp; docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md; CHANGELOG.md. No added files. SYS_EN startup, GPIOs, ADC/thresholds, fonts, display/touch/brightness/carousel, Core/frozen CCM, PhaseD/Mock and SDK configuration unchanged.

### Final amended artifact verification

Exact commands: `pio run -e remote01 -t clean -j 1`, then `pio run -e remote01 -j 1`. Clean SUCCESS (1.84s); build SUCCESS (516.14s), exit0, zero warnings/errors. Flash1801689 bytes /21.5%, **+224 bytes versus the initial Stage1 artifact**; static RAM20192 bytes /6.2%, **delta0**. No added task, heap policy storage or changed Session wrapper. Delta versus fresh entry baseline: flash+37712/RAM+416; versus accepted PhaseD: flash+37736/RAM+416.

Use only these amended artifacts for the acceptance session:

- `.pio/build/remote01/firmware.bin` SHA-256: `26b53bef47bef55bdb9b17f18ea6d391bb72c12490bc5dde20cea17ea2ab3d04`
- `.pio/build/remote01/firmware.elf` SHA-256: `c90dbddbede88a48ca6cc11b337928b0a7b3d2957962f8cad3be6703708a5dc4`

Final 1612-file baseline audit confirms exactly the six amendment files above changed and no additions. Executable source/tests match the pre-build manifest; subsequent changes are documentation only. Frozen CCM, main startup and protected hardware/Core/configuration sources remain byte-identical to amendment entry. No remaining former-capacity references. Original build/hash and complete-suite measurements above are historical, superseded where applicable; only the affected power suite was rerun for this amendment. No upload; Stage2 not started.

## Normal owner acceptance procedure - STOPPED after hardware failure

The historical corrected procedure below is suspended. Use only the focused reset-capture procedure at the end of this report until the defect is resolved.

1. **Final artifact / USB cold start.** Verify the amended BIN hash recorded above/below and flash only that artifact using the existing owner procedure; Codex does not upload. Battery is the protected1000 mAh Li-Po. Start with REMOTE off and battery connected; connect USB. Do not open the enclosure or press PWR. Monitor COM3/115200: early_hold=ASSERTED SYS_EN=EXIO6, UC_RUNTIME profile=0, injection=0, calibrated ADC/WoM status, and automatic_system_sleep=INHIBITED reason=EXTERNAL_POWER_UNKNOWN. B reports source/charging UNKNOWN and real voltage/--.
2. **PRIMARY USB -> battery handover.** After the USB boot/hold marker and functional UI, disconnect USB while leaving the battery attached. REMOTE must remain powered and functional. Insert/remove USB again during ACTIVE and DISPLAY_IDLE; no power loss/reset, unwanted hold release or Core corruption. Any cold-start/recovery uses USB connection, never PWR. Keep this result distinct from source-code verification until physically observed.
3. **Battery telemetry.** With USB attached and battery-only, compare raw/mv/filter in B and HOME rounded voltage against a trusted physical measurement at battery/BAT node where practical. Record measurement, load/brightness and difference; observe2s filtering. No capacity-based SOC/range/charging inference, no fake percentage. SIM identifies injection; invalid measurement displays--. Protection limits remain undocumented.
4. **Display idle and motion/touch wake.** Let30s physical inactivity elapse, or send I. Backlight goes dark while normal Core remains operational. Pick up/move: screen wakes with preserved brightness/page and coherent HOME, no playback intent. Repeat with touch: first touch wakes only; release, next deliberate touch operates. Check HOME/factory/brightness pages, rotation, carousel0->1->2 and reverse/snapping, accepted buttons/icons and PhaseD basic capability/session/ingress/delayed-completion regressions. Repeat at least20 idle/wake cycles, including battery-only handover.
5. **Charging/external-power automatic-sleep prohibition.** With battery attached and USB connected, leave untouched at least10min (beyond former300s threshold); do not send Z. Display idle is expected. B must show DISPLAY_IDLE, automatic_system_sleep=INHIBITED, sleep_requests=0, rounds=0 on a fresh boot with no forced test. Core m remains available; no owner_parked/system-sleep transition. Motion/touch wakes normally. Repeat at least10min battery-only after handover; the same prohibition applies because external source is UNKNOWN. Reconnect USB for B/m if needed; display must not have required PWR recovery. Neither charging LED nor serial connection is interpreted as source telemetry.
6. **Safe LOW/CRITICAL and endurance.** N,H (40s), C then H within2s: no critical decision/release. C70s then B: SIM CRITICAL, dry_run=1, hold_release=0; L/H test recovery/LOW, N restores real view. Real external/charging UNKNOWN and automatic sleep remain inhibited during injection. At least30min battery-only total, natural display idle and repeated pickup/touch wake; record measured start/end voltage and elapsed time rather than capacity-derived endurance. B/m: hold retained, sleep_requests/rounds0 without Z, sane stack/heap/accounting/ingress counters, no crash/reset/watchdog/freeze. No deep discharge required.
7. **Optional development-only tests, outside normal-policy acceptance.** Z is not required for acceptance and must never be sent during the automatic-sleep-prohibition test. If owner deliberately selects a controlled forced-light-sleep test, confirm working motion/touch and valid hardware prerequisites first; it may interrupt USB monitoring. Wake by motion/touch, reattach monitor as needed, inspect new session/coherent HOME and no stale replay. A separate K then ! actual terminal hold-release test is also optional, not simulation or normal shutdown: after terminal release, disconnect USB to turn off, then reconnect USB to recover and reassert hold. No step requires PWR. Do not confuse either override with automatic product sleep or automatic critical cutoff validation.

This normal procedure is STOPPED following the owner reset failure below. Stage1 is not accepted; use only the focused reset-capture retest. No Stage2 work or upload.

## Hardware defect / focused reset investigation - 2026-10-04

Owner flashed the final amended firmware: USB cold start **PASS**, early SYS_EN hold after USB removal **PASS**, immediate battery-powered display/touch/UI **PASS**; sustained battery operation **FAIL**. Approximately5–10s after USB removal, REMOTE restarts, repeating on battery. Inaccessible PWR was unused. Reconnecting USB did not recover the battery-only reset log. **Root cause UNKNOWN**; no attribution to battery sag, brownout, watchdog, software restart, SYS_EN loss or other cause.

USB-powered physical observations: `POWER: state=DISPLAY_IDLE wake=0 hold=1 display=0`, `POWER: state=ACTIVE wake=2 hold=1 display=1`, `POWER: manager_wake_to_ready_ms=202`. Display idle/wake operate physically; these partial passes do not close acceptance.

### Path audit

| Path | Finding; no cause inferred |
| --- | --- |
| Restart/panic | No Stage1 esp_restart or explicit abort. Existing startup ESP_ERROR_CHECK and display-buffer/mutex assertions can panic on failed prerequisites; unchanged. Source inspection cannot exclude SDK/driver faults or exceptions. |
| SYS_EN/TCA | Expander created/reset once in synchronous startup, EXIO6 output/high before LCD. Later direction masks0/2/3 inputs and1/7 outputs exclude bit6. Driver read-modify-write preserves unmasked bits. Worker initializes ADC/QMI only, never recreates/resets TCA or reconfigures EXIO6. Normal output writes BL_EN bit1; bit6 low requires explicit armed K then !. Electrical integrity is not established by this software audit. |
| Sleep | Real external defaults UNKNOWN, never assigned; only separate simulation gets ABSENT. tick requires safe && ABSENT after300s, runtime supplies real UNKNOWN. Z is the only other SYSTEM_SLEEP entry; no deep-sleep call. Configuring wake sources does not enter sleep. Entry ELF Machine::tick+0x65 compares External with2/ABSENT and combines safe before writing SYSTEM_SLEEP. New instrumentation leaves this policy unchanged. |
| Battery/ADC |2s calibrated sampling; failed/out-of-range measurement invalidates telemetry/sleep readiness. LOW/CRITICAL log inhibited shutdown; simulation is dry-run. No automatic reset/hold release or capacity-based behavior. |
| IMU/faults | Immediate QMI-only reset/WoM configuration, ACK bounds500/1000ms, bus20ms. Runtime status failure disables motion/sleep prerequisites. Expander/display faults back off2s, retain hold and refuse sleep/request ACTIVE. No fault-triggered restart. |
| Watchdogs | Interrupt WDT800ms; task WDT5s watches idle CPU0/1, task-WDT panic disabled. Panic print/reboot delay0s. POWER normally yields50ms; delays/handshakes yield. No Stage1 watchdog registration/feed/removal/config change. Actual watchdog events/blocking SDK or logging faults remain possible, unproven. |
|5–10s timing | Accepted HOME owner has unchanged5000ms startup delay before Session/USB driver setup. POWER samples2s, display idle30s, unavailable auto-sleep gate300s; explicit K arm expires10s. No USB-removal callback/source detector or delayed hold initialization found. Timing similarity proves no cause. |

Actual SDK from project_description.json is framework-espidf@3.50503.0 / ESP-IDF5.5.3. Audited that package's esp_system.h, S3 reset_reason.c, cpu_start.c and esp_attr.h. Supported S3 mapping distinguishes software/panic/task/interrupt/other watchdog, USB and brownout (hardware reset or supported hint). Numeric reasons remain available even for unmapped categories.

Entry executable source matches the prior amendment manifest; only documentation differs. Entry on-disk BIN hash was `5daea9f9a01ab015b26dd73bf296b2d55f6377e017ad2153b982ebc9bc9f05dc`, different from the documented amended hash. No owner flashed-artifact hash was supplied: independently verified byte identity is not claimed. The inspected entry ELF contains the automatic-sleep guard. New diagnostic hashes below supersede earlier artifacts for this retest.

### Retained diagnostic design

Fixed RTC_NOINIT_ATTR history640 bytes: four boots, two80-byte copies each. Magic/version1, semantic bounds and FNV word checksum validate records. Destination magic is invalidated, bounded words committed with memory fences, magic written last. Alternating copies preserve the preceding checkpoint if interrupted. An80-byte internal-RAM working record is the only new mutable non-RTC storage. No heap/PSRAM/task/NVS/filesystem writes or ISR logging. app_main owns writes before POWER starts; POWER alone writes/reports thereafter.

First app_main action saves esp_reset_reason() and esp_sleep_get_wakeup_cause(), before hardware setup. Checkpoints capture hold/setup/runtime, expander/IMU/ADC begin/result, commands/transitions/display/release/forced sleep; retain RTC-epoch boot count, POWER-runtime-reached flag, last manager state, REAL battery condition/validity/last known voltage/filter, hold observation, event/detail/uptime and sleep counters. Runtime-ready means POWER entered its normal loop, not proof of HOME/Core success. Unavailable initial state/hold are4/2; invalid voltage fields may be stale. Wake0 is undefined on normal boot; SDK wake is retained after successful forced sleep. Counters/uptime are32-bit diagnostic values, not lifetime accounting.

POWER_RESET prints only after boot hold and on existing development B: header plus at most four rows, no polling log spam. A row's reset reason describes the reset **leading into that boot**; its runtime/state/event/voltage describe **that boot before the next reset**. Pair bootN reset with bootN-1 checkpoints. brownout=1 only for SDK ESP_RST_BROWNOUT; neither an operation checkpoint nor a reset category alone establishes the complete root cause.

Retention is best effort. SDK startup excludes rtc_noinit from bss clearing; power loss/rail failure, reset before app_main, reflash or repeated resets can remove/overwrite evidence. Missing/invalid rows print retained=UNAVAILABLE; boot count starts1 when no valid record exists. Missing retention does not prove power loss. Reconnect promptly after the first restart to preserve the four-boot window.

References: [IDF5.5.3 RTC_NOINIT_ATTR](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-guides/memory-types.html), [S3 reset-reason implementation](https://github.com/espressif/esp-idf/blob/v5.5.3/components/esp_system/port/soc/esp32s3/reset_reason.c), [reset API](https://github.com/espressif/esp-idf/blob/v5.5.3/components/esp_system/include/esp_system.h).

### Diagnostic verification / artifact

Complete regression **175 PASS/0 FAIL/343748 assertions**,6500 combined stress cycles; complete soak **175/0/3108998**,65000 cycles. Power17/0/37185 default;17/0/109185 soak. Existing Core/Profile/HOME/PhaseC/PhaseD assertions unchanged. Two additive tests check zero/random/corrupt/version/semantic rejection, each word corruption, torn commit/fallback, prior runtime/voltage preservation, four-boot retention over1000 boots/10000 updates, sequence/boot wrap and fixed640-byte storage. Existing automatic-sleep inhibition, owner/ingress/suspend and allocation/fault regressions pass. No demonstrated software defect fixed; diagnostics collect missing evidence.

Commands: `python tests/control/run_tests.py`, `python tests/control/run_tests.py --soak`, `pio run -e remote01 -t clean -j 1`, `pio run -e remote01 -j 1`. Both host runs exit0, compiled with Wall/Wextra/Werror; clean SUCCESS1.96s, build SUCCESS510.47s, exit0, **zero target warnings/errors**. Sanitizer not rerun: the already documented Windows libubsan limitation remains; no sanitizer claim.

Flash **1804709 bytes /21.5%, +3020 versus the documented amended Stage1 build**; static internal RAM **20272 bytes /6.2%, +80**. RTC **640 bytes separately**, verified in ELF history symbol0x50000000/size0x280; current working record size0x50. No new task/stack reservation or PSRAM wrapper change. Existing POWER4096/owner24576/LVGL4000 task stacks remain; physical stack minima/retention await capture, not host proof.

Diagnostic artifacts for this focused retest only:

- `.pio/build/remote01/firmware.bin` SHA-256: `d6fd7ed04c1ae1dac1d26da882391697c03039df4cdf3d422ac95a867be2207d`
- `.pio/build/remote01/firmware.elf` SHA-256: `f9cb9baf0d8a322a7c70fcea58e83c08baba8001b6314762add72e1afe97642c`

Exact scope: modified `main/main.cpp`, power `CMakeLists.txt`, `hardware.cpp`, `runtime.cpp`, `tests/control/power_tests.cpp`, this report and CHANGELOG; added power `reset_diagnostic.cpp`, `include/power_management/reset_record.hpp`, `include/power_management/reset_diagnostic.hpp`. Ten files, three additions. Entry1612-file manifest confirms every other existing file unchanged, including policy model, Core/PhaseD/Mock, frozen CCM, GPIO/driver/UI/SDK configuration. Compiled-source manifest matches final executable sources/tests; only documentation completed after build. No upload or Stage2.

### Historical first reset-capture retest - completed; superseded below

1. Owner flashes only the diagnostic BIN matching the hash above. Keep protected1000 mAh battery connected. USB boot, COM3/115200 monitor; wait for HOME/UC_RUNTIME profile=0, send B and save complete baseline POWER_RESET/POWER/BATTERY output/boot count. Do not send Z, K or !; no PWR access.
2. Remove USB once. Note elapsed time to the first restart. **Reconnect USB immediately after that first restart**, before several repeated cycles. Keep battery connected; no deliberate shutdown or reflash before capture.
3. Reattach COM3 monitor; send B after HOME/serial is ready, then m. Save all four POWER_RESET rows, any SDK boot/reset/panic logs and B/m telemetry. B retrieves history even if startup logs were missed. Report restart time and whether reconnect/monitor attachment caused another boot. Interpret each reset-leading-into-boot reason alongside the preceding boot's checkpoint; do not infer cause from timing.
4. If retention is UNAVAILABLE or repetition persists, stop battery-only testing and return the capture first. No normal acceptance/endurance or repeated deliberate discharge; further investigation follows evidence.

Historical first-diagnostic status, superseded by the INT_WDT capture below: **HARDWARE VALIDATION FAILED — RESET OBSERVED AFTER HANDOVER / ROOT CAUSE UNKNOWN AT THAT TIME.** That diagnostic firmware was not a fix or hardware PASS. Normal acceptance remained stopped; no upload or Stage2.

## INT_WDT captured / GPIO configuration defect correction - 2026-10-04

Current owner evidence supersedes the earlier unknown-reset classification. **ESP_RST_INT_WDT is demonstrated; the blocking CPU/path is not yet captured.** Owner also observed at least one spontaneous reset with USB continuously connected. USB removal is not an established cause. USB cold start, early SYS_EN hold and immediate battery handover already PASS; sustained stability fails independently of power source. No captured brownout evidence; hold/voltage observations are not continuous electrical measurements at the reset instant.

Exact owner retained rows:

```text
age=0 boot=2 reset=5/INT_WDT brownout=0 wake=0 runtime=1 state=0
battery=1 valid=1 mv=4161 filtered_mv=4155 hold=1
event=COMMAND detail=66 uptime_ms=24901 sleep_requests=0 rounds=0
age=1 boot=1 reset=11/USB brownout=0 wake=0 runtime=1 state=0
battery=1 valid=1 mv=4071 filtered_mv=4064 hold=1
event=ADC_OK detail=0 uptime_ms=236721 sleep_requests=0 rounds=0
```

Boot2 reset5 identifies the reset ending boot1. Boot2 COMMAND66 is B, issued after reboot; it is not the failing operation. Boot1 ADC_OK means its four ADC conversions, calibration and filtering completed before that checkpoint; the record can remain unchanged during normal50ms waits until the next2s sample or IRQ/event. It does not prove the ADC caused, or was running during, the stall. No system-sleep requests/rounds were recorded in either boot.

Owner post-reboot POWER: automatic sleep INHIBITED; ACTIVE, hold1, wake2, requests0/rounds0/errors0, IMU1/motions2/bus_errors0, stack minimum1368 bytes. REAL calibrated battery4161mV/filter4155, valid1/NORMAL, external/charging UNKNOWN, read_errors0. Core: INT free180275/min177331/largest92160; PSRAM free7636184; failed allocations0/accounting corruption0; stack minimum15364; owner_ok1/queue_rejected0. These are owner physical measurements, not predicted values.

### Confirmed software defect; physical reset attribution remains pending

The Stage1 sequence configured GPIO8 falling-edge, installed/enabled its ISR, then called gpio_wakeup_enable(GPIO8, LOW_LEVEL) during normal awake startup. Actual pinned IDF5.5.3 gpio.c646–674 calls gpio_hal_set_intr_type with LOW_LEVEL, and S3 HAL writes GPIO.pin[8].int_type. Wake and awake IRQ share this trigger field. The call silently replaced the intended awake edge trigger with level-low while ISR routing remained enabled. The shared TCA interrupt can remain low until input comparison/IMU acknowledgement or during a held contact. GPIO ISR clears status and notifies POWER but does not acknowledge TCA over I2C. Hardware can immediately reassert a low-level interrupt and repeatedly occupy its CPU, preventing tick progress. This configuration conflict is proven from source and covered by a held-low regression; it is a valid INT_WDT mechanism. No panic CPU/backtrace was captured, so it is **not yet conclusively attributed to the owner's specific reset**.

Exact fix: omit GPIO low-level wake arming at awake startup; GPIO8 retains NEGEDGE. Only explicit development Z can reach the existing sleep path. Immediately before that sleep: mask GPIO8 ISR -> arm level wake -> call light sleep -> disarm wake -> restore NEGEDGE -> unmask ISR. Shared helper runs cleanup even after partial arm/sleep failure. Failed cleanup leaves IRQ masked, disables further sleep and retains existing task polling; it never enables an unsafe level ISR. Sleep policy, source UNKNOWN, IRQ pin/wiring, ISR notification, touch/controller setup, SYS_EN and Core ownership are unchanged. Level wake and awake edge use the same pin sequentially, not simultaneous conflicting modes.

Minimal additional diagnostics: boot/B read the actual S3 GPIO trigger register, log awake_irq_type and expected_negedge2, worker CPU and ISR-service allocation CPU. Current project installs service only from POWER CPU1; reused service reports core-1/unknown rather than guessing. This proves routing/configuration, **not the CPU that suffered the captured watchdog**. esp_reset_reason collapses CPU-specific IWDT information. Existing IDF panic_arch uses PANIC_RSN_INTWDT_CPU0/CPU1 to print the offending core and backtrace, and current panic.c writes to secondary USB Serial/JTAG when a host is listening. USB-connected stability retest can capture that reliable evidence without SDK modifications, panic hooks, flash coredumps or a larger retained record. Existing RTC history remains available via B if boot logs were missed; battery-only reset reason alone cannot recover the CPU retrospectively.

### Other Stage1 paths audited

- Worker view_mux covers only small fixed BatteryView copies; no ADC/I2C/calibration/logging/RTC checksum/wait inside it. ISR only notifies/yields; allocation/core mailboxes remain the accepted bounded PhaseD paths. RTC commits have memory fences, not interrupt-disabled critical sections.
- ADC SDK takes rtc_spinlock and waits for conversion completion in adc_oneshot_hal_convert; its done-event loop has no software timeout. This is a real interrupt-disabled potential stall if hardware never completes, not demonstrated here. Conversion/filter/calibration behavior and SDK remain unchanged; no speculative timeout/driver patch. TCA transactions have1000ms timeout; IMU20ms with bounded startup ACK waits, performed in task context outside application critical sections. Their blocking/error paths do not themselves establish tick starvation.
- Physical display idle only gates backlight. Normal UNKNOWN source prevents SYSTEM_SLEEP, owner/LVGL parking and timer shutdown. Forced-sleep coordination waits yield; LVGL mutex waits do not disable interrupts. Motion/status and display/ADC errors retain hold/refuse sleep; no reset path or unbounded application retry was found. Watchdog timeout800ms and configuration remain unchanged.
- POWER4096 stack minimum1368 leaves observed2728-byte usage/33% margin. IDF high-water marks are bytes; stack canary checks remain enabled. No stack-overflow capture; no evidence warrants increasing stack or identifies it as INT_WDT cause. This margin supports the observed path only, not every untested error path. Retest rechecks minimum after idle/wake/control stress; new helper is fixed local storage, no added task or heap.

Primary evidence: [IDF5.5.3 GPIO wake implementation](https://github.com/espressif/esp-idf/blob/v5.5.3/components/esp_driver_gpio/src/gpio.c), [S3 GPIO HAL](https://github.com/espressif/esp-idf/blob/v5.5.3/components/hal/esp32s3/include/hal/gpio_ll.h), [watchdog tick-starvation behavior](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/wdts.html). Local pinned SDK and final target build are authoritative.

### Correction verification and artifact

Full regression: **177 PASS/0 FAIL/345778 assertions**; full soak: **177/0/3111028**,65000 existing combined stress cycles. Power19/0/39215 default and19/0/111215 soak. Two additive regressions model IDF's shared awake/wake trigger: the original held-low unmasked level setup repeatedly retriggers;1000 corrected wake cycles never expose enabled level-low IRQ. Mask/partial-arm/sleep/disarm/edge/unmask fault injection verifies cleanup and refusal without unsafe re-enable. Existing assertions unchanged; host simulation proves sequencing, not physical watchdog causality. Commands `python tests/control/run_tests.py` and `python tests/control/run_tests.py --soak`, both exit0 with Wall/Wextra/Werror; previously documented sanitizer limitation unchanged.

`pio run -e remote01 -t clean -j 1` **SUCCESS2.14s**, then `pio run -e remote01 -j 1` **SUCCESS508.33s**, exit0, **zero warnings/errors**. Flash **1805705 bytes /21.5%, +996 versus previous diagnostic build**; static RAM **20272 bytes /6.2%, delta0**. RTC640 bytes, task stacks/PSRAM wrapper unchanged. No upload.

- `.pio/build/remote01/firmware.bin` SHA-256: `b37619c4db96ffba5a6c73fdd275ef67725282b6d67ba4395907812798a1f594`
- `.pio/build/remote01/firmware.elf` SHA-256: `f2b1935f456686562efa821e1748afc55885ab7b7d708f9a4b2b588abf4639e3`

Exact five-file scope: modified `components/power_management/runtime.cpp`, `tests/control/power_tests.cpp`, `CHANGELOG.md`, this report; added `components/power_management/include/power_management/wake_irq.hpp`. Entry1615-file manifest confirms all other files unchanged, including main/display/touch/brightness/carousel/GPIO constants/drivers, power model/ADC/IMU implementation, retained diagnostic format, Core/PhaseD/Mock/frozen CCM and SDK configuration. Compiled-source manifest matches final executable/test sources; post-build changes are documentation only. This correction/diagnostic artifact supersedes earlier hashes for retest; no overall hardware PASS claimed.

### ONE focused owner stability retest

1. Owner flashes the matching correction/diagnostic artifact above; USB boot, COM3/115200 monitor recording throughout. B/m baseline: awake_irq_type=2/expected_negedge=2, worker/created-service core1, no automatic sleep requests/rounds, normal HOME. Do not use Z/K/! or PWR; no forced-sleep testing in this stability run.
2. Run10min continuously USB-powered. Exercise HOME accepted controls,20 touch/pickup/idle-wake cycles (I may force display idle), including deliberate held touches, and factory/brightness/carousel checks. B after cycles must still show awake edge2. If spontaneous reset occurs, save the entire live panic text including CPU/registers/backtrace, then B/m retained history; STOP and return evidence before attempting battery testing.
3. If USB phase is stable, remove USB once with battery connected; run10min battery-only with normal HOME/touch/motion/idle use. Reconnect once at the end, send B/m, compare boot counts/counters and POWER stack minimum. If a reset occurs, reconnect immediately after the first restart and capture B/m; stop further cycles.
4. Return elapsed USB/battery times, restart count, trigger/core/stack markers and full reset/panic evidence if any. Pass requires zero crashes, resets, watchdog events or UI freezes. Even stability PASS does not automatically close normal Stage1 acceptance; owner must explicitly validate/accept the corrected artifact.

**STAGE1 HARDWARE VALIDATION FAILED — SPONTANEOUS INT_WDT; VERIFIED GPIO SOFTWARE DEFECT FIXED / HARDWARE STABILITY AND RESET ATTRIBUTION PENDING OWNER RETEST.** No upload/Stage2, no PhaseD weakening.

## Owner corrected-firmware hardware retest / closure review - 2026-10-04

This owner evidence supersedes the preceding pending-stability status, not the failure history. Exact hardware: Waveshare ESP32-S3-Touch-LCD-3.49B V2 / PCB V1.1; installed protected Li-Po1000 mAh, protection specifications undocumented.

Initial running configuration physically observed: awake_irq_type2/expected_negedge2, gpio_low0, irq_service_core1/worker_cpu1; hold1, battery NORMAL approximately4.09V, automatic_system_sleep INHIBITED; requests0/rounds0/errors0/bus_errors0/failed_allocations0/accounting_corrupt0/owner_ok1/queue_rejected0. Retained reset5/INT_WDT was historical pre-fix evidence, **not a reset of this corrected run**.

Owner exercised normal UI/controls/touch/motion and both ACTIVE->DISPLAY_IDLE and DISPLAY_IDLE->ACTIVE. Reached75 Core dispatches,601 motion events,8 wakes; observed manager wake-to-ready approximately202–235ms. USB operation stable. Owner removed USB without inaccessible PWR: battery maintained power, UI/touch/controls/display idle/motion wake remained functional, no spontaneous reset observed. Reconnected USB and immediately sent B/m. Boot remained4, reset11/USB, uptime1105644ms: serial disconnect/reconnect was not an MCU reboot; this reset reason describes the start of boot4, not USB reattachment during this interval.

Exact post-battery physical telemetry:

```text
POWER awake_irq_type=2 expected_negedge=2 gpio_low=0 irq_service_core=1 worker_cpu=1
state=ACTIVE hold=1 wakes=8 sleep_requests=0 rounds=0 errors=0
IMU=1 motions=601 bus_errors=0 stack_min_bytes=1368 manager_wake_to_ready_ms=202
BATTERY source=REAL calibrated=1 mv=4146 filtered_mv=4152 valid=1 condition=NORMAL
external=UNKNOWN charging=UNKNOWN read_errors=0
CORE dispatches=75 INT_free=169263 INT_min=94907 largest=53248
PSRAM_free=7636184 PSRAM_min=7636184 largest_PSRAM=7602176
failed_allocations=0 accounting_corrupt=0 stack_min_bytes=15364 owner_ok=1 queue_rejected=0
```

Accepted findings: corrected edge mode was real on hardware and remained2 after battery use; hold/handover without PWR and usable battery operation PASS; display idle and motion/touch wake PASS; reported-interval stability/no new watchdog PASS; real calibrated/filtered NORMAL voltage physically observed. Voltage accuracy against an independent instrument is a separate unreported gate. External/charging UNKNOWN remains truthful: no verified MCU VBUS/charger-status sense. Automatic SYSTEM_SLEEP and automatic critical hard shutdown remain inhibited; no policy change is authorized or needed.

The GPIO configuration defect was real, corrected and physically observed; the previously reproducible INT_WDT did not recur during this owner run. No CPU/backtrace evidence establishes it as the sole cause of every historical reset. No further speculative investigation or firmware improvement is made for closure.

POWER1368/4096 observed stack margin remains unchanged from the earlier capture; Core stack minimum15364, usable internal free/min/largest and zero allocation/accounting/owner/queue faults meet the existing observed resource-health checks. PSRAM current/min coincide. No explicit numeric heap/stack floor exists in Stage1 criteria; none is invented. These observations do not prove every future load/error path or long-term leak absence, and require no stack/memory optimization now.

### Mandatory acceptance review

Authority: original Stage1 owner acceptance A–K, the owner capacity/PWR/external-power corrections, and the already documented normal procedure above. The focused defect retest does not waive the remaining original acceptance checks.

| Existing gate | Result / evidence gap |
| --- | --- |
| A artifact identity / normal boot | Corrected configuration and normal running firmware owner-confirmed. Exact flashed BIN hash not supplied; current workspace artifacts differ from the recorded correction-build hashes. Confirm tested-file identity without reflashing. |
| B/I USB cold start, hold and battery handover | PASS; no PWR, stable hold/power/UI, unchanged boot4 across disconnect/reconnect. |
| C telemetry | Physical telemetry/valid/filter/NORMAL PASS. Mandatory independent physical voltage comparison from original C and owner correction item4 not reported. |
| D normal regressions | HOME/controls/touch exercised. Explicit carousel/snapping/brightness and PhaseD basic lifecycle/ingress sanity on this final artifact not separately reported; confirmation remains. Existing PhaseD acceptance is preserved. |
| E/F/G display idle and motion/touch wake | Physical behavior PASS. Existing normal procedure asks20 idle/wake cycles and no unintended wake action/first-contact consumption. Eight wakes reported;20 cycles and gesture-isolation results not established by601 motion events. |
| H normal system sleep / critical hard-off | Superseded by safety corrections: automatic sleep must remain inhibited entirely, hard-off remains inhibited for UNKNOWN source. Z/K/! tests optional, not closure gates. No requests/rounds observed. |
| USB/battery automatic-sleep prohibition | Inhibited marker/zero requests PASS for reported use. Normal procedure's untouched10min per source (beyond300s) not established by unspecified split durations. |
| J LOW/CRITICAL safety simulation | Mandatory safe L/C/H/N transient/qualification/hysteresis/dry-run verification not reported. Host tests do not substitute for owner hardware evidence. No physical deep discharge or hold release required. |
| K endurance and resource/stability health | Reported-interval stability/resources PASS. Existing normal procedure specifies30min battery-only; boot uptime1105644ms is18min25.644s total, not30min battery-only. Separate battery duration not supplied. |

**Overall Stage1 remains OPEN / NOT ACCEPTED / NOT CLOSED.** This is missing acceptance evidence, not a new observed hardware defect. No new thresholds or mandatory development-only forced-sleep/hard-release tests introduced.

### Artifact preservation / documentation-only audit

Original corrected-build BIN `b37619c4db96ffba5a6c73fdd275ef67725282b6d67ba4395907812798a1f594`, ELF `f2b1935f456686562efa821e1748afc55885ab7b7d708f9a4b2b588abf4639e3` remain historical build evidence. At this review entry the existing on-disk artifacts were:

- BIN SHA-256 `d7d4a8fa305752baa36a114cfa7ccc2533c666799308d9fbdc61a7f2061f6532`
- ELF SHA-256 `9584a69f0edbf471b10e5f88ec2436616e5afd191c9e2c3a564562e3404cba98`

No build/file replacement performed; these artifacts are preserved byte-for-byte. Source/tests/configuration still match the correction-build manifest; only prior documentation differs. Hash difference alone does not identify its cause or which BIN was flashed. Owner reports the corrected firmware/configuration; physical flashed-byte identity remains unconfirmed rather than silently relabeling either hash hardware-validated.

Only CHANGELOG.md and this report changed in this review. Every other repository file and both existing artifacts hash-match review entry. No firmware/test/configuration change or new build/test run for documentation. Stage2 not started.

### Minimum remaining owner evidence - same firmware, no new flash

Previously performed checks may be supplied as recorded results instead of repeated. Confirm the flashed BIN identity and any already-completed D/C/J/endurance checks first. Otherwise combine only these remaining checks into one session:

1. Compare REAL calibrated/filtered voltage with a trusted physical battery/BAT measurement under USB and battery operation where accessible; record measured/reported values and load/brightness. Confirm carousel/snapping/brightness and basic PhaseD lifecycle/ingress sanity while exercising normal controls.
2. Complete at least30min total recorded battery-only runtime and20 total documented idle/wake cycles, crediting already-recorded time/cycles where known. Confirm motion emits no unintended control and the first wake touch is consumed; next deliberate touch operates. Include10 untouched minutes on each source to verify display idle/zero auto-sleep requests/rounds past300s. These durations/counts are from the existing normal procedure, not new criteria.
3. On the existing DEVELOPMENT_ONLY serial interface, perform the existing safe simulation sequence: N/H40s; C then H within2s (no critical release); C70s then B (SIM CRITICAL, dry_run1/hold_release0); L/H recovery/LOW/hysteresis; N restores REAL. Record results and final B/m/boot/stack/counters. No Z/K/!, PWR, forced release or deep discharge. Stop if reset/watchdog/freeze occurs and return evidence.

Closure can follow documentation-only when these mandatory results are confirmed and the owner accepts the unchanged corrected artifact. **OWNER STABILITY RETEST PASS; STAGE1 ACCEPTANCE PENDING.** No Stage2.


## 2026-10-05 - automatic SYSTEM_SLEEP policy revision: audit STOP

### Decision, owner observation and unchanged artifact

DECIDED: automatic SYSTEM_SLEEP eligibility must be independent of external PRESENT/ABSENT/UNKNOWN. The historical prohibition for UNKNOWN/PRESENT is SUPERSEDED as product policy, but remains in unchanged executable source pending safe lifecycle work. Owner reports deep overnight battery discharge apparently below approximately3.5V; no physical voltage/current/endurance data establishes exact discharge level or sole cause. No new battery thresholds, automatic hard-off/SYS_EN release or charging claim. Planned external5V divider source/pin/resistor verification remains unchanged and separate.

**STOP BEFORE SOURCE IMPLEMENTATION** because two explicit owner stop conditions are demonstrated: NETWORK/Wi-Fi/HTTP sleep lifecycle is not coordinated, and the existing2s RTC wake timer remains enabled every round. This is an audit/documentation result, not a sleep-enabled build or owner hardware PASS.

### Exact current state machine / entry / wake

- components/power_management/include/power_management/model.hpp: Machine display_idle_ms30000, system_idle_ms300000. Both measured from last physical activity;5min is total idle, not5min after DISPLAY_IDLE. Source/model.cpp tick additionally checks not CRITICAL_SHUTDOWN/time regression, retry deadline; requires safe && external ABSENT. Refusal postpones another attempt60000ms.
- runtime.cpp tick passes safe=IRQ/WoM/wake hardware ready && REAL battery valid && SYS_EN hold, and real.external (UNKNOWN). Hence normal automatic SYSTEM_SLEEP remains inhibited in current artifact. Z development override can explicitly select SYSTEM_SLEEP when hardware/measurement/hold valid; it shares the same uncoordinated path and is not proposed as owner test now.
- Owner must acknowledge park within500ms and refuse pending Mock operation/unresolved Core effects. UI acknowledgement another500ms. POWER verifies input/hold and GPIO8 not already low; wake/prerequisite/output faults abort/refuse and retain hold.
- WakePort::sleep calls esp_light_sleep_start, not deep sleep. level_wake_cycle masks GPIO8 ISR, arms GPIO8 LOW_LEVEL, sleeps, disables GPIO wake on that pin and restores accepted NEGEDGE before enabling ISR. SYSTEM_SLEEP uses retained RAM, not reboot boot-count flow.
- Startup globally enables GPIO wake plus esp_sleep_enable_timer_wakeup(2000000), with VDDSDIO ON. No active code disables/replaces that timer or enables Wi-Fi/UART/touchpad wake. Physical touch/motion reach shared expander interrupt GPIO8; this is not the MCU capacitive-touchpad API.
- On GPIO wake POWER reads IMU status; motion sets wake MOTION, otherwise gpio_woke selects TOUCH. Motion emits no control intent. Touch latch and200ms settling consume first contact; following deliberate touch operates. Restore Core/reacquire/publish before restarting LVGL. No PWR recovery requirement added.

### Demonstrated2s safety-timer behavior

Timer is initialized unconditionally on the same startup path for normal and forced sleep. The SDK sets RTC_TIMER_TRIG_EN and sleep_duration2000000us; wake triggers persist across sleep calls. Every successful round therefore lasts at most approximately2s without an earlier GPIO wake (SDK timing adjustments/entry overhead apply). A timer wake is not translated into ACTIVE/user activity: gpio_woke is false, Machine stays SYSTEM_SLEEP, POWER samples input/ADC on its2s cadence and re-enters sleep. It increments rounds each attempted sleep; code does not log each poll.

This demonstrably causes repeated MCU wakes and active-loop work, not a continuous overnight sleep. CPU/RF duty/current and actual battery saving are NOT measured; it cannot honestly be claimed either to save enough or to defeat all meaningful savings. The recurring timer is itself a stated stop condition. Removing it blindly would also remove battery/input/fault polling while parked; normal-sleep timer/fault-monitoring strategy requires explicit design, not a speculative omission.

### Task/service audit

| Task/service | Current sleep coordination and periodic activity | Exact implication |
| --- | --- | --- |
| POWER4096/prio2/core1 | Controls entry; input/ADC2s, diagnostics publication1s while awake; otherwise task notification wait50ms | No source/stack/frame change; RTC timer repeats2s. |
| UC_owner24576/prio2/core1 | PowerSuspend parks after pending/effects checks; closes PhaseD inbox;20ms yielding wait,1s diagnostic publication remains before park branch | Task delays do not run during explicit whole-chip sleep; no new source wake enabled. Resume attaches new incarnation/session, restores availability/reachability, reacquires, discards old input and publishes. |
| LVGL4000/prio4/core0 | Stops the5ms esp_timer before ui_ack;20ms yielding wait, no lv_timer_handler/network ui_tick while parked |5ms tick is properly stopped. Resume resets input/invalidation, restarts tick with fallback handling. LVGL object timers are dispatched by handler, not independently running wake sources. |
| NETWORK6144/prio1/core0 | No sleep park/ack. power request only refuses/defer-rejects reboot/settings; normal inbox processing/connect retry/scans/health/service lifecycle continue. Awake waits1s,100ms with DNS | Incompatible missing suspend/resume integration. Delayed SDK events/reconnect deadlines/generation and setup/candidate state not fenced for sleep. |
| Connected-STA diagnostics HTTP6144/prio1 | Two bounded client sockets,3s recv/send timeout; NETWORK service keeps server while address healthy | No power-specific revoke/drain/stop or resume barrier. Existing health/disconnect recovery is not a validated sleep lifecycle. Browser5s poll occurs on client, not MCU RTC timer. |
| Provisioning HTTP6144/DNS | APSTA/Wi-Fi remain started; DNS runs bounded nonblocking polling in NETWORK100ms loop; HTTP has independent service task | No safe provisioning/client/candidate-work sleep refusal/drain/resume contract. Cannot claim setup is preserved by whole-chip sleep. |
| Wi-Fi driver/event loop/lwIP | Wi-Fi left started, SDK events asynchronously post bounded NETWORK inbox, disconnect/IP events revoke diagnostics | Explicit sleep loses wireless connectivity; no pre-sleep stop/post-sleep start/reconnect handshake. SDK timers/events not exhaustively measured; unsupported Wi-Fi sleep does not prove any specific extra wake cadence. |
| SDK timers/FreeRTOS waits | Active build application esp_timer is LVGL5ms and is stopped. Dormant factory producers excluded. Driver/event/TCP timers retained without power handshake | Ordinary task delays/software timers do not by themselves constitute configured RTC/GPIO wake sources during explicit light sleep; overdue work can run after return. No claim that20ms task waits wake MCU every20ms. |

### Actual SDK/config evidence and network recovery gap

Inspected installed framework-espidf@3.50503.0 (ESP-IDF5.5.3), components/esp_hw_support/sleep_modes.c: esp_light_sleep_start stalls/retains other CPU, performs timer wake preparation and advances esp_timer time after sleep; esp_sleep_enable_timer_wakeup retains its configured trigger/duration. sdkconfig.remote01 has CONFIG_PM_ENABLE disabled; generated header agrees (not defined). No esp_pm_configure, application esp_wifi_set_ps, connection-preserving automatic-light-sleep integration or Wi-Fi wake enable is present in active application paths.

Official pinned [ESP-IDF5.5.3 ESP32-S3 sleep documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html#wi-fi-bluetooth-and-sleep-modes) requires Wi-Fi stopped before explicit light sleep; wireless connections are not maintained. Connection-preserving modem/automatic light sleep is a different configured PM mechanism, not the current manual path. Thus NETWORK must own an explicit stop/resume design if keeping this sleep implementation. Suspending NETWORK task alone does not stop the Wi-Fi driver. No Bluetooth application is currently enabled.

Current ordinary NETWORK behavior may detect disconnect, retry saved credentials, revoke unhealthy address and stop/restart diagnostics. It does not guarantee sleep coordination: no acknowledgement that services/sockets/Wi-Fi are stopped, no saved-state resume protocol, no deliberate-wifi-stop event fencing, no CONNECTED versus provisioning/candidate-specific handling. Credentials use remote_wifi NVS and SDK RAM storage; unchanged artifact preserves them, but post-sleep reconnection is unvalidated. Core reattachment guarantees apply to current Mock only, not future real TUNER transport.

### Minimal next implementation scope, not executed

A source-state predicate deletion in model.cpp is insufficient. Smallest plausible extension retains existing POWER task and GPIO/wake/hold/timing: NETWORK-owned bounded prepare/ack/resume handshake, stop/revoke/drain HTTP/DNS and esp_wifi_stop, refuse unsafe provisioning/scan/submission/reboot/update work, preserve credentials/settings, fence transient events and deadlines, then esp_wifi_start/connect from appropriate saved state and restore diagnostics only after valid STA IP. Resume failure must not strand touch/motion/local UI. Exact supported modes and recovery contract must be reviewed before source implementation; automatic PM design would be a larger alternative, not substituted silently.

Normal-sleep safety timer strategy must be explicitly justified with monitoring/recovery and current evidence; no periodic2s workaround implemented. Wake contract, ADC thresholds/policy, SYS_EN hold, NVS, GPIOs and4096 POWER stack/publication correction remain protected. No automatic hard-off path.

Likely future source scope after approval: power_management model.hpp/model.cpp/runtime.cpp; network_manager api/internal/runtime and diagnostics lifecycle seam only as required; focused POWER/network/owner-suspend tests. Do not edit UI/GPIO/divider/stack/Core semantics unless a further verified gap requires explicit scope. This audit changes none of those files.

### Verification / owner validation boundary

No focused/regression/soak/build run: STOP condition occurs before any implementation change. Source/tests/configuration/BIN/ELF unchanged; flash/RAM/task-frame delta0 by unchanged artifacts, not a new compiler measurement. No new hashes/artifact prepared. Documentation changes only register/roadmap/this report/CHANGELOG. Prior test/build/hardware evidence remains historical.

No current automatic-sleep owner retest artifact is supplied and Z should not be used to infer the new policy. After separately reviewed lifecycle/timer implementation and focused/normal/soak/clean verification, owner procedure must cover30s display idle/300s system idle on USB/battery and PRESENT/ABSENT/UNKNOWN eligibility (host model until trustworthy sensing exists), pending/setup refusal, no repeated2s wake storm, motion-only/first-touch-only wake, post-wake Core fresh session/no replay, Wi-Fi credentials/reconnect/IP/browser diagnostics and saved-credential recovery, no SYS_EN release/reset/WDT. Measure actual sleep current/duty/endurance before claiming battery-saving adequacy; no deep-discharge/protection-cutoff experiment or new battery threshold.

## 2026-10-05 ? authorized sleep/network implementation

Earlier audit STOP resolved through existing-owner bounded handshake, not a predicate-only enablement. [Exact implementation/evidence](SYSTEM_SLEEP_NETWORK_IMPLEMENTATION.md). POWER requests NETWORK readiness (10s bound), NETWORK stops services and Wi-Fi/fences events without NVS changes, then existing Core/LVGL park. Wake resumes fresh Core/UI before saved-STA reconnect and healthy diagnostics. Invalid measurement/hold/WoM readiness, pending Core work, setup/mutation and owner failures retain safe refusal. Normal2s RTC fallback removed after recovering its battery/WoM monitoring purpose; monitoring resumes on physical wake. Development Z retains fallback, no production cadence. Configured stack/publication/WoM unchanged; current covered stack evidence is historical until new owner remeasurement. Tests/compiler/build preparation do not establish hardware sleep/endurance/current PASS. Historical mandatory Stage1 gaps remain OPEN.
