# TUNER//01 requirements from REMOTE//01

2026-10-07 — canonical REMOTE-side requirements handoff. Documentation/specification only; Native API v1 remains UNAPPROVED / UNFROZEN / NOT IMPLEMENTED at the audited baseline.

## 1. Purpose and authority

This document defines what the separate TUNER project must eventually provide to support REMOTE directly and safely. MUST denotes a required future contract; it does not certify current implementation. This document does not select wire schemas, endpoints, transport, authentication or a firmware implementation.

Precedence: explicit owner decisions establish product scope; [frozen CCM 1.0.0](COMMON_CONTROL_MODEL_V1.md) governs common semantics; [product decisions](REMOTE01_PRODUCT_DECISIONS.md) and [control architecture](CONTROL_ARCHITECTURE.md) establish responsibility boundaries. The [Native API candidate](TUNER_NATIVE_API_V1.md) supplies design proposals only. Any mapping mismatch requires a TUNER correction, explicit adapter contract, or unavailable capability, never relaxation of CCM.

REMOTE baseline: `C:/Users/RYZEN/Documents/REMOTE01/Code`; no Git metadata, so branch/HEAD unavailable. TUNER evidence: `OviOneKenoby/ESP32-WROVER-Internet-radio`, main, commit `371cdebce7ba2648ea71636d5bdb5d738b42e680`, firmware 1.1.0. Read-only audit checkout: `C:/Users/RYZEN/Documents/TUNER01-audit`. Evidence references below use:

- **A**: `docs/TUNER_IMPLEMENTATION_AUDIT.md`, sections A–L and defects B01–B11.
- **S**: `docs/TUNER_REMOTE_SYSTEM_ARCHITECTURE.md`.
- **M**: `docs/TUNER_REMOTE_CAPABILITY_MATRIX.md`.
- **P**: `docs/TUNER_REMOTE_IMPLEMENTATION_PLAN.md`.

These four documents are external baseline records, not files in this REMOTE checkout. Carry their exact baseline with this handoff; their source findings are not new hardware validation.

Every statement is classified under these categories:

| Category | Meaning |
| --- | --- |
| VERIFIED CURRENT TUNER IMPLEMENTATION | Audited source behavior at the exact baseline; not a Native support badge. |
| REMOTE PRODUCT REQUIREMENT | Required product behavior, independently of current implementation. |
| REQUIRED TUNER CHANGE | Missing or defective behavior needed to satisfy this handoff. |
| OPEN DESIGN DECISION | Choice requiring explicit design/review before implementation or API freeze. |
| DEFERRED | Outside the first conservative Native release. |

Sections distinguish evidence from obligations. All consolidated unresolved choices appear in section 24. No new executable work or hardware acceptance is established here.

## 2. Product boundary

**REMOTE PRODUCT REQUIREMENT:** REMOTE and TUNER are physically separate. TUNER owns internet-radio stream acquisition, decoding, playback/audio output, native station catalogs, Favorites/Recent and authoritative media state. REMOTE is a direct LAN client through its protocol-neutral Core; it does not decode/play radio audio.

Optional Home Assistant/openHAB clients use the same Native semantic contract. They MUST NOT become a gateway or command/state authority required between REMOTE and TUNER. Physical controls, web management and future clients converge on TUNER authority.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.A–D, M:** local MP3/AAC radio, Classic Bluetooth audio, persisted catalogs, Radio Browser, Wi-Fi and HTTP management exist. Native playback HTTP API, discovery, association, persistent Native identity, correlated commands, revisioned state, push, HA/openHAB/MQTT and unified multi-client mutation authority do not exist.

## 3. Compatibility invariant

**REMOTE PRODUCT REQUIREMENT:** direct REMOTE + TUNER operation MUST remain functional with Home Assistant absent, openHAB absent, MQTT broker absent, Matter controller absent, and no cloud dependency. DHCP changing TUNER's IP MUST NOT replace its associated identity.

