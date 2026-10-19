# Stage 2B.2 DEVELOPMENT_ONLY POWER stack path measurement

2026-10-04. **IMPLEMENTED / NORMAL AND SOAK PASS / CLEAN BUILD VERIFIED; OWNER PHYSICAL MEASUREMENT PENDING.** Historical Stage 2B hardware PASS remains valid. This is instrumentation, not a stack correction or new production feature. No upload, OTA, stack increase or publication restructuring.

## Baseline inspection

Before editing, all non-build files were compared with the Stage 2B.1 inspection manifest: the only intervening changes were its three documented documentation files. Accepted BIN/ELF/partition SHA-256 matched. POWER remains 4096 bytes, priority 2, core 1. IDF 5.5.3/Xtensa watermark units remain bytes. Source/protocol/partition/driver/POWER policy/CCM/Home/NETWORK/reset-history baselines were preserved.

Owner observations remain 608 bytes and 800 bytes after controlled reboot, neither attributed to a path. Stage 2B's 496 -> 1248 byte frame regression (+752) remains established; 608 was not reproduced by Stage 2B.1. Power448 and ResetSummary256 locals are unchanged.

## Design and ownership

Temporary `POWER_STACK_MEASUREMENT` defaults to 1 in the new measurement header for this development build. Compile with `-DPOWER_STACK_MEASUREMENT=0` to eliminate storage, probes, reporting and S/T command acceptance. No platform/sdkconfig change. Existing DEVELOPMENT_ONLY commands keep their semantics; this flag controls only the new experiment.

POWER alone owns a **948-byte internal static State**: fifteen records, sixteen chronological new-minimum events, counters and correlation epoch. No new task, queue, allocation, locks, HTTP fields/routes, RTC/NVS writes or persistent log. Records are boot-local and lost at reboot. POWER's normal owner task calls the probes; none run from ISR, HOME, Core or HTTPD.

Each noinline begin/end wrapper samples `uxTaskGetStackHighWaterMark(nullptr)` and updates fixed state. It never logs. Each completed record holds:

- last begin/end sequence, before/after bytes and operation count;
- lowest observed after value, with its matching before and sequence pair;
- count of strict before-to-after reductions.

Every strict reduction also enters a sixteen-entry overwrite ring with path, before, after and sequence pair. Overwrite count is explicit. Counters/sequences saturate instead of wrapping silently; `saturated=1` invalidates further sequence-order precision and calls for a fresh reboot. An invalid-bracket/monotonicity counter must remain zero. Fixed storage is size-asserted <=1024 bytes. All existing test cases remain intact.

## Interpretation and limitations

`new=1` means **the historical task watermark fell between these two probes**. `new=0` means this operation did not establish a new global minimum; it does not mean the operation consumes no stack or has spare capacity equal to its row's minimum. `min` is the smallest after reading observed in that row, which can inherit a floor established by a different path. No stack pattern is reset, no SP manipulated and no arbitrary stack memory inspected.

Epoch+sequence pairs correlate intervals within a boot. Only `drop` entries and before/after reductions establish interval evidence. Samples are not independent isolated per-path stack usage. An interrupt on POWER's stack during the bracket is included; a bracket does not prove which subcall or IRQ caused the reduction. Tiny gaps between brackets and probe bookkeeping cannot be attributed as measured operations. Probe frames and compiler layout can influence the experiment, even with unchanged task frame size. No fixed byte subtraction is justified.

Most intervals do not overlap. **ERROR is intentionally nested within ADC** for existing warning/dry-run logging: if both show the same reduction, enclosing sequence pairs identify inclusive parent/child evidence, not two independent consumptions. SNAPSHOT has two intervals per publication (construction/values and copied-row assembly). DISPLAY and SLEEP can also have multiple intervals per turn. HTTP parsing/serialization stays on HTTPD; polling may affect scheduling/lock contention but does not become a POWER child call.

## Checkpoints

| Path | Bracket |
| --- | --- |
| PROBE | Empty begin/end at task entry; calibration evidence for probe floor |
| STARTUP | Existing worker handle/reset checkpoint, ADC/IMU initialization, GPIO/wakeup setup and existing startup logging through RUNTIME_READY |
| NORMAL | Loop timestamp/input checks and normal 50 ms task notification wait |
| ADC | Real ADC reads/calibration/filter/checkpoints, development simulation and BatteryView update; probes outside the mux |
| INPUT | TCA9554 input/I2C, hold read/checkpoints and IMU status/acknowledgement |
| SNAPSHOT | Existing Power object construction/values; separately four-row assembly, without moving the objects |
| RESET_COPY | Existing reset_history_copy and sanitation/validation, including return |
| PUBLISH | Existing zero-wait publication, events and bookkeeping |
| B | Unmodified reset_report and all existing serial B logs, including logs after B's own watermark reading |
| DISPLAY | Motion/touch state handling and policy/display gate/wake coordination, existing transition and latency logs |
| SLEEP | Existing forced-sleep owner/UI handshake and separately actual light-sleep/wakeup/error/early-return paths |
| RELEASE | Existing development release handshake, logs and terminal checkpoint; no new means of executing release |
| COMMAND | Existing serial command checkpoint/log/switch; B's generic acknowledgement is here, full B report is separate |
| REPORT | New S report and T acknowledgement logging; output-induced minima cannot silently be credited to B/publication |
| ERROR | Existing initial battery warning and safe simulated critical dry-run warning; real SDK error logs remain in their INPUT/ADC/DISPLAY/SLEEP brackets |

