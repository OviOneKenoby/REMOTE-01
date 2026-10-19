# Phase C - HOME integration / runtime hardening

2026-10-03 to 2026-10-04. REMOTE//01 only. Entry gate: **Phase B OWNER ACCEPTED / CLOSED / PASS**, including physical USB Serial/JTAG RX acceptance after its correction. Current Phase C label: **OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED**. The preparation-stage pending gates below are historical and superseded by the owner acceptance section. No upload or Phase D work.

## Authority and exact baseline

Read the complete changelog, frozen CCM, architecture, Core implementation report, HOME integration report and REMOTE-local Native API candidate before editing. Inspected current Core/Mock/facade/channel/runtime/UI/profile code and existing suites. No applicable AGENTS.md was found in the workspace/ancestors.

`git status --short` returned `fatal: not a git repository (or any of the parent directories): .git`. No closure commit or Git diff exists in this supplied workspace; none was fabricated or initialized. Baseline is the actual post-Phase-B-closure working tree at `C:\Users\RYZEN\Documents\REMOTE01\Code`. A sorted SHA-256 manifest captured all 1,594 files outside `.pio`/`.git` before modifications: `%TEMP%\remote01-phase-c-baseline.json`, manifest SHA-256 `0ad0c7f7bea4798473d1eca675e4e67c82e800f1128f5e0553df5e4aa317b6f1`. Baseline clean/build changed none of these files. Original versions of all six changed firmware files were reconstructed and verified against their baseline hashes for the complete source diff review.

Unchanged pinned environment: PlatformIO `remote01`, espressif32 6.13.0, resolved `framework-espidf@3.50503.0` / ESP-IDF 5.5.3, Xtensa GCC 14.2.0+20251107; existing host compiler and strict warning checks. No package, toolchain, framework, library or configuration changes.

| Entry verification | Actual result |
| --- | --- |
| Foundation | 69 PASS / 0 FAIL / 2,425 assertions |
| Profiling | 6 PASS / 0 FAIL / 40 assertions |
| HOME including USB parser | 39 PASS / 0 FAIL / 3,541 assertions |
| Total | **114 PASS / 0 FAIL / 6,006 assertions** |
| `pio run -e remote01 -t clean -j 1` | SUCCESS, 2.25 s |
| `pio run -e remote01 -j 1` | SUCCESS, 488.27 s |
| Flash | **1,749,649 bytes / 20.9% of 8,388,608** |
| Static RAM | **19,648 bytes / 6.0% of 327,680** |
| Compiler warnings / errors | **0 / 0** |

Latest documented Phase B serial-fix build was 1,749,625 flash / 19,648 RAM. The fresh baseline is +24 / 0 despite unchanged workspace hashes. The older artifact/source provenance explaining the 24 bytes is not available; no explanation is guessed. Phase C deltas use the actual fresh baseline and additionally compare the historical measurement.

Baseline firmware.bin SHA-256: `eb195ab6854f5d14b35ae781dcb5217a26d37f72820f05d649ff3f138d044d0b`. Baseline ELF SHA-256: `3313593f94478976b88e35808682f2b69dfbc3a20e46fbd0f049cab995922bfb`.

## Additional owner Phase B evidence

These are supplied physical owner observations of the accepted Phase B firmware, not Phase C predictions or host measurements. Normal boot reported `synthetic_only=1 profile=0 owner_cpu=1 stack_bytes=24576`. USB reported `DEVELOPMENT_ONLY MOCK_ONLY input=USB_SERIAL_JTAG_DRIVER init=ESP_OK`.

| Bytes/counters | Initial snapshot | After approximately 994 s |
| --- | ---: | ---: |
| Internal free | 189663 | 181627 |
| Internal minimum | 186419 | 154483 |
| Internal largest block | 100352 | 65536 |
| PSRAM free | 7714632 | 7714632 |
| PSRAM minimum | 7714632 | 7714632 |
| PSRAM largest block | 7602176 | 7602176 |
| Tracked payload | 66696 | 73838 |
| Allocations | 1126 | 213550 |
| Frees | 898 | 213286 |
| Failed allocations | 0 | 0 |
| Accounting corruption | 0 | 0 |
| Owner stack minimum | 16168 | 16168 |
| owner_ok | 1 | 1 |
| queue_rejected | 0 | 0 |
| Dispatches | not supplied | 19 |

