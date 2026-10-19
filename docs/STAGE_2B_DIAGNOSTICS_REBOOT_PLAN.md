# Stage 2B diagnostics / serial reboot ? OWNER HARDWARE VALIDATION PASS

2026-10-04. **OWNER HARDWARE VALIDATED / PASS** for implemented connected-STA read-only diagnostics and controlled DEVELOPMENT_ONLY serialR reboot. Owner reports hardware validation complete on the physical ESP32-S3. This does not validate unimplemented OTA transfer or automatic deep sleep; automatic_system_sleep remainsINHIBITED because external power isUNKNOWN.

## Subsequent Stage 2B.2 development measurement

[POWER per-path stack measurement](STAGE_2B_2_POWER_STACK_PATH_MEASUREMENT.md): implemented / normal229 PASS / soak229 PASS / clean build verified; **OWNER PHYSICAL MEASUREMENT PENDING**, no upload or correction. S reports fixed POWER-owned records; T clears records without clearing the historical watermark. POWER frame1248 B and configured stack4096 B are unchanged; static internal growth952 B, BIN+1920 B. Diagnostics endpoints/semantics, reset history, power policy, provisioning and partition remain unchanged. Earlier Stage2B physical PASS is preserved; it is not a Stage2B.2 physical PASS.

## Physical owner evidence

- Existing Wi-Fi credentials survived migration/upload; STA reconnected without provisioning.
- Read-only diagnostics reachable at assigned STA IPv4; repeated5-second refresh remained stable.
- DEVELOPMENT_ONLY serialR accepted and executed, reporting software reset `reset_reason=3`.
- NVS/Wi-Fi credentials survived software reboot; diagnostics recovered automatically.
- RTC reset history retained and advanced from boot21 to boot22.
- No failed allocations, accounting corruption, network allocation errors, POWER errors or bus errors observed during validation. These are physical owner observations, not host predictions or claims about unreported tests.

## Observations / production freeze follow-up

Stage 2B.1 investigation: [POWER stack evidence and conclusions](STAGE_2B_1_POWER_STACK_INVESTIGATION.md). POWER stack remains 4096 bytes; compiled entry frame increased 496 -> 1248 bytes (+752) from the diagnostics publication block. Exact physical worst path / 608-byte reproduction remain unresolved; production headroom approval remains open. No firmware correction or upload. Owner also observed 800 bytes after controlled reboot.

- **Minimum observed POWER stack watermark:608 bytes.** Open headroom review/investigation before production freeze. Do not claim this margin is sufficient for production or identify it as a reset cause; no stack or firmware change made here.
- Two transient `diagnostic_errors` occurred during initial HTTP/browser tests. After software reboot, `diagnostic_errors=0`. **Root cause not established**; retain both observations without attribution. Post-reboot zero does not erase earlier evidence.
- An HTTP socket receive error occurred when the diagnostics connection was deliberately interrupted by software reboot. Retained as an expected observation of the controlled interruption, **not a demonstrated defect**.

## Roadmap / backlog ? not implemented

- Development Screen0 should display current Wi-Fi state and STA IPv4 address.
- Final UI must offer Network Settings with current IP information and a user-accessible Wi-Fi setup/reset wizard for router/network changes.
- Full factory reset must be a separate deliberate/confirmed action from Wi-Fi credential reset.

This update is documentation only: no firmware/source/test/partition change, build, upload or new milestone implementation. Earlier design, implementation, pending-retest statuses, test/build hashes and physical partition acceptance remain historical records below.

---

# Historical implementation report ? before owner hardware PASS

# Stage 2B diagnostics / serial reboot ? implementation result

2026-10-04. **IMPLEMENTED / FULL REGRESSION AND SOAK PASS / CLEAN TARGET BUILD VERIFIED. OWNER HARDWARE RETEST PENDING.** No upload performed. Physical partition migration remains owner-accepted; that acceptance does not validate these new diagnostics or software-reboot capabilities. Automatic deep sleep remainsINHIBITED/NOT VALIDATED; OTA transfer remainsNOT IMPLEMENTED.

The approved design below is retained, followed by the scope amendment, actual behavior, owner procedure and verification. Historical design-only stop language does not describe the current implementation status.

---