Unreachable/untested paths remain count=0, not PASS. No faults are manufactured. Release can power the board off or enter its existing terminal loop, so its RAM result cannot necessarily be retrieved; it is deliberately excluded from owner exercises. No PWR button is required.

## Reporting / command interface

Unused uppercase **S** and **T** were audited against all Mock, POWER and NETWORK keys. Existing USB Serial/JTAG UC input dispatch routes them via POWER's existing one-slot atomic command inbox. Queued/refused feedback remains on UC; only POWER samples/reports. Send keys separately and confirm `queued=1`; no line ending is required and existing CR/LF handling is unchanged.

- **S:** bounded on-demand `POWER_STACK` report, maximum 63 lines. Shows epoch/baseline/sequence/validity, each row count/drops/min, last and lowest pairs, then up to sixteen drop events/overwrite count. No automatic per-loop output.
- **T:** clears records/correlation sequence and advances epoch using the actual current historical watermark as baseline. Explicit `historical_HWM_not_reset=1`; does not reset the task watermark. Its acknowledgement is measured under REPORT.

Formatting happens after measured operations, outside existing locks. S measures its own output under REPORT and closes that bracket only after all output, without another log. Its new result is visible on **the next S**; a currently open REPORT last pair is not printed as a completed interval. Example (illustrative, not hardware evidence):

```text
POWER_STACK: DEVELOPMENT_ONLY epoch=1 baseline=... seq=... invalid=0 saturated=0
POWER_STACK: path=B count=10 drops=1 min=608
POWER_STACK: lowest path=B seq=101:102 before=800 after=608 new=1
POWER_STACK: drop path=B seq=101:102 before=800 after=608 new=1
POWER_STACK: ... report_result=NEXT_S historical_HWM_not_reset=1
```

If REPORT creates the lowest floor, repeat the desired test from a fresh boot without S/T beforehand. Clearing warm records does not restore the opportunity to see earlier minima. Captured boot/epoch and serial command order should accompany all owner results.

## Generated-code overhead and verification

A TEMP prototype was target-compiled before repository implementation. Final exact-flag `-fstack-usage` compilation and final ELF task entry comparison establish:

| Item | Bytes |
| --- | ---: |
| Accepted POWER entry frame | 1248 |
| Instrumented POWER entry frame | 1248 |
| Delta | **0** |
| Disabled instrument entry frame | 1248 |
| Begin / end wrapper frame | 32 each |
| State::begin / end bookkeeping frame | 32 / 48 |
| State::clear frame | 32 |
| Report / clear wrapper frame | 48 each |
| Pair-formatting helper frame | 64 |
| Fixed measurement State | 948 |

These helper frames are nested overhead, not additions to the persistent POWER frame. Watermark scanning invokes existing SDK functions. The empty PROBE bracket records a calibration observation, not an exact universal overhead correction. Report formatting includes the unchanged SDK/newlib/VFS stack; REPORT separately tracks it. Measurement increases CPU work through bounded stack scans/copies and changes scheduling; physical results pertain to this development artifact.

The disabled runtime's **all 39 .text/.literal section bytes** match the accepted-source compiler object using the same flags; no probe/report/storage code is retained. Other executable source/configuration files are unchanged. No complete disabled firmware is claimed to have been flashed or hardware tested.

Complete normal: **229 PASS / 0 FAIL / 394390 assertions**. Complete soak: **229 PASS / 0 FAIL / 3591640 assertions**. The original 215 tests were preserved; seven new measurement-model tests run in each enabled/disabled compilation. They test new-minimum attribution versus inherited floors, clear without watermark reset, inclusive nested intervals, bounded overwrite/stress, invalid brackets, saturation and nonconflicting keys. Each soak variant has 60042 assertions/10000 cycles. Actual-source namespace audit has five assertions. Host tests validate bookkeeping, not physical stack minima or SDK/hardware paths.

Final host runs and target inspection compilations had no warnings/errors. An initial new test compile failed under -Werror=misleading-indentation; braces fixed it and full normal/soak were rerun. A TEMP prototype initially had undefined anonymous helper declarations; the completed prototype compiled successfully before editing. Initial disabled-code comparison exposed an unnecessary disabled helper call; compile guards were corrected, final section comparison passed. These were investigation/development checks, not accepted artifacts.

## Clean target result and artifact comparison

