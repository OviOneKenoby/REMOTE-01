# Stage 2A ? accepted baseline for Stage 2B; reboot persistence hardware PASS

2026-10-04. Owner explicitly designated Stage 2A networking/provisioning as the **accepted baseline for Stage 2B**. This authorization supersedes the prior closure review's decision to stop progression; it does not turn unperformed tests into physical PASS.

**Criterion I: PASS, no longer DEFERRED.** Owner performed an actual MCU reset using esptool usb_reset/hard_reset. Without intervention, stored credentials led to CONNECTING (not PROVISIONING), WPA2 association and DHCP at 192.168.1.83. A later normal firmware upload/reset independently repeated stored credentials -> CONNECTING -> association -> DHCP.

After each reported reset: CONNECTED, provisioning=0, connected=1, stored=1, storage_ready=1, IP=192.168.1.83, reconnects=0, reason=0, errors=0, allocation_errors=0, HTTP/DNS running=0; first reset also reported RSSI=-61. These are physical owner observations, not inferred from stored=1. Earlier errors=1/reason=2 from the wrong-password attempt remain historical evidence, not contradicted by these later zero counters.

Current acceptance classification: A?I and M PASS within the previously recorded observation limits; J/K router loss/recovery and L deliberate reprovision remain DEFERRED. Independent manual direct-IP fallback remains unreported; Apple/Windows physical testing is not claimed. Existing successful Android captive/protected provisioning evidence and the original two-client portal failure/correction are preserved below.

Stage 2B partition preflight subsequently found an OTA blocker; see [Stage 2B preflight](STAGE_2B_LOCAL_OTA.md). No firmware or partition change occurred.

---

# Historical Stage 2A closure review ? before new reboot evidence and progression authorization

# Stage 2A ? owner hardware validation / closure review

2026-10-04. **CORRECTED PROVISIONING PATH OWNER HARDWARE PASS; STAGE 2A ACCEPTANCE / CLOSURE DEFERRED.** No new demonstrated defect in this retest. Original hardware acceptance items I, J, K and L remain unperformed; the original Stage 2A request provides no policy permitting acceptance/closure with those items deferred. Automated coverage supports them but does not replace their explicitly requested physical validation. Stage 2A is not marked fully OWNER HARDWARE VALIDATED / ACCEPTED / CLOSED. Stage 2B has not started.

## Original acceptance matrix

Evidence: owner's corrected-firmware hardware report, supplied on 2026-10-04; original Stage 2A request section 15, A?M. PASS describes the supplied observation, not additional inferred testing.

| Criterion | Status | Evidence / limit |
| --- | --- | --- |
| A. First/no-credential boot enters provisioning | PASS | Corrected boot reported ready=1, PROVISIONING, AP-only DNS and HTTP running. |
| B. Phone/laptop joins provisioning AP | PASS | Android joined protected setup AP using locally displayed key. No laptop retest claimed. |
| C. Nearby protected networks visible | PASS | Owner saw nearby networks and selected the protected home network; scan_count=12. |
| D. Select supported normal protected network | PASS | Intended protected home network selected and connected. Specific negotiated WPA version not supplied. |
| E. Password entry without logging password | PASS | Credential submission exercised; supplied diagnostics contain no password. Existing audited redaction/secret-handling and automated coverage retained. No exhaustive owner log audit claimed; actual password is not recorded here. |
| F. Leave provisioning and connect STA | PASS | CONNECTED, connected=1, provisioning=0; HTTP/DNS stopped. |
| G. IP/RSSI/CONNECTED diagnostics | PASS | Owner W: IP=192.168.1.83, RSSI=-61, CONNECTED. No independent RF measurement claimed. |
| H. HOME/touch/controls remain functional | PASS | Owner reports local usability and existing Stage 1 operation, with no HOME/touch failure. No separate exhaustive control/capability retest claimed. |
| I. Reboot retains credentials and reconnects | DEFERRED | Next natural firmware upload/reset. stored=1 is not reboot evidence; USB removal is not a reset with SYS_EN held. |
| J. Temporary AP loss retains credentials/local usability | DEFERRED | Owner cannot intentionally disable production Wi-Fi. Wrong password recovery is a different test. |
| K. AP restoration reconnects automatically | DEFERRED | Not physically exercised; follows a suitable natural/non-disruptive network loss opportunity. reconnects=0 is not recovery evidence. |
| L. Deliberate development reprovision clears only Wi-Fi | DEFERRED | G/Y not performed; automated isolation/lifecycle coverage retained. Original section 15 includes this physical criterion and gives no deferral waiver. No credential deletion requested now. |
| M. No crash/watchdog/reset/freeze/ownership/allocation failure | PASS | No observed crash, watchdog, spontaneous reset, freeze or unrelated corruption in supplied run; allocation_errors=0. Detailed Core ownership telemetry and a timed endurance run were not supplied and are not inferred. |

No original criterion is marked FAIL by this corrected retest. Earlier CONNECTION RESET remains a historical failure, not rewritten as PASS. No additional duration or detailed Phase D control sequence is introduced as a closure gate.

## Correction-specific physical evidence