# Stage 2B ? connected diagnostics and serial reboot plan

2026-10-04. **INSPECTION / DESIGN ONLY; IMPLEMENTATION NOT STARTED.** Owner accepted physical partition migration; the current firmware and approved table are frozen during this review. This plan implements no OTA upload/update, web reboot, protocol change, power policy change or automatic sleep. No source files modified.

## Inspection findings / integration point

- NETWORK is the sole existing Wi-Fi/netif/event-loop/NVS owner. Its bounded inbox handles copied events and W/G/Y; its task publishes a WebView through a mutex. WebView contains setup token and scan results, LocalSetup contains AP key, and task-local Credentials contains password. These types must never be handed to the diagnostics serializer.
- Existing AP-only HTTP (`web.cpp`) uses IDF5.5.3 dual-stack HTTPD, full sockaddr_storage and native/mapped IPv4 normalization, canonical192.168.4.1 Host checks, token/Origin protection for provisioning mutations, two sockets and6,144B stack. NETWORK owns nonblocking captive DNS. After successful connection it shuts HTTP/DNS and changes to STA; these paths remain intact. New service lifecycle belongs at the NETWORK owner, after this teardown, not in an event callback or HOME.
- Current power API only publishes a small BatteryView; B's full power/ADC/IMU/reset telemetry lives inside POWER. HOME's presentation snapshot lacks m's allocator/owner/ingress diagnostics; these are owned by UC runtime. Therefore HTTP cannot reconstruct B/m by accessing live owner objects or calling serial command functions. Small neutral copies must be published by their respective owners.
- Actual sdkconfig: IDF logV1/defaultINFO, primaryUART0/115200 and secondaryUSB Serial/JTAG. Pinned SDK log_write.c defaults to vprintf; console_write sends output to both ports, with secondary output-only. No application esp_log_set_vprintf hook found. UC owner installs USB Serial/JTAG driver and reads eight bytes per turn with zero wait. Logging output and character input are distinct paths; preserve both.
- Command audit: Mock uses lowercase p/a/u/v/d/r/o/c/s/l/n/f/m,0/1,-/+; POWER uses B/I/Z/L/C/H/N/K/!; NETWORK uses W/G/Y. Uppercase **R is unused**, distinct from lowercase r. Existing dispatch first routes network keys, then power keys, then Mock. Existing development features are labeled DEVELOPMENT_ONLY rather than comprehensively compiled out by a production flag; retain honest development labeling.
- Pinned ESP-IDF esp_system.h supports esp_restart from task context on either CPU. No NVS erase is needed for reboot. No OTA writer exists in this scope.

## Proposed service / endpoints

A separate `diagnostics` component holds sanitized data and its own HTTP handle/routes. NETWORK starts/stops it; the existing provisioning server, DNS implementation and their guards are not repurposed. Use normal STA port80, mutually exclusive with captive HTTP. Never run both servers; verify provisioning/AP/HTTP/DNS are inactive before start. On provisioning, loss/retry, IP change or generation change revoke diagnostic access before teardown, close sessions, then recreate only for the current healthy STA identity. Start/registration failure leaves HOME/network operation intact and is visible through W; retry at most once per5s, not a busy loop.

| Endpoint | Method | Purpose |
| --- | --- | --- |
| / | GET | Compact phone-friendly diagnostic page, immutable local HTML/JS |
| /api/status | GET | Copied SYSTEM/NETWORK/POWER/CORE diagnostics with per-owner timestamp/age |
| /api/events?after=<sequence> | GET | Up to16 sanitized events, oldest/next sequence and overflow/drop counters |

No POST/PUT/DELETE routes, configuration, credential submission, command bridge, reboot or OTA endpoint. Reject request bodies and unknown methods/paths without draining an unbounded body. The page polls every5s, one request in flight, pauses when hidden and reports stale/unavailable sections accurately. No SSE/WebSocket or long-lived streaming clients.

Status fields mirror useful B/m/W data: build/project/hash identity, uptime/reset reason; internal free/minimum/largest heap, PSRAM free/minimum/largest; owner task stack minima and allocation/accounting/queue/ingress counters; copied session/capability revision/dispatch counts; network state/stored-present/connected/provisioning/IP/RSSI/reconnects/reason/service/errors; power state/display/hold/wake counters/IMU/errors/battery raw/mV/filtered/valid/source/condition. External/charging remainUNKNOWN and SOCUNAVAILABLE; automatic_system_sleep remainsINHIBITED. SSID, if shown, is only the connected network name, never a credential record or password. Exclude setup keys/tokens, security material, arbitrary memory and NVS content.