`pio run -e remote01 -t clean -j 1`: SUCCESS, exit 0, 1.56 s.
`pio run -e remote01 -j 1`: SUCCESS, exit 0, 522.20 s.
Invoked through installed PlatformIO executable; no upload target. Final clean/build has **zero compiler/CMake warnings or errors**. Final ELF `task(void*)` entry is `entry a1,0x4e0` (1248); stack_records symbol size is 0x3b4 (948).

| Resource | Accepted Stage 2B | Instrumented Stage 2B.2 | Delta |
| --- | ---: | ---: | ---: |
| Full firmware BIN | 2476432 B | 2478352 B | +1920 B |
| Linked application (PlatformIO Flash used) | 2475933 B | 2477849 B | +1916 B |
| Static internal RAM | 45828 B | 46780 B | +952 B |
| POWER task configured stack / priority / CPU | 4096 B / 2 / 1 | 4096 B / 2 / 1 | 0 |
| POWER entry frame | 1248 B | 1248 B | 0 |
| Diagnostics arena / control | 8056 B PSRAM / 48 B internal | unchanged | 0 |
| HTTP stack / sockets | 6144 B / max2 clients | unchanged | 0 |
| Application runtime heap allocations / additional PSRAM | none for probes | none for probes | 0 |
| Approved per-OTA-slot size | 3145728 B | 3145728 B | 0 |
| Per-OTA-slot BIN headroom | 669296 B | **667376 B (651.734375 KiB)** | -1920 B |

Additional fixed measurement state948 B plus4 B BSS alignment accounts for952 B static internal growth. Flash text +1372 B and flash rodata +544 B account for1916 B linked growth. SDK dynamic resource use and physical minima still require owner observation; no measured physical heap delta is claimed. PlatformIO's29.5% Flash percentage refers to the retained8MiB factory slot, not the3MiB OTA slots. No OTA implementation is present.

| Artifact | SHA-256 |
| --- | --- |
| Accepted BIN | `a61a303aa23b3f02bb419831080f6d81ecb7e006f8ef11ece1a457ce1bc481e8` |
| New BIN | `b34ad9a8f319532aa7ad898b54d7cd70c08f944452e0090e0c6504a4da6dbd67` |
| Accepted ELF | `e50d4847af85bc96bd8719b44e798a71eec8edc892e91b3398432bc253b7271f` |
| New ELF | `ee6d6e57ac2aa595674e5281ddfb3897c4b0836e88e80b883da1886e7572fff1` |
| Partition BIN, unchanged | `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835` |

Accepted artifacts were retained in TEMP/remote01-stage2b2 before clean. Test/build logs, section comparison and target inspection objects are there too. All protected non-build inputs were hashed against the pre-edit manifest. Only the eight files listed below changed; no unrelated source, existing test definition, partition layout, NVS, provisioning, diagnostics API or hardware input was changed. Documentation edits after compilation do not change artifact hashes.


## Owner physical procedure

After owner approval/manual flash of the listed artifact hashes (no upload performed here), connect USB for cold boot and open COM3/115200 interactive PlatformIO monitor. Existing credentials must reconnect; use the assigned STA IPv4, keep browser diagnostics refreshing at its existing 5-second interval. No provisioning/NVS reset or PWR access.

1. **Fresh boot, no B/S/T yet:** let real ADC/input/publication run for at least two minutes while moving the device, using HOME playback buttons/swipes and cycling display idle/wake by motion and touch. For idle, wait its normal 30 seconds or send existing I, then wake. Send **S**, then **S** again; save both complete reports and ordinary telemetry. Look for ordered drop entries STARTUP/ADC/INPUT/RESET_COPY/PUBLISH/SNAPSHOT/DISPLAY/NORMAL. Ignore inherited row minima as causal evidence; record any REPORT reduction separately.
2. Send existing **R**, confirm controlled software reboot and reconnect. **Before S/T**, send **B** ten times, one key per second (verify queued=1), while keeping browser polling and motion/touch/UI activity. Then S twice. Compare B/COMMAND drop intervals with the first run; capture boot identity and full sequence order. No credential reset or automatic sleep change.
3. Optional safe existing logging exercise: send **C**, wait at least 35 seconds for the simulated critical dry-run warning, then **N** to restore real presentation. It never requests simulated hold release. S twice shows ERROR's nested ADC evidence. T may then be used to demonstrate record clearing: verify baseline remains at the historical floor, and B followed by S cannot be called a fresh measurement. Do not interpret warmed-up new=0 as safety.
4. **Do not use K/!**. Forced Z sleep is omitted from the required procedure: existing documentation does not establish physical forced-sleep/recovery acceptance for this enclosure. Its reachable paths are instrumented, but measurement here must not require power loss or recovery access. Only a separately owner-validated safe Z procedure could exercise them; count=0 is an honest unmeasured result. Automatic system sleep remains inhibited.

Return the full S reports/serial order, boot/epoch, browser-poll interval and actions leading up to any minimum. If a spontaneous reset occurs, capture the existing POWER_RESET history; records themselves are volatile and are not a crash recorder. No claim that 608 will reproduce, no Stage 2B.2 hardware PASS before owner evidence, no recommended correction yet. The provisional >=1024-byte production review target remains a target, not an ESP-IDF guarantee.