Without internet, local discovery/manual connection, association where its selected bootstrap permits, state, local catalog access and supported controls MUST remain available. Actual internet-radio stream acquisition may fail; internet-dependent Radio Browser and REMOTE artwork retrieval may also be unavailable. Their absence MUST NOT disable the local control contract or masquerade as device-offline. No guarantee of offline internet-radio audio is implied.

**REQUIRED TUNER CHANGE:** test this invariant on the eventual Native implementation, including DNS/internet failure distinct from LAN reachability (A.H–K, P steps 4–6).

## 4. Device identity

**REMOTE PRODUCT REQUIREMENT:** each TUNER MUST expose a stable device identity independent of IP, friendly name, firmware/API version and truncated MAC fragment; it survives ordinary reboot and DHCP change and distinguishes multiple units. An address is a locator, not identity. Firmware version, API compatibility version, capabilities revision and device identity are separate concepts.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.E–F:** setup SSID/password use MAC fragments; fixed Bluetooth name and firmware/build IDs do not establish Native identity. No stored Native ID or identity endpoint exists.

**REQUIRED TUNER CHANGE:** define provisioning, continuity, replacement and verification rules; reject a different device at a remembered address. **OPEN DESIGN DECISION O03/O16:** final representation/allocation and reset survival. CCM registry IDs and generations remain Core-owned; Native IDs are mapped, not automatically copied into common fields.

## 5. Discovery

**REMOTE PRODUCT REQUIREMENT:** mDNS is the selected discovery direction, with manual IP/hostname fallback. Discovery returns candidate endpoints with service/API compatibility and capability hints sufficient to begin verified negotiation. Advertised identity is a claim requiring verification; discovery MUST NOT associate or authorize a client.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.E, M:** no mDNS exists; captive DNS is setup infrastructure, Radio Browser is station lookup. **REQUIRED TUNER CHANGE:** implement the reviewed advertisement and identity verification; avoid automatic selection by name when multiple candidates exist.

**OPEN DESIGN DECISION O04/O06:** service type, instance naming, TXT schema/limits, port/path and handling obsolete advertisements. Discovery hints cannot override authoritative capability negotiation. Broadcast fallback is not selected by this handoff.

## 6. Association / pairing

**REMOTE PRODUCT REQUIREMENT:** deliberate user approval binds a persisted TUNER identity and a separately verified authorization relationship. It MUST support reboot, REMOTE sleep, DHCP change, multiple candidates, explicit unpair, deliberate replacement and revocation. Stale authorization MUST NOT silently reactivate queued work. Discovery is not association; association is not authorization.

**REQUIRED TUNER CHANGE:** supply a reviewed bootstrap/authorization lifecycle and revoke obsolete executable sessions. REMOTE retains association reference/settings; TUNER owns its authorization records. Explicit replacement cannot retarget old commands or references. Wi-Fi reset and general/factory reset remain distinct; document what each retains/erases, including identities, grants and pending/history records, before exposing reset.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.H–I:** none of this exists. Bluetooth phone pairing and stored Wi-Fi credentials are not LAN association. **OPEN DESIGN DECISION O05/O16:** secret exchange, approval UI, multi-controller limits, recovery/revocation and exact erase domains; no mechanism invented here.

## 7. Capabilities

**REMOTE PRODUCT REQUIREMENT:** capabilities MUST be negotiated independently for readable state and executable operations, including availability, context, domain, side effects and feedback guarantees. Firmware name/type or presence of an internal function does not establish support. Unknown, unsupported and temporarily unavailable are distinct.

| Capability to consider | Verified baseline (A.B–E, M) | Required Native treatment |
| --- | --- | --- |
| Radio play / explicit play, pause, resume, stop | Internal functions; radio pause stops decoder task consumption, no timeshift guarantee | Truthful explicit semantics; resolve O13 before advertising. |
| Absolute volume | Internal 0..100, default 80, +/-5, BT 0..127 | Validated domain/resolution and CCM mapping; no invented precision or clamping of invalid Native requests. |
| Source selection | Radio/Classic BT local paths | Independent source catalog/selection and availability; exact subset O08/O14. |
| Next/previous | Saved wrap; discovered Next adds favorite, Previous unavailable; BT AVRCP | Advertise only genuine context-specific traversal; favorite mutation is a different action. |
| Stations / Favorites / Recent | Persisted local catalogs, mutable positions | Independently negotiated listing/activation, guarded identity and truthful Recent semantics. |
| Metadata | ICY text, BT title/artist internally | Copied context-bound observations; missing information stays unknown. |
| Bluetooth controls | Optimistic toggle; connection observable, playback not confirmed | Optional, expose only verified explicit actions/observations, O08. |
| Browse/search | Radio Browser local UI | Optional/DEFERRED advanced search; O09 sets first-release scope. |
| Availability | Cached Wi-Fi state can stale | Separate LAN reachability, authorization, source/media readiness and internet failure. |