Owner PASS covers normal boot without automatic profiling; orientation; LEFT 0->1->2 / RIGHT 2->1->0; snapping; brightness/backlight; initial synthetic HOME; PREVIOUS/NEXT/PLAY-PAUSE through Core; observed tracks/playback and glyph changes; physical USB RX; external playback/volume and boundaries; UNKNOWN disabling toggle and valid observation restoring it; disconnect/recovery; NEXT withdrawal/restoration and disabled non-actionability; readiness disable/restore; stale-state rejection; delayed completion; no optimistic change; pending controls disabled; completion recovery; no unexpected queued replay in the observed interaction; continued responsiveness. No crash, panic, reset, watchdog or UI freeze was observed.

PSRAM free was exactly unchanged. Live allocation/free totals are not required to balance while this persistent runtime lives. Two snapshots neither prove an internal leak nor establish unlimited future headroom. **Phase A's 34,515-byte minimum internal free heap watch remains open**, unchanged. Significant future networking/adapters/TLS/discovery/catalog/artwork loads require renewed physical profiling.

## Demonstrated findings and local corrections

| ID | Reproduction and root cause | Exact correction |
| --- | --- | --- |
| C01 | Enqueue NEXT, disconnect/reconnect before pop; old implementation accepts it and dispatches under the new session. Intent contained only token/action and was first captured against Core at processing time. Withdrawal/restore had the analogous revision problem. | Intent copies neutral session/capability revision from the locked latest snapshot. Facade checks that scope before new capture; no rebasing. Missing/stale scope rejects. Exact retained duplicates compare the whole original Intent and reach Core dedup without another dispatch. |
| C02 | Queue delayed TOGGLE/NEXT plus another press; complete before the second pop; old implementation dispatches the buffered press. Pending refusals also bypassed the facade watermark, allowing the same refused token to revive later. | Channel rejects currently disabled/uninitialized actions. Publication compacts at most eight entries in FIFO order, discarding changed scopes or newly disabled actions permanently. Pending rejection passes through the generic facade acceptance gate and consumes new tokens. No queued replay on restore/completion. |
| C03 | Delay NEXT; prohibit completion evidence allocation; `f` executes but reporting fails; restore budget and retry `f`. Original Device advances from Mock Two to Mock Three for one dispatched intent. | Reset a private execution marker at delayed dispatch; apply its effect once. Retrying failed evidence delivery cannot execute again. Actual acquired state and result evidence remain separate; no optimistic UI state. Regression fails against hash-verified original Device and passes after correction. |
| C04 | Sweep allocation headroom until Core consumes the pair but cannot allocate its record. Facade reports accepted despite the returned terminal REJECTED result. It checked only Reply.error, while admission refusals are stored in CommandResult. | Both new submission and retained duplicate paths inspect the returned lifecycle: REJECTED/EXPIRED_BEFORE_SEND are not acknowledged as admitted. This does not treat acknowledgement as target completion or alter Core results. |
| C05 | 1,000 identical-clock ticks with unchanged HOME fields produce 8,000 allocation/free pairs. Each field reduction allocates an eligible-pointer List and deep-copies identical selected provenance. | Replace only this transient list with a fixed 16-pointer array/span and skip selected assignment only when the complete StateObservation is exactly equal. Sorting, freshness, conflict participation, age and selection rules remain unchanged. |

The two original queue traces printed accepted=1; final regression cases require no dispatch. Original completion and admission-acknowledgement regressions failed as expected during reproduction. An intermediate allocation test failed after eliminating only the pointer list, revealing provenance-copy churn; the equality guard corrected it. No unexplained intermittent failure or flaky result remains.