## Exact changed files / stopping point

- components/power_management/runtime.cpp: guarded static probes/reporting and existing POWER serial-key extension only.
- components/power_management/include/power_management/stack_measurement.hpp: bounded pure record model and DEVELOPMENT_ONLY compile gate.
- tests/control/stack_measurement_tests.cpp: additive model enabled/disabled tests.
- tests/control/run_tests.py: additive suite and S/T namespace checks.
- docs/STAGE_2B_2_POWER_STACK_PATH_MEASUREMENT.md: this report.
- docs/STAGE_2B_1_POWER_STACK_INVESTIGATION.md, docs/STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md, CHANGELOG.md: subsequent milestone links/results; historical evidence retained.

Reset diagnostic source/API/format/capacity/retention, diagnostics component/HTTP semantics, network/provisioning/NVS, partitions/configuration, hardware/drivers/GPIO, power policy and HOME/CCM/Core remain unchanged. No OTA implementation, heap relocation, publication helper or corrective stack change. Stop after verification/documentation; await owner physical results.


## Owner measurement and authorized publication-frame correction - 2026-10-05

The preceding sections are historical instrumentation evidence. The owner has now supplied the COM3 hardware measurement log (attachment `d2139527-5a60-4284-ba9d-9c4728889f96`). **608 BYTES REPRODUCED / INTERVAL CONTRIBUTORS ESTABLISHED; CORRECTED FIRMWARE HARDWARE REMEASUREMENT REQUIRED.** No exhaustion, corruption or watchdog is established by these measurements. Stage2B owner PASS is preserved; production POWER headroom is not accepted by this result.

### Physical evidence reconciled

- Fresh instrumentation baseline2112 bytes; STARTUP lowers historical minimum to800.
- First S starts with800. Second S exposes REPORT seq3235:3236, before800/after608/new1. B count is0 in that run, proving S independently contributes.
- NETWORK R is accepted/executed; log reports SOFTWARE reset and reconnects using saved Wi-Fi credentials.
- After reboot, ten queued B commands: COMMAND seq1963:1964 lowers800->768; B seq1967:1968 lowers768->608. B count10/drops1. Subsequent S reports remain608 without further reduction.

The deepest measured intervals are now REPORT and B, with a preceding COMMAND reduction. Historical claims that608 was unexplained/not reproduced are superseded by this owner evidence, not rewritten. The precise nested logging/formatter/interrupt subcall responsible within each interval remains unisolated. Later normal/ADC/publication rows at608 inherit the global floor and are not additional causal evidence. Startup800 is production-relevant in the instrumented artifact, not a measured release-build minimum. The >=1024-byte review target is still provisional, not a safety guarantee. Disabling POWER_STACK_MEASUREMENT removes S/T/probes, not existing B; DEVELOPMENT_ONLY text alone does not exclude B from compilation.

### Exact correction

Only production implementation file changed: `components/power_management/runtime.cpp`.

Added `__attribute__((noinline)) publish_power(...)`, called synchronously by the same POWER owner under the unchanged publication predicate. Moved diagnostics::Power448 B and ResetSummary256 B automatic locals into it; owner-local objects/counters are passed by reference where appropriate, with no heap/global scratch/queue/task. The original publication body is identical ignoring indentation: same selected REAL/SIM battery,23 values, validated reset accessor,4x16 copied reset fields, publish, conditional events, bookkeeping and probe ordering. The accessor remains the sole RTC export boundary; no mutable RTC reference/pointer is exposed. No stack-size, driver, policy, UI, serial or diagnostics API change. Stack watermark fields naturally measure the corrected call path; no physical improvement is inferred from changed code.

Exact-current-flags compiler reports:

| Frame | Before | After |
| --- | ---: | ---: |
| POWER task |1248 B|528 B|
| Non-inlined publication helper |not present|848 B|
| Configured POWER stack |4096 B|4096 B|

Persistent task-frame reduction is720 B. During publication, explicit task+helper frames total1376 B,128 B above the former1248 B task frame before unchanged child calls. Therefore publication/reset-copy's own nested peak must be measured as well as startup/COMMAND/B/REPORT. Compiler frames are not complete interrupt/SDK stack bounds, and no guaranteed hardware margin is claimed.

Two TEMP-only exact-flag `-fstack-usage` compilations of retained before-source/current source succeeded with zero warnings/errors. Before source/artifacts/inventory and logs are retained in `%TEMP%/remote01-power-publication`. Final clean-build ELF confirmation and artifact results follow below.

### Regression evidence

New `tests/control/power_publication_tests.py` extracts the actual helper and executes it against bounded host seams, rather than duplicating its implementation. Checks all snapshot values for REAL/SIM and validity, four reset rows including unavailable zeros, publish/event/probe ordering, conditional event suppression, bookkeeping, one watermark read per call and zero allocator calls. Three Python assertions additionally verify the large locals/accessor leave the outer task and exactly one helper call remains. Host seams do not validate physical RTC/FreeRTOS/SDK behavior; existing reset-sanitation and owner tests remain in the complete suites.

