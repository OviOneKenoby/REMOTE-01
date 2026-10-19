# Stage 2B.1 POWER stack headroom investigation

2026-10-04. **INVESTIGATION COMPLETE; production headroom approval remains open.** Stage 2B owner hardware PASS is unchanged. No firmware, test, stack-size, API, policy or hardware changes; no upload. The owner observed 608 bytes minimum remaining POWER stack, and 800 bytes after controlled reboot, without demonstrated corruption or POWER/bus/WDT/brownout errors.

## Established findings

- POWER is created by `power_management::start()` in `components/power_management/runtime.cpp`, using `xTaskCreatePinnedToCore(task, "POWER", 4096, nullptr, 2, nullptr, 1)`: **4,096 bytes**, priority **2**, core **1**, entry `power_management::{anonymous}::task(void*)`.
- Exact installed ESP-IDF **5.5.3** (`framework-espidf@3.50503.0`): `FreeRTOS-Kernel/include/freertos/task.h:1492-1513` documents `uxTaskGetStackHighWaterMark` in bytes. Xtensa `portable/xtensa/include/freertos/portmacro.h:88-91` defines StackType_t as uint8_t. `tasks.c` scans stack fill bytes and divides by sizeof(StackType_t), which is one. Null handle selects the caller's task. Both POWER serial B and POWER diagnostics call `uxTaskGetStackHighWaterMark(nullptr)` with no multiplication/division (B only casts to unsigned). **608 really is bytes**, not 608 words. HTTPD's independently named margin is sampled on HTTPD, not POWER.
- The watermark is the historical minimum untouched fill-pattern region since task creation, not current free stack or an identified call trace. Reserved but unwritten compiler slots can remain undetected. 4096 minus 608 = 3488 bytes touched by that scanner; this is not an exact simultaneous live-frame measurement.
- Retained pre-Stage-2B ELF task entry reserves **496 bytes** (`entry a1,0x1f0`). Current accepted ELF reserves **1,248 bytes** (`entry a1,0x4e0`), **+752 bytes**. The entire entry frame stays allocated throughout startup and every loop branch, including B logging, ADC/I2C and wake handling.
- Recompiling current runtime with its exact recorded target flags plus `-fstack-usage`, output only in TEMP, confirms **1248 static**. A TEMP-only counterfactual removing only the Stage 2B publication block and its three bookkeeping variables reports **496 static**. This establishes the source of the compiled-frame regression, without changing repository source or firmware. It does not establish that the RTC accessor itself produced the physical 608-byte minimum.

## POWER-context call paths audited

| Path | Synchronous work on POWER |
| --- | --- |
| Startup | reset_event(WORKER_INIT); Board::initialize; ADC unit/channel/calibration setup; Wom::configure/wait -> Board::read/write/now/delay -> I2C; GPIO configuration/ISR registration; wake/PD setup; startup logs; reset_snapshot/RUNTIME_READY |
| Input/motion | GPIO level; reset checkpoints; Board::input -> expander get_level -> TCA9554 read_input_reg -> I2C; Wom::status -> Board::read -> I2C; Machine::wake/activity |
| Battery, every 2 s | Board::battery -> four adc_oneshot_read calls, scalar averaging, adc_cali_raw_to_voltage -> curve-fitting callback; Battery::sample (fixed five-sample filter); reset_snapshot/event; optional development simulation; battery-warning formatting/logging; bounded BatteryView copy under mux |
| Idle/wake | Machine::tick; wait_flag -> time_now/vTaskDelay for owner/UI acknowledgements; set_display -> Board::display -> expander set_level -> read/write-output callbacks -> I2C; transition/wake-latency logs/checkpoints; settling/activity atomics |
| Development commands | command.exchange; reset_event(COMMAND); POWER_DEV log; B -> reset_report plus POWER/BATTERY/heap/stack/wake logs; I/Z/L/C/H/N/K/! existing policy paths |
| Forced sleep/release | owner/UI handshake; controlled_release -> Board::release_hold -> expander callback/I2C; WakePort -> level_wake_cycle -> GPIO mask/level arm, esp_light_sleep_start, disarm/edge/unmask; error logs/checkpoints. These are reachable development paths; automatic system sleep remains inhibited |
| Stage 2B publication | Power d; values assignment and watermark; reset_history_copy -> boot_record/validation -> sanitize_reset_record -> validation; copy four rows; diagnostics::publish -> publish_copy -> zero-wait atomic lock and fixed copy; diagnostics::event -> Ring::push; no JSON/HTTP/log formatting here |
| Loop wait | ulTaskNotifyTake, normally 50 ms |

