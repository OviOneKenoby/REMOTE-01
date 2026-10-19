# Automatic SYSTEM_SLEEP / NETWORK implementation ? 2026-10-05

Status: IMPLEMENTED / HOST REGRESSION + SOAK + CLEAN BUILD PASS; OWNER HARDWARE VALIDATION PASS for motion wake and SYSTEM_SLEEP -> touch wake -> first touch consumed -> second touch normal -> NETWORK/diagnostics recovery; remaining unreported checks PENDING. No upload. Historical Stage1 remains OPEN. Earlier audit STOP was real for the prior source and is superseded by the separately authorized owner handshake, not rewritten.

## Verified before-change behavior

POWER4096/priority2/core1; NETWORK6144/priority1/core0. DISPLAY_IDLE30s; SYSTEM_SLEEP300s total physical inactivity. Model required external ABSENT; real telemetry UNKNOWN inhibited automatic sleep. Forced development Z parked Core/LVGL but did not stop NETWORK, Wi-Fi, provisioning HTTP/DNS or connected diagnostics. The selected ESP-IDF5.5.3 manual esp_light_sleep_start path requires Wi-Fi stopped; connection-preserving automatic PM is not configured. [Pinned SDK documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html#wi-fi-bluetooth-and-sleep-modes).

The previously armed2s RTC timer was explicitly documented as periodic battery/WoM health fallback. It woke the MCU every round; timer-only wake did not activate the display and the runtime resampled/re-entered sleep. It was not required to generate the verified physical GPIO8 wake. Normal light sleep now uses physical GPIO8 wake only; no UART/USB/Wi-Fi wake is added. Health sampling pauses while asleep and resumes on physical wake; no claim of continuous battery/IMU monitoring while asleep or guaranteed cell protection is made. Installed1000mAh protection PCB limits remain unknown. Development Z retains2s fallback, deliberately different from normal policy. POWER owns a timer-armed flag: disable only when previously armed, because IDF rejects disabling an absent wake source.

## Implemented owner lifecycle

1. POWER model at300s considers hardware/WoM readiness, valid real measurement and hold, independent of external PRESENT/ABSENT/UNKNOWN. Existing faults/60s refusal retry retained; no hard-off or voltage threshold change.
2. POWER opens an atomic epoch-tagged request. NETWORK readiness wait is10s with10ms task delays. REFUSED/FAULT/timeout cancels; no SDK network operation executes on POWER.
3. NETWORK refuses active portal/OPEN_SETUP, candidate credentials, scanning/submission, reset confirmation, pending reboot, uninitialized/no-stored-credential state or exhausted counters. No-credentials boot retains existing provisioning; it consequently blocks sleep while setup is active. Healthy CONNECTED, CONNECTING and RETRY_WAIT with saved credentials are eligible. The existing10s development clear-arm expiry is honored; settings tickets retain existing expiry/invalidation. Pending messages atomically prevent inbox sealing; no pending destructive message is discarded or executed by the sleep lifecycle.
4. NETWORK seals its existing8-slot inbox, rejects public mutations/status commands during transition, gates driver callbacks, advances generation, clears transient connect/portal-close deadlines and publishes DISABLED/IP unavailable. It revokes diagnostics access, stops diagnostics HTTP with success check, stops/checks portal HTTP and DNS, then calls esp_wifi_stop on NETWORK. Portal is normally a blocker; stop hooks are defensive and tested. Saved credentials/NVS remain untouched.
5. A unique4-byte event-loop barrier drains stop callbacks before readiness. One existing default event loop is reused, no new queue/task. NETWORK responds READY only after all stop hooks and barrier succeed. Stop failure returns FAULT and owner recovery; POWER never sleeps on failure. SDK HTTP stop itself lacks an externally enforceable hard deadline: POWER's10s deadline bounds coordinator waiting, not forced termination of a blocked SDK call. A permanently blocked NETWORK cannot grant READY. Socket timeouts remain existing3s/bounded client limits.
6. Only then POWER requests existing Core park (500ms), then LVGL park (500ms). Unsafe pending Core work still refuses. Physical touch during the prepare/park interval cancels sleep and is consumed as wake. LVGL stops its5ms tick; existing handler/flush completion, display gating, GPIO8 level-wakeup/NEGEDGE restoration and esp_light_sleep_start sequence retained.
7. GPIO wake resumes existing POWER motion/touch handling. Motion is wake only; first touch is consumed until release with existing200ms settling. POWER clears Core request; Core resumes/reacquires a fresh session/incarnation and publishes before LVGL is unparked. After UI unpark POWER requests NETWORK resume.
8. NETWORK fences stopped-driver callbacks again, unseals admission with a fresh generation, selects STA and calls esp_wifi_start. It reuses existing configure/connect30s deadline and5..60s reconnect/backoff with the same RAM credentials/remote_wifi NVS format. No provisioning is opened merely on wake. Diagnostics restarts only through existing healthy current STA IPv4/subnet lifecycle. Local UI recovery does not wait for association/DHCP.

Resume SDK failure remains NETWORK-owned and retries at1s while admission is closed. Failed/cancelled request cannot be mistaken for READY; epoch identity and monotonic no-wrap counters fence late acknowledgements. Resume of a prepare refused before sealing is a no-op. Existing8-message capacity and overflow rejection remain; no extra queue.

ESP-IDF netif also has a delayed lost-IP timer. FIFO fencing alone cannot identify that future timer; current handler ignores lost-IP while reconnecting and retains existing live healthy association/IP check before treating a connected loss as real. Pending SDK/task/software timer expirations run after physical resume, not as newly configured RTC wake sources. NETWORK polling/DNS, UC diagnostic cadence and parked Core/LVGL task delays are not RTC wake configuration. No Bluetooth path, application PM lock/configuration, Wi-Fi wake or periodic2s normal wake is enabled.

## Memory / protected scope

No application heap allocation in the handshake/timer models; fixed atomics/owner state and task-local lambda captures only. Existing SDK default-loop handler registration adds one fixed handler/base/id/context; barriers use SDK event allocation for one4-byte ticket per bounded attempt and the existing bounded event queue. SDK Wi-Fi/HTTP stop/start retain their existing allocation behavior; hardware leak/resource validation remains pending. No persistent logs/NVS churn. POWER publication non-inlined helper and WoM instrumentation unchanged. Configured POWER4096, NETWORK6144, UC_owner24576 and LVGL4000 unchanged. No GPIO/driver/WoM/rotation/touch/carousel/brightness change; no external5V sensing, charging claim, OTA/partition change, new task or queue, automatic hard-off, threshold change or frozenCCM change.

## Verification

Focused handshake fixture:9 PASS/0 FAIL/5062 assertions/1000 cycles; source integration guard1 PASS/0 FAIL/21 assertions. Complete normal suite286 PASS/0 FAIL/544590 assertions; complete soak286 PASS/0 FAIL/5223840 assertions, including10000 sleep/resume model cycles and existing owner/power stress. Both WoM/stack-instrumentation enabled and disabled variants retained. Compiler host flags -Wall -Wextra -Werror. Normal/soak commands: `python tests/control/run_tests.py`, `python tests/control/run_tests.py --soak`.

Clean target commands `pio run -e remote01 -t clean -j 1`, `pio run -e remote01 -j 1`: SUCCESS (2.09s /549.86s); zero compiler/CMake warnings and errors. Local evidence logs/manifests in session TEMP/remote01-network-sleep. No serial/hardware command, upload, erase or migration executed.

| Artifact/resource | Accepted checkpoint | New clean artifact | Delta |
| --- | ---: | ---: | ---: |
| firmware.bin bytes |2480512|2483904|+3392|
| Linked flash bytes |2480017|2483405|+3388|
| Static DRAM bytes |46948|46980|+32|
| .dram0.data |24540|24540|0|
| .dram0.bss |22408|22440|+32|
| OTA slot bytes |3145728|3145728|0|
| OTA-slot headroom by BIN |665216|661824|-3392|
| ELF file bytes |17730600|17765480|+34880|

Headroom661824bytes,21.04% of each3MiB slot. PIO's flash percentage uses its8MiB board maximum; slot headroom above uses the actual accepted3MiB slot, not that display denominator. BIN/ELF hashes:

- BIN SHA-256: `4f2542151766d50aaae0c887daf6d97dc2c252da7ade3a555ce57db31fc0ef03`
- ELF SHA-256: `0bc16f10af5638baf9be56ed9e702a71b3e2ebab6efc3c511a586cdcc86e99e3`
- partitions.bin SHA-256: `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835` ? byte-identical to baseline, layout unchanged.

Target Xtensa objdump entry reservation before/after (baseline objects preserved before clean): POWER task528->544(+16); NETWORK task2176->2256(+80); POWER publish_power848->848. New NETWORK fence48, stop48, resume48, Owner::tick48, quiesce48, defensive portal-stop adapter96; POWER timer helper32. Nested SDK/caller frames are additional, these are not maximum whole-call-chain bounds. Existing NETWORK helper frames remain224/240/32/160/32/128. Configured task stacks unchanged. New target symbol sizes: Channel12, Owner16, callback admission1, barrier ack4bytes; Inbox976->984(+8), unchanged8-message capacity. Linker total DRAM delta32 accounts for actual alignment/link retention, not a sum of field sizes. SDK adds bounded handler registration nodes and short barrier-event allocations; exact allocator overhead/dynamic peak remains hardware-measurement pending. No new application task, queue or heap-backed model object.

Exact scope:17 modified existing files +5 added, zero deleted;22 files total. All other non-.pio files match entry SHA-256 manifest, including hardware/WoM/RTC/Core/UI/configuration/partition sources. Generated .pio artifacts are expected clean-build outputs.

- `components/power_management/CMakeLists.txt`
- `components/power_management/model.cpp`
- `components/power_management/runtime.cpp`
- `components/power_management/include/power_management/model.hpp`
- `components/power_management/include/power_management/sleep_timer.hpp`
- `components/network_manager/runtime.cpp`
- `components/network_manager/include/network_manager/api.hpp`
- `components/network_manager/include/network_manager/model.hpp`
- `components/network_manager/include/network_manager/sleep.hpp`
- `components/diagnostics/model.cpp`
- `components/diagnostics/service.cpp`
- `components/diagnostics/include/diagnostics/api.hpp`
- `tests/control/power_tests.cpp`
- `tests/control/diagnostics_tests.cpp`
- `tests/control/run_tests.py`
- `tests/control/network_sleep_tests.cpp`
- `tests/control/network_sleep_boundary_tests.py`
- `docs/REMOTE01_PRODUCT_DECISIONS.md`
- `docs/REMOTE01_COMPLETION_ROADMAP.md`
- `docs/STAGE_1_BATTERY_POWER_MANAGEMENT.md`
- `docs/SYSTEM_SLEEP_NETWORK_IMPLEMENTATION.md`
- `CHANGELOG.md`

Host tests cover actual portable owner/channel/stop ordering and eligibility, faults/resume failure/retries, cancel/late acknowledgement, epoch/generation exhaustion, saturated/sealed inbox, repeated cycles, external-state-independent30s/300s policy and unsafe work, existing first-touch/motion/owner-park behavior, and timer normal/development separation. Source integration guards cover SDK owner placement, protected credentials, live/generation fencing and POWER/Core/LVGL ordering. These do not simulate the Wi-Fi driver, actual HTTP thread shutdown, radio power, physical GPIO or target stack watermark. Compiler frame reservation is not physical stack improvement/safety proof; >=1024bytes remains a review target only.

## Owner hardware validation procedure

Use this newly built artifact only after owner-controlled flashing; no erase or partition migration. No PWR, K/!, G/Y or credential reset is needed. Preserve current credentials. Reattach115200 serial monitor if light sleep disconnects USB Serial/JTAG.

1. USB boot: capture POWER automatic_system_sleep=ENABLED external_gate=NONE system_idle_ms=300000, sleep_ready=1, IMU_WOM=1; W must show CONNECTED/stored=1/provisioning=0/current IP. Verify HOME, controls/carousel and browser diagnostics. B/S establish awake stack baseline. FAIL on initialization errors, missing wake readiness, credential loss or unintended provisioning.
2. Leave physically untouched, without I or Z. At~30s display goes idle. At~300s total inactivity expect NETWORK_SLEEP prepare=READY, POWER state=SYSTEM_SLEEP/hold=1/display=0, diagnostics becomes unavailable and Wi-Fi disconnects. Browser polling is not physical activity and must not prevent this. Leave asleep at least60s: there must be no normal2s sleep rounds/wake cadence. Serial may disconnect: lack of serial alone is not proof of sleep. Compare rounds/last_sleep_cause after wake; use a suitable physical current measurement to verify low stable sleep current. No endurance/savings claim without measurement.
3. Lift once: motion wakes only, no track/playback command. Capture NETWORK_SLEEP resume=STARTED, reconnect/IP recovery, browser recovery, B last_sleep_cause GPIO (SDK7), hold=1, errors/bus_errors unchanged and fresh session/no buffered control replay. W stored=1/provisioning=0. Association may use normal backoff; IP can change, read HOME/W. Check S/B repeated stack margins after recovery. FAIL on reset/WDT, spurious control, stuck UI or lost credentials.
4. Repeat untouched300s sleep; first touchscreen contact wakes only, release then second deliberate touch operates. Repeat at least10 sleep/wake cycles, including with browser polling and AP temporarily unavailable during wake. Restoring the same AP must use saved-STA reconnect, not provisioning. Do not use R as a wake mechanism.
5. Repeat normal300s sleep and both physical wake methods on battery after USB boot/hold handover. USB-powered normal sleep is intentional under the new owner policy. No source-present hardware claim: PRESENT/ABSENT/UNKNOWN eligibility is proven by host model until divider sensing exists. No deep-discharge experiment or automatic release test.
6. Active setup/candidate/scan/reset-ticket/reboot blockers and NETWORK stop/resume failure injection are host-covered. Network Settings lacks owner-accessible UI/harness; physical backend cases remain BLOCKED BY DESIGN, not PASS/FAIL. Do not erase credentials merely to test this policy. Do not treat development Z's2s fallback as normal sleep evidence. Report stack, memory/allocation counters and measured current/endurance separately before production acceptance.

Hardware acceptance remains PENDING for this new lifecycle; prior Today's Step1/Stage2B acceptance is preserved, broader historical Stage1 OPEN.

## Owner hardware evidence - 2026-10-05

**SYSTEM_SLEEP + NETWORK QUIESCE/RESUME - OWNER HARDWARE VALIDATION PASS for motion wake.** Exact owner scope: motion wake. This is physical owner acceptance of that path, not compiler/host inference. The owner did not supply new telemetry, timing, supply-condition, cycle-count, current/endurance or stack measurements in this result; none are inferred. Touch-wake acceptance and other unreported procedure checks remain pending. This supersedes the blanket pending status above only for the reported motion-wake path; earlier preparation/pending records remain history. Broader historical Stage1 remains OPEN. Documentation/status only, no source/tests/configuration/artifacts/build/upload changes.

## Owner touch-wake evidence - 2026-10-06

**SYSTEM_SLEEP -> TOUCH WAKE -> FIRST TOUCH CONSUMED -> SECOND TOUCH NORMAL -> NETWORK/DIAGNOSTICS RECOVERY: OWNER HARDWARE VALIDATION PASS.** This is the owner's physical validation of the reported end-to-end path. First contact is wake-only, subsequent contact operates normally, and NETWORK/diagnostics recover. It complements the2026-10-05 motion-wake PASS and supersedes touch-wake pending status for this covered path. Earlier motion-only and preparation records remain historical evidence. No additional telemetry, current/endurance, new stack minimum, cycle count, supply condition or other test result was supplied; none are inferred. Broader historical Stage1 remains OPEN, and remaining unreported checks stay PENDING. Documentation/status only; no implementation, tests, configuration, artifacts, build or upload changes.