**REQUIRED TUNER CHANGE:** version capability changes, invalidate incompatible unsent work and preserve in-flight historical evidence. A withdrawn action cannot be substituted with a toggle or another context's gesture.

## 8. Authoritative state snapshot

**REMOTE PRODUCT REQUIREMENT:** one coherent copied observation MUST carry identity, API version, Native server incarnation/session scope, state revision and freshness/context evidence. It MUST describe current source, observed playback state, actual volume and availability independently of UI mode or the last requested command.

Include current station/media identity, display name, useful codec information, title/artist where available, relevant catalog identities/revisions and optional native Favorite membership when supported. Fields need explicit validity/absence semantics; missing/unknown does not mean zero, false, stopped or empty. A coherent snapshot may truthfully contain unknown fields. No raw C++ globals, mutable pointers or array positions become protocol concepts.

Media identity/context changes MUST invalidate old metadata/artwork atomically before new context becomes visible. Native revisions/incarnations need documented causal ordering, gap and exhaustion rules; adapters cannot equate them to Core-owned generations or compare unrelated device clocks. Freshness evidence MUST distinguish acquisition from delivery and cached data from new sampling. A request nonce alone does not make old cached state fresh.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.A–D/G:** AudioPlayer, StationManager and main/UI split state; diagnostics offer partial source/state/codec/URL, not volume/current-station identity/coherent revisioned media snapshot. **REQUIRED TUNER CHANGE:** copied synchronized publication and truthful state after failed play, stream end, pause and BT transitions. O12/O13 resolve observation semantics.

## 9. Commands and results

**REMOTE PRODUCT REQUIREMENT:** operations MUST be explicit, validated and correlated to immutable identity/session/context and arguments. No play/pause toggle may substitute for explicit desired state. Define admission separately from execution/completion; HTTP success, decoder-start request and AVRCP emission cannot imply audible playback or confirmed target state.

Validation MUST reject malformed, unsupported, unauthorized, stale and out-of-domain requests deterministically. Check identity/catalog/media/authorization guards at the authoritative execution boundary, not only before queueing. A stale selection MUST NOT be rebound to another index or item by name.

Duplicates MUST NOT execute again: same correlated key/envelope returns retained evidence; key reuse with different content refuses; expired/unknown history cannot become a new executable command. Define bounded result retention and failure behavior, including reboot/session loss. Native quotas/durations remain O10/O11; CCM's Core bounds are not automatically server quotas.

After ambiguous delivery or execution, report uncertainty and permit reconciliation; never blind replay. A readback or timeout alone cannot prove an old command will never execute. Any advertised cancellation/fence must state its exact past/future and affected-resource coverage. Late correlated evidence may resolve historical effects without updating current fields, reopening old scheduling or resurrecting purged results (CCM sections 11–13).

**REQUIRED TUNER CHANGE — A.H–K:** correlation, ledger, guards and result contract are absent. No exact key encoding, durable ledger layout or new wire error enumeration is frozen here (O10/O12).

## 10. Multi-client ordering

**REMOTE PRODUCT REQUIREMENT:** physical TUNER controls, existing web manager, REMOTE, optional HA/openHAB and future clients MUST pass through one authoritative mutation/order boundary. Clients cannot become independent unsynchronized writers. Snapshot publication must reflect committed state and declared shared audio resources.