- **PASS:** Android automatically opened the portal immediately after protected AP association; owner never typed the IP. DNS/probe handling and real HTTP traffic succeeded. Boot: DNS running=1, AP_ONLY, port=53, owner=NETWORK, buffer=512, budget=4; HTTP running=1, port=80, dual_stack=1, captive_redirects=1. No initialization crash/watchdog.
- **PASS:** accepted=11, mapped_v4=11, http_rejected=0 physically exercise corrected IPv4-mapped normalization. This supports the diagnosed pre-handler socket defect and correction; it does not reconstruct every earlier failed client request.
- **PASS:** Wrong password failed safely; correct password then connected, without reported crash/watchdog/freeze, unrelated corruption or allocation failure. errors=1 and retained reason=2 belong to that invalid-password attempt, not an unexplained current CONNECTED fault.
- **PASS:** provisioning=0, http_running=0 and dns_running=0 after successful STA connection physically verify temporary service shutdown.
- **DEFERRED:** Independently opening direct `http://192.168.4.1/` was required in the correction procedure but not performed. The canonical page was served through Android captive navigation; separate manual fallback remains host-tested, not separately owner validated. No reprovision solely to repeat this check is requested.
- **NOT REQUIRED as separate original closure gates:** Apple and Windows physical client retests and validation of every supported WPA variant. Their software probe policies remain covered by regression; apple=0/windows=0 do not indicate physical PASS.

## Final supplied owner telemetry

```text
NETWORK:
state=CONNECTED provisioning=0 connected=1 stored=1 storage_ready=1
rssi=-61 ip=192.168.1.83 reconnects=0 reason=2 scanning=0 scan_count=12
rejected=0 errors=1 allocation_errors=0
stack_min_bytes=1796 http_stack_min_bytes=2472
INT_free=62239 PSRAM_free=7636068

NETWORK_PORTAL:
http_running=0 accepted=11 mapped_v4=11 http_rejected=0 requests=97
redirects=6 http_errors=0 last_reject=0 android=6 apple=0 windows=0
dns_running=0 dns_queries=55 dns_answers=55 dns_rejected=0 dns_errors=0
```

These are owner physical observations. Existing automated regression **205 PASS / 0 FAIL / 373,221 assertions**, soak **205 PASS / 0 FAIL / 3,381,471 assertions**, and corrected clean build remain the software evidence; none was rerun for this documentation-only review. No detailed hardware test beyond the report is claimed.

## Deferred follow-up and artifact identity

I is deferred to the next natural upload/reset; J/K to a non-disruptive opportunity; L remains an unfulfilled original physical criterion, supported by automated tests. These prevent closure under the original acceptance policy, which contains no waiver. The correction's independent direct-IP check also remains deferred. No enclosure disassembly, production-network outage, immediate G/Y deletion or new upload is requested. The historical procedures below are retained for provenance, not current demands.

No firmware/source/configuration changed, no build or upload performed, and no Stage 2B/OTA/web serial started. All non-document inputs match the previous correction final-input manifest. The documented corrected build BIN SHA-256 is preserved:

`d8398089b56133cd382c12d888c7a29bd7b4f3386559ebd101f2900f0b5e7b14`

Documented corrected ELF SHA-256:
`f7621ded0ac8d041d6eca1f16cb4422956c6d3fe42fb8a65381b8492b701a413`

At review entry, existing on-disk BIN instead hashes to `e716b4cf03c526041a85bd572098e3e8ca749e044971dfc6e5db93fc205756b5`, ELF to `2f25ccbb41b673458d09f07137d7e9fa50227b3945bb0c768314a8e35b156aec`. Both are left byte-identical. Their differing build identity is not explained by this review or attributed to the owner-flashed firmware; owner supplied no flashed-artifact hash. The preserved documented corrected hash is a build reference, not independently verified flash identity.

---

# Historical correction report ? status and procedure superseded by review above

# Stage 2A — HTTP failure correction and captive provisioning

2026-10-04. **FIRST OWNER HARDWARE ATTEMPT FAILED: HTTP CONNECTION RESET. CORRECTION / CAPTIVE PORTAL OWNER RETEST PENDING.** Stage2A is not owner validated. No Stage2B, OTA, web serial, device protocol or HOME/Core/power redesign.

## Owner failure retained

Owner flashed the initial Stage2A implementation: normal boot and protected AP association worked on two independent clients (phone and tablet), but both reported CONNECTION RESET opening `http://192.168.4.1/`. This is a reproduced firmware defect, not dismissed as a client problem. Reported W:

```text
NETWORK_DEV: key=W queued=1 DEVELOPMENT_ONLY
NETWORK: state=PROVISIONING provisioning=1 connected=0 stored=0
storage_ready=1 rssi=0 ip= reconnects=0 reason=0 scanning=0
scan_count=0 rejected=0 errors=0 allocation_errors=0
stack_min_bytes=1868 http_stack_min_bytes=0
INT_free=46855 PSRAM_free=7635900
```

The original zero HTTP watermark is consistent with no handler reaching `allowed()`, not independent proof of a cause. Owner did not provide the flashed BIN hash. At correction entry the source/configuration matched the previous final-build input manifest exactly; on-disk BIN was `0aa9033cf2a700f64f125e81826a7f2e0d1f477a938892c75fb78fcb520cae62`, ELF `046cbd95770134f758d4afea77b9bc6a41f5a8e30ec95a38b560fefaa0fdcf3b`. These are entry artifacts, not an inferred owner-flashed identity; initial reported build hashes remain in the historical record below.

## Exact demonstrated HTTP defect and correction

Actual `sdkconfig.remote01` has `CONFIG_LWIP_IPV6=y`. In pinned IDF5.5.3 `components/esp_http_server/src/httpd_main.c:httpd_server_init`, HTTPD creates `socket(PF_INET6, SOCK_STREAM, 0)` and binds IPv6 ANY. In `components/lwip/lwip/src/api/sockets.c:lwip_getaddrname`, IPv4 addresses on that IPv6 netconn are explicitly converted by `ip4_2_ipv4_mapped_ipv6` before `IPADDR_PORT_TO_SOCKADDR` and returned by `getsockname`/`getpeername`.