## Architecture, queue and transition audit

HOME still includes only neutral api.hpp. It enqueues and reads owned copies, knows no Mock/Store/registry, keeps no authoritative playback/volume, and performs no optimistic mutation. Core/Session/facade/Mock reads and mutations stay on the existing serialized CPU 1 owner. Dispatch-only publication remains confined to dispatch; independent acquisition uses owner-side observe/report. No non-LVGL task touches LVGL. UI timer deletion/reference clearing is unchanged.

Queue remains eight fixed slots, newest-full rejection, zero-wait request, one pop per owner turn; no replacement queue or retries. Compaction keeps survivor ordering even with a wrapped head. `queue_rejected` now includes disabled ingress and discarded queued scope/action as well as overflow. Owner-side scope/Core refusals also remain observable through accepted=0 logs. Repeated valid NEXT/PREVIOUS/toggles still use normal owner capture and frozen Core validation. Volume/title-only refresh does not discard otherwise valid presses. Context invalidation cannot revive already discarded work. There is no owning allocation in channel request/pop/publication.

State/session/capability checks cover disconnect/reconnect before publication, withdrawal/restoration, readiness changes, UNKNOWN/recovery, exact dedup and immutable scope. Old-session or old-media observations cannot enter current presentation. Equal-sequence duplicates do not refresh age; at age equality fields become stale. Existing conflict latch/resync tests remain intact, supplemented by all 16 routes at the selection bound.

Delayed/late cases cover capability withdrawal before completion, deadline then late completion, preserved first closure/deadline flag, duplicate completion, and new media context before historical evidence. Results alone do not manufacture title/playback. A changed track invalidates old media before acquisition; old context cannot overwrite it. A pending synthetic execution spanning reconnect remains conservatively blocked: `f` cannot execute in the replacement session; reset remains the documented harness recovery. No new replay/fence semantics were invented.

## Allocation understanding and measured optimization

The allocator observer measures Core owning payload, not malloc overhead, USB buffers, LVGL or task stacks. The benchmark uses identical initialized Session, inputs and virtual time; original Core bytes were SHA-256 verified. New scoped facade/device paths are identical on both sides of the isolated Core comparison. Every benchmark checks the same presentation/capability outcomes and no allocation failures.

| Deterministic work | Before allocations / frees | After allocations / frees | Live payload delta, before / after |
| --- | ---: | ---: | ---: |
| 1,000 snapshot copies/publications | 0 / 0 | 0 / 0 | 0 / 0 |
| 1,000 unchanged ticks | 8000 / 8000 | **0 / 0** | 0 / 0 |
| 100 fresh four-field acquisitions | 6500 / 6496 | **2900 / 2896** | +127 / +127 |
| 100 NEXT capability republications | 21100 / 21100 | **20300 / 20300** | 0 / 0 |
| 600 owner ticks at 50 ms plus 30 acquisitions | 6750 / 6750 | **870 / 870** | 0 / 0 |

The +127 acquisition payload change is identical in both implementations: initial lazy capacities settle and observation IDs gain decimal digits. It is not an unreclaimed per-iteration object. Session destruction returns the complete test payload to baseline.

The 30-second model reduces allocation events by 87.1%. This explains a substantial ordinary-runtime hot path; it does not assert an exact physical attribution of all 213,550 owner-observed allocations. Snapshot copying itself allocates nothing; unchanged Core reduction caused churn. Fresh acquisitions still allocate sample IDs, field/sequence/provenance/text records and selected provenance. Text copies share immutable bytes; HOME copies bounded UTF-8 without owning strings. Capability construction/copy/update allocates schemas/evidence but is infrequent and deliberately stressed by development hooks. Command capture/records/effects/result evidence also allocate within existing bounds and retention. No broad allocator or abstraction rewrite was justified/performed. The zero-allocation tick result applies to normal single-source HOME, not every conflicting/RESYNC Core state; conflict-set/participant copies remain bounded owning allocations. No ESP32 memory improvement is claimed until owner retest.