Define ordered admission, execution, context/catalog mutation and result publication, including async decoder/BT callbacks, revocation, failed persistence and client disconnect. This does not require a particular FreeRTOS task/queue or total ordering across unrelated devices.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.A/G:** web/physical handling shares Arduino loop today, but audio-task writers/getters have synchronization gaps. **REQUIRED TUNER CHANGE:** integrate all relevant writers, preserving working decoder lifetime synchronization; do not add playback routes that call mutable methods independently.

## 11. Stations and catalog identity

**REMOTE PRODUCT REQUIREMENT:** saved stations, Favorites, Recent and any exposed directory results MUST have catalog-scoped durable item identity and versioned membership/order/selection evidence. Mutable array index MUST NOT be the sole durable external identifier. Reorder/delete/replacement cannot select a different station through an old reference. Empty traversal must refuse safely.

A URL is a stream locator, not automatically durable identity; URL changes, duplicate URLs, alternate codecs and recreated entries require explicit policy. Consider Radio Browser UUID as an input, not a mandated universal ID: the current implementation does not retain it. Complete versus partial enumeration MUST be distinguishable; omitted partial results do not prove deletion. Native catalog revisions map to, rather than replace, frozen Core catalog-generation rules.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.C/G:** saved 15, Favorites 10, Recent 5; deletion/MRU shifts indexes. No stable IDs/revisions/guards; no UUID retained. Recent is discovered-attempt MRU, not all successful playback history. **REQUIRED TUNER CHANGE:** safe identity/guards and explicit traversal context. **OPEN DESIGN DECISION O07/O14:** durable policy, duplicate/replacement semantics and station mapping as content/channel; the candidate's CONTENT mapping remains a proposal.

## 12. Favorites and Recent ownership

**REMOTE PRODUCT REQUIREMENT:** TUNER remains authoritative storage owner. REMOTE reads/presents/activates native station/Favorite items and, if negotiated, Recent; it MUST NOT build a duplicate station/Favorite database. REMOTE association/settings and artwork cache are separate storage responsibilities.

The proposed minimum includes station/Favorite listing and activation. Favorite add/remove/edit, Recent clear and richer station management are optional and require negotiated capabilities, guarded mutations and truthful persistence results; no new mandatory editor is inferred. First-release Recent exposure/meaning is O09/O14.

Frozen CCM `DEVICE_NATIVE` favorites and `REMOTE_RECIPE` are distinct logical domains. TUNER favorites do not become remote recipes; model support for recipes does not mandate a REMOTE radio database.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.C/D:** local persistence exists, current HTTP lists saved/Favorites and deletes; no Recent HTTP export or Native activation. Persisted counts/index fields do not prove atomic transactions or successful writes.

## 13. Media context and artwork

**REMOTE PRODUCT REQUIREMENT:** REMOTE owns active-stream artwork lookup/retrieval/cache/display. TUNER MUST supply truthful media/context identity and sufficient locator information for that work. Artwork absence is valid and cannot block independent playback/volume control.

Capture device/binding/session/media context and locator/asset identity at acquisition; reject obsolete lookup/download/decode results after station/source/incarnation change. Generation changes cannot merely relabel an old title/asset as current. Unknown correlation requires unknown/absent presentation, not mixed media.

Credential-bearing stream URLs MUST NOT leak into common display fields, logs or UI. If a private locator is required, its authorized delivery/redaction and adapter-private handling need O15 review; a sanitized public lookup key may be preferable. The legacy `/api/diagnostics` stream URL is not evidence of safe handling (A.D/F). TUNER does not currently implement artwork; the candidate's target-hosted asset scheme is not a product mandate.

## 14. Reconnect semantics

**REMOTE PRODUCT REQUIREMENT:** REMOTE sleep, Wi-Fi interruption, TUNER restart, Native session change and DHCP/IP change MUST recover identity/authorization/capabilities and reacquire full current state before enabling affected controls. Invalidate unverified catalog/media selections and cached freshness; identical verified catalog continuity may preserve references under CCM rules.

An IP change updates a locator only after associated identity verification. A new TUNER incarnation rejects old executable messages. Old replies cannot become current observations; exactly correlated historical result evidence remains usable only under frozen historical-effect rules. No blind replay of unknown-outcome commands, no extension of Core deadlines and no buffering old UI gestures for wake replay.