## Access control / authentication decision for review

**Recommend read-only HTTP diagnostics without an authentication secret for this scope.** Only explicitly allowlisted non-secret operational fields/events are served. Accepted sockets/each request require current healthyCONNECTED state, no provisioning/AP service, local address equal to the current STA IPv4, and peer in that STA subnet. Apply validated native/IPv4-mapped normalization; reject trueIPv6 and malformed addresses. Require canonical currentSTA-IP Host (optional:80), preventing arbitrary DNS Host/rebinding access. Exact IPv4/subnet comparisons use the actual netif values, not a guessed192.168.1.x network. No wildcard captive redirect, permissive CORS or reflected Host. Responses use no-store, restrictive CSP, no external resources, DENY framing and text/JSON escaping; dynamic page text uses textContent.

This is **network access control, not user authentication or encryption**. Other devices on the same LAN/subnet can read the selected operational diagnostics. No authentication tokens/passwords are sent over plain HTTP. Do not port-forward/proxy this service; a router proxy/NAT can defeat source-subnet checks, so those checks cannot prove absolute Internet isolation. If owner requires private per-user diagnostics, revise the plan for authenticated HTTPS and audit certificate provisioning/heap cost before implementation; do not silently add plaintext bearer/Basic credentials.

No browser reboot is proposed. This avoids a remotely callable privileged action without verified authenticated transport/CSRF/replay protection. Physical serial remains the development command interface. Existing provisioning security semantics remain unchanged.

## Logging and ownership / resource limits

Use a structured **diagnostic event history**, not a raw console mirror: explicit field-enumerated events for boot/reset, network transitions/errors, power transitions/wakes/errors, Core lifecycle/refusals and reboot request. No arbitrary caller strings, SSID/password-bearing driver messages or formatted SDK log capture. Leave vprintf/serial logging untouched. This is not a panic backtrace recorder or persistent log; RAM history is lost on reboot, while existing validated RTC reset history remains available as a sanitized diagnostic summary.

Owners publish neutral fixed-size copies at most once per1s, plus immediate selected transitions. HTTP reads copies only, never Core/Mock/LVGL, hardware I2C/ADC, or credentials. Short zero-wait task-context locks cover fixed copies only; formatting, heap queries and socket I/O stay outside locks. Contention drops a publication/event with counters; stale age is shown. No waits or allocations on owner publication paths and no ISR logging. Core memory metrics are collected only by UC owner using existing m's sources.

Proposed hard limits for application-owned storage: **8,192 bytes PSRAM total**, comprising32x128-byte event slots (4,096B), copied telemetry storage<=1,024B and one bounded3,072B response buffer. Small synchronization/lifecycle control structures must remain<=512B internal static storage, enforced by size checks. Allocate once, check failure, clean partial initialization; no per-event allocation. Immutable page bytes reside in flash. Emit at most16 events/request; overwrite oldest on full ring and increment overflow counter, report sequence gaps. All string/JSON output is bounded/escaped; reject or mark truncation, never overflow.

One HTTP task **6,144B internal stack**, priority1, max2 client sockets, receive/send timeout3s, URI64B, headers1,024B, LRU purge. No new telemetry/log/reboot task, queue, websocket or per-client application buffer; HTTPD's existing3 internal sockets plus2 clients fit configured socket budget. In connected mode this is an additional active HTTP stack/service relative to current baseline, although never simultaneous with provisioning HTTP. SDK HTTPD/TCP/netif allocations are additional and bounded by configuration; exact internal/PSRAM cost and physical minimum margins require build/owner measurement, not an invented hard total. Failure must disable diagnostics, not local UI or networking.

## Controlled serial reboot