## Stack/task safety and development classification

Target compiler frame reports use the unchanged actual Xtensa compile commands plus -fstack-usage, with outputs outside the build tree. Reports are individual frames, not whole-chain bounds or hardware stack PASS.

| Target frame bytes | Phase B | Phase C |
| --- | ---: | ---: |
| Core::reduce | 1040 | 1088 |
| Owner task | 304 | 336 |
| Facade::snapshot | 48 | 48 |
| Facade::handle | 1104 | 1088 |
| Device::publish | 1456 | 1456 |
| Device::command | 1536 | 1536 |

Target ABI: Intent 16->32 bytes; Snapshot unchanged 88; Channel 248->376; PSRAM Session 5616->5640. The effect marker fits existing padding; no separate heap allocation. Selection scratch holds 16 pointers (64 target bytes), with bounded sorting depth. No new recursion, task, large owning stack container or stack-size reduction. Existing larger schema/observation locals remain a physical headroom consideration; owner stack stays 24,576 and prior observed minimum remains 16,168 bytes. No LVGL blocking read was added. USB input still reads at most eight raw bytes with timeout 0; diagnostic TX/ESP_LOG can take time under output backpressure, so item budgets are not a hard wall-clock latency guarantee.

| Code/evidence | Classification and decision |
| --- | --- |
| Neutral channel, facade, copied snapshot, serialized Core owner | Required current runtime |
| Device/Mock/process-local Journal and serial parser/acks | DEVELOPMENT ONLY / MOCK ONLY synthetic harness; no production transport guarantees |
| Core allocation observer and on-demand m telemetry | Useful bounded development instrumentation; retained |
| Phase A finite profile/runtime | Useful opt-in compile-time diagnostics; retained; main selects it only with explicit REMOTE01_PHASE_A_PROFILE=1 |
| Default normal boot | profile=0; no automatic profile stress/retention waits |
| Host fixtures, budget injection, benchmark and finite soak | Test-only; never invoked in firmware |
| Prior Phase A/B logs/failure/pending entries | Historical documentation; preserved |

No obsolete branch could be removed with zero diagnostic loss, so profiling sources were not deleted. Relevant source checks found no new TODO/FIXME, unsafe cast, borrowed-pointer escape, ignored queue retry, string boundary error, or ownership violation. Existing UI's ignored request boolean remains safe because refusal is final/measured, never retried.

## Development input mapping and v discrepancy

USB Serial/JTAG driver installation, bounded RX/TX, console configuration, zero-wait eight-byte reads and existing acknowledgements are unchanged. All 256 byte values are classified in tests; CR/LF/nonprintable and high bytes are silent; printable unknowns log rejection; wrong-owner input performs no work.

| Key | Actual current source contract |
| --- | --- |
| p / a / u | External PLAYING / PAUSED / UNKNOWN; UNKNOWN disables toggle |
| 0 / 1 / v | External volume **0 / 100 / 25** |
| d / r | Unreachable/invalidate; new session, readiness of surviving capabilities, reacquire |
| - / + | Withdraw / republish supported NEXT |
| o / c | NEXT CONTEXT_UNAVAILABLE / READY, revision advances |
| s | Inject older ordinal volume 99; accepted=1 denotes expected rejection, current volume unchanged |
| l / n | Enable delayed / normal confirmed mode; n does not finish an existing pending operation |
| f | Finish original pending synthetic execution and report its completion; failed evidence retry cannot repeat execution |
| m | On-demand telemetry; no state mutation |

**v investigation:** Device::command explicitly assigns 25; existing external_volume/coalesced/stale tests and new parser/transition tests all assert 25; rational 1/4 is presented as 25. No current source/test/documentation contract defines 50. Therefore no value or semantics were changed to match the owner's observed 50. The reported physical 50 remains unexplained by the current source and is a **NON-BLOCKING INVESTIGATION / HARDWARE VALIDATION REQUIRED** artifact/result discrepancy. Retest the exact final artifact below and capture v plus the visible number/bar. Earlier wording is retained as history; do not claim the physical discrepancy has been resolved by a host test.