The GPIO ISR notifies POWER; it is not a synchronous child call of task(). LVGL touch/UI acknowledgement and Core acknowledgement/serial enqueue run in their respective calling tasks. HTTP request parsing, JSON construction, response buffers and socket handling run on HTTPD, not POWER. Polling can contend with publication's zero-wait lock and cause a drop; it cannot nest HTTP serialization on POWER. Expander and ADC scheme callbacks do execute synchronously on POWER. No new heap allocation occurs in POWER publication.

Interrupt accounting is not simply the GPIO ISR's C frame: the pinned Xtensa context/vector source documents a first interrupt/exception frame on the interrupted task's stack before the system ISR stack. Context save, task wrapper, register spills and indirect SDK/ROM calls must be included in any complete stack bound.

## Target sizes and nested frames

Sizes measured by compiling a TEMP inspection translation unit with the actual target flags and reading symbol sizes with the target nm (not host pointer sizes):

| Stack-local type/object | Bytes |
| --- | ---: |
| diagnostics::Power d | 448 |
| ResetSummary h (four rows) | 256 |
| ResetRow | 64 |
| reset_diagnostic::Record passed by value | 80 |
| Battery real / simulated | 64 each |
| Machine | 48 |
| Board | 12 |
| gpio_config_t | 24 |

The new explicit caller objects total 704 bytes; bookkeeping, alignment, temporaries and spills account for the compiled frame layout rather than a simple sum of sizeof values. Aggregate assignment/return temporaries must not be counted as additional full structs without compiler evidence. RTC History is retained static storage, not a POWER-local history array. No capacity/format/ownership change is proposed.

Compiler stack-usage reports: reset_history_copy **176**, sanitize_reset_record **96**, reset_report **160**, Board::initialize **96**, Board::battery/input/read/write **48**, Board::display/release_hold **32**, publish_copy<Power> **32**, diagnostics::event **32**. ELF validation function frame **32**. These are individual frames, not peak call-chain totals; return storage may reside in the caller.

A concrete RTC sanitation chain has 1248+176+96+32 = **1552 bytes of explicit frames**, before wrapper/context/ABI overhead. In contrast, current ELF logging frames are esp_log **96**, esp_log_va **48**, vprintf **48**, _vfprintf_r **800**. Thus B -> reset_report -> log -> formatter already has **2400 bytes of explicit frames**, before formatter helpers, stream/VFS/USB writes, locking, context and interrupt handling. The formatter also contains conditional helper/indirect paths; summing only these entry frames is not a global maximum or proof of safety.

**Leading maximum-stack candidate:** synchronous logging, especially B/reset_report, and SDK error logs inside deeper I2C/ADC/startup paths, now sharing the larger outer frame. No per-path hardware watermark record exists to rank these against forced sleep/wake SDK paths. The exact hardware worst-case path and the cause of the 608-versus-800 difference remain unestablished. Do not attribute either observation to browser polling, corruption, or a watchdog.

## Baseline and measurement limits

Current ELF SHA-256: `e50d4847af85bc96bd8719b44e798a71eec8edc892e91b3398432bc253b7271f`.
Retained baseline ELF (`TEMP/remote01-ota-migration/baseline-firmware.elf`): `17fa6f213ac108d8e4aefb88d470bbd7a040e439ce4cd3fbfadf02496913e155`.
Pre-migration and pre-diagnostics source manifests have identical POWER files. This supports the pre-Stage-2B comparison; it is not a matched physical before/after run. A previously reported 1368-byte Stage 1 hardware margin likewise is not a controlled comparison. **Stage 2B demonstrably increased compiled frame pressure by 752 bytes; a matched runtime watermark regression has not been measured.**