Focused test:1 PASS/0 FAIL/80668 assertions,1000 cycles, zero allocator calls. Complete normal:262 PASS/0 FAIL/510422 logged assertions; complete soak:262 PASS/0 FAIL/4730672 logged assertions. New helper soak alone:806668 assertions/10000 cycles. All existing261 cases preserved. Counts exclude the three additional Python source assertions. Both complete runners exit0. Initial focused compiler invocation failed because the child lacked MSYS compiler PATH; passing the prepared environment fixed it. No firmware correction or weakened assertion was used to address that tooling failure.

### Owner remeasurement procedure

After separate owner approval/manual flash of the final hashes below (no upload performed here):

1. USB boot without PWR, retain credentials, confirm STA reconnect and5-second browser diagnostics polling. From a fresh boot, before B/S/T, exercise periodic ADC/publication and normal HOME/carousel/brightness/display-idle/touch wake. Exercise motion wake only with existing IMU-ready evidence; record any unavailable coverage without investigating it in this correction.
2. Send S twice, verifying queued1. Capture baseline/STARTUP and publication/RESET_COPY/PUBLISH intervals, then REPORT's completed result on the second S. Record invalid/saturated/overwrite counts. Compare to the former STARTUP800 and REPORT608 without predicting a new minimum.
3. Send R; verify SOFTWARE reboot and preserved STA credentials. Before S/T, send B ten times separately with queued1 acknowledgements; then S twice. Capture COMMAND/B minima and all publication intervals. Continue existing5-second browser polling and safe idle/touch/UI activity.
4. Review all covered minima against the provisional>=1024-byte target. Below-target results require further review; above-target results alone do not establish production safety. Retain actual boot/epoch, command order and reset/error telemetry. Stop on spontaneous reset, corruption or new functional failure. No K/!, forced sleep, Wi-Fi reset or PWR requirement.

No IMU/WoM implementation or investigation was included. No physical stack-improvement PASS or overall Stage2B.2 production headroom closure is claimed.

### Corrected final clean-build evidence

`pio run -e remote01 -t clean -j 1`: SUCCESS/exit0,2.35s. `pio run -e remote01 -j 1`: SUCCESS/exit0,572.03s. Installed PlatformIO executable used with those arguments; no upload. Zero compiler/CMake warnings/errors in both final logs.

Final ELF confirms POWER task `entry a1,0x210` (528 B) and separate publication helper `entry a1,0x350` (848 B). All58 executable .text/.literal sections of the exact-flag inspection object match the clean-build POWER object. Helper is not inlined;4096-byte task allocation/priority2/core1 unchanged.

| Resource | Actual pre-correction artifact | Corrected artifact | Delta |
| --- | ---: | ---: | ---: |
| BIN |2480480 B|2479648 B|-832 B|
| ELF file |17722712 B|17722572 B|-140 B|
| Linked application flash |2479981 B|2479145 B|-836 B|
| Static internal DRAM |46908 B|46908 B|0|
| DRAM data / BSS |24540 /22368 B|24540 /22368 B|0|
|3MiB OTA-slot headroom |665248 B|666080 B (650.46875 KiB)|+832 B|

The actual pre-correction rebuilt ELF hash matched the owner's log prefix `dd17fa45da2d606a`; this is distinct from the older historical instrumentation hashes above. Entry artifact copies are preserved. Build metadata and link layout affect hashes; compiler/resource results do not prove physical stack improvement. No new runtime heap/PSRAM allocation, task or queue was introduced. The partition BIN is byte-identical.

| Artifact | SHA-256 |
| --- | --- |
| Actual entry BIN |`15065a62ea46045f2cafdefaa5c19eb5b9fbea1c912c20b6e411d48a87f24155`|
| Actual entry ELF |`dd17fa45da2d606a1c6ebf65ba00d8aecbc8af91d0fbd30705fca51836a2d3d4`|
| Corrected BIN |`4aff1ebc81b19e6b50b94f85e36324fce0509555bb89fb69b3a2d07d5b2a50bf`|
| Corrected ELF |`2a8912952260385813ae523db3fa7124ad76c8b875916e8f2ff5baa32b885e6b`|
| Partition BIN unchanged |`d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`|

Correction scope: modified runtime.cpp, tests/control/run_tests.py, this report and CHANGELOG.md; added tests/control/power_publication_tests.py. Deleted0. All other1654 entry files unchanged by SHA-256 comparison, including IMU/WoM, reset-history source/format/capacity, diagnostics implementation/API, GPIO/drivers, configured stacks, network/provisioning/security/NVS/partitions, HOME/UI and frozen CCM. Final source inventory/logs are retained in session TEMP. Stop after verification; **OWNER HARDWARE REMEASUREMENT REQUIRED**.