## Final tests and finite stress

Commands:

```text
python tests/control/run_tests.py
python tests/control/run_tests.py --soak
python tests/control/run_tests.py --measure
```

The default and opt-in soak commands each rerun every pre-existing suite, USB input tests and all new tests. --measure runs the pre-existing suites and the isolated allocation benchmark, not the complete hardening suite. No new analysis framework or toolchain was introduced. No sanitizer PASS is claimed; historical unavailable UBSan remains documented.

| Final suite | PASS | FAIL | Assertions |
| --- | ---: | ---: | ---: |
| Foundation | 69 | 0 | 2425 |
| Profiling | 6 | 0 | 40 |
| HOME / USB parser | 39 | 0 | 3541 |
| Phase C default | 26 | 0 | 278368 |
| **Default total** | **140** | **0** | **284374** |
| Phase C opt-in soak | 26 | 0 | 2773618 |
| **Soak run total including existing suites** | **140** | **0** | **2779624** |

26 new cases cover empty/single/full queue, overflow recovery, reconnect and revisions before publication, withdrawal across reconnect, delayed burst discard and consumed refused tokens, ordinary/wrapped FIFO compaction, readiness recovery, token/generation boundaries, scoped duplicates, UTF-8 truncation at 2/3/4-byte boundaries and 4096-byte input, snapshot lifetime/coalescing, delayed withdrawal/completion, allocation-failed completion without reexecution, Core admission refusal acknowledgement, new-context late evidence, old-session pending/history, duplicate age, all 16 selection routes/conflict, every serial byte, allocation-free refresh, benchmark and three stress patterns plus source/task boundaries.

| Reproducible pattern | Default | Opt-in soak | Final tracked payload, default / soak (host bytes) |
| --- | ---: | ---: | ---: |
| Fresh acquisition, stable media/state | 2000 iterations | 20000 | 98502 / 98506 |
| Alternating NEXT/PREVIOUS or eight toggles, overflow, complete batch purge | 250 batches / 2000 commands | 2500 / 20000 | 102477 / 102484 |
| Fixed four-case session/capability/readiness/delayed-late cycle | 500 cycles / 625 dispatches | 5000 / 6250 | 100666 / 100672 |
| **Counted stress iterations** | **4500** | **45000** | — |

No randomness or indefinite loop. All finite nominal sequences complete without model-level stall, duplicate dispatch, unexpected queued replay, stale acceptance or failed allocation. RTOS deadlock/starvation and real task scheduling still require physical validation. Explicit fault-injection cases deliberately refuse allocations. Acquisitions keep four samples; command batches purge all records/effects; post-purge checkpoints remain within their warmed bounded payload, allowing only bounded ID digit growth. Every test returns owning payload to its pre-test baseline. Persistent envelope/mock/capacity state remains live during measurement; allocations/frees do not have to balance before teardown. These host runs do not prove ESP32 scheduling, fragmentation, USB or UI behavior.

## Final clean build and source preservation

Final reviewed source, commands invoked through the existing absolute PlatformIO executable:

```text
pio run -e remote01 -t clean -j 1
pio run -e remote01 -j 1
```

Clean **SUCCESS, 1.95 s**; clean firmware build **SUCCESS, 479.16 s**. **0 compiler warnings / 0 errors**. Final resolved SDK remains ESP-IDF 5.5.3. No upload.

| Link measurement | Fresh Phase B baseline | Final Phase C | Exact delta | Delta from documented Phase B serial fix |
| --- | ---: | ---: | ---: | ---: |
| Flash | 1749649 / 20.9% | **1751289 / 20.9%** | **+1640 bytes** | +1664 bytes |
| Static RAM | 19648 / 6.0% | **19776 / 6.0%** | **+128 bytes** | +128 bytes |