Route uppercase R through the existing UC serial dispatch -> NETWORK development-command inbox; log explicit DEVELOPMENT_ONLY queued/refused and NETWORK accepted/executing markers. If inbox/owner unavailable, refuse; no direct restart from parser/ISR/HTTP. Duplicate requests coalesce. NETWORK schedules a500ms output opportunity and then invokes esp_restart from its own task outside any lock. This is a scheduling interval, not a guaranteed USB log flush; attempt only a bounded TX wait where the installed driver supports it. No NVS writes, credential deletion, partition erase, GPIO/power-hold changes or forced sleep. Reject if a forced sleep/shutdown lifecycle is already pending. The deferred policy must also reject an active flash-writer guard before any future OTA implementation integrates it; no claim that an OTA writer exists now. Normal boot should report ESP_RST_SW, retained credentials -> CONNECTING -> CONNECTED, no provisioning. Production command remains explicitlyDEVELOPMENT_ONLY under the current development policy, not falsely represented as compile-disabled today.

## Exact planned implementation file scope

New:

- components/diagnostics/CMakeLists.txt
- components/diagnostics/include/diagnostics/api.hpp
- components/diagnostics/include/diagnostics/model.hpp
- components/diagnostics/model.cpp
- components/diagnostics/service.cpp
- tests/control/diagnostics_tests.cpp

Modify:

- components/network_manager/runtime.cpp ? service lifecycle, sanitized publication, owned reboot handling
- components/network_manager/include/network_manager/model.hpp ? reboot message kind/development mapping only
- components/network_manager/CMakeLists.txt ? diagnostics dependency
- components/control_home/runtime.cpp ? sanitized owner publication; existing network dispatch handlesR
- components/control_home/CMakeLists.txt ? diagnostics dependency
- components/power_management/runtime.cpp ? sanitized owner publication; no power-policy change
- components/power_management/CMakeLists.txt ? diagnostics dependency
- tests/control/run_tests.py ? additive diagnostics suite
- docs/STAGE_2B_DIAGNOSTICS_REBOOT_PLAN.md, docs/STAGE_2B_OTA_PARTITION_MIGRATION.md, docs/STAGE_2B_LOCAL_OTA.md, CHANGELOG.md ? implementation/result/owner-procedure records

Existing portal web.cpp/portal.cpp/dns.cpp, NVS format, sdkconfig, platformio.ini, approved partitions.csv, GPIO/hardware drivers, frozenCCM and HOME assets remain unchanged. If implementation requires expanding this scope, report the specific reason rather than silently changing these boundaries.

## Planned verification / stopping point

Add tests for STA/provisioning/retry/IP/generation separation, native/mappedIPv4/trueIPv6 and Host/subnet guards, safe escaping/secret-free types, stale/unavailable snapshots, bounded buffers/client policy, overwrite/drop counts, no web mutation, uniqueR and serialized/deferred/coalesced/refused reboot, no NVS/partition effects. Regress previous complete205 tests and full soak, add10,000 diagnostic lifecycle/ring cycles, then clean target build. Record exact BIN/hash,3MiB per-slot headroom, RAM/PSRAM/task/socket limits, warnings/errors and owner acceptance procedure. Hardware networking/resource/reboot behavior remains owner tests, not host predictions.

**STOP NOW:** inspection/design and owner acceptance documentation only. No source changes, tests/build/upload or new service. Existing diagnostic logs over Wi-Fi andR are proposals, not capabilities in the current artifact.

## Approved scope amendment

Owner approved adding `components/power_management/include/power_management/reset_diagnostic.hpp` and `components/power_management/reset_diagnostic.cpp`. The original plan could not export its required sanitized RTC summary: storage was private and the public API only printed records. The amendment adds a bounded by-value ResetSummary and a validated Record-by-value sanitizer; POWER is the sole production caller of reset_history_copy and sole exporter to diagnostics. No pointers/references to stored records escape. Original640B/four-boot RTC format, retention, save semantics, capacity and serialB output remain unchanged; no persistence added.

## Implemented behavior / owner validation procedure

The implementation follows the read-only HTTP decision above. Endpoints are GET-only, address/Host/subnet restricted, with no auth secret or browser commands. Operational metadata is visible to same-subnet LAN clients and unencrypted; no Internet-isolation guarantee under router proxy/NAT. SDK/serial logs are not intercepted. Event rows are typed numbers only; strings are limited to escaped build/network identifiers. The event page resets its cursor if a reboot invalidates the former sequence. Snapshot sections expose available/time_ms/age_ms; they are independent owner publications, not one atomic cross-task system instant. Publication contention increments drops, and stale data remains explicitly timestamped. Oversized serialized status returns503 rather than overflowing or emitting incomplete JSON.