**REQUIRED TUNER CHANGE:** publish restart/session/gap evidence and bounded recovery behavior; repair runtime network truth/recovery before claiming availability. **VERIFIED CURRENT TUNER IMPLEMENTATION — A.G B05:** cached connection/IP can stale; no application reconnect loop is established. Credential persistence alone is not reconnect conformance.

## 15. Transport requirements

**OPEN DESIGN DECISION O01/O02/O06:** exact transport, encoding, endpoints, event versus polling profile and API port remain open. Existing HTTP/JSON does not select REST-only; historical candidate WebSocket/HTTPS/JSON proposals are not a freeze.

**REMOTE PRODUCT REQUIREMENT:** provide request/result exchange and synchronized state with bounded polling or push, causal revision/gap detection and full refresh on lost continuity. Specify client/socket/message/body limits, deadlines, backpressure, slow-client handling, disconnect cleanup and low-memory refusal without disrupting audio or admitting untracked mutations.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.D/J/L:** synchronous HTTP WebServer/ArduinoJson already exist; playback routes and push/WebSocket/MQTT do not. HTTP reuse may reduce migration work; polling adds traffic. Push adds sockets/queues/gap handling. Measure both where considered. MQTT can be an optional adapter but MUST NOT require a broker for direct REMOTE use. Audio/metadata and directory stalls must be bounded before either design is accepted.

## 16. Security requirements

**REMOTE PRODUCT REQUIREMENT:** distinguish discovery, association, authorization, transport protection, replay protection and OTA image trust. Each has its own evidence; none implies another. Native mutations MUST require the reviewed authorization contract, recheck revocation at execution and fail closed on identity/version/security mismatch.

Bound malformed inputs and abuse; sanitize errors, metadata and diagnostics; never return Wi-Fi passwords, grants or secret-bearing locators through public fields. Define whether reads require authorization and how legacy web routes coexist without bypassing Native controls.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.D–F:** existing mutations have no application auth/CSRF/replay policy; HTTPS outbound uses `setInsecure`; MAC-derived setup password is not Native trust. These are facts, not the selected future security model. **OPEN DESIGN DECISION O05/O17:** bootstrap, transport protection, replay and legacy-route policy. REMOTE no-password LAN OTA direction does not authorize unauthenticated TUNER control; OTA trust is separate/DEFERRED.

## 17. Smart-home compatibility requirements

**REMOTE PRODUCT REQUIREMENT:** Native API first. Optional HA/openHAB adapters MUST map the same identity, capability, command, result and state semantics used by REMOTE, with independent authorized client identities and multi-client ordering.

Likely mapping categories: availability, media-player state, explicit play/pause/stop, actual volume, source, station/preset/catalog, Favorites and metadata. Missing state stays unknown; connection is not playing; request accepted is not observed success. Framework-specific entities, discovery, configuration/bindings and packaging stay outside Native semantics.

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.E/K:** neither integration exists. **DEFERRED:** implementation and framework-specific validation; compatibility is a design obligation, not an earned support badge. Direct standalone operation MUST pass before optional integration can be accepted.

## 18. Resource / bounds requirements

**VERIFIED CURRENT TUNER IMPLEMENTATION — A.L:** ESP32-WROVER target with PSRAM use, 128 KiB AAC allocation, 16 KiB audio ring, 8192-byte AudioTask stack; directory/TLS/String/JSON pressure and Classic Bluetooth coexistence constraints. Directory response `reserve(24576)` is not a hard ceiling. Active configuration targets a single-app 4 MiB layout, not REMOTE's accepted 16 MiB layout.

Historical stabilization BIN 1,998,768 bytes versus inactive proposed 2,031,616-byte OTA slots left 32,848 bytes. This is historical arithmetic, not current v1.1.0 size, verified physical flash capacity or OTA acceptance. Current artifact/runtime margins are unmeasured by the audit.