RAM percentages use 327680 bytes; flash percentages use 8388608. The +128 static bytes match the enlarged fixed Channel. PSRAM wrapper grows 24 target bytes; dynamic allocation/stack/fragmentation effects are not static RAM or hardware measurements.

Final firmware.bin SHA-256: `a714ea95c6b460a2fd15e7510a45c222cef37a46d68c6f98295589ed84cf0d2b`. Final ELF SHA-256: `787ee69ecb245c30008eb270b4d6976e745bec79d28c377dc8b8f5b039d7d2e3`.

Full manifest comparison confirms exactly eleven modified existing files plus two added files listed below, no deletions, and all other baseline bytes unchanged. Tests/soak and final clean build use the final firmware/test source; only documentation completion followed the build.

Final source diff review: no HOME Mock knowledge or authoritative UI state; no ownership weakening, pointer escape/tearing, less deterministic queue, weakened stale/session/capability gates, optimistic delayed result, allocation regression, uncontrolled stack growth, default profiling, Mock-specific HOME policy, accepted hardware edit, frozen semantic edit or TUNER-specific assumption. Demonstrated regressions above were corrected before the final clean build.

Protected whole-file SHA-256 values, unchanged:

- Frozen CCM: `7a46f9fffe706476de458aac078d0af1c94844df339248d3f9f4b5cf033011d7`.
- Architecture: `c0c67c8960539b0dd0f06f25a7d15f1150833bfeac835367241c96429369574f`.
- REMOTE-local Native API candidate: `830648708076820d90187c4a081bbe7c483e82983c43289c05f13667b6981a1b`.
- HOME/UI source: `e431b3589faa6982f4067b2f74bd1de74f2051d383ddca06492118a723da4e0e`.
- PlatformIO configuration: `e455b728cecf0d72d391e7bb841e8795ce899c7af85a0ed64e15db67d0c9f175`.

Main/runtime scheduling source, Core schemas/capacities/allocator, Mock foundation, profile sources, factory/brightness pages, LVGL/carousel/spacer, GPIOs, LCD/touch/rotation/mapping/drivers, backlight, fonts/palette/geometry, all configuration/package locks and historical reviews remain unchanged. TUNER/yoRadio repository was not accessed; no real adapter, discovery, networking, transport, IR, cloud or protocol shim was implemented.

## Exact file manifest

Six modified firmware files:

- `components/control/core.cpp` - local allocation optimization only.
- `components/control_home/include/control_home/api.hpp` - scoped fixed FIFO and invalidation.
- `components/control_home/include/control_home/facade.hpp` - immutable scoped intent and generic acceptance gate.
- `components/control_home/facade.cpp` - scope/token checks and truthful admission acknowledgement.
- `components/control_home/include/control_home/development.hpp` - pending rejection routing and execution marker.
- `components/control_home/development.cpp` - once-only delayed execution before evidence retry.

Modified tests/docs: `tests/control/home_tests.cpp` (scope-aware existing fixtures; all 39 tests retained), `tests/control/run_tests.py` (new suite/finite opt-in flags), `docs/HOME_CORE_INTEGRATION_V1.md`, `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md`, `CHANGELOG.md`.

Added: `tests/control/hardening_tests.cpp`, this report. No deletions or unrelated edits. Verification logs and baseline/original-source/stack artifacts are in `%TEMP%\remote01-phase-c-*`, outside firmware inputs.

## Historical pre-acceptance gates and owner retest

**OWNER DECISION REQUIRED:** none for this implementation. The v=50 observation needs artifact/result evidence, not an invented UX choice. **FROZEN CONTRACT BLOCKER:** none found in this slice; full CCM conformance/retry/recipe/real persistence remain outside it. **HARDWARE VALIDATION REQUIRED:** all Phase C firmware corrections and real allocation/stack/headroom behavior; Phase C is not owner hardware accepted. **FUTURE ADAPTER WORK:** real persistence/reboot recovery, bounded asynchronous adapter event ingress and significant-load profiling. Synthetic Journal and correlation never authorize a real transport. The existing conservative cross-session pending limitation remains documented.