## 2026-10-05 ? owner remeasurement and DEVELOPMENT_ONLY WoM initialization diagnostic

### Owner evidence (physical observations)

After the publication-frame correction: baseline2832 B, STARTUP1520 B, normal/ADC/input/publication1488 B, REPORT1376 B, Bx10 minimum1248 B; subsequent S remains1248 B. All measured intervals exceed the provisional>=1024 B review target. This is physical remeasurement, not a compiler prediction or general production-safety guarantee. POWER stack/publication correction unchanged.

Separate reproducible failure: clean/build/upload boot reports IMU_WOM=0/sleep_ready=0, IMU=0/motions=0/bus_errors=0, and lift-to-wake fails on USB and battery while touch wake works. On the same pre-instrumentation firmware, serial NETWORK R software reboot restores physical lift-to-wake immediately. The post-R IMU_WOM line was NOT captured; no value is inferred. Root cause remains unproven. Initial configuration failures do not increment the later runtime bus_errors counter, and IMU polling is skipped when imu_ready is false, so bus_errors=0 is consistent with initialization failure.

### Authorized instrumentation only

One44-byte fixed static POWER-owned record captures current/failed stage, operation/register, underlying esp_err_t, expected/mask/actual/validity, cumulative/current-stage poll attempts, failing/final-stage elapsed milliseconds, total milliseconds and result/cause. Guard POWER_WOM_DIAGNOSTIC=0 removes the record/hooks/summary. Default-enabled code is temporary DEVELOPMENT_ONLY instrumentation, not a production feature.

Board read/write still return the same bool and use the same transport calls/20ms deadline; transport errors are captured only while configuration is running. No heap, task, queue, RTC format/storage change, retry, added scheduled delay, register/address/speed/check change or per-poll logging. Configuration transaction/short-circuit ordering and original5ms poll delays are preserved. Additional clock reads/record assignments have small instrumentation overhead; no timing workaround is applied. Completed record is not overwritten by runtime polling. One bounded ESP_LOGI summary is emitted after configure returns, before the existing ADC/IMU startup line. No change to SYS_EN, display/touch/UI, network/reboot behavior or automatic sleep policy.

### Summary interpretation

Ignoring the normal ESP_LOG timestamp/tag prefix, the exact payload format is:

```text
POWER_WOM: DEVELOPMENT_ONLY reset=<reason> ok=<0|1> stage=<1..16> failed=<0|1..16> op=<1|2|3> reg=0x<rr> err=<decimal esp_err_t> expected=0x<ee> mask=0x<mm> actual=0x<aa> actual_valid=<0|1> polls=<total> phase_polls=<current stage> elapsed_ms=<stage duration> total_ms=<configuration duration> cause=<0..3>
```

Operation1=read,2=write,3=poll. Cause0=success/no failure;1=transport (err nonzero);2=poll deadline (err0, last actual available);3=readback mismatch (err0, actual_valid1). failed identifies the first failed stage, not a later inferred cause. actual is meaningful only when actual_valid=1. Write stages have no invented readback. Poll count includes failed read attempts; phase_polls is the failed/final stage only. Timeouts retain the last successfully read value and unchanged500/1000ms deadlines.

| Stage | Operation/register | Existing expectation | Distinguishing evidence |
| --- | --- | --- | --- |
|1 IDENTITY|read00|05/ff|cause1 transport versus cause3 wrong identity|
|2 RESET|write60|b0|cause1 failed reset transaction|
|3 RESET_ACK|poll4d|80/ff,500ms|cause1 transport versus cause2 reset acknowledgement timeout|
|4 CTRL1|write02|48|cause1 transport|
|5 DISABLE|write08|00|cause1 transport|
|6 ODR|write03|2d|cause1 transport|
|7 THRESHOLD|write0b|c8|cause1 transport|
|8 INT_CONFIG|write0c|84|cause1 transport|
|9 APPLY|write0a|08|cause1 apply-command transport failure|
|10 APPLY_ACK|poll2d|80/80,1000ms|cause1 transport versus cause2 apply acknowledgement timeout|
|11 ACK|write0a|00|cause1 command-clear transport failure|
|12 ACK_CLEAR|poll2d|00/80,1000ms|cause1 transport versus cause2 acknowledgement-clear timeout|
|13 ENABLE|write08|01|cause1 transport|
|14 VALIDATE_CTRL7|read08|01/ff|cause1 transport versus cause3 final readback failure|
|15 VALIDATE_CTRL1|read02|48/ff|cause1 transport versus cause3 final readback failure|
|16 CLEAR_STARTUP|read2f|read only,mask00|cause1 startup-status transport failure; no new status-value check|

On success, both boot types use exactly this shape (variable fields are explicitly placeholders):