**REMOTE PRODUCT REQUIREMENT:** measure final artifact and internal/PSRAM allocation, fragmentation, task headroom, latency and bounded clients alongside MP3/AAC+/TLS/BT. Refuse at capacity without corruption, unbounded growth, audio starvation or hidden replay. Set actual quotas only from measurements (O11), preserving allocation-failure truth. No partition or dependency change is authorized here.

## 19. TUNER prerequisites before Native mutations

**REQUIRED TUNER CHANGE:** address source-confirmed blockers relevant to each exposed path before claiming its contract. This table references A.G; it is not authority for unrelated refactoring or a substitute for the TUNER bug tracker.

| Finding | Contract prerequisite and evidence |
| --- | --- |
| B01 | Strict delete parsing/range/stale guard; malformed suffix and narrowing cannot delete another item. |
| B02 | Resolve actual pause/resume semantics; test stream continuity and report truth, not mute-comment assumptions. |
| B03 | Preserve actual AAC codec when adding discovered favorite, including URLs without `.aac`. |
| B04 | Durable identity after shifts; empty-list traversal safe; UI cursor cannot become current media truth. |
| B05 | Authoritative link/IP/availability and tested recovery after loss. |
| B06 | Absolute-volume and +/- operations cannot underflow 1..4 to 100; validate before conversion. |
| B07 | Failed play/teardown cannot leave false PLAYING/source; separate historical locator from current validity. |
| B08 | All relevant writers/getters share coherent synchronization/owner semantics. |
| B09 | BT connection/AVRCP emission cannot fabricate observed playback; expose only verified controls. |
| B10 | Bound truncated/stalled ICY metadata reads so stop/state/diagnostics cannot wait indefinitely behind audio mutex. |
| B11 | Check task creation and persistent-write/commit results; failed storage cannot report success. |

Existing previously fixed decoder teardown/Recent alias/BT codec bugs MUST NOT be reopened as current defects. Preserve working audio and regress local controls/web alongside Native correctness.

## 20. Minimum Native API v1 subset

**REMOTE PRODUCT REQUIREMENT / REQUIRED TUNER CHANGE:** proposed smallest useful release: verified identity/API compatibility, independently negotiated capabilities, coherent current state, saved station and native Favorite listing/guarded activation, explicit play, truthful pause/resume and stop, validated absolute volume, correlated results, mDNS/manual access, deliberate association/revocation and reconnect reconciliation.

This is a proposed release subset, not an API freeze. Resume may map to explicit play when paused if the reviewed semantics permit; no new CCM operation is introduced. Station selection semantic role, pause behavior, stop side effects and exact source scope remain O13/O14. Next/previous, Recent, Bluetooth and search are independently optional until their safe contract is reviewed. No action is advertised merely to complete a HOME control row.

Before freeze, review both mapping directions against CCM: types/units/representability, absence/freshness, context/selection guards, outcomes, dedup/retry/barriers and restart. Unsupported mappings remain unavailable rather than modifying CCM.

## 21. Deferred functionality

**DEFERRED:** OTA writer/transfer/trust/rollback and partition work; MQTT adapter; HA-specific discovery and openHAB binding details; advanced directory search; richer artwork/asset schemes; unsupported capabilities and other ecosystem implementations. Bluetooth Native exposure is optional pending O08, not a requirement to reproduce phone controls. No deep-sleep, SD, encoder, production UI or unrelated REMOTE milestone is authorized by this handoff.

## 22. TUNER implementation acceptance checklist

Carry this section with sections 1 and 24 into the separate TUNER development chat. For each item record one status from **NOT STARTED / IMPLEMENTED / SOFTWARE VERIFIED / HARDWARE VERIFIED**, evidence links, exact commit/artifact, supported subset and omissions. Initial status below means Native-contract verification has not started; it does not deny existing audited internal functionality. No status is promoted by this documentation milestone.

