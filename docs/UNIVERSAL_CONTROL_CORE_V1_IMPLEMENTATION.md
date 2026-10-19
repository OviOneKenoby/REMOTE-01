# Universal Control Core v1 foundation

Current owner milestone status (2026-10-04): **Phase D OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED**. Owner acceptance supersedes the Phase D pending-retest gate; [the Phase D report](PHASE_D_GENERIC_ADAPTER_BOUNDARY.md#current-status-owner-hardware-acceptance-and-closure---2026-10-04) records the verdict and evidence limits. Phase C remains accepted and closed. Earlier phase preparation/status sections below are historical. Documentation only; firmware unchanged, no build/upload or next milestone.

2026-10-03. Authority: [Common Control Model v1.0.0](COMMON_CONTROL_MODEL_V1.md), OWNER APPROVED / FROZEN. This implementation is a bounded, local foundation slice, not full CCM conformance or a real-device adapter. No frozen document or historical review was changed. No frozen-model contradiction was discovered.

The sections preceding Phase A record the completed foundation milestone and its detached-runtime build. The Phase A and Phase B preparation sections are historical; the owner acceptance sections below supersede their pending gates. Phase B HOME/Core/Mock hardware validation is **OWNER ACCEPTED / PASS; CLOSED / PASS**.

Subsequent [Phase C runtime hardening](PHASE_C_RUNTIME_HARDENING.md) retains that owner acceptance and records the additional physical evidence, demonstrated local defects, regression/soak/allocation checks and new firmware retest gate. Frozen CCM semantics and the accepted HOME/display/touch/carousel/hardware design remain unchanged.

## Repository inspection and boundary

The application uses ESP-IDF components, C++23, FreeRTOS and a mutex-owned LVGL task. Exceptions and RTTI are disabled. `main/main.cpp` initializes the existing Waveshare hardware and UI; `components/user_app` owns HOME/carousel content. There was no project-owned host test framework. The portable `components/control` component fits the existing layout and requires neither LVGL nor ESP-IDF APIs. Existing application initialization, UI, drivers, GPIOs, rotation, touch mapping, carousel and backlight remain byte-identical.

The verified environment remains `remote01`, espressif32 6.13.0, ESP-IDF 5.5.3 and Xtensa GCC 14.2.0+20251107. Existing PSRAM-through-malloc configuration remains unchanged. Host tests use the installed MSYS2 UCRT64 GCC 16.2.0; no packages were installed or upgraded.

| New file | Responsibility |
| --- | --- |
| `components/control/include/control/storage.hpp` | Bounded allocator, immutable shared UTF-8 text, opaque IDs, lazy bounded lists, optional and tagged absent values |
| `components/control/include/control/model.hpp` | CCM records, enums, schemas and central capacities |
| `components/control/include/control/core.hpp` | Generic adapter/sink/journal contracts, registries, decision state, Core API |
| `components/control/core.cpp` | State ingestion/reduction, capture/admission, scheduling, lifecycle, deadlines, dedup and barriers |
| `components/control/include/control/mock_adapter.hpp` | Synthetic adapter and process-local Mock Journal |
| `components/control/mock_adapter.cpp` | Synthetic descriptors, observations and configurable evidence |
| `components/control/memory_metrics.cpp` | Target-ABI sizeof table retained in the ELF |
| `components/control/CMakeLists.txt` | ESP-IDF component compilation and detached code retention |
| `tests/control/core_tests.cpp` | Executable deterministic tests against Core and Mock |
| `tests/control/run_tests.py` | Small compiler/test runner with temporary external build directory |
| `docs/UNIVERSAL_CONTROL_CORE_V1_IMPLEMENTATION.md` | This implementation/validation record |

`CHANGELOG.md` is the only pre-existing file edited. No application hook or runtime instance was added. Component-only linker anchors retain implementation code for an honest flash-cost measurement; anchors are not called at runtime.

## Interfaces and ownership

Tests call Core registration, capability, observation, capture/submission and mutation APIs. Core dispatches through the abstract `Adapter`; Mock publishes through `AdapterSink`. Core contains no Mock-specific calls, device names, protocol encodings or networking concepts. Dispatch includes the original immutable request, captured attempt and optional common-to-native numeric mapping result. IDs are scoped opaque values; labels, addresses and pointers are not identities.

One serialized owner must perform **all** Core operations, reads and registry access. Dispatch may publish to a bounded event queue, but cannot call Core mutation reentrantly. Later asynchronous adapters must marshal observations/evidence to that owner and call `observe`/`report`; they cannot retain a dispatch reference after the call returns. Registered adapters, Store and Journal must outlive Core. Result pointers are borrowed until cache purge; capacity-refusal pointers use scratch storage and expire on the next refusal. No task, cross-task queue or thread safety is claimed here. Registry accessors expose internal records for the local foundation; direct caller edits are not a supported mutation mechanism.

Journal commit precedes executable handover; failure blocks scheduling. Mock Journal preserves caller-owned Store **in process only**, allowing synthetic restart tests. It is not durable flash/NVS storage and must never authorize a real adapter after a physical reboot. Unresolved effects survive full-result purge and synthetic epoch replacement. Unknown recovered state requires blocking/fencing, not replay. Real persistence/recovery remains deferred.

## Implemented slice

- Device/target/binding/capability registries, independent connectivity and power, positive generation/revision captures, and capability invalidation on resource-membership/session changes.
- Named CCM request/result/attempt/observation/error/descriptor/context/catalog/resource records retain their normative fields. Device maps are represented by scoped references into Store; tagged `Value<T>` preserves UNKNOWN, UNSUPPORTED, NOT_APPLICABLE and UNAVAILABLE. CANCELLED is an error, not a new lifecycle; unsent expiry uses EXPIRED_BEFORE_SEND.
- Verified-sequence observations enforce identity, mapping/session, freshness policy, acquisition age and media/catalog references. Duplicate sequence does not rejuvenate state. Domain reset invalidates the entire binding and readiness. Commands never manufacture observed state.
- Deterministic authoritative/priority/ID reduction, sorted conflict output, persistent conflict participants and verified post-conflict RESYNC clearing. Expired contributors do not clear the latch.
- One-route, one-attempt commands: schema/domain checks, captured scope/revisions, support/auth/context gates, field preconditions, immutable envelopes, monotonic deadlines, cancellation and deterministic terminal folding. Known partial feedback-NONE failure is distinct from genuine extent/correlation uncertainty. Late valid evidence updates history without rescheduling the old intent; contradictory or duplicate evidence is ignored.
- Core-generated `intent:<lifetime counter>` ID/ticket bijection, lifetime non-reuse counter and current-epoch watermark provide compact pairing tombstones. Capture proposes the next pair; submission commits/consumes it, including refusals. Submit each proposal before capturing the next independent intent. Map argument order is irrelevant to dedup equality; ordered lists remain ordered. Purge occurs at `max(first_terminal_time + 60000, deadline)`, with checked overflow and deterministic time/serial ordering. Active/unexpired results are not evicted.
- Qualified TARGET/RESOURCE keys sorted before all-or-none dispatch acquisition. A valid no-past/no-future proof releases only covered historical keys before folding. Future-only fencing does not assert past nonexecution. Delivery history and progress lower bounds are preserved. Mandatory effect identity, unit extent and proof state outlive cache records. Independent already-admitted work may proceed through its normal gate after proof release.
- Local catalog full/partial refreshes preserve omitted partial items/order, append new items in ID order and advance generation once for membership/order change. Stale SelectionRefs reject. No binding-private external token rebind or real enumeration protocol is implemented.
- Two explicit numeric profiles: exact normalized hundredth grid, and two-endpoint TABLE nearest-common-row quantization with ties upward. Exact `17/25` passes; `1/3` fails on the exact grid. TABLE `1/2` maps to the upper external endpoint, `1/3` to the lower. Other profiles are refused, not approximated.

Mock identity is `adapter.mock`, with clearly synthetic device/target labels. Its installed operations are `power.set`, `power.toggle`, `volume.set`, `volume.step`, `volume.mute`, `playback.play`, `playback.pause`, `playback.stop`, `playback.previous`, `playback.next`; fields are power, volume.level, volume.mute and playback.state. `source.select` is additionally published by catalog tests only. Mock stop declares a synthetic semantic variant. No mock device is presented as TUNER//01.

Modes: confirmed completion, open-loop completion, explicit target refusal, unavailable/not-sent failure, delayed/no result, known halted partial, ongoing partial, ambiguous extent and guaranteed-no-execution proof. Observation helpers exercise stale/duplicate/media/session/revision changes; tests simulate restart through retained Store/new epoch. Closed result metadata includes nested Attempt, Evidence, Error, Progress, Value, time and catalog schemas; symbolically empty placeholders were removed during review. This synthetic in-process contract is the only evidence claimed.

## Storage and costs

| Capacity | Limit |
| --- | ---: |
| Devices / targets / bindings | 4 / 8 / 16 |
| Descriptors / fields per target | 32 / 16 |
| Shared resources / keys per ordinary intent | 16 / 17 |
| Full dedup records / unsent queue | 1024 / 64 |
| Independent effects / ingress events | 1152 / 64 |
| Catalogs / items per catalog | 40 / 1024 |
| Schema nodes / edges or enum members per node | 128 / 16 |
| Arguments / field preconditions | 16 / 16 |
| Evidence per attempt or effect | 32 |
| UTF-8 text / opaque ID bytes | 4096 / 128 |
| Owning payload allocation budget | 4,194,304 bytes |

Lists allocate lazily and grow geometrically within their bound. Text copies share immutable refcounted bytes. No owning `std::string`, unbounded owning STL container, exception or RTTI dependence is introduced. Heap payload accounting excludes the aligned allocation header and malloc bookkeeping/fragmentation. Exhaustion refuses admission or blocks handover; allocation-failure tests include atomic rollback of a partially allocated attempt reservation. Bounds are this foundation's profile, not enlarged/narrowed definitions of frozen types. Configured count maxima are not a promise that every maximum-sized record fits simultaneously within the byte budget.

Actual target ABI values come from the Xtensa-compiled `remote01_control_sizes` symbol, in the listed file order:

| Record | ESP32 bytes | Host bytes |
| --- | ---: | ---: |
| Text / Id | 16 / 16 | 32 / 32 |
| CommandRequest / CommandResult | 360 / 376 | 568 / 600 |
| StateObservation / CapabilityDescriptor | 552 / 616 | 832 / 944 |
| Record / Effect | 896 / 144 | 1448 / 240 |
| Store / Core | 4304 / 416 | 8560 / 664 |

The executed host suite peaks at **3,027,218 payload bytes**, primarily the 1024-record capacity case, and checks allocator balance after every test. This is measured host allocation, not ESP32 runtime heap or a physical memory PASS. The firmware has no Store/Core instance; future integration must budget its stack, heap, event queue, simultaneous text/schema payloads and PSRAM use alongside LVGL. Existing PSRAM malloc support does not prove sufficient physical runtime headroom.

Build commands, using the unchanged verified environment:

```text
pio run -e remote01 -t clean -j 1
pio run -e remote01 -j 1
```

The installed executable was invoked by its absolute path, `C:\Users\RYZEN\.platformio\penv\Scripts\pio.exe`. Actual clean build: **SUCCESS**, 482.39 seconds. Final correction rebuild with `pio run -e remote01 -j 1`: **SUCCESS**, 28.26 seconds. The initial measurement build discarded the detached code despite whole-archive; explicit anchors and component-only section grouping corrected that measurement, followed by the clean build and final correction rebuild. ELF symbol inspection confirms both implementation anchors, the target-size table and allocator counters are retained.

| PlatformIO measurement | Before | Final | Delta |
| --- | ---: | ---: | ---: |
| Flash bytes | 1,612,541 | 1,723,541 | +111,000 |
| Static RAM bytes | 19,216 | 19,288 | +72 |

Both clean/final build logs contain **0 compiler warnings** and no compiler errors. The 12 directly attributable mutable global bytes are allocator used/peak/budget counters; the net link RAM delta is 72 bytes, including additional linked support/alignment. Neither figure includes dynamic Core instances, runtime heap or stack. No static Store/Core instance exists. Target sizeof was read from the retained Xtensa ELF/object, not inferred from host measurements. No upload was performed.

## Executed tests and repaired-finding coverage

`python tests/control/run_tests.py` compiles actual Core/Mock with C++23, `-Wall -Wextra -Werror`, exceptions/RTTI disabled, then executes a small deterministic assertion harness. No documentation vector is counted as an executable test by merely copying its text. Full test names and assertions are in `core_tests.cpp`; the suite covers all applicable minimum requested categories, including catalog references, 1024 dedup slots and 64 queued requests.

Final executed result: **69 PASS / 0 FAIL; 2,425 assertions**. Each test verifies return-to-baseline owning allocation count. The allocation-failure sweep exercises 32 constrained budgets. Final source-reset recovery assertions additionally prove old-session sequence metadata cannot trigger perpetual session resets. A previous weaker-delivery regression failed during development, was corrected, and passed in this final run.

An optional `--sanitize` run was attempted. The installed host compiler cannot link `-lubsan`; that run did not execute and is **not** a sanitizer PASS. No toolchain upgrade was made. Normal strict-warning tests passed.

| Finding | Executed implementation regression; limits |
| --- | --- |
| FR01 | Ongoing PARTIALLY_SENT/DISPATCHED and admitted expiry; multi-attempt retry deferred |
| FR02 | Mandatory late completion/proof, duplicate proof, contradiction quarantine, no old-parent replay |
| FR03 | Deadline before invalidation, retained terminal flag, single revision per deadline event |
| FR04 | Exact 59999/60000 purge boundary, full cache/capacity, epoch retirement, effect survival/reclamation |
| FR05 | Sorted qualified keys, all-or-none reservation, conflicting queue, covered/uncovered resources, allocation rollback |
| FR06 | Recipe fields represented; recipe scheduling rejected as outside slice, no recipe execution coverage claimed |
| FR07 | Count-prefix PARTIAL_EXECUTION preserves cause; recipe parent folding deferred |
| FR08 | UNKNOWN/UNAVAILABLE/UNSUPPORTED reduction, unknown contributor, no command-created power |
| FR09 | Participant expiry does not clear conflict; agreeing verified RESYNC clears; sequence conflict/reset |
| FR10 | Exact rational grid and two-row TABLE tie/output exercised; general numeric profiles deferred |
| FR11 | Partial catalog membership/order/generation and stale reference exercised; private token continuity deferred |
| FR12 | Version, envelope/map equality, old epoch, stale capture, domain, cache/queue/OOM refusals |
| FR13 | Deterministic selection/conflict presentation exercised by conflict tests; diagnostic history not conformance output |
| FR14 | Closed typed schemas, graph-cycle rejection, absence distinction and actual descriptor-domain enforcement |
| FR15 | Partial/whole/future-only proof, retained AMBIGUOUS delivery, covered-key release, independent queued intent, post-purge unit extent |
| FR16 | Full open-loop NONE acceptance with STRICT barrier; known partial NONE failure; declared-correlation timeout |

Adversarial review corrected incomplete nested schemas, resource revision invalidation, descriptor-invalidated state, sequence-reset scope, media scope, decreasing progress/delivery evidence, incomplete completion proofs, independent retained unit extent, double deadline revision and OOM reservation rollback. Regressions execute the corrected code. No full 96-vector or full CCM conformance certification is claimed.

## Deliberately deferred and validation level

Recipe execution, automatic retry/equivalent fallback, multi-route execution, generic nested/union/list operation payloads, remaining operations, general numeric mapping/inverse conversion, verified serial acquisition, owner reassociation/deletion, private token rebinding, real asset service, durable NVS serialization/recovery, real asynchronous task marshaling and production UI snapshot facade are deferred. Their represented types grant no operational permission; unsupported execution slices are refused rather than simplified into successful commands.

Validation labels: **CORE IMPLEMENTATION TESTED** and **REMOTE FIRMWARE COMPILE VERIFIED**, limited to the documented foundation slice. No TUNER, protocol or universal-control hardware validation is established. Owner physical HOME/display/touch/navigation PASS remains separate evidence.

No separate TUNER//01/yoRadio repository was accessed. No native adapter, transport, networking, IR, discovery, cloud, capability API or power-management implementation was added. HOME is not integrated; firmware was not uploaded.

Recommended next REMOTE milestone: **HOME UI -> Universal Control Core integration using Mock Adapter for hardware validation**, following owner review of this foundation and a small serialized runtime/memory profile. That milestone has not started.

## Phase A runtime ownership plan — 2026-10-03

Plan recorded before runtime implementation. Phase A uses one dedicated development profile task on CPU 1, priority 2, with a separately measured 24,576-byte internal task stack. The task starts only after existing UI initialization and release of the LVGL mutex. It owns Store, Core, Mock and Mock Journal for their entire lifetime, performs every Core read/mutation, and destroys them on that same owner. It never calls LVGL, changes HOME or invokes Core from a touch callback. CPU 0's existing LVGL task and all hardware/UI behavior remain unchanged.

The containing runtime object is explicitly allocated in PSRAM; Core's existing bounded payload allocator keeps its existing malloc placement policy. Optional allocation observation records actual payload address classification and allocation/free/failure counts without altering allocation semantics. Internal and PSRAM heap APIs report free, minimum-free and largest block at creation, registration, state, activity, two bounded stress waves and cleanup. Two 60-second real-monotonic retention waits exercise purge without inventing future clock ticks. A portable profile driver is also executable with an injected host clock; host execution is not ESP32 measurement.

No serial device was enumerated in this session. This milestone therefore prepares diagnostics and manual owner testing only. **Phase A hardware gate PENDING; Phase B NOT STARTED.** No hardware PASS or CORE ESP32 RUNTIME PROFILED label is authorized by a successful build or host run. Measurement results and the final build/test procedure are recorded below when preparation completes.

### Implemented Phase A preparation

New files: `components/control_profile/CMakeLists.txt`, `components/control_profile/profile.cpp`, `components/control_profile/runtime.cpp`, `components/control_profile/include/control_profile/profile.hpp`, `components/control_profile/include/control_profile/runtime.hpp`, `tests/control/profile_tests.cpp`. Updated files: `main/main.cpp` (include and post-LVGL-unlock start call only), `main/CMakeLists.txt` (dependency), `components/control/include/control/storage.hpp` (optional non-allocating allocation observer), `components/control/include/control/mock_adapter.hpp` and `components/control/mock_adapter.cpp` (installation accepts current monotonic time; default zero preserves existing callers), `tests/control/run_tests.py`, this report and `CHANGELOG.md`. No HOME facade/integration document was created.

The finite development-only runtime contains one synthetic device/target, two bindings, 28 descriptors, four primary fields plus secondary power (five source samples), and one shared resource. The second binding exercises actual multi-source conflict/resync. It is not a second real device. Mock Journal is still process-local. All construction, Core reads/mutations, callbacks, registry access and destruction run on the dedicated owner; the profile never calls LVGL. Generic Core has no Mock-specific integration code.

Clock time derives from `esp_timer_get_time()` relative to profile start. Waits yield in at most 50-ms chunks. Representative activity covers confirmed commands, separate changed observations, duplicate/no extra dispatch, stale sequence, reconnect/old-session rejection, descriptor republishing, resync, conflict/clearing, delayed completion, no-result expiry and no-execution proof, AMBIGUOUS shared-resource barrier and normal release of a separate queued intent. Proof preserves SENT/AMBIGUOUS history.

Stress: **32 iterations in two waves of 16**, two completed commands per iteration, observations, periodic capability/session changes and conflict/resync. Each wave waits **60,001 real milliseconds**, then verifies deterministic purge leaves zero full results/effects and unchanged registry/source counts. A successful driver makes 69 synthetic dispatches. At the end it destroys/frees the entire Session, removes its observer and deletes the task; it does not leave an interactive Core service running. HOME stays static.

### Allocation evidence and checkpoints

Unchanged configuration: PSRAM-through-malloc enabled; `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384`, internal malloc reserve 32768. Small malloc payloads prefer internal memory; PSRAM enabled alone proves no placement. ESP-IDF's FreeRTOS allocator uses INTERNAL|8BIT for ordinary task/stack allocations. The containing Session explicitly requests SPIRAM|8BIT and reports its actual pointer classification; payload malloc placement is not changed.

The observer classifies every Core payload with `esp_ptr_internal`/`esp_ptr_external_ram`. It reports per-region live/peak payload bytes, actual allocated-block bytes from `heap_caps_get_allocated_size` on the allocator base, live blocks, maximum requested payload, total allocation/free/failure counts and unexpected-region peak. Allocated-block bytes include the aligned Core header; payload counters exclude it. Heap bookkeeping, wrapper, stack/TCB and LVGL allocations are outside these Core counters.

At each checkpoint, `heap_caps_get_info` reports INTERNAL|8BIT and SPIRAM|8BIT free bytes, **boot-lifetime** minimum free bytes, largest free block and allocated blocks. These aggregate readings include concurrent unchanged LVGL/system activity; the observer separately attributes Core allocations. Stack minimum free bytes, reset reason, owner CPU/check and Store blocking are also logged.

| Serial checkpoint | Physical result |
| --- | --- |
| before_owner_task; before_runtime (startup settled, task already allocated) | NOT OBTAINED |
| after_runtime_creation | NOT OBTAINED |
| after_registration; after_initial_state | NOT OBTAINED |
| after_activity | NOT OBTAINED |
| stress_progress (every four iterations); after_stress_wave_1/2 | NOT OBTAINED |
| after_purge_wave_1/2 | NOT OBTAINED |
| after_cleanup | NOT OBTAINED |

Cleanup checks require zero Core payload/live blocks, matching allocation/free counts, no failed allocation, unexpected region or accounting corruption, plus internal/PSRAM heap integrity. The stack/TCB still exists at after_cleanup; idle reclaims it after task deletion. It explains a remaining task footprint versus before_owner_task. Target compiler stack reports show 3,472 bytes for the representative driver frame and 176 for the task frame. Individual frames are not a complete call-chain bound or a physical stack PASS; actual high-water readings must be reviewed.

### Preparation validation and gate

Host: **69 foundation PASS / 0 FAIL, 2,425 assertions**, unchanged; **6 profile PASS / 0 FAIL, 40 assertions**. Total **75 PASS / 0 FAIL**. Profile tests cover nominal/advancing timestamps, owner rejection, interrupted waits, observer balance and observer allocation-failure reporting. They use virtual time and provide no physical heap evidence.

Actual commands: `pio run -e remote01 -t clean -j 1`, then `pio run -e remote01 -j 1`, using the unchanged installed PlatformIO executable/environment. Clean build **SUCCESS**, 491.36 seconds; diagnostic correction rebuild **SUCCESS**, 26.69 seconds; final verification after preserving original main.cpp line endings **SUCCESS**, 9.56 seconds. Compiler warnings: **0**. Final flash **1,742,537 bytes (+18,996)**, static RAM **19,392 bytes (+104)** versus the foundation's 1,723,541/19,288. These are link measurements; the task stack, container and owning payloads allocate dynamically and are not included in static RAM. No upload performed.

**Phase A PENDING PHYSICAL EVIDENCE**, neither hardware PASS nor a measured hardware FAIL. Execution source: none; `pio device list` enumerated no serial devices. All real heap/headroom/fragmentation, allocation-location, stress, reset/watchdog and stability observations are UNKNOWN. No arbitrary free-RAM threshold is invented. Future `END local_checks=PASS` means the finite checks succeeded and also prints `phase_a=OWNER_REVIEW_REQUIRED`; the hardware gate additionally requires measured combined LVGL/Core headroom and observed stability. **Phase B NOT STARTED.** No new runtime/hardware validation label is claimed.

### Owner procedure — upload manually

1. Connect the same REMOTE board and identify its port. The prepared firmware is `.pio/build/remote01/firmware.bin`; use the normal project upload so bootloader/partition handling stays correct. These PowerShell commands are instructions only, never executed by Codex:

```powershell
$remotePio = 'C:\Users\RYZEN\.platformio\penv\Scripts\pio.exe'
& $remotePio device list
# Replace COMx with the enumerated board port.
& $remotePio run -e remote01 -t upload --upload-port COMx
& $remotePio device monitor --port COMx --baud 115200 2>&1 |
    Tee-Object -FilePath remote01-phase-a.log
```

2. Reset once with the monitor attached and capture the full boot log. Expect `UC_PROFILE BEGIN` about five seconds after UI initialization, all checkpoints, two retention waits and one `END`. Allow **three minutes**. If USB re-enumerates, reconnect/reset to capture a full run.
3. While it runs, check unchanged orientation, swipe LEFT 0->1->2, swipe RIGHT 2->1->0, snapping and brightness. HOME controls are still static. Product TUNER//01, battery and Wi-Fi literals are not measurements or TUNER evidence.
4. Return the complete boot/UC_PROFILE log: both heap regions at all checkpoints, allocation counters, purge/cleanup, stack minimum, reset reason, failures and END. Report panic/watchdog, repeated boot/BEGIN, missing END, freeze/reset or display/touch regression. `CHECK_FAIL`, unexpected allocation failure or local FAIL blocks Phase B.
5. Continue carousel/brightness use for one minute after END. Report stability. Review post-purge baselines and largest internal blocks; zero accounting imbalance alone does not prove usable headroom. Existing DMA draw buffer is 22,016 bytes, PSRAM draw buffer 220,160 bytes; LVGL uses existing custom malloc. Combined operational headroom must be established from the real run.

Next action: owner upload and Phase A serial/memory review. Only an actual Phase A PASS permits the deferred HOME->Core->Mock milestone. No upload, networking/real adapter, frozen-model change or separate TUNER repository access occurred.

## Phase A owner acceptance and closure — 2026-10-03

Owner-provided physical evidence, reviewed and explicitly accepted before Phase B implementation: automated/local checks **PASS**; physical ESP32 execution **COMPLETED**; owner review **COMPLETED**; owner acceptance **ACCEPTED**; **CORE ESP32 RUNTIME PROFILED — OWNER ACCEPTED / PASS**; **PHASE A — CLOSED / PASS**. Historical `phase_a=OWNER_REVIEW_REQUIRED` remains correct for the time of emission and is unchanged. This acceptance covers runtime profiling only. Phase B is authorized for implementation, with **HOME-CORE-MOCK HARDWARE VALIDATION — PENDING OWNER TEST**.

All measurements below are bytes. `—` means the owner supplied no value for that checkpoint; no value is inferred.

| Checkpoint | Internal free / minimum / largest | PSRAM free / minimum / largest | Tracked payload | Other captured facts |
| --- | --- | --- | ---: | --- |
| Before runtime | 267191 / 258295 / 184320 | 7720268 / 7720268 / 7602176 | — | Wrapper 5112; PSRAM=1, internal=0 |
| Creation | 266935 / — / — | 7715144 / — / — | 153 | owner_ok=1 |
| Registration | 45935 / 36123 / 31744 | 7628840 / 7619364 / 7602176 | 283642 | owner=1, failures=0, corruption=0 |
| Initial state | 39139 / 36035 / 31744 | 7623844 / 7619364 / 7602176 | 294320 | owner=1 |
| Representative activity | 68923 / 34527 / 31744 | 7580292 / 7580292 / 7471104 | 306374 | dispatches=5, commits=94; failures=0, corruption=0, owner=1 |
| Stress wave 1 | 40323 / 34515 / 31744 | 7536532 / 7522316 / 7471104 | 369909 | iterations=16, dispatches=37, commits=368; failures=0, corruption=0, owner=1 |
| Purge wave 1 | 114815 / — / — | — / — / — | 305574 | failures=0, corruption=0, owner=1 |
| Stress wave 2 | 49499 / 34515 / 31744 | 7537276 / 7517580 / 7471104 | — | iterations=32; failures=0, corruption=0, owner=1 |
| Purge wave 2 | 113983 / — / — | — / — / — | 305574 | failures=0, corruption=0, owner=1 |
| Cleanup | 267191 / — / 184320 | 7720268 / — / 7602176 | 0 | allocations=23976, frees=23976; failures=0, corruption=0, owner=1 |

Final captured checks: local_checks=PASS, balance=1, heap_integrity=1, completed_iterations=32. No monotonic heap loss across stress/purge waves; internal and PSRAM free heap recovered exactly to their initial values. All 23,976 tracked allocations were released. No allocation failure, accounting corruption, watchdog, crash or ownership failure was observed in the captured profile.

**Memory watch item: minimum observed internal free heap 34,515 bytes.** Accepted nonblocking for Phase A. Substantial networking, protocol adapters, discovery, TLS/security, catalogs, artwork or additional adapters require renewed physical ESP32 memory profiling. Measure before optimizing; this watch item alone does not authorize premature Core optimization.

## Phase B HOME integration preparation — 2026-10-03

Phase B now implements the previously deferred HOME facade and persistent serialized ESP32 owner using the same Core foundation. Full architecture, exact file manifest, dynamic fields/controls, bounded development hook, telemetry, limitations, adversarial review and manual physical procedure: [HOME integration report](HOME_CORE_INTEGRATION_V1.md).

Normal boot replaces the automatic Phase A stress task with CPU 1 / priority 2 / 24,576-byte-stack `UC_owner`, preserving the existing LVGL owner and hardware initialization. Explicit PSRAM Session owns Store/Core/Mock/process-local Journal/Facade until reboot. An eight-intent FIFO and copied fixed-size coalesced snapshot cross the boundary; UI sees no Core/Mock pointer and performs no Core calls. PLAY/PAUSE derives from fresh non-conflicted observed state, with a Core field precondition. Unknown/unsupported/unavailable/conflicted/pending controls cannot dispatch. Volume remains read-only; independent synthetic acquisition/results drive widgets. Existing geometry/fonts/palette/carousel/hardware remain unchanged. Phase A profiler remains opt-in source instrumentation, never normal-boot stress.

Controlled generic Core APIs now support binding reachability changes (lost reachability invalidates samples/readiness) and descriptor withdrawal (revision advancement and revalidation). Mock readability metadata supports only the requested CCM media.title/media.subtitle String fields. No direct registry edits, frozen semantic changes, Core capacity expansion, real persistence or networking.

Complete final host suites: **109 PASS / 0 FAIL, 5972 assertions**: foundation 69/2425, profile 6/40, HOME 34/3507. All existing tests remain unchanged. New cases include all 25 requested integration categories plus conflict, expiry, delayed completion, coalescing, actual withdrawal/readiness revision, lost reachability and repeated command purge. Native font bounds separately pass 16 dynamic text/symbol samples. These are not physical tests.

Adversarial corrections: replaced dispatch-only observation publication outside dispatch with owner-side observe; changed tracks invalidate media context before acquisition; narrowed the initial broad media schema helper to title/subtitle; retained historical profiling evidence; added session/revision serial markers for owner verification. The conflict test correctly expects rejected contradictory sequence plus retained conflict latch. Complete suites reran successfully after final source corrections. Final clean build measurements are recorded below and in the HOME report.

Preparation labels after the original successful build: **COMMON CONTROL MODEL v1.0.0 — OWNER APPROVED / FROZEN; UNIVERSAL CONTROL CORE FOUNDATION — HOST TESTED; CORE ESP32 RUNTIME PROFILED — OWNER ACCEPTED / PASS; PHASE A — CLOSED / PASS; HOME-CORE INTEGRATION — COMPILE / HOST TEST VERIFIED; HOME-CORE-MOCK HARDWARE VALIDATION — PENDING OWNER TEST**. No TUNER/protocol/end-to-end validation claim. No separate TUNER repository accessed, no real adapter/networking, no upload. Stop at Phase B; next action is owner physical testing, not another implementation milestone.

Final exact `pio run -e remote01 -t clean -j 1` SUCCESS (1.99 s), then `pio run -e remote01 -j 1` SUCCESS (519.46 s), unchanged verified environment. Flash **1,741,653 bytes / 20.8%, -884** from Phase A 1,742,537. Static RAM **19,648 bytes / 6.0%, +256** from Phase A 19,392. **0 compiler warnings/errors**. Complete host suites passed before this final clean build. No upload; Phase B physical validation was pending at this preparation stage.

## Phase B owner hardware acceptance and closure — 2026-10-03

Owner reports **HOME/Core/Mock hardware validation: PASS** following correction of the development-only Mock USB Serial/JTAG input defect. **HOME-CORE-MOCK HARDWARE VALIDATION — OWNER ACCEPTED / PASS; PHASE B — CLOSED / PASS**. Both the serial-input retest and overall Phase B hardware gates are closed. The preceding preparation/pending records describe earlier stages and are superseded by this acceptance.

The [HOME integration report](HOME_CORE_INTEGRATION_V1.md) retains the owner hardware results, serial root cause/correction, latest software test/build evidence and acceptance. This physical owner PASS covers the synthetic HOME/Core/Mock integration, not a real TUNER adapter or end-to-end protocol. No new numeric memory measurements were supplied; the Phase A 34,515-byte internal-memory watch remains open for future significant loads. Closure updates documentation only: no firmware/configuration/frozen CCM changes, build, upload or new milestone.

## Phase C runtime hardening - 2026-10-04

Entered only after Phase B CLOSED / PASS, using the current hashed working tree (no Git metadata). The owner subsequently supplied approximately 994 s of Phase B telemetry and detailed accepted behavior; see the [Phase C report](PHASE_C_RUNTIME_HARDENING.md) for exact values, evidence boundaries and preserved 34,515-byte watch.

Small corrections protect buffered intent session/revision and pending-token lifetime, prevent duplicate synthetic execution after failed completion evidence allocation, report Core admission refusals accurately, and replace only transient selection scratch/deep copies of identical selected evidence. No Core model/capacity/allocator rewrite, frozen contract change, HOME redesign, hardware change, real transport or TUNER access. Existing foundation/profile/HOME tests remain; deterministic hardening/soak tests and measured allocation comparisons supplement them. Phase C requires owner firmware retest and grants no new physical PASS. Final source manifest, tests/build measurements and exact manual retest are authoritative in the Phase C report; stop at this milestone.

Phase C final verification: **140 PASS / 0 FAIL / 284374 assertions**; opt-in 45000-iteration soak **140 PASS / 0 FAIL / 2779624 assertions**. Final clean/build **SUCCESS**, flash **1751289 bytes / 20.9% (+1640 from fresh baseline)**, static RAM **19776 bytes / 6.0% (+128)**, **0 warnings/errors**. **HOST TESTED / CLEAN BUILD VERIFIED; HARDWARE VALIDATION PENDING OWNER RETEST**. Exact artifact hash and manual COM3 retest are in the Phase C report. No upload; no Phase D.

## Phase C owner hardware acceptance and closure - 2026-10-04

Owner reports **Phase C hardware validation: PASS; OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED** after approximately 960 seconds. Prescribed functional retests passed: **v=25**, capability/session behavior, disabled actions, no buffered replay, delayed completion and >3 s late completion. The current v=25 behavior is physically confirmed, closing the previous discrepancy/retest gate without changing firmware semantics. No crash, panic, reset, watchdog or UI freeze observed.

Exact final physical telemetry is recorded in the [Phase C acceptance report](PHASE_C_RUNTIME_HARDENING.md#phase-c-owner-hardware-acceptance-and-closure---2026-10-04): session 3, capability revision 45, dispatches 14; internal free/min/largest 167519/166491/94208; PSRAM free/min/largest 7714504/7714504/7602176; payload 85765; allocations/frees 30465/30114; failed allocations/accounting corruption 0/0; stack minimum 15888; owner_ok=1; queue_rejected=0. The m command was recognized and accepted with owner_ok=1.

This physical owner acceptance supersedes previous Phase C pending labels; Phase C is closed. The historical Phase A internal-memory watch remains for future significant loads. Documentation only: firmware unchanged, no build/upload, Phase D or next milestone.