```text
POWER_WOM: DEVELOPMENT_ONLY reset=<actual boot reason> ok=1 stage=16 failed=0 op=1 reg=0x2f err=0 expected=0x00 mask=0x00 actual=0x<status byte> actual_valid=1 polls=<N> phase_polls=0 elapsed_ms=<E> total_ms=<T> cause=0
POWER_WOM: DEVELOPMENT_ONLY reset=3 ok=1 stage=16 failed=0 op=1 reg=0x2f err=0 expected=0x00 mask=0x00 actual=0x<status byte> actual_valid=1 polls=<N> phase_polls=0 elapsed_ms=<E> total_ms=<T> cause=0
```

First line is clean/upload boot; do not predict its reset reason. Second is successful configuration after controlled serial R/esp_restart (software reset3). Success is NOT assumed for either boot. If failure recurs, ok=0, stage=failed at the failing step and cause=1/2/3; the actual fields identify the failure. A successful configuration summary alone does not prove physical motion wake.

### Host and compiler evidence

Focused instrumentation-enabled:7 PASS/0 FAIL/27555 assertions; disabled:7/0/21509, zero allocations. Both execute the frozen pre-instrumentation configuration as an oracle and compare outcomes, exact read/write/delay traces and register state. Cases cover success/endurance, transport failure at every successful-sequence operation, identity/final readback mismatch, each polling timeout, delayed acknowledgement and clock wrap. Completed record retention is tested. No original validation check was weakened.

Full normal:276 PASS/0 FAIL/559486 logged assertions; full soak:276/0/5193736. Initial focused compilation caught three misleading-indentation warnings under Werror; braces corrected in the test fixture before final runs. Final host compilation has no warnings/errors.

Exact target compilation with the build flags plus fstack-usage: Board::initialize96 B, Wom::configure32 B, poll32 B, wait64 B, separate non-inlined report_wom112 B. Board object remains12 B. The post-config logging helper runs after configure returns; it is not nested inside polling. Physical startup watermark must still be remeasured; these frames are not a physical stack-improvement claim.

### Minimal owner retest (no upload performed here)

1. After owner approval/manual upload, capture the complete clean/upload boot log, including POWER_WOM, POWER_RESET and the existing ADC/IMU/sleep_ready lines. No exact failing stage is predicted. Keep this log before rebooting.
2. Send B and retain IMU/motions/bus_errors. Allow normal display idle (or existing I command); lift to test motion wake, use touch to recover, then B again. Record USB/battery context; no PWR button required and no forced sleep/Wi-Fi reset.
3. With NETWORK ready, send uppercase R once. Capture its existing acknowledgement and the next full startup log. Confirm software reset3 and retained Wi-Fi credentials. Capture the new POWER_WOM line explicitly; do not infer ok from observed wake alone.
4. Repeat the same idle/lift/touch/B sequence. Compare failed stage, transport error, readback/mask, polling counts/durations and reset reason between the two boots. Stop on spontaneous reset/new error. Save both logs for cause-based correction planning; no retry/timing/register fix has been implemented.

Hardware validation of this diagnostic artifact remains PENDING OWNER RETEST. Original Stage2B owner PASS retained; no new functional fix or hardware acceptance claimed.


### Final clean artifact and scope verification

Clean command `pio run -e remote01 -t clean -j 1`: exit0/SUCCESS,2.50s. Build `pio run -e remote01 -j 1`: exit0/SUCCESS,546.07s. Zero compiler/CMake warnings/errors in both final logs. No upload.

Final linked ELF: POWER task528 B and publication helper848 B unchanged; report_wom112 B. Record symbol44 B. Board remains12 B;4096-byte task stack/priority2/core1 unchanged. Source instrumentation adds no heap/PSRAM allocations, tasks, queues or runtime diagnostic client resources. Physical initialization/stack remeasurement pending.

| Resource | Entry artifact | Instrumented artifact | Delta |
| --- | ---: | ---: | ---: |
| BIN |2479648 B|2480512 B|+864 B|
| ELF file |17722572 B|17730600 B|+8028 B|
| Linked application flash |2479145 B|2480017 B|+872 B|
| Static internal DRAM |46908 B|46948 B|+40 B|
| DRAM data / BSS |24540 /22368 B|24540 /22408 B|0 /+40 B|
|3MiB OTA-slot headroom |666080 B|665216 B (649.625 KiB)|-864 B|

The record symbol is44 B; the measured net linked DRAM delta is40 B. These are distinct measurements, and no runtime heap reduction is inferred. ELF file size includes debug information. PlatformIO reports flash against8MiB; actual approved OTA-slot fit is independently calculated against3145728 B. Partition BIN byte-identical.

| Artifact | SHA-256 |
| --- | --- |
| Instrumented BIN |`2cad36cac5504ee539825d1dc3b65b5ef43fadc3bcf900635ae7c29d1f2fba8b`|
| Instrumented ELF |`0cb61c112b2696243f61cc9be560624fb6526f26819188449a3ad710d988e570`|
| Partition BIN unchanged |`d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`|

