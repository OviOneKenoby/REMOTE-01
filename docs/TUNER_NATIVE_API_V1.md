# Canonical decision overlay - 2026-10-05

> **Current evidence overlay ? 2026-10-07:** the separate TUNER v1.1.0 source audit at main `371cdebce7ba2648ea71636d5bdb5d738b42e680` now supplies implementation evidence. Use [canonical REMOTE-side TUNER requirements](TUNER_REQUIREMENTS_FROM_REMOTE.md), sections 1?24, for the current evidence/requirements boundary and handoff. Historical ?no implementation evidence?, all-support-UNKNOWN and next-read-only-audit statements below describe the pre-audit review only. Internal radio/volume/catalog functions and HTTP management are now verified; the Native playback API, stable Native identities, association, correlation and revisioned authoritative state remain absent. TUNER uses audited Arduino 2.0.17 / ESP-IDF 4.4.7, not this candidate's REMOTE IDF 5.5.3 feasibility assumptions. REMOTE-owned artwork remains the product boundary. Preserve the candidate as UNAPPROVED / UNFROZEN: its transport, serialization, security, identity and hosted-asset proposals are not adopted by this overlay. Frozen CCM is unchanged.

[Owner register](REMOTE01_PRODUCT_DECISIONS.md), items10-12/28-34: mDNS discovery DECIDED; manual IP/hostname fallback allowed. Proposed service type/TXT/ports/endpoints/JSON and WebSocket+HTTPS details below remain candidate material. Actual TUNER protocol UNKNOWN / NOT RECOVERED; transport DESIGN REQUIRED after verified target source/capabilities. This overlay DOES NOT approve/freeze the Native API or claim server support.

Product persistence: TUNER favorites/recent stations; REMOTE settings/last associated TUNER/artwork cache, no REMOTE station/favorite database. REMOTE owns active-stream-URL artwork lookup/retrieval/cache/display with context safety; exact retrieval/source/format/cache policy DESIGN REQUIRED. Candidate target-asset sketches do not require target-owned artwork or override this product boundary. Native control authorization remains a separate protocol design gate; no-password LAN OTA policy does not change it.

The complete historical candidate below is preserved verbatim. Its former mDNS-only-as-proposal status is superseded at discovery-method level only; all other unapproved candidate details remain unapproved. No frozen CCM amendment.

---

# TUNER//01 Native API v1 — specification candidate

2026-10-03. Revision: **1.0.0-candidate.1 — UNAPPROVED / UNFROZEN**. Depends on [Common Control Model (CCM) 1.0.0 — OWNER APPROVED / FROZEN](COMMON_CONTROL_MODEL_V1.md). Candidate rules below describe a future contract, not existing TUNER behavior. No server, adapter, networking or control implementation is established.

Evidence classifications used throughout:

- **VERIFIED FROM EXISTING TUNER PROJECT EVIDENCE (VERIFIED):** requires a versioned TUNER source/contract or recorded test available here. **No qualifying TUNER implementation evidence was found in this repository.**
- **DESIGN REQUIREMENT FOR FUTURE TUNER IMPLEMENTATION (DESIGN REQUIREMENT):** proposed behavior in this candidate; implementation and verification remain future work.
- **UNKNOWN — REQUIRES TUNER PROJECT VERIFICATION (UNKNOWN):** an actual firmware fact cannot be established here. UNKNOWN never grants support.

All candidate protocol rules, records, examples and proposed mappings carry DESIGN REQUIREMENT status unless marked UNKNOWN. This convention applies to every section/table, not merely the capabilities table. SDK documentation establishes possible platform mechanisms, never existing TUNER support. MUST/MUST NOT express candidate requirements; they do not amend frozen CCM.

Repository evidence inspected before drafting: [architecture](CONTROL_ARCHITECTURE.md), CCM §20, [CHANGELOG](../CHANGELOG.md), [README](../README.md), [hardware evidence](HARDWARE.md), `components/user_app/user_app.cpp` static HOME literals, `main/main.cpp`, `platformio.ini`, and repository-wide TUNER/yoRadio/API text/file searches excluding build output. Architecture §7 explicitly calls TUNER entries design directions; §5 leaves station semantics unresolved. HOME's CONNECTED, volume 68, PLAYING and Radio Paradise are samples, not radio observations. REMOTE's Waveshare Wi-Fi/audio sources and ESP-IDF version are not TUNER's stack. No separate radio repository was opened, read or modified.

## 1. Purpose and scope

DESIGN REQUIREMENT: direct local REMOTE//01 ↔ TUNER//01 control over the LAN, without Home Assistant, Matter infrastructure, cloud, internet or an external automation hub. Internet-radio streaming may need internet independently; a streaming outage must not disable local control or mean the local API is unreachable.

Scope is a typed capability/command/result/state/catalog/asset contract for a future native adapter. Static HOME is unchanged. Existing playback, power, volume, station or API behavior is UNKNOWN. No mandatory product capability is inferred from the word TUNER; a read-only or empty supported subset is valid.

## 2. Layering

`REMOTE UI -> Universal Control Core -> CCM 1.0.0 contract -> TUNER Native Adapter -> Native API -> selected transport -> TUNER device implementation`.

CCM is the Core/adapter contract, not a network hop. UI emits neutral intent and reads common snapshots/results; it never sees native session IDs, paths, tokens, numeric registers or auth material. Core owns registry identities, common generations, validation, immutable captures, lifecycle folding, deduplication and ordering. Adapter translates private native identity/revisions, arguments and evidence; transport reports connection/emission facts. Server validates and serializes target work and reports only evidence it can establish.

Wire tokens must not become common IDs by assumption. Persisted verified associations map native identities to registry-owned device/target/binding IDs, native items to common item IDs, and native command keys to captured Core attempts/steps.

## 3. Protocol requirements and resource boundaries

DESIGN REQUIREMENT: bounded messages/queues/connections, explicit capacity refusal before execution, deterministic typed parsing and serialized target decisions. Native U64 serials/revisions/sequences start at 1 and never wrap; exhaustion stops new mutation/work until a new verified scope/identity is established, without forgetting historical effects. Handle Wi-Fi loss, reconnect, duplicate/delayed/reordered messages, both reboots, stale caches and version evolution. No automatic executable retransmission at any layer by default; a read-only resync is distinct from retrying an action.

Active execution/evidence/barrier records cannot be silently evicted. Where memory cannot preserve a required guarantee, refuse new work or keep the capability UNKNOWN/unavailable; never weaken CCM to fit RAM. Product profiles must declare and measure maximum message bytes, connections, outstanding commands, catalog chunks, asset decode memory, result retention and freshness/heartbeat timing before implementation approval. Their exact values are OPEN; this candidate does not invent RAM/flash availability. CCM limits (including 1024 catalog items per logical snapshot, three attempts per ordinary request and 60000 ms maximum Core deadline horizon) remain authoritative; wire profiles may narrow supported domains.

One device-side execution owner must arbitrate all API and non-API writers. Networking callbacks must not bypass it. Current TUNER task ownership is UNKNOWN.

## 4. Transport evaluation and decision

**Final transport selection: OPEN.** The leading design option is authenticated WebSocket for commands/results/state, with HTTPS for bootstrap and separate assets. This is a recommendation to evaluate, not a selected or implemented TUNER transport. If it fails measured stack/resource/security gates, revise this candidate before approval; no unreviewed alternate wire path.