Lowest existing physical measurement: **608 bytes, owner observed**. Host tests do not run the FreeRTOS POWER worker, Xtensa logging/ISR stack, physical ADC/IMU or actual RTC accessor; no host-observed POWER watermark exists. **608 was not reproduced in this investigation.** Snapshot sampling occurs before the RTC copy/publish calls and B samples before its remaining logs; later samples capture previous deeper paths but cannot identify them.

## Verification

Unchanged complete suite `python tests/control/run_tests.py --soak`: **215 PASS, 0 FAIL, 3,471,554 assertions**, exit 0; no compiler warnings/errors. POWER: 19/0, 111215 assertions, 10000 stress iterations. Diagnostics: 9/0, 90080 assertions, 10000 ring cycles. Existing cases cover battery/filter faults, policy idle/wake, motion/expander fault matrices, touch gating, owner/session preservation, reset validation/torn records, wake IRQ cleanup, sanitation, bounded serialization/ring and command routing. They do not exercise real serial B logging, simultaneous physical wake/HTTP activity, actual POWER snapshot publication or MCU stack usage. No extra host model was added to claim such coverage.

Six investigation-only target compilations (runtime, counterfactual, sizes, hardware, reset diagnostic, diagnostics service) succeeded without warnings/errors. Objects/reports reside in TEMP/remote01-power-stack-audit; disassemblies in TEMP/remote01-power-{current,baseline}-disassembly.txt. No instrumentation/test source was added. No PlatformIO rebuild was necessary or performed; accepted BIN/ELF/partition artifacts remain byte-identical. Firmware/RAM/resource delta: **0**.

## Recommendation and stopping point

608 bytes is neither approved production headroom nor demonstrated exhaustion. There is no universal ESP-IDF margin that proves safety. Recommend a **provisional review target of at least 1024 bytes remaining after deliberately exercised worst covered paths**, with additional reserve justified for uncovered error/SDK/interrupt paths. This is an engineering acceptance target, not an SDK guarantee; 608 and 800 are below that target but do not establish a defect.

Before production freeze, use DEVELOPMENT_ONLY per-path minimum checkpoints in POWER to distinguish startup, ADC, IMU/I2C, B/reset-report, RTC-copy/publication, idle/wake and permitted forced-sleep error paths. Publish those bounded measurements after the operation, outside locks; account for probe overhead and do not use a global historical watermark as independent per-path usage. Repeated B, motion/touch idle-wake and 5-second browser polling should be combined on hardware. Host tests cannot replace that measurement. This further instrumentation has not been implemented or uploaded.

If measurements confirm logging + enlarged entry-frame overlap as the limiting path, first evaluate an out-of-line bounded publication helper in **components/power_management/runtime.cpp** so its snapshot locals are not reserved during unrelated B/startup paths; confirm compiler does not inline them back and measure publication's own nested peak. No heap/PSRAM relocation, stack enlargement, reset-record or diagnostics API change is justified yet. That would be a separately reviewed corrective implementation. DEVELOPMENT_ONLY checkpoint instrumentation would also need runtime.cpp; existing host tests cannot assert a physical stack watermark. No corrective files changed here. Stop at this report, retain Stage 2B PASS and the open production headroom gate.


## Subsequent Stage 2B.2 measurement milestone

[Development-only per-path measurement](STAGE_2B_2_POWER_STACK_PATH_MEASUREMENT.md) is now implemented and normal/soak/clean-build verified, **OWNER PHYSICAL MEASUREMENT PENDING**. It adds guarded fixed POWER-owned records and unused serial S/T, without correcting the stack, increasing4096 B, or restructuring publication. Final POWER frame remains1248 B (delta0). Historical 608/800 evidence and Stage2B PASS remain unchanged; no isolated physical maximum or correction is inferred. The report above describes the completed Stage2B.1 baseline and remains historical.