Owner only, from this repository in interactive PowerShell:

```powershell
$remotePio = 'C:\Users\RYZEN\.platformio\penv\Scripts\pio.exe'
Get-FileHash .pio/build/remote01/firmware.bin -Algorithm SHA256
& $remotePio run -e remote01 -t upload --upload-port COM3
& $remotePio device monitor --port COM3 --baud 115200 --echo --filter log2file
```

1. Confirm final SUCCESS and artifact hash in the build section; manually upload. Reset with monitor attached. Expect profile=0, CPU 1, stack 24576, PSRAM wrapper 5640, USB driver init=ESP_OK. Capture initialized telemetry. No automatic Phase A profile.
2. Recheck orientation, LEFT/RIGHT 0/1/2 order, snapping/end stops, brightness/backlight and unchanged HOME layout. Recheck single PREVIOUS/NEXT/toggle, tracks, accepted playback and glyphs.
3. Type a/p, u/p, 0/1/v/s. CR/LF stay silent; ? logs rejection; m logs telemetry. **v must be 25 in this source**; capture the number/bar and key acknowledgement if it still shows 50. Preserve the firmware hash and full log.
4. Repeat -/+, o/c, d/r; disabled actions cannot execute, restored fresh actions work. Withdraw NEXT, then d/r: NEXT stays withdrawn until +. Rapidly press around disconnect/reconnect or revision changes; no old press may execute after restoration. Discarded presses can legitimately increment queue_rejected.
5. Type l, rapidly press playback controls, confirm pending/no optimistic change. Type f: exactly one synthetic effect for the accepted pending intent, controls recover and no buffered replay. Type n. Repeat completion after a >3 s delay. Do not expect pending old-session work to execute after reconnect; reset that documented conservative case.
6. Repeat mixed interaction for at least the prior approximately 994 s and finish with m. Capture initialized/final internal and PSRAM free/minimum/largest, payload/counters/failures/corruption, stack minimum, owner_ok and rejection/dispatch evidence. No crash/panic/reset/watchdog/freeze; responsiveness, failure-free accounting and usable physical headroom require owner review. Host allocation savings are not a physical acceptance claim. The deliberately constrained-budget failures are host-only and cannot be invoked via serial.

Stop here. Recommended next REMOTE milestone: owner Phase C hardware acceptance, then a separately authorized generic bounded adapter-ingress/persistence design review with renewed resource budgeting. No Phase D, real adapter or TUNER work was begun.

## Phase C owner hardware acceptance and closure - 2026-10-04

Owner reports **Phase C hardware validation: PASS; OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED** after approximately 960 seconds. This is physical owner evidence, not host prediction. The prescribed functional retests passed, including **v=25**, capability/session behavior, disabled-action behavior, no buffered replay, delayed completion and >3 s late completion. The earlier v=50 discrepancy is closed by confirmation of the current source's v=25 behavior; the earlier observation's cause was not established and no value was changed.

Final owner-supplied telemetry (bytes where applicable):

```text
UC_MOCK:
key=m recognized=1 accepted=1 owner_ok=1
session=3 capability_revision=45 dispatches=14

UC_RUNTIME:
INT free=167519 min=166491 largest=94208
PSRAM free=7714504 min=7714504 largest=7602176
payload=85765 allocations=30465 frees=30114
failed_allocations=0 accounting_corrupt=0
stack_min_bytes=15888 owner_ok=1 queue_rejected=0
```

No crash, panic, reset, watchdog or UI freeze was observed. Owner acceptance closes the Phase C physical validation gates and supersedes the historical pending-retest/investigation labels and retest instructions above. Live allocation/free totals need not balance while the runtime remains live; these final readings alone do not establish an exact before/after physical allocation reduction. Phase A's historical 34,515-byte internal-memory watch remains for future significant loads; acceptance does not validate networking or real adapters.

Closure changes documentation only. Firmware unchanged; no build, upload, Phase D or next milestone. Stop at accepted Phase C.