Exact scope: components/power_management/hardware.cpp, hardware.hpp, include/power_management/qmi_wom.hpp; tests/control/run_tests.py; new tests/control/wom_diagnostic_tests.cpp; this report; CHANGELOG.md. Six modified/one added/zero deleted. All other1653 entry files unchanged by SHA-256 comparison; historical report prefix byte-identical. Entry/final inventories, artifact/resource results and test/build logs retained in session TEMP. **DIAGNOSTIC BUILD ONLY / ROOT CAUSE UNPROVEN / OWNER RETEST REQUIRED**. Stop without functional correction or upload.


## 2026-10-05 - OWNER HARDWARE CLOSURE: Stage 2B.2 / current WoM test

**STAGE 2B.2 CLOSED for the authorized publication-frame correction, covered POWER measurements and current WoM investigation/test.** This latest owner evidence supersedes earlier pending-status statements prospectively; historical observations and build/test records above remain intact.

| Item | Exact closure classification |
| --- | --- |
| POWER publication-frame correction | **OWNER HARDWARE VALIDATION PASS for measured/covered paths** |
| S/B -> WoM interference hypothesis | **DISPROVEN on current hardware/firmware** |
| WoM/lift-to-wake current firmware | **PASS in the current owner test** |
| Previous IMU_WOM=0 incident | **REAL HISTORICAL OBSERVATION / CURRENTLY NOT REPRODUCED / ROOT CAUSE UNKNOWN** |

### Exact owner boot evidence

Current instrumented firmware after clean/build/upload:

```text
POWER_WOM: DEVELOPMENT_ONLY reset=11 ok=1 stage=16 failed=0 op=1 reg=0x2f err=0 expected=0x00 mask=0x00 actual=0x00 actual_valid=1 polls=8 phase_polls=0 elapsed_ms=0 total_ms=29 cause=0
POWER: IMU_WOM=1
sleep_ready=1
```

Configuration completed successfully with no recorded failure/transport error/mismatch/timeout. The owner also reports that an earlier instrumented software reboot succeeded with ok=1; no additional raw reboot summary is supplied in this closure, and none is invented.

### Owner S/B interference sequence - no serial R used

1. Before any S/B: lift-to-wake **PASS3/3**, wake=2, manager_wake_to_ready_ms approximately220-247ms.
2. First S: no functional regression; lift-to-wake remains **PASS3/3**.
3. Second S: no functional regression; lift-to-wake remains PASS. REPORT lowers the observed POWER minimum to1304 B.
4. First B: no functional regression; IMU=1, motions>0, bus_errors=0, stack_min_bytes=1304.
5. B repeated to total10: no functional regression; POWER minimum settles at1256 B, IMU remains1, bus_errors remains0.
6. Final lift-to-wake after all S/B: **PASS3/3**, wake=2, manager_wake_to_ready_ms approximately239-246ms.

No serial R occurred during this sequence. Thus S/B disabling lift-to-wake is experimentally disproven for the current tested hardware/firmware; this is not a claim about every untested firmware or path.

### Covered POWER headroom decision

| Latest measured interval | Minimum remaining stack |
| --- | ---: |
| Baseline |approximately2840 B|
| STARTUP |1416 B|
| REPORT |1304 B|
| Repeated B/COMMAND deepest measured minimum |1256 B|

All currently covered paths exceed the provisional>=1024 B review target. **1256 B is the latest run's lowest covered measurement**, not a replacement for the previously recorded1248 B on an earlier run; both remain valid historical observations. The target is a review target, not a safety guarantee or proof of all production/error/forced-sleep paths. No POWER stack-size or publication correction change is justified by these results.

### Historical incident and closure boundary

Earlier IMU_WOM=0/sleep_ready=0 and failed lift wake were real owner observations. Lift wake subsequently worked after software R, but that earlier post-R IMU_WOM value was not captured. Current bounded instrumentation shows successful upload initialization; instrumented software reboot also previously succeeded per owner. The earlier failure has not been reproduced with this instrumented firmware. Root cause remains unknown; successful current testing neither invalidates the incident nor proves instrumentation cured it. **No WoM functional fix, retry, delay/workaround or weakened check has been applied.** Preserve current instrumentation for evidence if it recurs.

Stage2B.2 is closed at this bounded owner-validated scope. This does not close overall Stage1 (owner's Step1) or the broader completion roadmap. The existing Stage1 mandatory acceptance ledger still requires tested-artifact identity, independent physical battery-voltage comparison, explicit normal regression/lifecycle confirmation,20 idle/wake cycles and gesture isolation,10 untouched minutes per power source with no automatic sleep, safe LOW/CRITICAL/recovery simulation, and30min recorded battery-only endurance where not already evidenced. Latest S/B/lift results do not supply all those counts/durations or waive them. See [Stage1 mandatory acceptance review](STAGE_1_BATTERY_POWER_MANAGEMENT.md#mandatory-acceptance-review). No new acceptance requirement, deep-sleep claim or feature is introduced.

Documentation-only closure: this report, CHANGELOG.md and the canonical REMOTE01_COMPLETION_ROADMAP.md updated. Source/tests/configuration/artifacts untouched; no tests/build/upload. Current POWER stack/publication correction and bounded WoM instrumentation preserved.