The initial `portal_socket()` supplied only `sockaddr_in`/its IPv4-sized buffer and required `local.sin_family == AF_INET`. The SDK copies only the supplied length; the family is still AF_INET6 and the IPv6 address is truncated. Therefore the custom `open_fn` deterministically rejected legitimate IPv4 SoftAP connections before parsing/URI handlers. HTTPD's `httpd_sess_new` closes the rejected session; no application HTTP failure counter existed. This source defect explains the CONNECTION RESET and unentered handlers despite successful AP association. HTTP startup/registration failures would have made the original manager DISABLED, rather than the observed PROVISIONING; no successful-provisioning teardown occurs with stored0. Host validation inside the handler was not the cause of this pre-handler rejection.

Correction: use full `sockaddr_storage` buffers for local **and peer** addresses; validate returned family/length; normalize native IPv4 and exactly `::ffff:a.b.c.d` to four IPv4 bytes. Genuine IPv6, partial/malformed addresses and other families remain rejected. Require AP netif up, actual local AP address and a peer in192.168.4.2–254. STA-address access remains blocked; identical STA/AP addresses fail closed. Canonical pages/status/submission remain restricted to Host192.168.4.1[:80], with existing session token/Origin checks and credential-body bounds/scrubbing. No socket/Host guard was simply removed.

HTTP counters now distinguish running server, accepted connections, mapped connections, rejected checks, handler requests, redirects, errors and last rejection reason. The first three accepted sockets and first four guard failures log concise secret-free markers. W includes these counters, DNS counters and Android/Apple/Windows probe counts in a `NETWORK_PORTAL` line. HTTP watermark now samples **socket-open callbacks as well as handlers**; `requests` is the separate handler-entry evidence. Original `errors` remains the Wi-Fi/storage/deadline counter; HTTP/DNS failures have explicit separate counters, so a reset cannot be hidden behind that generic zero. Guard reason1=socket lookup,2=unsupported/truncated address,3=netif,4=AP/peer/STA-address boundary,5=inactive/request socket,6=Host,7=authorization/policy,8=HTTP rejection;100+SDK enum indicates parser/protocol failure. Counters are lifetime failure/check events, not unique client requests; a rejection and its error response can both count.

## Captive portal implementation and SDK audit