| Option | Push / command latency / reconnect / framing | Duplicate handling / cost / security / discovery / evolution |
| --- | --- | --- |
| HTTP request/response | Straightforward commands/snapshots; push needs long polling or another stream. Reconnect uses fresh requests; HTTP supplies framing. Poll frequency trades latency and load. | Application command keys still required. Familiar debugging and schema evolution. Repeated headers/connections and TLS need measurement; persistent connections may help. Advertise endpoint via discovery; authenticate every executable request. |
| WebSocket | Bidirectional push and commands on one connection; standard message boundaries. Reconnect needs new application session and snapshot; connection order is not order across reconnects. | Application dedup remains mandatory. ESP-IDF exposes server support; persistent socket, buffers, TLS and client count consume resources. JSON is inspectable. Authenticate upgrade and messages; discovery only locates server. Version handshake stays explicit. |
| UDP | Datagrams frame bounded messages; loss/reorder/duplication expected. Push possible, but reliability, reconnect domains and congestion must be supplied. Low transport overhead does not establish lower total cost. | Would require application reliability/authentication/reassembly and stronger evidence of safety; no custom crypto. Untrusted broadcast cannot authorize control. No compelling project need presently justifies this complexity. |
| Raw TCP | Bidirectional ordered byte stream within a connection; application length/framing and reconnect/resync needed. Avoids HTTP headers but requires bespoke tooling. | Dedup and session semantics still required; sockets/TLS/buffers need measurement. Security/discovery not supplied by TCP. Future evolution possible with explicit envelope, but custom framing increases maintenance. |
| MQTT | Push and commands via broker topics; acknowledgements/QoS do not prove target completion. Reconnect and retained/outstanding messages require stale-intent rejection. | ESP-MQTT is a client communicating with a broker. An external broker violates the direct-control requirement without a compelling reason; embedding one adds an unverified device burden. Topics/framing/debug tooling cannot replace command keys/auth/context guards. No such reason exists here; not selected. |

Platform evidence only: ESP-IDF 5.5.3 documents [HTTP/WebSocket server support](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32/api-reference/protocols/esp_http_server.html), [HTTPS and WSS server examples](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32/api-reference/protocols/esp_https_server.html), and [lwIP socket APIs](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32/api-guides/lwip.html). [ESP-MQTT 5.4 documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/protocols/mqtt.html) describes broker/client operation. These versions support feasibility review only; TUNER's SDK remains UNKNOWN. All comparative cost/latency judgments above are design inferences; no byte counts or benchmarks are claimed.

Transport-selection gate: verify actual TUNER server/library/version, TLS facilities, concurrent playback cost, free/peak heap and flash, reconnect behavior, client count and debug access. No endpoint path, port or transport is advertised as existing.

## 5. Discovery