1. **Correctness prerequisites:** close applicable B01–B11 for exposed paths, preserving audio/local/web behavior. Evidence: trigger-specific deterministic regressions, failure injection and MP3/AAC+/BT/local/web hardware regression. Status: NOT STARTED.
2. **Stable identity/versioning:** distinguish units/name/IP/version, survive reboot/DHCP, refuse impostor/replacement at old locator. Evidence: persisted identity tests, version mismatch/refusal, two physical TUNERs and reboot/IP changes. Status: NOT STARTED.
3. **Discovery/manual fallback:** reviewed mDNS metadata identifies candidates only; manual locator negotiates same API. Evidence: service/schema tests, multiple candidates, stale advertisements and discovery unavailable. Status: NOT STARTED.
4. **Association/authorization:** deliberate approval, persistence, reconnect, unpair/replacement/revocation and reviewed reset domains. Evidence: authorized/unauthorized/revoked client tests, no automatic trust from discovery, reboot/sleep recovery. Status: NOT STARTED.
5. **Capabilities:** independent fields/actions/domains/context/availability, versioned withdrawal, optional features absent safely. Evidence: descriptor/mapping vectors and withdrawal during queued/in-flight work. Status: NOT STARTED.
6. **Coherent state:** truthful source/playback/volume/media/catalog/freshness under failed play, stream end, pause/BT and concurrent changes. Evidence: immutable snapshot and causal-order tests; physical observed playback comparison, no UI-mode substitution. Status: NOT STARTED.
7. **Commands/results:** explicit operations, admission distinct from execution, immutable correlation, invalid inputs and stale guards. Evidence: request/result vectors, async failure/delayed completion and unsupported action refusal. Status: NOT STARTED.
8. **Duplicates/uncertainty:** bounded retention, expiry/key mismatch/reboot behavior, no blind replay or fabricated no-execution proof. Evidence: lost replies, duplicate/late/out-of-order commands, capacity/purge and historical-barrier reconciliation vectors. Status: NOT STARTED.
9. **Catalog safety:** durable IDs, complete/partial enumeration, revisions, reorder/delete/empty traversal, codec and stale-reference safety. Evidence: catalog mutation/activation tests, concurrent deletion and duplicate URL/name cases. Status: NOT STARTED.
10. **Persistence ownership/results:** station/Favorite/Recent stay on TUNER; exact Recent semantics declared; failed writes report failure. Evidence: reboot/interrupted-write/fault tests, persisted codec/IDs and no REMOTE duplicate database. Status: NOT STARTED.
11. **Media/artwork context:** truthful private locator/context, sanitized public data, obsolete lookup results discarded by REMOTE. Evidence: station/source change during delayed artwork and credential-redaction tests; absent artwork remains usable. Status: NOT STARTED.
12. **Reconnect:** REMOTE sleep, network loss, TUNER restart/session/IP change trigger verified reacquisition, no buffered replay. Evidence: physical sleep/wake and router/TUNER restart tests with old samples/results/selections injected. Status: NOT STARTED.
13. **Multi-client order:** local physical, web and Native mutations share authority with truthful synchronized snapshots. Evidence: interleaving/async callback stress and physical concurrent client tests; no unsynchronized writer bypass. Status: NOT STARTED.
14. **Security/bounds:** approved identity/auth/replay/transport/legacy coexistence model; malformed/oversized/slow/unauthorized clients refuse safely. Evidence: adversarial tests, revocation-at-execution and secret-redaction review; exact selected profile documented. Status: NOT STARTED.
15. **Resource envelope:** measured bounded memory/sockets/tasks/history/latency with MP3/AAC+/TLS/BT and allocation failures. Evidence: exact artifact sizes, runtime high/low watermarks, soak/load/slow-client/disconnect plateau and audio regression. Status: NOT STARTED.
16. **Standalone invariant:** direct REMOTE/TUNER with HA/openHAB/MQTT/Matter/cloud absent, DHCP changed and internet unavailable except stream-dependent behavior. Evidence: owner end-to-end controls/state/catalog test, truthful stream failure, continued local control and network recovery. Status: NOT STARTED.
17. **Frozen CCM mapping:** verified supported subset passes common conformance without modifying CCM or copying Native epochs into Core generations. Evidence: versioned adapter vectors, context/freshness/quantization/late-result/barrier cases and exact hashes. Status: NOT STARTED.
18. **Optional smart-home compatibility:** same API semantics for HA/openHAB including concurrency and reconnect while direct control remains independent. Evidence: integration tests and separate physical acceptance where claimed. Status: NOT STARTED; implementation/validation DEFERRED until optional adapters are authorized. Design compatibility remains required.