Captive presentation is now an explicit product requirement, superseding the initial direct-IP-only decision. Audited the pinned `examples/protocols/http_server/captive_portal` and `examples/wifi/softap_sta`: SDK socket/DHCP DNS facilities and HTTP redirects suffice without a new dependency or configuration/partition change. The example DNS name parser lacks the input bounds required here, and its task deletion/ownership is unsuitable for bounded cleanup; it is not copied wholesale. See the [Espressif captive example](https://github.com/espressif/esp-idf/tree/v5.5.3/examples/protocols/http_server/captive_portal).

- NETWORK owns one nonblocking AF_INET UDP socket bound specifically to192.168.4.1:53. It advertises that AP address through DHCP option6 using the pinned supported `esp_netif_dhcps_option`/`esp_netif_set_dns_info` sequence with checked stop/restart. STA DNS is not replaced. No DNS task, queue or per-query allocation is added.
- Fixed RX513/TX512 byte buffers detect/reject oversized512-byte queries. Up to four packets per owner turn; only while provisioning the existing wait becomes100ms to service DNS. Outside provisioning the original1s wait remains. No continuous query/name/secret logging. HTTP/DNS startup, socket, parse, send and rejection failures are observable.
- The portable parser bounds every header/name/label/question/EDNS field, permits one IN question, rejects compression/loops/unsupported framing and oversized packets. Any valid A hostname resolves to192.168.4.1 with TTL0; AAAA/HTTPS/other IN types get a bounded NOERROR/no-data answer, allowing IPv4 fallback without inventing an IPv6 service. Optional EDNS0 framing is checked and omitted from the small response. Responses never exceed512 bytes. AP peer/address isolation and STA/AP collision checks also apply to DNS.
- Exact routes keep priority over a final read-only wildcard GET handler. Android `/generate_204` and `/gen_204`, Apple `/hotspot-detect.html` and `/library/test/success.html`, Windows `/connecttest.txt`, `/ncsi.txt` and `/redirect`, plus future HTTP GET probes, receive **302 to the constant absolute `http://192.168.4.1/` with a nonempty local HTML body**. No204/Apple Success/Microsoft Internet-success payload is fabricated. Probe-specific counters identify the paths actually seen. Sources: [Android NetworkMonitor](https://android.googlesource.com/platform/frameworks/base/+/b98766ec7785296b642b0a809c1be6107d1f9bcf/services/core/java/com/android/server/connectivity/NetworkMonitor.java), [Windows NCSI](https://learn.microsoft.com/en-us/windows-server/networking/ncsi/ncsi-frequently-asked-questions), and the pinned Espressif example's Apple/nonempty redirect handling.
- Host exception is limited to **read-only redirects**: a syntactically valid arbitrary DNS Host can GET a probe or noncanonical root and be redirected. The page/token, `/networks`, credential POST and scan POST are never served under arbitrary probe Hosts. Malformed Hosts, non-AP sockets, inactive sessions and noncanonical/missing-token/foreign-Origin POSTs remain denied. No request Host/URL is reflected; credentials never enter URLs or logs. Existing CSP, no-store, protected random-key AP, NVS isolation and secret scrubbing remain.
- Re-provisioning/failure cleanup and successful AP shutdown close DNS on its sole owner and stop HTTP. The socket is not left blocked or force-deleted from another task. Counters remain cumulative across sessions. Existing HTTP task stays6,144B/priority1, with two client sockets and now five URI routes; DNS adds one socket. HTTP's three internal sockets plus two clients plus DNS fit the existing10-socket configuration. No new task stack is allocated.

Automatic presentation depends on actual OS probing/cache/settings and remains **PENDING OWNER RETEST**. HTTPS/DoH/TLS interception is not implemented; strict Private DNS, VPNs, OS policy or previously cached no-Internet state can bypass ordinary captive discovery. Direct HTTP IP remains the reliable fallback. DHCP option114/CAPPORT is not enabled in the current SDK configuration and is not claimed. NVS/flash remain unencrypted as before; no claim of protection against physical flash extraction.

## Correction verification and artifact

Final full regression: **205 PASS / 0 FAIL / 373,221 assertions**. Full soak: **205 PASS / 0 FAIL / 3,381,471 assertions**, including10,000 added captive lifecycle cycles and10,000 deterministic parser-fuzz cases (95,000 combined prior/network/portal stress iterations). All previous190 tests/assertions remain intact. Fifteen added tests cover IPv4-mapped regression/truncation, native/real IPv6, AP/STA/collision boundaries, direct GET and authenticated POST policy, malformed Host/session/origin, all three probe families, direct fallback, wildcard A/AAAA/EDNS0, maximum255-byte wire names, malformed/truncated/oversized DNS, disabled/repeated sessions, bounded fuzz guard bytes and secret-free failure diagnostics.

Commands: `python tests/control/run_tests.py`; `python tests/control/run_tests.py --soak`; final `pio run -e remote01 -t clean -j 1` **SUCCESS, exit0,1.90s** then `pio run -e remote01 -j 1` **SUCCESS, exit0,510.14s**. Final regression, soak, clean and build have **zero compiler/CMake warnings or errors**. Final source manifest verified unchanged against build inputs. Initial incremental preflight failed linking the newly listed CMake translation units because PlatformIO retained its old source list; a clean regeneration compiled them correctly. That failure is recorded, not shipped or silently ignored. An added maximum DNS-name boundary check required final verification after the preliminary successful build; final build includes that correction.

| Resource | Initial Stage2A source | Corrected captive artifact | Delta |
| --- | ---: | ---: | ---: |
| Flash link usage | 2,452,697 B | 2,460,521 B | +7,824 B |
| Static RAM | 44,652 B | 45,796 B | +1,144 B |
| RTC retention | 640 B | 640 B | 0 |
| NETWORK / HTTP stack | 6,144 / 6,144 B | 6,144 / 6,144 B | 0 |

No new task, inbox or task stack. Added static DNS buffers1025B, counters/snapshot fields, one transient UDP socket/netconn and five rather than four HTTP routes. SDK UDP/socket/DHCP allocations are runtime costs not captured by static RAM; physical heap and minimum-stack margins remain owner checks. HTTP and DNS lifetime follow provisioning, with no per-query heap buffer. Existing memory/power/ownership behavior and all unrelated files/configuration are unchanged against entry.

Corrected [firmware.bin](../.pio/build/remote01/firmware.bin) SHA-256:
`d8398089b56133cd382c12d888c7a29bd7b4f3386559ebd101f2900f0b5e7b14`

Corrected [firmware.elf](../.pio/build/remote01/firmware.elf) SHA-256:
`f7621ded0ac8d041d6eca1f16cb4422956c6d3fe42fb8a65381b8492b701a413`

Exactly12 repository files added/updated, listed below. No upload performed. **FIRST HARDWARE ATTEMPT FAILED; CORRECTED ARTIFACT AUTOMATED VERIFICATION PASS; OWNER HTTP/CAPTIVE/STABILITY RETEST PENDING.** Stage2A is not accepted or closed.

Automated scope is portable normalization/request/DNS policy plus the complete unchanged existing suites. It does not emulate the SDK's real sockets/DHCP, an actual phone browser or physical OS portal presentation. Physical HTTP/CNA and stability acceptance remain pending.

## One corrected owner procedure — current, supersedes historical procedure

1. **Upload the corrected identified BIN once manually**, using the normal firmware upload method. Enter provisioning: if stored credentials exist, send `G`, wait for its acknowledgement, then `Y` within10s. Wait for NETWORK_DNS running1 and NETWORK_HTTP running1. Use `W` to obtain diagnostics/redisplay the local AP card. No PWR button is required; connect USB for cold start as before.
2. Forget the saved REMOTE setup network from the failed attempt if needed, then connect the phone to the protected REMOTE-01-Setup-XXXX AP using the newly displayed key (join its two lines without spaces). **Verify the OS's captive login opens automatically**. Record phone OS/version and whether it opened; do not call the milestone PASS if it fails. No owner password disclosure is needed.
3. **Also open http://192.168.4.1/** explicitly. It must show the same setup page without CONNECTION RESET. Send `W`: expect accepted/mapped_v4/requests and DNS queries/answers to increase, nonzero HTTP stack minimum, running1, no unexpected HTTP/DNS errors. If either presentation fails, retain W plus the secret-free NETWORK_HTTP/DNS markers, exact client behavior and OS version; stop acceptance rather than dismiss the client. Where available, disconnect the phone and repeat those checks on the tablet (AP allows one associated client).
4. Continue with this same artifact: scan protected networks, select supported WPA2/WPA3 or manually enter hidden SSID, enter password locally and submit. Optional wrong-password test should retain provisioning and not save; correct credentials should report CONNECTED/stored1 and remove AP/HTTP/DNS within about2s. `W` must show http_running0/dns_running0/provisioning0 and valid IP/RSSI. Check home-LAN access to the STA address cannot reach provisioning. Neither AP nor home password may appear in serial/returned pages.
5. Check unchanged HOME/Core Mock controls/capabilities/disabled actions, all carousel directions/snapping, brightness, touch, motion wake and DISPLAY_IDLE. Capture normal `m`, `B` and `W`, especially internal heap minimum/largest and task margins during scanning/provisioning. Initial owner INT_free46855 and NETWORK minimum1868 are real baseline observations; no new physical margin is assumed from the host tests.
6. Reboot without erasing or using PWR: close the monitor, run the installed read-only chip query/reset shown below, then reopen COM3/115200. Expect credentials retained, automatic STA connect and no AP/DNS/HTTP:

   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --chip esp32s3 --port COM3 --before usb_reset --after hard_reset chip_id
   pio device monitor -p COM3 -b 115200
   ```

7. Turn the router off temporarily: HOME remains locally usable, credentials stay saved and bounded retry continues. Restore it; allow up to60s backoff plus30s attempt. Verify reconnection. Unarmed `Y` must refuse deletion; deliberate `G`→`Y` must clear only Wi-Fi credentials and restart a fresh protected AP, DNS and HTTP session. Recheck portal/direct access, then provision again without another flash. No stale DNS/server socket or stored-secret endpoint should remain.
8. Continue the original consolidated stability requirement: at least30 minutes including USB→battery handover and repeated idle/touch/motion wakes. Capture W/m/B/reset history after USB reconnection. No new crash/watchdog/reset/freeze, ownership/accounting/queue or allocation failure. Controlled bad-password/router-loss/rejected-input counters may increase; unexplained HTTP/DNS failures are defects. Return observations and telemetry without either password. Stage2A remains owner validation pending, and Stage1's separately documented remaining acceptance gaps are not silently closed.

## Correction file scope

New: `components/network_manager/include/network_manager/portal.hpp`, `portal.cpp`, `dns.cpp`, `tests/control/portal_tests.cpp`.

Updated: `components/network_manager/CMakeLists.txt`, `internal.hpp`, `include/network_manager/api.hpp`, `runtime.cpp`, `web.cpp`, `tests/control/run_tests.py`, this report and `CHANGELOG.md`. Other existing firmware, UI, frozen model, drivers, GPIOs, power and configuration remain unchanged against correction entry.

---

# Original Stage 2A implementation — historical, owner HTTP attempt failed

The original design/build/procedure below is retained as history. Its direct-IP-only decision, IPv4-only socket filter and owner procedure are superseded by the correction above; its automated PASS was not a hardware PASS.

2026-10-04. **IMPLEMENTED / AUTOMATED VERIFICATION RECORDED BELOW / OWNER HARDWARE VALIDATION PENDING.** No upload, OTA, web serial, device protocol or new Core capability is included.

## Entry baseline and audit

The owner explicitly starts Stage 2A. Phase D remains owner accepted/closed. Stage 1 corrected GPIO8 firmware has the reported physical stability retest PASS; its overall acceptance remains pending the evidence listed in [the Stage 1 report](STAGE_1_BATTERY_POWER_MANAGEMENT.md). Historical INT_WDT evidence and acceptance gaps are preserved; proceeding here does not close Stage 1.

Entry on-disk artifacts: BIN `d7d4a8fa305752baa36a114cfa7ccc2533c666799308d9fbdc61a7f2061f6532`; ELF `9584a69f0edbf471b10e5f88ec2436616e5afd191c9e2c3a564562e3404cba98`. The source matched the documented GPIO correction, but that report distinguishes the earlier correction-build hashes and does not prove which bytes were flashed. Resource comparison uses the recorded corrected-source link result: flash 1,805,705 bytes; static RAM 20,272 bytes; RTC 640 bytes.

Audited PlatformIO `espressif32@6.13.0`, pinned ESP-IDF package `framework-espidf@3.50503.0` (**IDF 5.5.3**) and Xtensa 14.2.0+20251107. Existing NVS partition is 0x6000 bytes; factory application is 8 MiB. Partition/configuration/fonts are unchanged. SDK already enables Wi-Fi SoftAP, WPA3 SAE, HTTP server, NVS and lwIP. NVS encryption is **off**.

The only existing Wi-Fi application component, `esp_wifi_bsp`, is an unused factory example with hardcoded demonstration credentials and unchecked operations. It has no active initialization caller; it is not activated or altered. No running netif/event loop, credential store, captive DNS, provisioning server or network abstraction was found in the application. The new component uses the existing SDK facilities, with no new library dependency. Firmware reuses an already-created default event loop if the SDK reports it exists.

Actual SDK evidence includes `esp_wifi_types_generic.h`, `esp_wifi_default.h`/`src/wifi_default.c`, `esp_random.h`, HTTP server header/parser and generated `sdkconfig.remote01`. The default netif convenience constructors assert on allocation failure; this implementation composes their checked netif/attach/default-handler APIs instead. Scan is asynchronous and SDK scan results must be retrieved or explicitly cleared. See [IDF 5.5.3 Wi-Fi API](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/network/esp_wifi.html) and [NVS API](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/storage/nvs_flash.html).

## Ownership and lifecycle

`network_manager` is independent of Core, Mock, HOME intents and the frozen model. One NETWORK task (6,144 bytes, priority 1, CPU0) initializes six seconds after start, so networking does not gate local HOME/Core startup. SDK event callbacks copy small messages into an eight-slot fixed inbox. Posting uses a nonblocking atomic try-lock, never an interrupt-disabled spin. Overflow/busy rejection is counted and actual link/IP is reconciled; scan cleanup and connect deadlines remain bounded even if events are lost. Generations discard obsolete queued application work after re-provisioning. A delayed disconnect does not cancel a newly CONNECTING attempt: its 30-second deadline determines failure.

Only NETWORK owns Wi-Fi configuration, connection, scanning, NVS and policy transitions. HTTP handlers parse/check input and enqueue; they do not configure Wi-Fi or touch Core/LVGL. The existing serial reader routes three unused uppercase keys into that same network inbox. It retains existing power/Mock command behavior and Core ownership.

Published status and provisioning snapshots use one mutex. The LVGL owner uses a zero-wait snapshot read; callbacks never call LVGL. Its temporary top-layer card contains only AP connection instructions/key, is dismissible, and disappears when provisioning ends. `W` can redisplay it. HOME, the three pages, geometry, carousel, snapping, touch mapping and font configuration are unchanged. Key-label memory is scrubbed before deletion. Inactive AP key/token application buffers are scrubbed; submitted secret buffers/consumed queue slots are scrubbed.

States: DISABLED (initialization fault), PROVISIONING (no usable record), CONNECTING, CONNECTED, RETRY_WAIT. Attempts are asynchronous, with a 30-second maximum before disconnect/failure. Saved-credential retries wait 5, 10, 20, 40, then at most 60 seconds, without an attempt limit or credential deletion. A validated association to the submitted SSID plus a live STA netif/IPv4 is required before reporting connection. Failed candidate credentials remain unsaved and the AP remains available for correction. Existing saved credentials survive failures. SDK initialization failure disables networking and cleans up its initialized resources; it does not reboot or reset NVS. Startup is singleton/idempotent. Re-provisioning stops the old server/radio, clears scan resources, advances the generation and creates a new AP session.

## Provisioning and security

- AP: `REMOTE-01-Setup-XXXX`, using the final two bytes of the non-secret SoftAP MAC. WPA2 Personal; one associated client. Each provisioning session gets a fresh 16-character uppercase hexadecimal key (64 random bits), displayed **only locally**. Wi-Fi is first enabled in STA mode without connecting, so SDK hardware RNG has RF entropy; secrets are generated, the radio stopped, and the protected AP configured before starting APSTA. No transient open AP is started.
- Open **http://192.168.4.1/** directly after joining the AP. Automatic captive detection/DNS is deliberately omitted: there was no existing captive service, and another UDP parser/task/probe implementation is unnecessary for this reliable direct-IP workflow. Phones may need “stay connected without Internet”; temporarily disable mobile-data/VPN switching if needed.
- `/` provides scan/select/manual hidden SSID, security and masked password input. `POST /scan` requests asynchronous scanning. `GET /networks` exposes only scan SSID/RSSI/security, scanning flag and provisioning state. `POST /credentials` accepts URL-encoded SSID/password/security. There is no serial/debug endpoint, stored-secret endpoint or credential URL.
- HTTP is temporary and AP-local: accepted sockets and handlers require the AP's local IPv4; Host must match that address. IPv6 sockets are rejected. If DHCP assigns STA the same IPv4 as the AP, requests fail closed because address alone cannot identify ingress; serial `W` remains authoritative while the AP closes. POST requires a fresh 128-bit session token and, when supplied, the correct Origin. Responses are no-store, deny framing, and use a nonce CSP; SSIDs enter browser text/option nodes rather than HTML. The home-LAN STA address cannot access the handlers.
- The server has two client sockets, four routes, 6,144-byte stack, priority 1, 1,024-byte header limit, 64-byte URI limit and three-second receive/send timeouts. Credential bodies are at most 512 bytes, with a five-second overall receive budget plus at most one pending three-second receive. Rejected bodies close the session rather than invoking an unbounded SDK body purge. No secrets are reflected in errors. The browser clears the password after submission, updates the network picker only when results change, and stops after success or five connection failures.
- Scans retain at most 16 records in fixed owner storage. SDK scan allocations are freed after retrieval, cancellation or a 15-second deadline. SDK driver/netif allocations remain subject to available heap; failures are observable, not a claim of allocation-free Wi-Fi. cJSON response allocations are bounded by the 16-entry snapshot and freed after each response; allocation failures generate bounded owner fault messages.
- Supported: WPA2 Personal, WPA/WPA2 mixed using WPA2, WPA2/WPA3 mixed using the WPA2 selection, and WPA3 Personal SAE using the compiled SDK support/PMF. Passphrases are 8–63 printable ASCII bytes; WPA2 also permits a 64-hex PMK. SSIDs are 1–32 bytes, with no control/NUL characters; hidden networks use manual entry. WPA3 64-hex PMK, Enterprise, WPA1-only, WEP and open networks are outside this milestone; unsupported scan entries cannot be selected. Physical WPA3/router interoperability remains pending, not implied by compile-time support.
- On successful association/IP **and** successful NVS replacement, the portal/AP close after a two-second browser acknowledgement window, even if the router subsequently disappears. STA remains active. Failure to stop AP is counted and retried every two seconds; diagnostics show provisioning still active. No permanent configuration AP is intentionally retained.

Security boundaries: provisioning HTTP is carried over the generated WPA2 AP, not TLS; the locally displayed key permits provisioning access. NVS encryption/flash encryption are not enabled here. Therefore credentials are **not protected against physical flash extraction**. No custom cryptography, eFuse changes or new partitions are introduced; the record checksum is corruption detection only. Future production Settings needs deliberate local re-provision controls; the serial deletion mechanism is development-only.

## Persistence and diagnostics

NVS namespace `remote_wifi`, single blob key `credentials`, 108 bytes: magic/version, bounded SSID/password lengths, security mode, fixed string areas and checksum. Length/version/checksum/terminators/security are checked before use. A malformed record safely opens provisioning. The driver uses RAM storage with `nvs_enable=0`; it cannot silently persist a separate Wi-Fi configuration. Replacement uses SDK `nvs_set_blob` plus `nvs_commit`, only after a candidate has actually connected. Normal operation and retries do not write NVS. SDK initialization/full/version errors disable networking rather than erase the partition. Erasure targets this one key only; unrelated namespaces survive. Power-fail integrity relies on SDK NVS, not a custom persistence journal; physical power-interruption testing remains pending.

Interactive PlatformIO USB Serial/JTAG monitor, COM3 / 115200, single uppercase keys (no Enter needed):

| Key | DEVELOPMENT_ONLY action |
| --- | --- |
| `W` | Network state, AP/STA flags, NVS presence/readiness, RSSI/IPv4, reconnect/disconnect reason, scan status/count, rejected/errors/allocation errors, NETWORK/HTTP minimum stack and current internal/PSRAM free bytes. Redisplay the local AP card if active; **no passwords in serial**. |
| `G` | Arm deletion for 10 seconds. Wait for `clear_armed_ms=10000`. |
| `Y` | Confirm while armed; erase only `remote_wifi/credentials`, stop prior provisioning/link and enter a new protected provisioning session. Unarmed/expired confirmation is refused. |

Each key reports `NETWORK_DEV ... queued=...`. NETWORK reports a state snapshot on `W`, not a continuous telemetry stream. A disconnect reason is the SDK Wi-Fi reason; 65536 denotes `IP_EVENT_STA_LOST_IP`. `errors` includes controlled connection deadlines/driver/storage failures, so injected bad passwords/router loss can increase it. `allocation_errors` and `rejected` should remain zero during normal acceptance. HTTP stack minimum is sampled during requests, retained across AP sessions, and zero before any handler has run; physical margins require owner measurement. Existing `m` and `B` diagnostics still cover Core and power.

## Power and rendering checks

All Stage 1 firmware/configuration is unchanged, including early EXIO6 hold, GPIO8 NEGEDGE and retained reset diagnostics. Network traffic does not count as touch/motion user activity. DISPLAY_IDLE switches the display/backlight off as before; the network worker/radio continue and provision/connect offline without LVGL dependency. Touch/motion restore the same local state. External power/charging remain UNKNOWN; automatic system sleep and hard shutdown remain inhibited.

This milestone does not implement networking coordination for the development-only forced sleep/hard-off commands. They are outside this acceptance procedure. Future deeper power saving must explicitly quiesce/restart Wi-Fi, HTTP, scans and event generations through NETWORK ownership before sleep, or use the SDK's supported power-management integration. Backlight-off alone does not imply low radio consumption or a battery endurance claim. NETWORK checks timers once per second when idle; it does not add high-rate serial/power polling.

Card bounds: x6/y148, 160x268, bottom416 within172x640; text x10 relative/card width140. Vendored Montserrat12 line height15 and Montserrat14 line height16 verified enabled. Original single-line SSID/key bounds would exceed140 and were corrected: SSID prefix/suffix are explicit two lines; AP key is two eight-character lines, **concatenated without spaces** when entered. Parsed native glyph/kerning bounds: prefix115px, eight hex characters at most80px, instruction132px, URL95px, title89px, button text95px. Text y12,42,62(two lines),102,122(two lines),164,184; button y216/h36. No overlapping labels, clipping, unintended wrapping, new fonts or HOME layout change. This is a source/font bound check; physical setup-card readability is pending.

## Automated verification / artifacts

Complete final regression: **190 PASS / 0 FAIL / 368,111 assertions**. Complete soak: **190 PASS / 0 FAIL / 3,331,361 assertions**. Existing suites retain 177 tests and their assertions; NETWORK adds13 tests,22,333 normal/220,333 soak assertions and1,000/10,000 bounded networking cycles. Combined existing plus networking soak is75,000 cycles (45,000 hardening,10,000 ingress,10,000 power,10,000 networking). Portable coverage: absent/saved boot, preservation on failure, exact capped retry/reconnect, every byte of corrupt records, credential bounds/replacement/deletion, strict malformed POST parsing, real secret-free diagnostic formatter, eight-slot overflow/busy/consumed-slot scrub, stop/restart generations and networking soak. Existing negative profile-test markers are intentional rejection tests; final suite results are PASS.

`node --check` of the extracted embedded portal script: PASS. Native-font bounds and compiled font flags checked as above.

Reproduction: `python tests/control/run_tests.py` and `python tests/control/run_tests.py --soak`, using the existing MSYS2 UCRT64 g++23 harness. Final `pio run -e remote01 -t clean -j 1` **SUCCESS, exit0,1.85s**; then `pio run -e remote01 -j 1` **SUCCESS, exit0,515.86s**. All final regression/soak/clean/build logs contain **zero compiler/CMake warnings or errors**. Build input manifest verified unchanged after completion. Preliminary builds also passed; final artifact comes from the clean build after all corrections, not a preliminary link.

| Resource | Pre-Stage2A corrected source | Final Stage2A | Delta |
| --- | ---: | ---: | ---: |
| Flash link usage | 1,805,705 B | 2,452,697 B (29.2% of8MiB) | +646,992 B |
| Static RAM | 20,272 B | 44,652 B (13.6% of320KiB) | +24,380 B |
| Retained RTC history | 640 B | 640 B | 0 |

Added dynamic task stacks: NETWORK6,144B for firmware lifetime; HTTP6,144B only while provisioning (12,288B combined at peak). Existing Core/LVGL/POWER stack allocations are unchanged. SDK Wi-Fi task/buffers, default event-loop task2,304B, lwIP task3,072B, netifs, NVS cache and HTTP sockets add runtime allocations not represented by static RAM. Wi-Fi remains configured CPU0/staticRX10/dynamicRX32/dynamicTX32, unchanged SDK defaults. Application wrappers add the fixed eight-message inbox, fixed16-record scan storage, secret-free published scan/status snapshot, small local-only key snapshot and one mutex. SDK/cJSON allocation failures and inbox rejections are observable; Core's own allocation accounting does not count every SDK allocation. Actual total heap/stack margins require `W` plus normal `m` telemetry on hardware, especially scanning/provisioning.

Final [firmware.bin](../.pio/build/remote01/firmware.bin) SHA-256:
`b1561e47e1698eddc572ec7edf83e44dc66191df5a5d5f2dc1b14ddf43347a96`

Final [firmware.elf](../.pio/build/remote01/firmware.elf) SHA-256:
`f68ad30eef1f27ecf35fc71eb28e0a0bbca8c6310f5ee29a0946ec370db0b9af`

Exactly16 repository files added/updated, listed below. Entry baseline comparison confirms all other existing files, including frozen CCM, SDK/PlatformIO configuration, partitions, vendor drivers, GPIO8/power implementation, fonts and carousel/UI content remain byte-identical. No upload or owner hardware PASS is claimed.

Host tests exercise portable policy, parsing, persistence seam, redacted formatter, fixed inbox and lifecycle. They do **not** simulate RF, SDK tasks/HTTP, real flash interruption, router security negotiation or physical wake. Those remain owner tests. No existing test/assertion was weakened.

## One consolidated owner acceptance procedure

Use the one final artifact identified below; build/upload **once manually**. No inaccessible PWR button, actual password disclosure or per-subtest firmware is required.

1. Connect USB to cold-start with the installed protected 1000mAh Li-Po connected. Open `pio device monitor -p COM3 -b 115200`; wait at least seven seconds for NETWORK initialization. Preserve normal boot/Core/power logs. If credentials already exist, send `G`, wait for its arm acknowledgement, then `Y` within10s. Expect only Wi-Fi credentials cleared, PROVISIONING/stored0 and a local setup card. Send `W` to show the card again if dismissed; touch/motion wakes an idle display.
2. On one phone/laptop, join `REMOTE-01-Setup-XXXX` using the two displayed eight-character key lines joined with **no spaces**. Keep the no-Internet AP connection and open `http://192.168.4.1/`. Scan. Confirm protected SSIDs/security/RSSI appear; select a normal WPA2 network (WPA2 option for mixed networks), or WPA3 Personal only if your actual router supports it. Manual SSID is available for a hidden network.
3. Submit your password locally; never send it to the developer. Confirm neither password is in serial output or returned HTML/JSON. If desired, first use an intentionally wrong password: within about30s the page should report failure, AP remain, stored0 and HOME still work. Enter the correct password; expect success, STA CONNECTED and AP disappearance after about2s. If the phone loses the AP before reading success, use `W` as the authoritative check and rejoin your normal Wi-Fi. Check nonempty valid IPv4, sensible RSSI, stored1, provisioning0. Verify the setup page is unavailable through REMOTE's home-LAN IP.
4. Dismiss the card when present and check unchanged HOME controls/Mock intents, icon/track updates, all carousel directions/snapping, brightness, touch, and capability/disabled-action behavior. Use `m` and `B` to confirm owner/resource counters and hold/ADC/GPIO8 state. Allow normal30s DISPLAY_IDLE, then motion/touch wake while the network stays connected.
5. Reboot without PWR or erasing flash: stop the monitor and use the installed esptool's read-only chip query with USB reset, then reopen the monitor:

   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --chip esp32s3 --port COM3 --before usb_reset --after hard_reset chip_id
   pio device monitor -p COM3 -b 115200
   ```

   This resets the board/queries its chip; it does not upload or erase. Expect saved1, no provisioning card/AP, local HOME boot and automatic connection. If the port re-enumerates, select its new COM number rather than assuming failure. USB reconnection alone does not guarantee a reboot while SYS_EN holds power.
6. Temporarily turn your router/AP off. During loss, verify HOME/touch/controls/idle/wake work, `W` shows CONNECTING/RETRY_WAIT with stored1 and provisioning0, and retries/backoff do not busy-loop or reset. Restore the router; allow up to the current60s backoff plus a30s connection attempt. Expect automatic CONNECTED/IP/RSSI, stored1 and no AP. Expected disconnect/deadline errors may increase; credentials must not be cleared.
7. Exercise deliberate re-provision: unarmed `Y` must be refused. Send `G`, wait for acknowledgement, then `Y`. Expect only Wi-Fi erasure, new AP key/session and provisioning; Core/Mock/battery/other persistent state remain intact. Provision again and verify AP stops. This also checks repeated server/radio/scan lifecycle without another flash.
8. Leave this same firmware running for at least30 minutes including normal USB-to-battery handover and repeated idle/motion/touch wakes; no PWR action is needed. After reconnecting USB, record `W`, `m`, `B` and retained POWER_RESET history. Check no new reset/watchdog/panic/freeze, Core ownership/accounting/queue failures or network allocation/queue failures; compare stack/free/min/largest memory to baseline, particularly under provisioning and scanning. Report durations, functional results and telemetry **without either password**. Stage2A remains pending until these physical results are supplied; Stage1's separately documented remaining acceptance evidence is not automatically satisfied by this test.

## Exact changed files

New: `components/network_manager/CMakeLists.txt`, `model.cpp`, `runtime.cpp`, `web.cpp`, `ui.cpp`, `internal.hpp`, `include/network_manager/model.hpp`, `include/network_manager/api.hpp`; `tests/control/network_tests.cpp`; this report.

Updated: `main/main.cpp`, `main/CMakeLists.txt`, `components/control_home/runtime.cpp`, `components/control_home/CMakeLists.txt`, `tests/control/run_tests.py`, `CHANGELOG.md`. No other firmware/configuration/frozen-model file is changed.