DESIGN REQUIREMENT: mDNS primary, manual endpoint configuration fallback. Espressif documents [service/query/TXT mechanisms](https://docs.espressif.com/projects/esp-protocols/mdns/docs/1.5.0/en/index.html); current TUNER mDNS support is UNKNOWN.

Proposed service type `_tuner-native._tcp` is proposed by this candidate for evaluation if a TCP-based binding is selected, not an existing advertisement or assigned standard. SRV address/port plus bounded TXT hints: `api_major`, `binding`, `bootstrap_path`, `pairing_required`, optional `identity_hint`. Exact TXT encoding/lengths and path are profile OPEN. Resolve authoritative identity/version/targets after authentication; TXT is never capability evidence or trusted identity.

Discovery produces provider, endpoint hints, observed name, expiry, API/binding hints and onboarding requirements. Verified authenticated identity/target records subsequently enable CommonDevice/Target/Binding creation. Refresh address/expiry without replacing persistent common identities. Same name/IP never merges devices. Manual IP/hostname is a route locator and follows identical authentication checks.

Broadcast fallback is **not selected**: no evidence shows mDNS plus manual configuration insufficient. If later justified, separately review bounded traffic/framing/ports/authentication; do not make discovery packets executable.

## 6. Stable identity

DESIGN REQUIREMENT: authenticated `identity = {native_device_id:Id, targets:[{native_target_id:Id, display_name:String}], display_name:String, firmware_version?:String}`. Both native IDs obey the exact CCM OpaqueId lexical/length domain, are persistent, non-reused and survive DHCP/reconnect/ordinary reboot. Factory reset or identity loss is an explicit replacement requiring owner reconciliation, never silent reuse.

Identity source, provisioning and existing TUNER identifier are UNKNOWN. MAC, hostname, station name and display name are not chosen as identity. A provisioned persistent ID bound to authenticated pairing evidence is a future option, not an existing fact. Device identity and cryptographic identity continuity must be verified together; conflicting assertions quarantine the route. Each target is associated independently; do not invent a zone/player layout.

## 7. Independent version negotiation

DESIGN REQUIREMENT: negotiation exchanges Native API versions (candidate suffix included when applicable), supported CCM versions and operation/field subsets, plus optional firmware version. This candidate API is `1.0.0-candidate.1`; it must not advertise frozen API v1. CCM offered here is frozen `{major:1,minor:0,patch:0}` without candidate.

Same candidate API revision requires exact equality. A future frozen API accepts exact major plus an explicitly negotiated compatible minor/subset; patch cannot alter behavior. Unsupported major/candidate mismatch prevents control readiness. No compatible CCM version/subset prevents binding creation as a usable control mapping; Core requests currently carry exact CCM 1.0.0 per frozen §10.

Unknown operations are rejected, not coerced. Missing capabilities mean UNKNOWN. Closed executable argument/envelope records reject unknown fields. Additive diagnostic/observation fields may be ignored or retained privately only when declared optional and not needed to interpret existing semantics; unknown required semantics invalidate the relevant mapping. Never reinterpret unknown enum values, units or versions. Firmware version is diagnostic evidence, not protocol negotiation.

## 8. Sessions, reconnect and historical evidence

DESIGN REQUIREMENT: server supplies a non-reused `server_incarnation:Id` on each reboot or loss/reset of execution/sequence decision state. Each authenticated connection creates a non-reused `session_id:Id`; replaced sessions lose executable authority before a replacement becomes ready. Client uses a non-reused `controller_incarnation:Id` per REMOTE restart, bound to its authenticated principal. No wall clock is involved. Allocation/persistence mechanisms are UNKNOWN and must be verified.

All session messages carry both server incarnation and session ID. Executable messages also carry controller incarnation. Mismatch rejects before executable handover. Server restart rejects old-incarnation commands even if bytes arrive later; controller restart cannot recreate old intent. A new authenticated connection performs identity/version/capability checks and verified state resync.

Adapter increments persistent common binding session_generation on creation/reconnect/sequence-domain reset, independently of native IDs. Core clocks, native clocks and counter scopes are never equated. Disconnect invalidates old current observations and unsent captures under CCM. Server session replacement must cancel unstarted old-session work; already-started work keeps its evidence and may still execute. Revocation requires another atomic authorization check at execution.

Old-session packets cannot update current fields. Read-only `result.lookup` on a new authorized session may request an original command key, with response explicitly preserving original incarnation/session/target/operation and evidence scope. Adapter may use verified historical proof to resolve an old attempt/barrier under CCM §§11-12, including after result-cache purge; it cannot reconstruct scheduling or a purged result. Reconnect/reboot alone never proves no past/future execution.

## 9. Capabilities and readiness

DESIGN REQUIREMENT: authenticated `capabilities.get` returns each target's `capability_rev:U64`, independent operation and field entries, support SUPPORTED/UNSUPPORTED/UNKNOWN, availability, closed typed schemas, units/domains, context guards, feedback/completion definition, affected resources, numeric mapping and field freshness/acquisition contract. Native revisions are scoped by server incarnation and target; adapter translates changes into common capability_revision/mapping_revision rather than copying numbers.

SUPPORTED requires both actual server support and a complete verified CCM mapping. This candidate alone supplies PROJECT_DESIGN evidence, which cannot verify external support. READY additionally requires identity, authorization, negotiated versions, current matching captures and required context/guard evidence. Missing entries remain UNKNOWN; explicitly unimplemented entries may become UNSUPPORTED only from verified server evidence. Offline/unavailable does not erase support.

Power, absolute/step volume, mute, individual play/pause/stop, previous/next, seek, source selection, station/content selection, favorites, each metadata field and artwork are separately negotiable. **All actual TUNER supports are UNKNOWN here.** UI samples enable none. Guard/resource/completion changes and authorization/availability changes invalidate affected captures through the corresponding common revisions. On capabilities.changed, suspend affected dispatch, retrieve descriptors and revalidate; do not silently rebase queued work. Descriptor chunks form one atomic revision; incomplete descriptors grant no support.

## 10. Command envelope, guards, deduplication and deadlines

DESIGN REQUIREMENT: one native executable envelope per actual adapter attempt/logical recipe step; Core retains its own outer request/recipe identity. No whole remote recipe is sent as an opaque server macro.

Proposed closed semantic record, serialized per §20:

`Command = {type:"command", api_version:Version, common_model_version:Version, server_incarnation:Id, session_id:Id, controller_incarnation:Id, command_serial:U64, native_target_id:Id, operation:String, arguments:Record, capability_rev:U64, guards:GuardSet}`.

Native command key = authenticated principal + controller_incarnation + command_serial. Adapter allocates strictly increasing serials without reuse and persists its immutable association to Core request/attempt/step before possible executable handover. Retransmission/duplicate lookup of the same attempt retains its key; it cannot allocate a replacement to evade dedup. A genuinely new Core-approved retry/fallback attempt, allowed only by frozen CCM policy/proof/timer/deadline checks, has a new immutable native key mapped to the same original Core request. New user intent gets a new Core request and native key. No native-key change itself authorizes retry. Core admission ticket, Core deadline clock epoch, registry IDs, route plan and lock-union internals are not transmitted.

GuardSet is a closed record {catalog?:NativeSelectionRef,media_context_id?:Id,seek_window_rev?:U64,fields?:[{field_id:String,field_rev:U64,expected:Value<T>}]} with no duplicate field IDs. NativeSelectionRef = {kind:SOURCE/CHANNEL/FAVORITE/APPLICATION/CONTENT,native_target_id:Id,catalog_id:Id,catalog_rev:U64,native_item_id:Id}. Reference arguments use this native record in the exact corresponding CCM argument path/tag; the adapter translates the full common SelectionRef privately, never omits its Core-side scope/generation validation. The catalog guard must equal the supplied argument ref and target; media/window guards must describe the same target/context. Field guards sort by exact field_id and require current verified typed evidence. Guards never permit read-then-toggle without the separately verified atomic conditional contract. Unneeded guards are absent; needed unverified guards prevent dispatch. Server compares captured capability and all supplied guards atomically at actual execution, including queued work. Never translate a common catalog generation directly into a native revision; adapter stores the verified association.

Dedup order on a current authenticated session: validate key primitives; lookup retained key; identical complete typed envelope returns its latest result without new work; changed envelope returns ID_MISMATCH. After full record eviction, compact per-principal/controller-incarnation high-watermark rejects any previously issued serial at/below it as HISTORY_EXPIRED, never new work. Higher serials consume a key on admission/refusal; skipping never-executed serials is allowed, but they subsequently cannot execute. Unique controller incarnations are retired on authenticated replacement; retired incarnations cannot admit commands. This native policy is separate from CCM's exact Core ID-ticket pairing/cache policy.

Server must retain active records and immutable correlation/effect evidence; persistence before target handover plus incarnation rotation/fail-closed recovery are requirements. Exact record/tombstone resource profile and full-result retention are OPEN. Capacity refusal must be guaranteed unsent; cannot forget unresolved work to accept more commands. No claim of global exactly-once execution, reboot completion or replay safety is made.

Server admission is serialized: authenticate principal/current executable scope; validate key primitives; run the duplicate/history decision; validate versions and closed argument/guard schema; target ownership; captured revisions; independent operation support; domain/context; capacity; atomic ledger registration before queuing or execution. For a valid new key, rejection consumes its serial even when no full record fits. Structural errors without a valid key return an error without fabricating a correlated result. Key/high-watermark persistence failure refuses execution and closes that executable scope. Before actual target handover, recheck authorization, revisions, guards, support/domain and resource ownership atomically; changed queued work never executes against rebased captures. This server order does not replace CCM's Core admission/expiry ordering.

Core owns immutable MonoTime deadline and all retry policy. Adapter checks CCM's atomic dispatch gate immediately before each executable emission; at/after deadline no new adapter work, retry or suffix is started. A connection may deliver a previously handed-over command after Core's deadline: this is possible execution of the existing attempt, not an allowed new Core attempt. Result timeout therefore never proves nonexecution. Do not put Core ticks on the wire or use a server receive-time TTL to claim execution stopped at the Core deadline. An optional future server-expiry mechanism needs a separately verified clock/expiry mapping and candidate review; none is claimed here.

## 11. Results, completion and ordering evidence

DESIGN REQUIREMENT: `Result = {type:"result", server_incarnation, session_id, original:{server_incarnation,session_id,controller_incarnation,command_serial,native_target_id,operation}, result_rev:U64, stage:RECEIVED/ACCEPTED/REJECTED/COMPLETED/FAILED/UNRESOLVED, evidence:EvidenceRecord[], error?:NativeError}`.

result_rev increases for semantic/evidence change within the original key; exact repeated payload/revision is duplicate, different payload at equal revision is contradictory and quarantined. New-session lookup has current outer scope and original inner scope. A missing/unverified original key cannot resolve a Core barrier.

| Native evidence | Common consequence, subject to captured contract and whole-request fold |
| --- | --- |
| Transport write/socket success | Local emission extent only; full -> SENT, unknown -> AMBIGUOUS, known incomplete -> PARTIALLY_SENT. No target completion. |
| RECEIVED | Verified recipient receipt may supply ACKNOWLEDGED; target effect UNCONFIRMED. Never CONFIRMED_COMPLETED. |
| ACCEPTED | Validated/queued for execution only; declared CORRELATED_RESULT remains pending. |
| REJECTED plus proved no past AND no future execution on all affected keys | Refusal may fold REJECTED, preserving delivery/progress. Bare rejection/error lacks that guarantee. |
| COMPLETED with exact command correlation, declared whole-effect criterion and no pending future execution | Supplies completion/recipient evidence for this attempt only; Core decides whole request, prefix/suffix and remaining barriers. |
| FAILED / UNRESOLVED | Exact known progress/failure or genuine uncertainty, respectively; Core folds using frozen §11. Never invented success. |

EvidenceRecord is a tagged record: RECEIPT; EXECUTION{logical_units_total,emitted_lower_bound,confirmed_lower_bound,extent_known,completion_contract_id}; ZERO_EXECUTION{covered_native_keys,no_past:true,no_future:true}; FUTURE_FENCE{covered_native_keys,no_future:true}; FAILURE{cause,known_progress}. Counts are bounded U32 and internally consistent; target unit/resource references map to captured common keys. Proof must identify the original attempt and exact covered target/resources, and be supported by a verified server contract. Unknown proof variant or contradictory proof is diagnostic only.

COMPLETED means the descriptor's effect actually occurred, not queue insertion, function invocation, network ACK, HTTP 200 or matching later state. Actual completion criteria are UNKNOWN until TUNER verification. If feedback cannot be correlated, descriptor feedback NONE permits complete local ACCEPTED/UNCONFIRMED; STRICT barrier remains until verified future-execution evidence. No fabricated feedback guarantee to avoid blocking.

Proof coverage keys are tagged TARGET{native_target_id:Id} or RESOURCE{native_resource_id:Id}, translated only through the original verified captured resource association. COMPLETED must cover the attempt's whole declared effect/resources and prove no outstanding future work for it; otherwise report limited EXECUTION evidence or UNRESOLVED, never whole completion. RECEIVED/ACCEPTED are noncompletion stages; REJECTED/FAILED/UNRESOLVED must include an error and explicit known/unknown progress. Result revision and evidence must be committed together before notification. Unknown resource continuity, contradiction or missing proof cannot release a common barrier.

Server execution owner must serialize conflicting target/resource work, including other controllers and local/web writers, and resolve asynchronous delayed work before promising a fence. A future-only fence resolves future order on covered keys; it never proves past absence, N-operation retry safety or completion. Whole zero-execution proof requires both halves on every affected key. Independent other-attempt barriers survive. No proof overwrites historical delivery/emitted counts. For compound count operations, complete transmission of one command packet does not by itself prove emission/execution extent of every logical unit: the descriptor must define verified per-unit handover/progress evidence. Unknown unit extent stays uncertain; mappings lacking such evidence narrow supported count to 1 or remain UNKNOWN. Server progress never substitutes for missing local delivery evidence, and no count suffix is replayed.

State changes are separate observations; a result does not manufacture fields. Default Core retry/fallback remains disabled. Duplicates return history, not another execution. No hidden transport/application resend after disconnect/timeout. Partial count/recipe work never restarts/resumes. Recipe progression, strict/local-only modes, supersession, retry timers and resources remain CCM responsibilities.

## 12. State synchronization, sequence and defensible age

DESIGN REQUIREMENT: `state.sync{nonce:Id,native_target_id}` establishes a subscription and a fresh acquisition at the target execution owner. Response `state.snapshot` includes current incarnation/session/target, nonce, capability revision, `stream_id:Id`, `sequence:U64`, media context, complete field entries and catalog revision hints. Subscription installation and snapshot cut are atomic: all committed changes after the cut are delivered in sequence. No read-then-subscribe loss window.

`state.update` carries the same scope, strictly increasing stream sequence, and an atomic batch of changed fields or explicit invalidations. Each field has a typed Value, current context where applicable, acquisition proof and field revision. Sequence is serialized across this subscription, not compared across sessions. Duplicate equal sequence/payload is ignored without age refresh; lower discarded; equal sequence/different payload invalidates source/latches conflict; gap invalidates current acquisition continuity, suspends context/state-dependent work and starts a new session/stream with verified RESYNC. Buffer overflow closes/resyncs rather than silently dropping a required update. Unchanged omitted delta fields retain their original observation age, not the update's receipt time.

Freshness proof deliberately avoids synchronized clocks: Core records monotonic t0 before sending an unpredictable, unique sync/probe nonce. Server may attach `acquired_after_nonce` to a field only when that field's actual acquisition occurred after server processing that nonce. At receipt t1 in the same Core epoch, t1-t0 is a conservative upper bound on sample age, including transport delay. Adapter captures nonce session/mapping/association/media scope; validates causality and bounds; supplies age_at_receipt to CCM. Delayed snapshot beyond descriptor max_age is stale, not READY truth. If a stored field was acquired earlier, it cannot claim the new nonce merely because memory was reread or the packet sent now; it remains unknown-age unless another verified original-age contract exists.

A live update may refer to the most recent valid anchor nonce only when its acquisition was actually later. Per-field acquisition revisions advance for a new verified sample, including equal values; this is distinct from retransmission of the same sample, which cannot refresh age. Adapter maps each field's source sequence to its verified stream/field scope and retains the last accepted sample independently of other delta fields. As anchor ages, this bound becomes too coarse; a bounded read-only probe/resync supplies a new anchor when needed. No source tick/UTC comparison or receipt-as-acquisition shortcut. Exact freshness policies/probe interval are profile OPEN and must keep CCM's explicit 1..60000 ms domain. Unknown-age fields can be retained privately but cannot enter current truth or state-dependent emulation.

Initial snapshot/RESYNC is complete for the negotiated field set: omitted supported fields explicitly invalidate to UNKNOWN, not previous-session cache. Per-field support/absence/confidence and actual observation availability remain independent. AUTHORITATIVE needs verified direct-target acquisition; intermediaries are REPORTED, no command-derived ASSUMED value disguised as an observation. Persistent Core conflict participants still require fresh post-conflict acquisitions from every participant; native stream restart cannot clear the Core latch by itself.

## 13. Coherent media context

DESIGN REQUIREMENT: server allocates non-reused `media_context_id:Id` on source/item/playback-session change or uncertain continuity. Every title, subtitle, artist, duration, position, playback, current content and artwork observation is tagged with that context. A transition atomically announces the new context and invalidates all dependent old fields before any new-context data is exposed. Unknown values remain tagged absence; no old title plus new artwork.

Adapter maps verified changes into Core media_context_generation and stores context associations privately. Late updates/fetches are checked against captured device/target, association, binding mapping/session, context and asset identity at completion. Obsolete data is discarded regardless of arrival order. Reconnect retains continuity only if explicitly proved; otherwise allocate a new common media generation. Seek requires current verified inclusive window and atomic target-side media/window guards; duration alone cannot establish a seek window or support. TUNER metadata/context/timeline behavior is UNKNOWN.

## 14. Catalogs and stale selections

DESIGN REQUIREMENT: optional `catalog.get{kind,query_id}` returns `Catalog = {native_target_id,kind:SOURCE/CHANNEL/FAVORITE/APPLICATION/CONTENT,catalog_id:Id,catalog_rev:U64,items:[{native_item_id:Id,label:String}],complete:Bool}` in current session/causal scope. Catalog ID is stable for that target/kind, item IDs opaque/non-reused; names/index positions are never selectors. Native revision changes on membership/order/item identity/private token-binding changes; labels alone preserve it. Association continuity and actual station/favorite IDs remain UNKNOWN.

Each catalog query ID is uniquely mapped to its captured Core enumeration attempt; a verified causally current response supplies query-result evidence, never physical action/state evidence. A confirmed Core query includes the exact translated CatalogSnapshot under frozen §11; HTTP success or an unverified catalog cannot confirm it. All chunks of one logical snapshot share query/revision and explicit final completeness; a bounded snapshot has at most 1024 items for this v1 mapping. Larger catalogs require explicitly partial subsets or a later reviewed streaming extension, not silent truncation called complete. Adapter confirms a full enumeration query only when its captured schema/coverage guarantee is fulfilled; incomplete data remains explicitly incomplete.

CCM catalog_generation is Core-owned. Complete verified snapshots replace membership/order. Partial snapshots retain omissions and relative known order, update proved metadata, append new IDs by common unsigned ASCII item_id order; positively proved deletion/token change requires explicit evidence. Native rev equality/change is evidence to inspect, not permission to blindly copy/increment common generation. Change affecting safe selectors advances applicable common catalog/token generations once.

Selection arguments carry native catalog ID/revision/item ID resolved from the immutable common SelectionRef. Server atomically checks catalog revision and stable item identity at execution, including queued requests; stale returns STALE_GUARD with zero-execution proof if established. No reference silently rebases after refresh. Positional-only selector without verified stable/conditional guard cannot be READY.

Proposed station mapping is CONTENT catalog + content.launch REFERENCE, conditional on verified selectable-content semantics. CHANNEL alternative requires independently proved channel/ordering semantics and a reviewed mapping; stations are never assumed SOURCE inputs. DEVICE_NATIVE favorites may map favorites.activate; REMOTE_RECIPE remains in Core with individually captured native attempts. Actual catalogs/semantics remain UNKNOWN.

## 15. Artwork and separate retrieval

DESIGN REQUIREMENT: state carries optional `asset_token:Id` plus media context and asset revision, not embedded binary/base64 artwork. Adapter maps it to neutral AssetRef{asset_id}. Authenticated separate retrieval must return matching context/token/revision and bounded content type/length/dimensions. A relative same-device asset path is a candidate transport binding; path syntax remains OPEN with transport selection. UI never sees credentials or native URLs.

Prefer device-hosted immutable tokens to arbitrary remote URLs. Arbitrary internet URLs, redirects and credential-bearing URLs are excluded from this candidate path; adapting an existing external artwork source requires later review of fetching, identity, trust and caching. No internet dependency is introduced for local control.

Exact formats, byte/dimension limits, decoder peak RAM, cache/eviction and concurrent fetch bounds are profile OPEN pending measurements. Missing limits prevent artwork mapping READY; they do not block unrelated operations. Reject oversized/unsupported payloads before unbounded allocation; fetch/decode in bounded chunks only with verified format handling. Check captured context/session/associations again before asset publication; late old artwork never overwrites current context. Current TUNER artwork support/source is UNKNOWN.

## 16. Heartbeat and connectivity

DESIGN REQUIREMENT: negotiated bounded liveness/probe policy, refreshed by authenticated valid traffic; heartbeat/ping is not playback evidence or field-age proof. Silence past the negotiated timeout closes the session, invalidates current observations and begins capped reconnect backoff; pending command effect remains uncertain, never auto-replayed. Exact interval/backoff values are profile OPEN, not guessed aggressive polling.

Connected/READY requires an authorized negotiated/resynced session. Reconnecting is adapter scheduling/diagnostic state, not an invented common Connectivity enum. An independently verified route may show device REACHABLE while authorization/API availability prevents usable control; service failure without independent reachability evidence leaves reachability UNKNOWN/UNREACHABLE as evidenced. Mapping availability can be OFFLINE/BLOCKED/UNKNOWN separately. DHCP address changes update the route after authenticated identity continuity; never infer power off, playback stop or identity replacement.

## 17. Error normalization

DESIGN REQUIREMENT: structured `NativeError{code:String,detail?:String}`; detail is bounded private diagnostics with no credential leakage. Adapter emits exact common semantic code/reason and rejection_origin where proven, independently of transport status.

| Native cause | Common mapping / required evidence |
| --- | --- |
| UNSUPPORTED | UNSUPPORTED_OPERATION; actual descriptor evidence required |
| BAD_SCHEMA / BAD_DOMAIN / VERSION_MISMATCH | INVALID_ARGUMENT with exact schema/domain/version detail |
| BAD_REFERENCE / WRONG_TARGET | INVALID_REFERENCE; no name-based fallback |
| STALE_GUARD / CAPABILITY_CHANGED | STALE_PRECONDITION before execution; in-flight invalidation retains historical effects |
| NOT_AUTHORIZED / REVOKED | UNAUTHORIZED; refusal is unsent only with proof |
| OFFLINE / BUSY_CAPACITY / API_UNAVAILABLE | UNAVAILABLE; does not erase support |
| HISTORY_EXPIRED / ID_MISMATCH | REQUEST_HISTORY_EXPIRED / REQUEST_ID_MISMATCH; no replay |
| WAITING_FOR_ORDER | Nonterminal waiting_reason ORDERING_BLOCKED when scheduling remains open; never terminal error on QUEUED |
| Known incomplete count/recipe stopped | PARTIAL_EXECUTION wrapping exact normalized cause and step; preserve prefix |
| Transport/result timeout after possible execution | TIMEOUT at deadline and CCM uncertainty fold; not no-execution |
| Unknown native error / unrecognized result | No success: unknown detail remains private; definitive proved unsent failure -> UNAVAILABLE/UNKNOWN_NATIVE_ERROR; possible unresolved execution -> AMBIGUOUS_DELIVERY before deadline or TIMEOUT at deadline, underlying UNAVAILABLE cause as applicable |

CCM validation precedence and folding override a naive one-to-one error lookup. DELAYED/STALE_SESSION is not a new common error enum: discard stale current observations; executable scope refusal maps STALE_PRECONDITION if its context is validated, with delivery/effect evidence preserved. Cancellation/local deadline cannot prove a server stopped. Contradictory/invalid evidence is quarantined, not converted into invented success.

## 18. Security and authorization gate

LAN is untrusted: impersonated discovery, passive credential capture, unauthorized control, stale replay, malicious input, resource exhaustion and unauthorized asset access are threats. Discovery identity hints are unauthenticated. Owner association is not permission to execute.

DESIGN REQUIREMENT: owner-mediated pairing binds authenticated device identity to an authorized controller principal; later sessions authenticate both continuity and controller authorization before capability/state/asset access or execution. Revocation invalidates sessions and must be checked atomically before queued execution. New session IDs alone are not authentication. Credentials are adapter/server-private, never UI state or URL query parameters.

**Mechanism selection OPEN:** evaluate maintained TLS/ESP-TLS facilities with pinned device credentials plus scoped controller credentials, or mutual TLS if resource/provisioning costs fit. Evaluate a physical confirmation or out-of-band authenticated bootstrap; do not invent a pairing button/display capability on TUNER. TLS protects bearer credentials only after device identity is authenticated. HTTPS availability on ESP-IDF is platform evidence, not proof TUNER can afford or implements it.

No plaintext fallback, custom cryptography, trust-on-first-network-advertisement, unauthenticated command session or auth bypass is approved. Exact bootstrap, credential provisioning/storage/rotation/revocation, TLS version/profile and rate limits need TUNER stack/resource review and owner usability review. Until that gate is resolved, executable authorization/readiness cannot be claimed. Protocol semantic sessions remain independent of credential lifetime so the mechanism can be specified without changing CCM.

## 19. Multiple controllers and shared execution

DESIGN REQUIREMENT: every writer (REMOTE, another REMOTE, web UI, phone, automation or local controls) enters the verified target/resource execution owner or supplies equivalent guarded ordering evidence. No assumption that current TUNER has such integration. Native state reports device samples, not this REMOTE's last desired command.

Global target/media/catalog/field revisions change for external writers too. Guards check at execution prevent read-then-toggle and stale station/seek races. Whole-state revision is not a substitute for explicitly scoped media/catalog guards. Core scheduling orders this Core's work only; it does not claim global inter-controller FIFO. Device serializes its actual accepted execution, with documented resource effects; another controller can legitimately change state immediately after completion. That does not invalidate proved completion or authorize a current-state inference.

Dedup keys include authenticated principal/controller incarnation; unrelated controllers' serials cannot collide. On drift/conflict use CCM observations/conflict reconciliation; never use priority to claim another writer did not act. Absolute power through read-plus-toggle is prohibited without verified atomic conditional semantics.

## 20. Serialization, framing and message families

**Selected candidate serialization: UTF-8 JSON**, for inspectability and explicit typed schemas. This is a design requirement, not a claim about current radio JSON endpoints. CBOR/other compact binary are deferred unless measured RAM/flash/bandwidth proves JSON unsuitable; changing format requires reviewed equivalence, not weakened numeric semantics.

Wire representation: Bool -> JSON boolean; U32 -> integer JSON number within range; UInt64/Int64 -> canonical decimal JSON strings (unsigned digits without leading zeros except "0"; signed may use "-" only for negative, no "-0"/"+"). Rational -> {n:signed-decimal-string,d:unsigned-decimal-string}, reduced d>0; Normalized bounded [0,1]. Reject fractional/overflow integer, malformed UTF-8, duplicate object keys, NaN/infinity and noncanonical rational. String limit is CCM's 4096 UTF-8 bytes unless a narrower schema. Id is exact CCM OpaqueId domain. Value is tagged {tag:"PRESENT",value:typed} or {tag:"ABSENT",reason:UNKNOWN/UNSUPPORTED/NOT_APPLICABLE/UNAVAILABLE}; null never means zero/false/unknown by convention. Enums are exact strings; closed argument records retain CCM's types/variants. JSON map order has no semantic significance; lists are ordered.

Family records in §§6-14 are candidate semantic schemas. Version = {major:U32,minor:U32,patch:U32,candidate?:U32}. Guards are closed per negotiated operation, not arbitrary extensible bags. Common field types and operation schemas are imported by exact CCM §6/§7 meanings. Descriptor Schema/NumericMapping constraints are imported from CCM §§5/14 with native ref tokens translated privately; they cannot widen a supported common domain.

Read-only families: identity/versions, capabilities.get, state.sync, freshness.probe, catalog.get, result.lookup, liveness. Notifications: capabilities.changed, state.snapshot/update, command result. Executable family: command only. Read-only request messages carry a unique correlation ID and current authenticated scope; responses echo it. No read family can execute queued actions as a side effect.

Framing remains separate and **OPEN with transport binding**: recommended WebSocket binding is one reassembled text message per JSON object, bounded before allocation; recommended HTTP binding is one bounded JSON body, with binary assets separate. TCP would need explicit bounded length framing; UDP would need datagram limits/loss contracts. No executable endpoint, method or port is final in this candidate. JSON serialization is selected independently of that unresolved binding.

Semantic illustration only (future conditional volume capability, not an existing endpoint):

~~~json
{"type":"command","api_version":{"major":1,"minor":0,"patch":0,"candidate":1},"common_model_version":{"major":1,"minor":0,"patch":0},"server_incarnation":"boot-example","session_id":"session-example","controller_incarnation":"remote-example","command_serial":"1","native_target_id":"target-example","operation":"volume.set","arguments":{"level":{"n":"17","d":"25"}},"capability_rev":"1","guards":{}}
~~~

Example IDs/version/capability are illustrative; numeric representability/idempotence/support must still be verified. No server is implied by the example.

## 21. Bidirectional CCM ↔ native mapping register

D = DESIGN REQUIREMENT; U = UNKNOWN / REQUIRES TUNER VERIFICATION; V = VERIFIED. **Every row is D for the proposed wire contract and U for actual TUNER support; no row is V.** "Conditional" means no usable mapping until complete verified descriptors and all unresolved requirements are satisfied. Outgoing operations never create incoming state. Device results are normalized into Core evidence, not trusted as Core lifecycle enums.

| Common operation/field | Native wire concept | Direction | Evidence | Mapping status / unresolved facts |
| --- | --- | --- | --- | --- |
| power.set | command "power.set", state ON/OFF/STANDBY subset | Core -> device | D / U | Conditional; actual states, physical/logical power meaning, completion and endpoint survival unknown; no toggle emulation |
| power.toggle | command "power.toggle", {} | Core -> device | D / U | Conditional N; exact toggle meaning/effect proof unknown |
| volume.set | command "volume.set", exact normalized Rational -> verified native level | Core -> device | D / U | Conditional; range, affine/TABLE, representable levels, rounding/error and side effects unknown |
| volume.step | command "volume.step", UP/DOWN,count | Core -> device | D / U | Conditional N; discrete unit/range/partial evidence unknown; no count replay |
| volume.mute | command "volume.mute", muted Bool | Core -> device | D / U | Conditional; independent mute/state/side effects unknown |
| playback.play | command "playback.play", {}, media guard | Core -> device | D / U | Conditional; real entry point and distinct play semantics unknown |
| playback.pause | command "playback.pause", {}, media guard | Core -> device | D / U | Conditional; no play/pause toggle substitution |
| playback.stop | command "playback.stop", {}, media guard | Core -> device | D / U | Conditional; stop variant/side effects unknown |
| playback.previous | command "playback.previous", {}, media guard | Core -> device | D / U | Conditional N; target traversal semantics unknown, never channel-down alias |
| playback.next | command "playback.next", {}, media guard | Core -> device | D / U | Conditional N; target traversal semantics unknown, never channel-up alias |
| playback.seek | command "playback.seek", integer ms, media/window guards | Core -> device | D / U | Conditional; actual seekability, inclusive window, units/precision unknown |
| source.enumerate / source.select | catalog.get SOURCE / guarded source ref | Both (query/result vs action) | D / U | Conditional; actual inputs/catalog/stable IDs unknown |
| content.enumerate / content.launch REFERENCE | catalog.get CONTENT / guarded item ref | Both | D / U | Conditional station candidate; actual selectable-content/atomic identity contract unknown |
| favorites.enumerate / favorites.activate DEVICE_NATIVE | catalog.get FAVORITE / guarded favorite ref | Both | D / U | Conditional N; actual native favorites/atomic effect unknown; remote recipes stay Core |
| channel.enumerate / channel.select / channel.step | No station-to-channel mapping selected | Both if later reviewed | U | Excluded from proposed native subset pending channel evidence; no inferred numbers/order/wrap |
| power | state field "power", Value<ON/OFF/STANDBY> | Device -> Core | D / U | Conditional; actual observation/precision/provenance unknown |
| volume.level | state field "volume.level", inverse verified NumericMapping | Device -> Core | D / U | Conditional; actual samples/quantization/sentinels unknown |
| volume.mute | state field "volume.mute", Value<Bool> | Device -> Core | D / U | Conditional; independent true mute observation unknown |
| playback.state | state field "playback.state", PLAYING/PAUSED/NOT_PLAYING/BUFFERING | Device -> Core | D / U | Conditional; actual states unknown; idle/off/error not silently translated |
| source.current / content.current | guarded current native ref -> SelectionRef<SOURCE/CONTENT> | Device -> Core | D / U | Conditional; actual catalog/current-item continuity unknown |
| media.title | context-tagged Value<String> | Device -> Core | D / U | Conditional; actual source/fresh acquisition unknown |
| media.subtitle | separate context-tagged Value<String> | Device -> Core | D / U | Conditional; do not merge artist/subtitle |
| media.artist | separate context-tagged Value<String> | Device -> Core | D / U | Conditional; actual source/absence meaning unknown |
| media.duration_ms | context-tagged Value<U64 ms> | Device -> Core | D / U | Conditional; unknown duration is absence, not 0/unlimited |
| media.position_ms | context-tagged sampled Value<U64 ms> | Device -> Core | D / U | Conditional; precision/time source unknown; extrapolation stays ASSUMED |
| media.artwork | context-tagged asset token -> neutral AssetRef | Device -> Core | D / U | Conditional; source/format/bounds/retrieval unknown |
| CommonDevice / Target / Binding | authenticated native identities/target records/session | Both association and session exchange | D / U | Conditional; persistent identities/target layout/auth unknown |
| StateObservation | state sample + sequence/context/nonce acquisition proof | Device -> Core | D / U | Conditional; actual causal acquisition/age evidence unknown |
| CommandRequest / CommandResult | private attempt key / typed command / scoped result evidence | Both | D / U | Design mapping; actual dedup/persistence/feedback/ordering proof unknown |
| CatalogSnapshot / SelectionRef | native catalog/token association + atomic selection guard | Both | D / U | Design mapping; full/partial enumeration and stable references unknown |
| RemoteRecipe / shared-resource barriers | separate captured steps; per-attempt proof key translation | Both evidence, Core-local recipe scheduling | D / U | No opaque server recipe; actual resource effects/fences unknown |

Navigation, rewind/fast-forward, channel number/step, app catalog/launch, content URI launch and vendor operations are not proposed supported native operations in this candidate. If later requested, require separate semantics/evidence/review; current absence means UNKNOWN, not a claim that the radio cannot implement them. All unspecified fields likewise remain UNKNOWN.

## 22. Intended initial and recovery flow

| Stage | Required transition / failure behavior |
| --- | --- |
| Discover | mDNS or manual locator -> candidate route. Failure leaves discovery pending/manual option, never a guessed device. |
| Establish authorized session | Verify paired device/controller identity, allocate fresh session. Auth failure -> UNAUTHORIZED/BLOCKED as evidenced; no reads/control outside allowed pairing bootstrap. Security mechanism unresolved -> not READY. |
| Negotiate versions | Exact candidate/compatible frozen subset and CCM contract. Mismatch -> unavailable mapping, INVALID_ARGUMENT/VERSION_MISMATCH for attempted incompatible work. |
| Obtain identity | Verify stable device/target continuity against authenticated evidence. Conflict -> quarantine; no automatic merge or retarget. |
| Obtain capabilities | Complete descriptors/revisions and verified mapping. Partial/unknown -> relevant support UNKNOWN; no UI hard coding. |
| Obtain initial state | Atomic subscription+nonce snapshot cut, defensible ages/context. Missing/stale data -> absent/stale; no cache rejuvenation. |
| Subscribe/receive changes | Confirm continuous stream through snapshot cut; gaps/conflicts -> suspend dependent work and verified RESYNC. |
| Become READY | Per-operation gate, not whole-device blanket support. Read-only device may have ready fields without writes. |
| Execute command | Core validation/deadline/lock gate -> immutable native attempt -> server dedup/auth/context/execution gate. Refusal before execution preserved; possible send starts historical effect state. |
| Correlate results | Capture original key/contract, fold evidence separately from fields. Missing result -> unresolved at deadline, no resend. |
| Resync after reconnect | Fresh native/common session; versions/identity/capabilities/state reverified. Query original results read-only if available; old callbacks cannot affect current fields, old scheduling never resurrects. |

Each failed stage is observable through common support/availability/connectivity/results plus private diagnostics. No generic CONNECTED indicator implies support, freshness, authorization or playback.

## 23. Separate TUNER project handoff — later read-only milestone

No radio repository access occurred here. Next verification must record exact repository/commit/configuration and source locations without modifying or building it.

| Verify in separate TUNER repository | Evidence to bring back / future implementation requirement |
| --- | --- |
| Identity and boot domains | Existing durable ID source, factory-reset behavior, provisioning/storage and non-reuse; otherwise new identity/incarnation contract required |
| SDK/network/server | MCU/SDK/library versions, Wi-Fi stack, HTTP/WS/TLS facilities, current endpoints, mDNS, port/path ownership and concurrency |
| Existing control entry points | Each power/volume/mute/play/pause/stop/prev/next/seek action, arguments/effects, task ownership and actual completion signals |
| Volume | Exact numeric range/representable set, units, sentinels, precision, quantization, independent mute and power side effects |
| Station/source/favorites | Actual semantic role, identity continuity, ID/index reuse, order/wrap, full/partial enumeration and atomic guarded selection possibilities |
| Metadata/timing/artwork | Source/acquisition age, current-item correlation, duration/position validity, cache behavior, late callbacks, formats/source and retrieval feasibility |
| Sessions/dedup/reboot | Durable correlation state, admission watermark, execution ledger/barrier/fence feasibility, delayed work and fail-closed recovery |
| Other writers/resources | Web/local/automation paths and shared actuators; whether all can enter one verified execution/guard owner |
| Resource budget | Playback plus TLS/socket/JSON/catalog/asset peak heap, allocation/task stacks, flash and connection/message/queue limits |
| Security/onboarding | Maintained TLS/auth facilities, credentials/secure storage, physical confirmation possibilities, revocation and identity continuity |
| Discovery/recovery | mDNS advertisement/query support, manual endpoint, DHCP/reconnect/power transitions and separate API reachability |

Return findings as VERIFIED with source/commit and exact limited claims, DESIGN REQUIREMENT for missing future server work, or UNKNOWN. Existing legacy HTTP or JSON endpoints, if found, are not automatically this Native API. Verification precedes final transport/security/profile decisions and candidate correction/freeze; server implementation is a later separately authorized milestone.

## 24. Validation plan and adversarial self-review

| Stage | Required evidence / present status |
| --- | --- |
| SPECIFICATION REVIEW | This initial candidate self-review below completed; independent/owner review and open binding/profile/security decisions remain. Not API freeze or runtime conformance. |
| REMOTE ADAPTER UNIT TESTED | Future executable typed mapping, lifecycle/evidence/age/guard/reboot tests. NOT PERFORMED. |
| TUNER SERVER IMPLEMENTED | Future source/versioned implementation of approved contract with measured profile. NOT ESTABLISHED. |
| PROTOCOL TESTED | Future real server/client fault injection, duplicate/reorder/reconnect/persistence/security/catalog/age tests per version/capability. NOT PERFORMED. |
| END-TO-END HARDWARE VALIDATED | Future actual UI -> Core -> adapter -> network -> TUNER execution/state observations. NOT PERFORMED; HOME UI physical PASS does not qualify. |

Candidate self-review is written-contract reasoning against frozen CCM; PASS means the proposed contract gives conservative behavior, not that TUNER supplies it.

| Case | Expected mapping / written review |
| --- | --- |
| Duplicate command | Same key/envelope returns retained evidence; changed key payload rejects; expired serial/history rejects without execution. CCM outer dedup unchanged. PASS design; server ledger UNKNOWN. |
| Delayed result | Correlate original captured attempt; idempotent late fold; no current-field rollback/new scheduling. Purged Core result not recreated; independent barrier evidence may resolve. PASS design. |
| Stale state after reconnect | Old session/domain rejected; fresh nonce RESYNC required, cache age preserved. PASS design. |
| TUNER reboot | New server incarnation rejects old executable messages; old effects/barriers remain unless exact proof resolves them; no replay. PASS design; recovery storage UNKNOWN. |
| REMOTE reboot | New controller/Core clock epoch; old unsent scheduling abandoned, historical keys/barriers persisted; old serials never new intent. PASS design. |
| Multiple controllers | Per-principal dedup; device execution owner and atomic guards; no last-command-as-state or global Core FIFO claim. PASS design; other writer integration UNKNOWN. |
| Capability change | Native revision event invalidates affected captures; in-flight history retained, no further attempts under changed mapping. PASS design. |
| Stale station reference | Captured catalog/item guard checked atomically at execution; stale refuses, no friendly-name/index rebasing. PASS design; stable IDs/guards UNKNOWN. |
| Station change + late artwork | Atomic new context invalidation; fetch completion checks original scope/context; discard old asset. PASS design. |
| Timeout + delayed completion | Core closes uncertain after possible execution; valid late whole completion updates retained history and covered barriers, preserves first closure/deadline, sends nothing. PASS design. |
| Unsupported operation | Explicit verified unsupported rejects UNSUPPORTED_OPERATION; missing/unknown contract never READY. PASS design. |
| API version mismatch | No executable negotiation; version refusal INVALID_ARGUMENT/VERSION_MISMATCH, no coerced enum/operation. PASS design. |
| Lost update / sequence gap | Invalidate continuity, new session/stream + nonce snapshot; no arrival-order repair or age refresh. PASS design. |
| Unauthorized controller | No executable session/queue admission; atomic recheck revocation; discovery doesn't authorize. PASS design requirement; mechanism OPEN implementation gate. |
| DHCP/IP change | Update locator only after authenticated stable identity; no common ID replacement/automatic merge. PASS design. |
| Partial count / recipe | Preserve proved prefix/emission; no full replay or suffix resumption; late prefix proof cannot complete whole request. PASS design. |
| Future-only / partial fence | Only covered keys released, no past-nonexecution inference, no N fallback or fabricated completion. PASS design. |
| Capacity exhaustion / cache purge | Refuse before handover; never evict active/barrier state; expired native/Core history cannot replay. PASS design; profile/persistence UNKNOWN. |
| Delayed freshness anchor / cached snapshot | Age bound includes full Core elapsed time; stale/unknown-age cannot become current; copied cache can't claim fresh nonce. PASS design. |

**TUNER API DESIGN BLOCKER: none found for representation against frozen CCM in this initial self-review.** Conditional features without verified guarantees remain UNKNOWN/unavailable. OPEN transport/security/profile choices block final API approval/implementation readiness; they are explicit evidence-dependent gates, not hidden adapter guarantees. If verification reveals that required atomic guards, completion, freshness or persistence cannot be supplied, record a TUNER API DESIGN BLOCKER for the affected proposed mapping and revise this candidate; do not amend frozen CCM.

Exact next milestone: **B — separate TUNER repository READ-ONLY protocol/implementation verification** to obtain the evidence in §23. Do not begin it automatically. Then review/correct transport/security/profile/mappings here before proposing API freeze or implementation.

## 25. Open decisions

| Decision | Current status | Evidence required | Blocking / non-blocking | Next verification location |
| --- | --- | --- | --- | --- |
| Final transport and framing/endpoints/ports | OPEN; HTTPS + authenticated WebSocket leading option, not selected | Actual SDK/server compatibility, peak resource/concurrency and security review | Blocks API binding approval/implementation; not model freeze | Separate TUNER repo read-only, then this candidate |
| Serialization | JSON selected for candidate, no implementation | Parser/schema/resource measurements; binary only if justified | Resource gate before implementation | Both projects in later authorized implementation/tests |
| mDNS service/TXT profile | Primary mDNS + manual fallback design; actual support UNKNOWN; proposed service type only | Current mdns/server stack, naming/length/path review | Blocks automatic discovery advertisement, not configured direct locator | TUNER read-only then protocol review |
| Broadcast fallback | Not selected; no justification | Actual mDNS/manual failure scenario | Non-blocking future option | Later LAN discovery validation |
| Stable native device/target identity | UNKNOWN; persistent authenticated non-reuse required | Provisioning/source/reset/association evidence | Blocks safe registry/control binding | TUNER read-only |
| Security/pairing/credentials | OPEN mandatory gate; no insecure fallback | TLS/profile/heap, identity provisioning, owner bootstrap/revocation | Blocks executable authorization/API freeze | TUNER read-only + owner/security design review |
| Existing capability subset | All actual support UNKNOWN | Exact entry points, domains, stop/traversal/power/seek semantics and independent reads | Blocks affected mapping SUPPORTED/READY | TUNER read-only |
| Volume numeric mapping | UNKNOWN | Range/steps/resolution/sentinels/side effects | Blocks volume mapping | TUNER read-only |
| Catalog identity and station semantic role | CONTENT candidate, channel/input roles UNKNOWN | Stable IDs/reuse/order/guard/full-enumeration contract | Blocks safe selection/catalog mappings | TUNER read-only |
| Session/execution ledger/persistence/fences | Future design, current guarantees UNKNOWN | Durable keys, reboot recovery, partial/delayed execution, exact key coverage | Blocks claims of dedup/guarded completion/ordering; unresolved barriers fail closed | TUNER read-only + later protocol fault tests |
| Field/media acquisition and nonce proof | Future design, existing acquisition UNKNOWN | Actual sampling/cache/task/context semantics | Blocks current truth/context-sensitive mappings | TUNER read-only |
| Multiple-writer arbiter and shared resources | UNKNOWN | All control paths and asynchronous work ownership | Blocks verified guards/fences for affected resources | TUNER read-only |
| Message/queue/connection/retention/freshness/heartbeat limits | Profile OPEN; CCM maxima unchanged | Measured heap/flash/task/concurrency and required latency | Blocks bounded implementation/ready descriptors | TUNER read-only then measured implementation profile |
| Artwork format/size/path/decode/source | UNKNOWN / profile OPEN | Source, local retrieval, decoder peak RAM, bounds/trust | Blocks artwork only; unrelated controls remain independently negotiable | TUNER read-only |
| API freeze and support badges | Candidate only; none earned | Reviewed binding/security/profile + owner approval; separate protocol/hardware evidence | Blocks API frozen/support claims | Future REMOTE specification review, then separate tests |
