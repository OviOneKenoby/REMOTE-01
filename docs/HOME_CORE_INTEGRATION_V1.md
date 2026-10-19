# HOME / Universal Control Core integration v1

Current owner milestone status (2026-10-04): **Phase D OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED**. Phase C remains accepted and closed. [The Phase D acceptance record](PHASE_D_GENERIC_ADAPTER_BOUNDARY.md#current-status-owner-hardware-acceptance-and-closure---2026-10-04) supersedes earlier pending-retest labels; the phase-specific results/procedures below remain historical. No additional physical measurements were supplied with this verdict. Documentation only; firmware unchanged, no build/upload or next milestone.

Latest owner hardware result (2026-10-03): **HOME/Core/Mock hardware validation: PASS**. The owner acceptance closes the development serial input retest and overall Phase B hardware gate: **PHASE B CLOSED / PASS**. Earlier failure and pending-retest records below are historical and superseded by this acceptance.

2026-10-03. Scope: REMOTE//01 Phase B only, using an entirely synthetic in-process device. Frozen CCM v1.0.0 is unchanged. Phase A owner acceptance and every supplied physical checkpoint are recorded in [the implementation report](UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md#phase-a-owner-acceptance-and-closure--2026-10-03). Phase A is **CLOSED / PASS**; it does not approve Phase B hardware behavior.

Current Phase B status: **HOME-CORE INTEGRATION — COMPILE / HOST TEST VERIFIED; HOME-CORE-MOCK HARDWARE VALIDATION — OWNER ACCEPTED / PASS; PHASE B — CLOSED / PASS**. No TUNER repository access, native adapter, transport, discovery, networking, IR, cloud or upload occurred.

Subsequent Phase C hardening is recorded in [the Phase C report](PHASE_C_RUNTIME_HARDENING.md): additional owner Phase B evidence, scoped queue invalidation, failure-safe delayed completion, admission acknowledgement, measured allocation corrections and owner retest gate. The Phase B implementation/procedure sections below remain historical; Phase C is not yet owner hardware validated.

## Ownership and boundaries

```text
LVGL callback -> eight-entry intent FIFO -> CPU 1 Core owner
 -> generic Facade capture/submit -> Core -> synthetic Adapter
 -> separate acquired observations/result evidence -> Core
 -> copied Snapshot -> one coalesced mailbox -> existing LVGL owner timer
```

Normal boot starts `UC_owner`: CPU 1, priority 2, 24,576-byte internal stack, after unchanged UI initialization and mutex release. It waits five seconds for the existing factory splash to finish. It exclusively constructs, reads and mutates the persistent Store, Core, process-local Mock Journal, development Adapter and owner-side Facade. The containing Session explicitly uses PSRAM; Core's existing bounded malloc policy is unchanged. These objects remain alive until reboot. No second Core implementation exists.

The existing CPU 0 LVGL task retains its mutex, priority and stack. HOME callbacks do no Core work: they enqueue a neutral `Action` plus a monotonically issued token. Core performs maintenance, capability checking, capture and submission on its owner. PREVIOUS/NEXT map to their exact CCM operations. TOGGLE is resolved from fresh, accepted, non-conflicted playback on the owner: PLAYING -> playback.pause, PAUSED -> playback.play. The submitted toggle includes the selected observation/value precondition. UNKNOWN, conflict, other playback states and insufficient availability cannot dispatch a guessed toggle. No UI-local playback boolean or optimistic widget update exists.

Tokens identify independent presses. Repeating the latest token/action resubmits the same immutable Core envelope, reaching Core deduplication; mismatched repeats and older tokens are rejected. Only one retained envelope is needed. The FIFO has eight fixed slots, drops the newest press when full, increments `queue_rejected`, and never retries it. At most one intent is processed per owner turn. While synthetic delayed work is outstanding, new presses are rejected and all three buttons are disabled; nothing is replayed after completion.

The generic public `api.hpp` exposes no Core, Store, adapter or LVGL pointer. Snapshot is a trivially copyable value with two 32-byte UTF-8 buffers (31 bytes plus NUL), presentation enums, volume/availability flags and session/capability revision. Truncation preserves complete UTF-8 codepoints. A short FreeRTOS critical section protects FIFO operations and the latest snapshot. There are no heap allocations or Core references in a mailbox copy. The UI owns its copy; later publication/destruction cannot invalidate it.

The owner reduces Core state before copying. One snapshot includes the complete observed HOME state and capability availability; no consumer sees partially acquired media fields. Changed tracks first invalidate the Core media context, then publish the newly acquired playback/title/subtitle. A new synthetic device read occurs once per second with a new source ordinal, not a cache replay with a refreshed timestamp. Command results and state observations remain distinct.

The owner yields every 50 ms, handling at most eight serial bytes and one intent. Core deadlines/purge run on its real monotonic clock. LVGL's 100-ms timer reads only the latest mailbox generation; unchanged generations cause no widget work. Changed snapshots update only differing label strings/volume widths; disabled state operations are idempotent. Intermediate generations coalesce, with no queued LVGL events. Page deletion deletes the timer and clears all widget references. Core never touches LVGL; carousel and brightness remain independent.

## HOME behavior and preserved geometry

Dynamic fields: connectivity label; title; secondary metadata; playback label and PLAY/PAUSE symbol; volume number and 148-pixel bar; PREVIOUS/TOGGLE/NEXT availability. The initial synthetic state is CONNECTED, `Mock One`, `Synthetic`, PLAYING, volume 68, all four playback operations supported/READY. PREVIOUS/NEXT cycle `Mock One` / `Mock Two` / `Mock Three`. These are development data, not radio stations or TUNER behavior.

Existing HOME coordinates, panel dimensions, palette, fonts, artwork placeholder, static device/footer literals and navigation rows remain unchanged. Battery `87%` and the Wi-Fi symbol remain decorative/demo values with no measured battery/RSSI meaning. Static `TUNER//01` product literals do not identify the bound synthetic device. Quick navigation remains visual/static; volume remains display-only. Disabled controls use the existing panel style at 40% opacity; no redesign or new font/asset/library.

Status/playback labels now explicitly use the existing 148-pixel single-line region; title retains ellipsis and subtitle retains clipping. Native vendored font measurements pass for all 16 new dynamic strings/symbol samples: longest title `Mock Three` 118x22 in 148x22; `Synthetic` 70x16; `UNAVAILABLE` 89x15; three-digit `100` 33x22 in 36x22; control glyphs 18x22 in 44x52. Seven direct glyph lookups also confirm the required control/status/Wi-Fi symbols exist in the selected font, without fallback. Existing enabled Montserrat 12/14/16/20/28 availability was verified in sdkconfig; no font was enabled. Existing layout remains within 172x640, with no newly introduced overlap/wrapping. Volume level is bounded to its 148x6 parent, including 0 and 100. Host font geometry is not physical validation.

Page 2 creation, ID, carousel parent/style/geometry and the logical position X=344 are unchanged. Pages 0/1, non-snappable spacer X=516, end stops, snapping, page order, touch transform, LCD rotation/initialization, GPIOs, backlight, drivers, LVGL configuration and PlatformIO package versions remain unchanged.

## Development-only / Mock-only serial hook

Existing configuration provides a secondary USB Serial/JTAG console. Following the owner-discovered serial defect, the Core owner now installs ESP-IDF's USB Serial/JTAG driver with its default bounded 256-byte RX and 256-byte TX buffers, switches the existing USB VFS log backend to that driver, and directly calls `usb_serial_jtag_read_bytes(..., 0)` on the same USB interface. No UART, console configuration, pins, touch/LCD driver or HOME button path changes. No shell is added. CR/LF and other nonprintable bytes are silent; unknown printable ASCII bytes receive `recognized=0 accepted=0`. Every recognized key, including `m`, receives an acknowledgement with `owner_ok`. If driver initialization fails, `input=UNAVAILABLE init=<error>` is reported and UI operation continues without attempting a read. **Serial input OWNER ACCEPTED / HARDWARE PASS** following the owner-reported retest; driver initialization success alone was not a hardware-validation claim.

| Key | Synthetic test effect |
| --- | --- |
| `p` / `a` | External PLAYING / PAUSED observation |
| `u` | External UNKNOWN playback; toggle disables |
| `0` / `1` / `v` | External volume 0 / 100 / 25 |
| `d` | Unreachable; current observations invalidate and descriptors go OFFLINE |
| `r` | Reconnect increments session, republishes readiness of surviving capabilities, reacquires state |
| `-` / `+` | Actually withdraw NEXT descriptor / republish supported NEXT |
| `o` / `c` | NEXT CONTEXT_UNAVAILABLE / READY; revision advances |
| `s` | Inject older-sequence volume=99; accepted=1 means expected rejection, HOME must not change |
| `l` / `n` | Enable delayed results / normal confirmed mode |
| `f` | Complete pending synthetic execution with original correlation, then acquire actual synthetic state |
| `m` | Print on-demand memory/owner telemetry |

Reconnect never resurrects a withdrawn descriptor; use `+` explicitly. If a delayed attempt spans reconnect, its old-session completion cannot execute in the new session; controls remain blocked conservatively. Reset the synthetic test runtime to restart that case rather than inventing a cross-session replay/fence. Conflict behavior is automated-test-only; no hidden HOME gestures were added.

## Validation and review

Initial Phase B `python tests/control/run_tests.py`: **69 foundation PASS / 0 FAIL, 2425 assertions; 6 profile PASS / 0 FAIL, 40 assertions; 34 HOME PASS / 0 FAIL, 3507 assertions. Total 109 PASS / 0 FAIL, 5972 assertions.** Strict compiler warnings are errors. Host execution uses virtual time, including profile retention waits; it is not ESP32/UI hardware evidence.

HOME tests cover every requested minimum category: coherent copies, connectivity, playing/paused/unknown/conflict, volume endpoints, external changes, stale sequence rejection, all four operation routes, unsupported/withdrawn/restored/unavailable/revised capabilities, session invalidation, token/Core dedup, HOME dependency/callback checks, copy lifetime, deterministic FIFO overflow, coalescing, freshness expiry, delayed evidence, repeated acquisitions and 150-command retention/purge. Each test checks tracked allocation balance. Source boundary checks are structural checks, not proof of every possible future edit.

Implementation review checked all prompt hazards: no UI->Mock/Core access, no Core->LVGL access, no escaped Core pointers, no UI-owned truth/optimism, conservative toggle, stale/conflict/session guards, bounded work/storage, coalesced copies, same accepted hardware/layout configuration, no normal-boot stress, and accurate validation labels. The first acquisition hook incorrectly used Core's dispatch-only `publish` outside dispatch; tests exposed missing initial observations and a test dereference fault. Corrected to serialized owner-side `observe`, with media-context invalidation before track acquisition; complete suites reran successfully. The conflicting-sequence test was corrected to expect Core's deliberate observation rejection while preserving its conflict latch. No frozen-model contradiction was found.

Generic Core additions are controlled reachability update (invalidates observations and descriptor readiness on lost reachability) and descriptor withdrawal (advances revisions/revalidates commands). No caller directly edits Core registries. Mock field descriptors additionally support existing CCM media String values. No Core capacity, persistence policy or frozen semantics changed.

### Initial Phase B build and telemetry (before serial correction)

Exact commands, unchanged installed executable `C:\Users\RYZEN\.platformio\penv\Scripts\pio.exe`:

```text
pio run -e remote01 -t clean -j 1
pio run -e remote01 -j 1
```

Final reviewed-source clean: **SUCCESS, 1.99 seconds**. Final clean firmware build: **SUCCESS, 519.46 seconds; 0 compiler warnings, 0 compiler errors**. An earlier clean build also passed (516.37 seconds); after the final schema restriction/session diagnostics, the complete host suites and both exact clean/build commands were repeated. No package/toolchain/configuration version changed and no upload was performed.

| Link measurement | Phase A baseline | Final Phase B | Exact delta |
| --- | ---: | ---: | ---: |
| Flash | 1,742,537 | 1,741,653 (20.8% of 8,388,608-byte application limit) | -884 |
| Static RAM | 19,392 | 19,648 (6.0% of 327,680 bytes) | +256 |

The normal image excludes unreferenced long-profile execution code; profile sources remain available for the explicit opt-in build. Static link RAM excludes the dynamically allocated task stack, PSRAM Session and Core payloads. Flash size is not a runtime memory/headroom measurement.

Normal boot emits `UC_RUNTIME BEGIN synthetic_only=1 profile=0`, wrapper bytes/PSRAM placement, and initialized telemetry. It never executes the old retention/stress sequence. Profiler sources and historical log wording remain unchanged; manual compile-time `REMOTE01_PHASE_A_PROFILE=1` opts into the old profiler instead of creating the normal owner (not enabled in this build).

`UC_RUNTIME` telemetry reports actual internal/PSRAM free/minimum/largest blocks, Core tracked payload, allocations/frees/failures, accounting corruption, owner stack minimum, owner_ok and queue rejection count. `m` obtains a later reading. Memory observation also checks allocation ownership. Persistent live allocations are expected; allocations need not equal frees while the runtime is alive. Boot-lifetime minimum free/stack readings include preceding activity. No Phase B physical readings are invented.

**Open watch item: Phase A minimum internal free heap 34,515 bytes**, accepted nonblocking for Phase A. Owner must review Phase B combined headroom/stability. Future substantial networking/adapters/discovery/TLS/catalog/artwork loads require repeated real ESP32 profiling. No optimization was made solely because this watch item exists.

Limitations: one synthetic target/binding; process-local Journal; no durable reboot recovery, real device, protocol or transport; bounded presentation buffers; volume/quick navigation/battery/Wi-Fi remain as described; debug hook requires the existing secondary USB console. Future adapters must marshal through the owner and supply their verified capabilities/observations; HOME's neutral boundary remains unchanged.

## Owner physical procedure — instructions only

Run from `C:\Users\RYZEN\Documents\REMOTE01\Code` in a real interactive PowerShell terminal. Identify the REMOTE USB Serial/JTAG port; replace COMx. These upload/monitor commands were **not executed by Codex**:

```powershell
$remotePio = 'C:\Users\RYZEN\.platformio\penv\Scripts\pio.exe'
& $remotePio device list
& $remotePio run -e remote01 -t upload --upload-port COMx
& $remotePio device monitor --port COMx --baud 115200 --echo --filter log2file
```

Use the interactive monitor's `log2file` filter to retain output; do not pipe it into Tee-Object in a noninteractive runner. Type each single key from the table (Enter optional). `Ctrl+C` exits. If USB re-enumerates, reconnect/reset and capture a complete boot. Select the USB console port, not a separate UART bridge, for these input tests.

1. Verify the clean build SUCCESS and recorded measurements below before upload.
2. Manually upload to the actual REMOTE board with the command above.
3. Open the interactive serial monitor with capture enabled.
4. Reset the board.
5. Confirm normal boot has `UC_RUNTIME ... profile=0`, no `UC_PROFILE BEGIN`, stress waves or retention waits.
6. Capture initialized UC_RUNTIME heap/counters/owner/stack/wrapper readings and `UC_MOCK ... input=USB_SERIAL_JTAG_DRIVER init=ESP_OK`.
7. Confirm accepted 180-degree orientation remains correct.
8. Swipe LEFT 0 -> 1 -> 2; next page enters from RIGHT.
9. Swipe RIGHT 2 -> 1 -> 0; previous enters from LEFT.
10. Check unchanged snapping and both end stops.
11. Check unchanged brightness page/slider/backlight.
12. On HOME, verify CONNECTED / Mock One / Synthetic / PLAYING / 68 and enabled controls; static 87% is not a sensor result.
13. Press PREVIOUS: Mock Three, one logged dispatch.
14. Press PLAY/PAUSE: PAUSED and PLAY glyph; press again: PLAYING and PAUSE glyph.
15. Press NEXT: next synthetic title, one dispatch per press.
16. Type `l`, press PLAY/PAUSE: presentation stays at accepted observed playback and controls dim; type `f`: completion/observation changes playback; type `n`. Confirm no optimistic change and no queued replay. Also wait over three seconds before `f` once to exercise late completion.
17. Type `a`, then `p` for external playback changes without button presses.
18. Verify PAUSED/PLAYING labels and toggle symbol follow observations.
19. Type `v`, `0`, `1` for external volume changes.
20. Verify number/bar 25, empty 0, full 100; no slider interaction was introduced.
21. Type `d` to disconnect.
22. Verify UNAVAILABLE, absent values `--`/UNKNOWN and dimmed non-actionable playback controls.
23. Type `r` to reconnect.
24. Verify accepted state returns; `UC_MOCK key=r` must log the increased session and capability revision. No stale replay.
25. Type `-` to withdraw NEXT.
26. Verify NEXT dims and tapping it cannot increment dispatches; other supported controls remain usable.
27. Type `+` to restore NEXT.
28. Verify NEXT returns and dispatches once; additionally test `o` disables and `c` restores readiness.
29. Type `v`, then `s` to inject stale volume 99.
30. Verify volume remains 25 (`UC_MOCK key=s accepted=1` denotes expected rejection). Type `u`: UNKNOWN and disabled toggle; tapping must not dispatch. Type `p` to recover.
31. Repeat HOME controls, carousel and brightness use for several minutes. Include several disconnect/reconnect/capability cycles; finish delayed work with `f` before reconnect tests.
32. Type `m`; capture final UC_RUNTIME readings alongside initialized readings.
33. Report any crash/panic; none may be assumed absent from host tests.
34. Report any reset or repeated BEGIN.
35. Report any watchdog.
36. Report any UI freeze and confirm responsive swipe/brightness after the run.

Return the complete captured log, physical results and owner acceptance decision. **Do not mark HOME-CORE-MOCK hardware PASS until owner review/acceptance.**

After owner Phase B PASS, recommended next REMOTE//01 milestone: review/refine HOME interaction/status feedback against the observed hardware results while retaining the generic facade; separately plan future adapter integration and renewed memory profiling. Do not automatically start it or access TUNER.

## Phase B serial-input hardware defect and correction — 2026-10-03

Historical correction/pre-retest record: pending statuses in this section were superseded by the owner acceptance below.

### Owner evidence (before this correction)

Owner hardware PASS: normal boot; `UC_RUNTIME profile=0`; display orientation; carousel LEFT 0 -> 1 -> 2 and RIGHT 2 -> 1 -> 0; snapping; brightness; initial synthetic HOME presentation; PREVIOUS, PLAY/PAUSE and NEXT; track/playback changes and PLAY/PAUSE icon changes; HOME intents reaching Core and being accepted. No crash/reset/watchdog was observed in that run. These are owner physical observations, not inferred host results.

Owner hardware FAIL: the development-only Mock serial interface. On the same interactive PlatformIO COM3/115200 monitor that receives logs, documented keys with or without Enter produced neither UC_MOCK acknowledgements nor HOME changes. Opening the VFS successfully and printing `input=USB_SERIAL_JTAG` did not establish RX readiness. Serial input is defective in the prior firmware; **serial correction PENDING OWNER RETEST**, overall hardware validation incomplete.

### Exact diagnosis against the resolved SDK

The build's `project_description.json` selects `C:\Users\RYZEN\.platformio\packages\framework-espidf@3.50503.0`, whose version is **ESP-IDF 5.5.3**. This versioned SDK, not another installed unversioned SDK, is the source authority for the diagnosis. Project configuration/package versions are unchanged.

| Question | Verified result |
| --- | --- |
| ESP_LOG output | Standard logging uses stdout; configured console duplicates writes to primary UART0 and secondary USB Serial/JTAG. `esp_vfs_console/vfs_console.c`: console_write writes both descriptors. |
| COM3 interface | Windows Win32_SerialPort reports `USB Serial Device (COM3)`, PNP `USB\VID_303A&PID_1001&MI_00\8&4A6CC01&0&0000`: ESP32-S3 native USB Serial/JTAG serial interface, not a UART bridge. Port enumeration was read-only; no monitor/upload/board command was executed by Codex. |
| Prior reader | `open("/dev/secondary", O_RDONLY | O_NONBLOCK)` then read up to eight bytes. Correct USB endpoint, incomplete driver initialization. |
| RX configuration | `CONFIG_USJ_ENABLE_USB_SERIAL_JTAG=y`; secondary USB console enabled; USB CDC and primary USB console disabled; primary UART0 configured at 115200. No configuration disables the native USB capability. |
| Exact silent failure | `usb_serial_jtag_vfs.c::usb_serial_jtag_read`: in nonblocking mode it skips the initial raw FIFO character read, computes fetch_size from `usb_serial_jtag_get_read_bytes_available()`, then reads only if fetch_size > 0. `usb_serial_jtag.c` returns zero available bytes without an installed driver/RX ring buffer. No driver installation existed in the application's old path. Thus fetch_size remains zero and read returns EWOULDBLOCK without reading incoming FIFO bytes. |
| Why TX still works | The registered no-driver VFS TX callback writes directly to the USB hardware FIFO. Secondary VFS registration at SDK startup does not install the buffered RX driver. Successful logging/open was incorrectly treated as evidence that the nonblocking RX path was ready. |
| stdin / API mixing | No Arduino Serial API or UART reader is used by the Mock hook. Ordinary stdin is the **primary UART0** (`console_read` forwards only to fd_primary), so switching to stdin would not fix COM3. Secondary duplication applies to output, not automatic input merging. |
| CR/LF / buffering | Enter cannot fix the zero-availability gate. Old POSIX read bypassed stdio buffering; missing newline was not the cause. Corrected direct driver reads return raw bytes, with CR/LF ignored by the fixed byte dispatcher. |

Relevant SDK sources: `components/esp_vfs_console/vfs_console.c`, `components/esp_driver_usb_serial_jtag/src/usb_serial_jtag_vfs.c`, `components/esp_driver_usb_serial_jtag/src/usb_serial_jtag.c`, and driver/VFS headers, all under the resolved versioned SDK above. The USB driver installation enables RX packet interrupts and the ISR transfers bytes into its bounded ring buffer.

### Small correction and regression checks

The existing Core owner installs the standard USB Serial/JTAG driver once (or reuses an installed driver), with default RX/TX buffers of 256 bytes each. `usb_serial_jtag_vfs_use_driver()` pairs the existing secondary log output with that driver; no stdout/stdin destination is reassigned. Mock input uses `usb_serial_jtag_read_bytes(keys, sizeof keys, 0)` directly: same COM3 hardware endpoint, zero wait, maximum eight bytes per existing 50-ms owner turn. On initialization failure, input is disabled and the actual error name is logged; working HOME still runs. No new task, shell, UART pin/configuration, networking, Core/HOME architecture or Mock behavior change.

The development-only fixed byte dispatcher requires the serialized owner before invoking the unchanged Device.command method. All existing keys are preserved. Recognized keys (including `m`) acknowledge `key=<key> recognized=1 accepted=<0|1> owner_ok=1` plus existing session/revision/dispatch facts. Unknown printable ASCII acknowledges `recognized=0 accepted=0`, with no state change. CR/LF and other nonprintable bytes have no echo/diagnostic. Accepted synthetic observations continue through Core -> copied snapshot -> existing LVGL owner, never direct HOME mutation.

Complete host suites after correction: **foundation 69 PASS / 0 FAIL / 2425 assertions; profile 6 PASS / 0 FAIL / 40; HOME 39 PASS / 0 FAIL / 3541. Total 114 PASS / 0 FAIL / 6006 assertions.** Five added regressions exercise a recognized single byte/telemetry, silent CR, silent LF, deterministic unknown printable bytes, and owner rejection/accepted owner route. Source checks additionally verify zero-wait driver reads and dispatcher placement in the existing owner task. These are not physical USB/COM validation.

Exact correction manifest (six modified files, no added/deleted files): `components/control_home/runtime.cpp`, `components/control_home/include/control_home/development.hpp`, `components/control_home/CMakeLists.txt`, `tests/control/home_tests.cpp`, `docs/HOME_CORE_INTEGRATION_V1.md`, `CHANGELOG.md`. Core, facade/API, synthetic Device semantics, HOME/button implementation, display/touch/carousel/brightness, GPIO/backlight, frozen documents, SDK configuration and package/toolchain versions remain unchanged.

USB driver buffers/control structures/ISR resources allocate dynamically outside Core's tracked allocator; final static RAM does not include that runtime cost. Capture initialized and on-demand UC_RUNTIME heaps/stack on the owner retest. The accepted Phase A 34,515-byte internal-memory minimum remains an explicit watch item.

Final correction commands in the unchanged environment: `pio run -e remote01 -t clean -j 1` **SUCCESS (1.97 s)**; `pio run -e remote01 -j 1` **SUCCESS (505.07 s)**. Invoked using the existing absolute PlatformIO executable. **0 compiler warnings, 0 compiler errors.** Final build metadata confirms the resolved versioned ESP-IDF 5.5.3 package. No upload.

| Measurement | Prior Phase B | Serial correction | Exact delta |
| --- | ---: | ---: | ---: |
| Flash | 1,741,653 | 1,749,625 (20.9% of 8,388,608) | +7,972 |
| Static RAM | 19,648 | 19,648 (6.0% of 327,680) | 0 |

Final checksum comparison verifies the six-file correction manifest and all other inspected source/config files unchanged. Existing HOME button path and Mock Device semantics are byte-identical. No new milestone or physical serial test was performed; these build/test results do not grant hardware PASS.

### Exact owner retest (manual upload only)

From the REMOTE repository in interactive PowerShell, with the same board/COM3:

```powershell
$remotePio = 'C:\Users\RYZEN\.platformio\penv\Scripts\pio.exe'
& $remotePio run -e remote01 -t upload --upload-port COM3
& $remotePio device monitor --port COM3 --baud 115200 --echo --filter log2file
```

1. Check correction clean/build SUCCESS recorded below; manually upload. Open the monitor and reset. Wait for `UC_RUNTIME profile=0` and `UC_MOCK DEVELOPMENT_ONLY MOCK_ONLY input=USB_SERIAL_JTAG_DRIVER init=ESP_OK`. Capture initialized telemetry. Initialization errors are a retest failure; do not treat the interface name alone as RX proof.
2. On HOME, type `a` **without Enter**: expect `UC_MOCK key=a recognized=1 accepted=1 owner_ok=1`, PAUSED/PLAY icon. Type `p` without Enter: explicit key=p acknowledgement, PLAYING/PAUSE icon. Observe changes only through the normal snapshot refresh.
3. Press Enter/send CR and LF: no UC_MOCK key acknowledgement or state change for either. Monitor local echo is host-side, not firmware echo. Repeat `a` and `p` with Enter; each key acknowledges once and line endings stay silent.
4. Type `?`: expect `key=? recognized=0 accepted=0 owner_ok=1`, no HOME change. Type `m`: expect recognized/accepted acknowledgement followed by `UC_RUNTIME stage=requested` heap/stack/owner telemetry.
5. Exercise `v`, `0`, `1` (25/0/100 volume); `u` then `p` (UNKNOWN/recovery); `d` then `r` (unavailable/reconnect, increased session); `-` then `+` (NEXT withdrawal/restore); `o` then `c` (NEXT unavailable/ready); `v` then `s` (stale injection rejected, stays 25). Capture every key marker. `s accepted=1` means the expected stale rejection, as before.
6. Type `l`, press PLAY/PAUSE, confirm no optimistic state change; type `f`, verify correlated completion/observed update; type `n`. Complete pending work before reconnect cases. Preserve the documented conservative cross-session limitation.
7. Repeat accepted HOME buttons, swipes both ways, snapping and brightness; run several minutes, then type `m` again. Capture any loss of logging/input, UI freeze, crash/reset/watchdog, initialization error or unexpected allocation/accounting/owner failure.
8. Return the captured `logs/device-monitor-*.log`, visible results and owner decision. **Serial input PENDING OWNER RETEST; overall HOME-CORE-MOCK hardware validation is not complete.** Codex performed no upload or physical serial test. Stop at this defect correction; no new milestone/TUNER work.

## Exact Phase B file manifest

This is the original Phase B implementation manifest. The following correction changes only the six existing files listed in its own manifest.

New files:

- `components/control_home/CMakeLists.txt`
- `components/control_home/facade.cpp`
- `components/control_home/development.cpp`
- `components/control_home/runtime.cpp`
- `components/control_home/include/control_home/api.hpp`
- `components/control_home/include/control_home/facade.hpp`
- `components/control_home/include/control_home/development.hpp`
- `tests/control/home_tests.cpp`
- `docs/HOME_CORE_INTEGRATION_V1.md`

Modified files:

- `components/control/core.cpp`
- `components/control/include/control/core.hpp`
- `components/control/mock_adapter.cpp`
- `components/user_app/user_app.cpp`
- `components/user_app/CMakeLists.txt`
- `main/main.cpp`
- `main/CMakeLists.txt`
- `tests/control/run_tests.py`
- `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md`
- `CHANGELOG.md`

No files deleted. Source/configuration checksum comparison verifies all other inspected workspace sources unchanged, including frozen authority, fonts, LVGL/carousel, Waveshare drivers, factory/brightness generated content, hardware configuration and package versions. Removing only the runtime include/start replacement from main.cpp recovers its exact pre-Phase-B bytes, including line endings. Existing 69 foundation and six profile tests/profiling sources are unchanged.

## Phase B owner hardware acceptance and closure — 2026-10-03

Owner reports **HOME/Core/Mock hardware validation: PASS** after the serial correction. This closes both the serial-input owner-retest gate and the overall HOME-CORE-MOCK hardware gate. **PHASE B CLOSED / PASS**. The prior normal boot, display, carousel, snapping, brightness and HOME button/state results remain recorded above; the earlier serial defect and correction remain part of the validation history.

This is physical owner acceptance of the synthetic integration. No new per-command evidence or numeric memory measurements were supplied; no real TUNER/protocol/networking validation is implied. The Phase A 34,515-byte internal-memory watch remains open for future significant loads. Documentation only at closure: firmware unchanged, no build/upload and no new milestone started.

## Phase C owner-evidence addendum and hardening - 2026-10-04

After closure, the owner supplied detailed Phase B behavior and two telemetry snapshots approximately 994 s apart. They are recorded exactly in [PHASE_C_RUNTIME_HARDENING.md](PHASE_C_RUNTIME_HARDENING.md#additional-owner-phase-b-evidence), including unchanged PSRAM free, failed_allocations=0, accounting_corrupt=0, owner_ok=1, queue_rejected=0 and stack minimum 16168. This new evidence does not rewrite the earlier closure's statement that numerical readings had not yet been supplied.

The actual current v command, source and tests all define volume 25. The reported physical 50 remains a nonblocking artifact/result discrepancy requiring the exact Phase C artifact retest; no value was changed to make either observation fit. Phase C preserves the serial interface/keys, UI/hardware and generic ownership boundaries. Channel requests now capture neutral session/revision and discard invalid/disabled buffered presses; pending rejection consumes tokens; delayed execution is not repeated after evidence allocation failure; facade admission acknowledgement honors Core rejection; unchanged field reduction avoids unnecessary allocation. Current semantics and all final measurements/gates superseding this historical Phase B preparation are in the Phase C report. No automatic upload or new physical PASS is claimed.

Phase C final verification: **140 PASS / 0 FAIL / 284374 assertions**; opt-in 45000-iteration soak **140 PASS / 0 FAIL / 2779624 assertions**. Final clean/build **SUCCESS**, flash **1751289 bytes / 20.9% (+1640 from fresh baseline)**, static RAM **19776 bytes / 6.0% (+128)**, **0 warnings/errors**. **HOST TESTED / CLEAN BUILD VERIFIED; HARDWARE VALIDATION PENDING OWNER RETEST**. Exact artifact hash and manual COM3 retest are in the Phase C report. No upload; no Phase D.

## Phase C owner hardware acceptance and closure - 2026-10-04

Owner reports **Phase C hardware validation: PASS; OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED** after approximately 960 seconds. Prescribed functional retests passed: **v=25**, capability/session behavior, disabled actions, no buffered replay, delayed completion and >3 s late completion. The current v=25 behavior is physically confirmed, closing the previous discrepancy/retest gate without changing firmware semantics. No crash, panic, reset, watchdog or UI freeze observed.

Exact final physical telemetry is recorded in the [Phase C acceptance report](PHASE_C_RUNTIME_HARDENING.md#phase-c-owner-hardware-acceptance-and-closure---2026-10-04): session 3, capability revision 45, dispatches 14; internal free/min/largest 167519/166491/94208; PSRAM free/min/largest 7714504/7714504/7602176; payload 85765; allocations/frees 30465/30114; failed allocations/accounting corruption 0/0; stack minimum 15888; owner_ok=1; queue_rejected=0. The m command was recognized and accepted with owner_ok=1.

This physical owner acceptance supersedes previous Phase C pending labels; Phase C is closed. The historical Phase A internal-memory watch remains for future significant loads. Documentation only: firmware unchanged, no build/upload, Phase D or next milestone.