One manual owner firmware upload after approval, no erase or second test firmware:

1. Use normal `pio run -e remote01 -t upload --upload-port COM3 -j 1`, then monitorCOM3/115200. Verify stored credentials -> CONNECTING -> CONNECTED, no provisioning/AP/captiveHTTP/DNS. Record B,m,W and DIAGNOSTICS running1/STA_ONLY. No passwords needed.
2. Open `http://<W-reported-IP>/` from the same LAN. Check status fields against B/m/W: build/reset/uptime, heap/stack/allocator/Core health, network and battery/power. Verify all sections become available, age stays normally below3s and automatic_system_sleep staysINHIBITED. Check selected events update after HOME Mock controls and display-idle/touch/motion transitions; validate RTC copied summary against B. No passwords/setup/security tokens/NVS contents should be present.
3. Check read-only behavior using browser/curl: wrongHost, POST and bodies, /credentials, /reboot and /ota are denied without state/credential mutation. Example `curl.exe -i -X POST http://<IP>/reboot` and `curl.exe -i -H "Host: invalid.example" http://<IP>/api/status`. Do not expose service via port forwarding. Provisioning regression must be checked only when a fresh provisioning session is naturally available or owner elects it; do not erase production credentials merely to create a test. It remains unavailable on the provisioning AP by design; no new physical captive-session PASS is inferred.
4. Leave polling for10minutes while exercising HOME/buttons/touch/carousel/brightness and battery handover/idle/wakes. Capture B/m/W before/after, including POWER/NETWORK/Core/diagnostic stack minima, INT/PSRAM minimum/largest, drops/errors, failed_allocations/accounting/owner/queue health. Ring is32 records and returns<=16 per request; overflow/gaps are observable, not heap growth. SDK connection allocation can fluctuate; no unbounded loss or new crash/WDT/reset/freeze is acceptable. Deep sleep remains untested/inhibited, never a PASS claim.
5. With USB serial connected, send uppercaseR once. Expect NETWORK_DEV queued then ACCEPTED/EXECUTING DEVELOPMENT_ONLY markers, controlled software reboot (SDK ESP_RST_SW=3), retained credentials and reconnect without provisioning. Reopen monitor/browser if needed; check B's retained reset summary and W/status after reboot. DuplicateR requests before execution may report COALESCED. Lowercaser retains its original Mock behavior. No NVS erase or browser reboot path. Reboot requested during an already-pending forced power lifecycle is refused/deferred-policy-tested; do not force sleep/hard release just to manufacture that test.
6. Return secret-free observations and telemetry. Hardware status remainsOWNER RETEST PENDING until these checks pass. No current OTA writer exists; future active-writer refusal is covered by policy tests, not a present OTA implementation claim.

## Final verification and artifact

Final normal suite: **215 PASS /0 FAIL /382,304 assertions**. Final soak: **215 PASS /0 FAIL /3,471,554 assertions**, including10,000 new ring/lifecycle/reboot-policy stress cycles. This comprises previous205 unchanged cases,9 new C++ diagnostic cases and1 actual-source serial-namespace audit (3 assertions). All suites exit0; no tests weakened. Portable tests cover native/mappedIPv4/truncation/IPv6, wrongHost/subnet/peer/network/broadcast, read-only routes/body/cursor rejection, generation/IP identity differences, JSON escaping/unavailable/size guards, RTC sanitizer validation/by-value isolation, serialized inbox handling, delayed/coalesced/refused reboot and ring wrap/overflow. They do not emulate real SDK sockets, actual shutdown/restart or physical stack/heap margins. SDK integration is compiled, and browser script `node --check` passed.

Commands: `python tests/control/run_tests.py`; `python tests/control/run_tests.py --soak`; `pio run -e remote01 -t clean -j 1` **SUCCESS exit0,2.24s**; `pio run -e remote01 -j 1` **SUCCESS exit0,553.00s**. Completed final tests/clean/build have **zero compiler/CMake warnings/errors**. Initial incremental preflight failed because PlatformIO cached CMake without the newly added diagnostics component; clean regeneration resolved it. An earlier clean compile passed544.18s, then the final settled source was clean-built again after review refinements. Initial standalone g++ probe failed without its runtime DLL directory on PATH; corrected probe and complete suites passed. These intermediate failures are recorded, not shipped or concealed.