## 23. REMOTE-side obligations

**REMOTE PRODUCT REQUIREMENT:** REMOTE owns the protocol-neutral Core, durable association reference/settings, discovery client/manual fallback and approval UI, bounded Native adapter, reconnect/stale-response rejection, artwork lookup/cache/display, UI/local-control intents and sleep lifecycle. UI never directly mutates TUNER state or bypasses Core.

Persist mandatory Core history/non-reuse/barrier evidence before executable handover as required by CCM; a persisted last-TUNER setting is not that journal. Map verified Native identities, revisions, domain/absence/freshness and historical outcomes without manufacturing guarantees. Clear stale presentation, consume first asleep touch for wake only, preserve motion-wake-only semantics and reacquire target state after network resume. TUNER does not own REMOTE sleep or UI.

Next REMOTE milestone: **Native API v1 binding/security/resource-profile reconciliation and mapping review using this handoff plus the audited TUNER baseline**, with explicit resolution of section 24 before proposing API freeze. Adapter/association/discovery firmware comes only after separately approved protocol and implementation scope. In the separate TUNER chat, the prerequisite implementation milestone is the control/state correctness and ownership foundation (P step 1).

## 24. Open decisions

**OPEN DESIGN DECISION:** all choices below remain unresolved by this milestone. Existing candidate preferences are options, not approvals; resolve affected gates before advertising support.

| ID | Choice | Required decision/evidence |
| --- | --- | --- |
| O01 | Native transport/serialization | Reviewed framing/schema/compatibility and measured HTTP/push alternatives; no REST/WS/HTTPS/JSON freeze here. |
| O02 | State synchronization | Poll/push cadence, sequences, gaps, full resync and acquisition-age proof. |
| O03 | Device-ID representation | Allocation, verification, collision/non-reuse and persistence; independent of names/IP/MAC fragments. |
| O04 | mDNS profile | Service/TXT/instance names, lengths, compatibility hints, expiry and discovery-to-negotiation behavior. |
| O05 | Association/authentication | Approval/bootstrap, grants/secrets, multiple controllers, revocation/replacement/recovery. |
| O06 | API port/path layout | Binding/discovery endpoints and coexistence with current port80 portal/web; no proposed path accepted here. |
| O07 | Station durable-ID policy | Saved/Favorite/Recent/directory identities, URL changes, duplicates, UUID applicability, migration and delete/recreate. |
| O08 | Bluetooth Native scope | Verified explicit actions/state/feedback; unsupported controls remain absent. |
| O09 | Browse/Recent/search release scope | Minimum export/activation versus optional/deferred browsing and mutations. |
| O10 | Command result retention/recovery | Key/correlation encoding, dedup/expiry/ledger/restart guarantees, past/future proof and measurable limits. |
| O11 | Resource/client bounds | Measured sockets, bodies, queues, timeouts, rates, history, memory/stack and failure/backpressure quotas. |
| O12 | Native state/revision contract | Snapshot causality/incarnation/context/capability revisions, freshness, clock separation and exhaustion handling. |
| O13 | Playback operation semantics | Radio pause/resume continuity, explicit play and stop effects; truthful observation versus emission. |
| O14 | Source/station/traversal mapping | Content/channel/source role, guard/resource domains, context-specific next/previous and Recent attempt/success semantics. |
| O15 | Artwork/locator boundary | Authorized private locator or sanitized lookup key; formats/cache/retrieval/privacy limits remain REMOTE design work. |
| O16 | Reset/persistence domains | Identity, grants, catalogs, association/cache and command-history retention for each deliberate reset; interrupted writes/migration. |
| O17 | Security/legacy coexistence | Transport protection, authorized reads/mutations, replay/abuse/CSRF policy and old web-route bypass prevention; OTA trust separate. |

Required product boundaries, standalone operation, TUNER persistence ownership, REMOTE artwork ownership and mDNS direction are already decided; they are not reopened by this table. Native API freeze, implementation and hardware compatibility remain future gates.