| Resource | Frozen accepted-layout baseline | Final | Delta / limit |
| --- | ---: | ---: | --- |
| Full BIN bytes | 2,461,024 | 2476432 | +15,408 |
| Linked application bytes | 2,460,521 | 2,475,933 | +15,412 |
| Static RAM bytes | 45,796 | 45,828 | +32 total linked RAM |
| Each OTA slot bytes | 3,145,728 | 3,145,728 | Unchanged |
| Each slot full-BIN headroom | 684,704 | 669296 | 653.609375KiB |
| BIN / each OTA slot | 78.233846% | 78.723653% | Full BIN denominator |
| RTC history bytes | 640 | 640 | Unchanged format/capacity |
| Application-owned diagnostic PSRAM | 0 | 8,056 | <=8,192 hard bound; allocator overhead separate |
| Internal diagnostic control block | 0 | 48 | <=512 hard bound; linker map verified |
| New connected HTTP stack | 0 | 6,144 | One priority1 HTTPD task, never alongside provisioningHTTP |
| Client sockets | No connectedHTTP | <=2 | Plus SDK internal sockets, no new log/reboot task/queue |

Target initialize literal confirms8,056B PSRAM allocation; compile-time size assertions bound snapshots<=1,024B and each32 event slot128B. Arena is snapshots872B +ring4,112B (slots plus sequence/overflow) +response3,072B. Allocation occurs once on NETWORK, with checked failure. No per-event/per-request application heap allocation. SDK HTTPD/TCP/netif resources and allocator overhead are additional to this arena/control/stack accounting; exact live internal/PSRAM heap impact and minimum margins remain OWNER TEST REQUIRED. POWER/NETWORK/Core task stack allocations are unchanged, but their new publication frames may lower minima; do not assume baseline margins persist. PlatformIO29.5% uses factory8MiB, not the3MiB OTA slots. The32B total static RAM delta is a link measurement, not a claim that all new control storage is32B.

Final BIN SHA-256:
`a61a303aa23b3f02bb419831080f6d81ecb7e006f8ef11ece1a457ce1bc481e8`

Final ELF SHA-256:
`e50d4847af85bc96bd8719b44e798a71eec8edc892e91b3398432bc253b7271f`

Partition binary SHA-256 remains `d7ffd497cfcd33e697304b3cf8112e7a1f1cb0619639fe9879ed431cbbdc8835`. Both3MiB slots fit the final full BIN. Source manifest matched the build inputs; only the subsequent additive test-runner namespace audit changed during the final build, and its final complete suites passed. No firmware source changed during that final clean build. Approved20-file scope verified; all remaining repository files byte-identical to the frozen entry manifest, including partition/source configuration, captive web/DNS, hardware/GPIO, UI assets, frozen CCM and Core files.

Actual changed scope: the6 new files and10 existing source/test/build files (including the2 amended reset-history files), and the4 documentation records (20 total). More precisely source/test/build changes are components/diagnostics/{CMakeLists.txt,include/diagnostics/api.hpp,include/diagnostics/model.hpp,model.cpp,service.cpp}; components/network_manager/{runtime.cpp,include/network_manager/model.hpp,CMakeLists.txt}; components/control_home/{runtime.cpp,CMakeLists.txt}; components/power_management/{runtime.cpp,CMakeLists.txt,include/power_management/reset_diagnostic.hpp,reset_diagnostic.cpp}; tests/control/{diagnostics_tests.cpp,run_tests.py}; documentation is this plan, STAGE_2B_OTA_PARTITION_MIGRATION.md, STAGE_2B_LOCAL_OTA.md and CHANGELOG.md.

No authentication, HTTPS, SSE/WebSocket, web reboot, persistent log, SD, OTA transfer, automatic sleep, power detection, TUNER, encoder, button or UI redesign. Firmware awaits owner approval before any manual flash. **This task stops after verified implementation/documentation; no hardware upload.**
