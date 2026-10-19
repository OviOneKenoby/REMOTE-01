# A-REMOTE-002: bounded durable recovery and minimum operation support

2026-10-08. **READY FOR REVIEW — DESIGN ONLY. NOT IMPLEMENTED.**

Current-main execution/publication recheck input: `8e1ab9d87c4647964bbc48d0e50982472a0b9cca`. Original design input: `dce15583873f66fd56239b3b6cb805135477719a`. Device: REMOTE. Firmware workspace: `C:/Users/RYZEN/Documents/REMOTE01/Code`. Actual coordination worktree: `C:/Users/RYZEN/AppData/Local/Temp/remote-a002/publication`; originating checkout: `C:/Users/RYZEN/Documents/REMOTE-TUNER-SYSTEM/remote_tuner_system`.

The prior completed result package was UNPUBLISHED following owner delegation to PM. The latest owner instruction now explicitly authorizes this REMOTE executor to deliver and publish from current main. Existing design and byte-preserved evidence were rechecked and reused; this branch supplies the complete result for review. Administrative publication already completed is recorded below to prevent duplication. No firmware/test/config edits, build, upload, hardware activity, Git initialization or shared-contract approval.

## 1. Reverified inputs and authority

Read current rules, README, PROJECT_SYNC, API_STATUS, DECISION_REGISTER, both input handoffs, exact templates/footer, assignment and both reviewed A-001 reports. Original design input had the starter TUNER handoff; current main contains the reviewed TUNER A-001 handoff/report, re-read alongside the pinned report at `31205936ba12ec46a4e66d1c8766cac75334f5ce`. REMOTE review head is `2163358f2f364df768d0db2ddd4589028d4aebf2`. Later fetched TUNER branch changes were not merged or promoted to reviewed executable evidence. TUNER corrections need not be finished for this design.

Original design entry:4039 files. Current-main recheck:4040 files; only the previously completed A-REMOTE-002 document/changelog distinguish these inventories. Against A-001's 1704 non-.pio paths, only CHANGELOG differs; only `docs/A_REMOTE_001_BASELINE_RECONCILIATION.md` is new outside .pio. Relevant firmware/configuration source is unchanged. Device remains Git-less: branch, HEAD and dirty-against-commit unavailable; no Git initialized.

Frozen CCM1.0.0 SHA-256 `7a46f9fffe706476de458aac078d0af1c94844df339248d3f9f4b5cf033011d7` matches recovered approved artifact. Original approval excerpt remains in A-001 evidence. Native API v1 UNAPPROVED / UNFROZEN / NOT IMPLEMENTED; O01–O17 OPEN. PM assignment permits design, not approval of an implementation or a shared guarantee.

Evidence: [index](https://github.com/OviOneKenoby/remote_tuner_system/blob/bfaf9a579a41b45d7c3e7ded0ac1af5c2d956fec/references/device_evidence/remote/A-REMOTE-002/INDEX.md), [baseline](https://github.com/OviOneKenoby/remote_tuner_system/blob/bfaf9a579a41b45d7c3e7ded0ac1af5c2d956fec/references/device_evidence/remote/A-REMOTE-002/BASELINE.json), [source hashes](https://github.com/OviOneKenoby/remote_tuner_system/blob/bfaf9a579a41b45d7c3e7ded0ac1af5c2d956fec/references/device_evidence/remote/A-REMOTE-002/SOURCE_SHA256.json). All touchpoints below have byte-preserved source snapshots. Estimates below are proposed internal representations, not measured firmware resources.

## 2. Requirement-to-source map

| Requirement | Frozen CCM | Current source / gap |
| --- | --- | --- |
| Persistent IDs/generations | §2, §16 | `core.hpp::Store`; `core.cpp::expected_id/capture/submit`: IDs are intent:(epoch_base+serial). Only durable recovery makes this lifetime bijection valid; random boot epoch is insufficient. |
| Atomic allocation including refusals | §10, §12 | submit increments watermark/lifetime before admission; end/persist must retain consumed pair on cache/memory/queue refusal. capture is a proposal, not a durable issued pair. |
| Commit before executable handover | §10–12 | schedule creates Effect/Attempt, persists DISPATCHED, then AMBIGUOUS, then Adapter.dispatch. MockJournal only counts/returns success; no storage. |
| Independent effect/conflict recovery | §9, §11–13, §17 | Effect keys/no_past/no_future survive in-process result purge; current Effect alone lacks enough full immutable envelope/contract/proof for production recovery. Conflict participant rosters require separate persistence. |
| Bounded exact dedup retention | §12–13 | Core::purge/maintenance:1024 full records, purge=max(first terminal+60000,deadline), checked overflow; old epoch scheduling/results retired separately from effects. |
| Reference/catalog ingestion | §5, §8, §15, §18 | ingress Sample/Event/Inbox is eight copied scalar events; references/catalog/query completion absent. Core::catalog merges complete/partial but lacks asynchronous evidence, explicit token rebind/deletion and atomic allocation-failure staging. |
| Favorite/content/query execution | §6, §10–12, §15 | known_operation/schema recognize names; foundation_operation rejects SUPPORTED descriptors. No query-result bridge or DEVICE_NATIVE Favorite dispatch. |
| Exact numeric mapping | §3, §14 | synthetic_numeric_profile and schedule accept normalized synthetic grid/two-row table only; Facade::snapshot rounding is presentation, not adapter conversion. |

Runtime currently creates development Session (fresh Store, MockJournal, synthetic adapter) in PSRAM. Future production startup must validate/load durable state before Core construction or executable adapter attachment. No HOME direct storage/target access. Raw Store serialization is forbidden: owning pointers, routes, padding and ABI are not a storage format.

## 3. Recommended owner boundary and durable protocol

Recommendation subject to qualification: synchronous UC_owner DurableJournal, versioned canonical safety projection, redundant NVS checkpoints and a fail-closed transaction anchor. No new task; flash stall/owner latency must be measured before acceptance. Transport producers continue posting copied events and never mutate/commit Core. No hidden adapter retries or emission before dispatch.

Separate durable domains:

1. Registry: persistent registry identity, original device/target/binding/resource IDs, lifetime allocation counters, persistent generations, association provenance and rollback history.
2. Settings/authorization: selected target, locator candidates, secret/grant references separately versioned. Current Wi-Fi `remote_wifi` untouched. Settings are not authorization proof; Wi-Fi reset cannot clear Core effects. Erase policy remains O16.
3. Mandatory Core safety: lifetime intent counter/clock epoch allocator; immutable request/attempt captures, original execution contract, target/resource keys and ordering modes, evidence/progress and independent past/future uncertainty; conflict rosters and causal decision watermarks.
4. Volatile presentation: current samples, selected state, queue/result cache. Reacquire after reboot. Diagnostic history cannot reconstruct decision state.

Canonical little-endian fields, explicit tags/lengths, optional/absence tags, exact Rational components, canonical map ordering and ordered routes/steps/attempts. Full correlation data needed to validate proof must be retained; an envelope digest alone is insufficient. Private token/secret references stay private. CRC detects accidental corruption, not malicious rollback/authentication.

Required projection inventory before codec approval: CommandRequest version/ID/ticket/deadline/original device and target; exact operation and all typed arguments; binding and capability/mapping/device-association/target-association generations; optional catalog/token/media/seek-window captures; ordered field preconditions, route contracts and recipe capture if applicable. Each handed-over attempt retains index/step, original session/mapping, dispatch/closure epoch-time, delivery/history, emitted/confirmed lower bounds, declared feedback/completion/no-execution contract, evidence identity and full claims/provenance needed to authenticate later proof, and all actual historical keys/modes with independent no_past/no_future. Preserve first closure/cause where needed to validate historical outcomes; never rewrite it from current state. Persistent catalog/token identity-continuity tables, scoped generation allocators and deleted-ID non-reuse must also be included or current catalogs held UNKNOWN until a verified non-reusing reconstruction is possible. Conflict state retains revision, participant binding roster, conflict-ingress and post-conflict resync watermarks/evidence, separately from diagnostic samples. Reboot-invalid old observations cannot satisfy fresh conflict reconciliation; retained roster still blocks state-dependent work pending new verified resync. Missing any mandatory projection is a D0 failure, not a reason to silently narrow evidence.

Proposed header: magic u32, format u16, header length u16, generation u64, payload length u32, feature mask u32, registry/allocator revision u64, predecessor generation u64, CRC32 u32, reserved u32 =48 bytes. Payload is a closed bounded record set. Anchor records transaction phase, expected generation/digest and allocator high-water mark. Exact anchor representation and backend failure guarantees require qualification; not already implemented.

### Handover order and crash boundaries

1. Start serialized event: purge due full results before lookup; preserve CCM validation precedence. Stage atomic ID/ticket/lifetime advancement plus refusal/result registration. Commit before exposing consumption/admission. DEDUP_CAPACITY still consumes pair. Ambiguous storage outcome blocks allocation, never reissues possibly consumed IDs.
2. Capture immutable route/domain/deadline/context/preconditions. Admission reserves no keys. At handover acquire full write-key union atomically/FIFO; queries have no write reservation. Pre-reserve durable space for full effect, required proof growth and a resolution transaction.
3. Revalidate then commit DISPATCHED and independent effect on actual historical keys. Commit emission-possible/AMBIGUOUS before calling dispatch. Reset before actual send may leave conservative uncertainty; never infer no execution from that reset.
4. Dispatch exact captured request. Local emission/delivery and target completion are separate facts. Synchronous sink callbacks remain serialized bounded evidence events.
5. Commit verified proof/per-key release before exposing release or folding result. A future-only r1 fence resolves only that attempt's future r1 barrier, not past absence or another attempt. On commit failure retain old durable barrier and block; no usable transient in-RAM release.
6. Full-result purge never deletes unresolved correlation/effect. Late proof after purge resolves independent history only; no recreated result/replay/suffix. Resolution GC is itself committed, preserving lifetime/generation metadata.

### Checkpoint and startup recovery

Write validated PREPARE anchor for generation g before replacing inactive bank. Retain active bank. Write inactive canonical g, check set/commit status and bounded readback/CRC/semantic validity, then commit/verify SEALED anchor for exact g/digest. Only a validated sealed projection grants executable authority. Multiple NVS keys are not an atomic transaction. Failures return Journal failure and block.

Never choose an older readable checkpoint below observed newer transaction intent: PREPARE without valid seal, missing/corrupt sealed bank, absent/corrupt anchor or inconsistent ancestry => global executable block. The anchor's ability to expose interrupted updates is a backend qualification requirement. No automatic namespace erase/reinitialization. Whole-flash rollback to an internally valid older image is not prevented by CRC/dual-bank; O05/O16/O17 security/reset policy must address it separately.

On verified recovery, durably allocate new monotonic clock epoch using persistent registry/boot allocator, retire old unsent work/full dedup/ticket lookups, retain effects/conflicts, increment persistent sessions/incarnation where required and reverify authorization/mappings/state. No recovered command is automatically resent, including previously logged DISPATCHED/NOT_SENT. Old ticket => OLD_EPOCH; current pair past purge => HISTORY_EXPIRED; swapped pair => ID_TICKET_PAIR. A random epoch alone is not uniqueness proof.

Unknown/missing identity history is not factory-fresh. Affected scopes remain blocked; if lost keys cannot be proven, block all writes and dependent queries until separately approved reconciliation. Read-only safe diagnostics may remain. Session replacement, deletion, association rollback, Wi-Fi reset, timeout or elapsed wall time cannot clear uncertainty. Historical keys remain tied to original effects, not current association.

## 4. Storage alternatives, arithmetic and qualification gates

SOURCE-DERIVED: partitions.csv NVS0x9000/0x6000=24576 bytes/six4096-byte pages; layout unchanged. Existing `remote_wifi` credentials occupy the same partition; actual free entries/fragmentation UNKNOWN. RTC POWER history is not a durable Core journal; no SD fitted in measured baseline. Factory/OTA slack cannot be repurposed in this task.

Inspected installed pinned IDF5.5.3: nvs.h requires checked set/commit/get; nvs_storage.cpp writes blob chunks then BLOB_IDX and cleans prior version; constants give126 entries/page,32 bytes/entry. `NVSHandleSimple::commit()` only validates handle and returns ESP_OK. Its name does not establish multi-key transactional batching. App-level protocol/readback plus power-cut tests are still necessary; namespaces share GC/storage/wear.

| Strategy | Atomicity/recovery approach | Capacity/wear assumptions | Recommendation |
| --- | --- | --- | --- |
| A: redundant canonical checkpoints + anchor, existing NVS | inactive bank and explicit validated seal; fail closed on unresolved newer intent | simplest state equivalence/migration; full safety projection rewrite amplification, shared GC, anchor durability need tests | First bounded qualification candidate, conditional, not approved implementation |
| B: immutable NVS objects + bounded root manifest/log | write immutable envelope/effect/proof objects then validated root; collect only unreferenced committed objects | lower small-update amplification; many keys/metadata, root atomicity and compaction/scan bounds more complex | Feasible alternative if A budget/wear fails; separately reviewed design |

Both can serve a small bounded profile without partition changes; neither is proven to fit full Store/CCM maximum in24KiB. No fabricated physical durability/endurance claims.

| Proposed object | Concrete ESTIMATED representation/bound | Arithmetic / caveat |
| --- | --- | --- |
| Exact-ID dictionary | u16 length + <=128 ASCII bytes, u16 canonical handle |130 bytes/max ID;8 IDs1040 bytes;32 IDs4160. Never truncate or reuse handles while referenced. |
| Effect key | tag/version2 bytes +device/target/resource handles6 +mode/flags2 |10 bytes;17 current maximum keys170; dictionary separately counted. |
| Effect fixed spine | handles, attempt index, scoped counters, progress/delivery/proof flags, offsets |192-byte planning allowance; enumerate every §10/11 field before implementation. |
| One simple effect |192 spine+170 keys+1024 small full capture/contract/proof budget |1386 bytes excluding shared IDs; actual capture may exceed it => refuse before send, no omission. |
| Candidate checkpoint |48 header, total<=3072; two banks |6144 payload, not physical consumption. 1040 dictionary+1386 effect+48 header leaves598 for registry/allocators/conflicts; may not fit. No guaranteed two-effect capacity. |
| Approximate NVS footprint |3072/32=96 data entries +~2 chunk/index overhead |~98 entries/bank;196/two, plus simultaneous old/new chunks, anchor, namespaces, existing settings and GC.756 theoretical entries are not all usable. Measure actual reserve/fragmentation. |
| ID/ticket pairing |counter+epoch base+watermark, checked bijection |O(1) persistent metadata; no8192-ID lifetime exhaustion assumption. Refusal consumes pair. UInt64 overflow blocks. |
| Full result cache |keep existing1024 maximum/retention |volatile;4MiB local Memory budget does not guarantee model worst case. No early active/unexpired eviction. |
| Catalog staging |max1024 items;128 ID+256 label+64 variant+32 tags/length/index per item |480 bytes/item=491520; two slots983040 +headers/known catalog/owning conversion. Profile bounds narrower than Text4096; reject unsupported oversized payload, no truncation. |
| Reference ingress |four max length-prefixed IDs520 +five u64 counters40 +tags |proposed<=576-byte dedicated payload; actual existing Event sizeof/frame impact UNMEASURED. |
| Journal scratch |two persistent3072-byte codec buffers |6144 RAM estimate +bounded scratch; owner object, not task automatic frame. Placement/frames measured later. |

3072 bytes is a feasibility probe, not certified capacity. Before implementation, encode actual worst approved registry/profile, complete correlations and mandatory conflict participants, obtain NVS stats and prove room for checkpoint update AND resolution while retaining credentials/GC. If not feasible, block execution and request revised bounded storage/profile proposal; no weakening CCM or secret partition expansion. Internal estimates cannot become Native O11 limits.

Reserve evidence/resolution growth before handover. Current Effect evidence IDs bound32 does not authorize dropping mandatory proof; retain pending barrier on exhaustion. An unresolved effect never ages out to admit a new one. Capacity must not prevent the only safe resolution transaction. If resolution write fails, remain blocked.

Avoid wear from `Core::end` commits every tick: durable-projection equality can return success without physical write when safety state unchanged. Do not persist uptime/UI refresh/sample text/diagnostic counters. Mandatory allocators/generations/conflict/barrier changes still commit immediately; never coalesce pre-send commit past emission. Flash endurance, write amplification, GC, owner latency and exact partition free space remain UNKNOWN until qualification.

## 5. Fault and recovery acceptance matrix

All rows are future tests NOT RUN. Fake backend first; physical power cuts only after separate owner authorization with exact artifact. Independent emission oracle required.

| Fault/boundary | Required outcome |
| --- | --- |
| Allocation commit fails/ambiguous |no send/success; allocator blocked, no possibly consumed ID reuse |
| Allocation committed; cache/queue/memory refusal |pair consumed; duplicate lookup according to retained cache/tombstone; reboot OLD_EPOCH; no send |
| QUEUED crash |abandon old scheduling, no resend |
| DISPATCHED committed, before/within emission |retain conservative independent correlation/keys; no replay; conflicting work blocked |
| SENT, target silent/deadline/reconnect |delivery not completion; no past/future proof invented; barrier persists |
| Proof received before commit, crash |old durable barrier remains; reacquire proof, never resend |
| Per-key proof committed before fold |recover only its covered released keys; other attempts remain; idempotent closed fold |
| Exact result purge then late proof |HISTORY_EXPIRED; independent resolution only; no recreated result/intent |
| Restart then old proof |OLD_EPOCH scheduling; verify historical capture, do not restamp current field |
| Future-only/partial-key proof |G01/G02/G05: no whole no-execution/retry permission; retain all unresolved other keys |
| Interrupted PREPARE/bank/seal or failed readback |fail closed; do not fall back below newer intent or erase namespace |
| CRC/schema/anchor/version corruption |global block if unknown affected scope; diagnostic recovery-needed, not factory reset |
| ID/counter/memory/storage/evidence exhaustion |no wrapping/drop/early eviction; refuse before emission, reserve resolution |
| Migration interrupted/downgrade |preserve old decision state; unsupported required feature blocks; no uncertain-effect deletion |
| Association/delete/Wi-Fi reset |old original effect keys retained; new references invalidated; no uncertainty clearance |
| Newer conflicting intent then old late proof |old retry eligibility remains cancelled; no parent/suffix resurrection |
| Duplicate/contradictory evidence |idempotent same proof; quarantine contradictions, preserve prior evidence |
| Whole-flash erase/rollback |not protected by CRC alone; history/identity UNKNOWN; no inferred fresh safety |

Migration uses independent internal format version, not altered CCM version. Decode bounded old projection, validate all persistent scopes/history, encode inactive new bank, prove semantic equivalence then seal. Preserve old bank until approved recovery procedure permits GC. Never migrate executable old scheduling across epochs; never erase on incompatible image. Full result boundary/overflow and per-attempt resolution follow CCM §19 E10/F01/F19/G01–G11.

Profile overflow reports local UNAVAILABLE with stable capacity detail, preserving CCM error schema and waiting/rejection precedence; storage corruption marks Core blocked/recovery-needed rather than inventing a new common error enum. Exact valid allocation-refusal/commit-failure API behavior must be pinned by D1 tests before runtime callers depend on it. Both current end/persist and synchronous dispatch callbacks require an audit that persistence failure cannot leak a successful admission reply or released in-RAM barrier.

## 6. Reference/catalog ingress and minimum operations

Extend existing owner normalization, not HOME. Producer captures outer Scope once; reject stale epoch/incarnation/session/mapping/association/media. Reference payload preserves complete SelectionRef tuple and presence/absence; no index/string coercion. Minimum slice REFERENCE; artwork/asset fetch pipeline excluded. Unknown asset remains absent; no secret URL in common fields.

Two fixed catalog staging slots: producer reserves, fills validated snapshot +acquisition sequence/age +private token continuity evidence, posts immutable slot index/generation and captured Scope. Owner validates slot token/scope, consumes and releases exactly once. FULL/STALE/shutdown releases/cancels. No borrowed mutable memory, no huge catalog variant in every inbox entry. Gap/overflow causes verified resync, not fabricated completeness. Full event validation precedes mutation; oversized whole update refused while preserving known catalog.

Native revision is evidence, never common catalog generation. Verified identity continuity maps private item tokens to persistent local IDs; positional reuse cannot certify continuity. Current Core complete/partial merge extended with atomic staging/commit/publication, explicit proved deletion and same-item token rebind. Complete [A,B], partial[B] preserves A/order/generation; new C appends sorted new IDs once; complete[B,A,C] increments once; label-only preserves generation. Partial query_result includes returned items in merged-relative order, not the full retained catalog. Failed allocation must not publish half a catalog/counter/reference invalidation. Old current refs become absent UNKNOWN until reacquired. Media generation changes/invalidation precede new fields. Local checks do not prove execution-time remote guard.

| Slice | Existing frozen contract / exact extension | Gate/exclusion |
| --- | --- | --- |
| source/favorites/content.enumerate |{}; bounded query, verified correlated response -> CatalogSnapshot query_result/confirmed query completion |no automatic retry/write lock; empty complete known empty; cached list alone not new query completion; internal AdapterSink typed bridge needed, no Native endpoint chosen |
| favorites.activate DEVICE_NATIVE |{favorite:SelectionRef<FAVORITE>}; single guarded activation |distinct REMOTE_RECIPE; context/selection/execution proof; no recipe/favorite mutation implementation |
| content.launch REFERENCE |current internal {tag:REFERENCE,content:SelectionRef<CONTENT>} closed union variant |station role O07/O14 unresolved; URI launch not advertised in minimum slice |
| source.select/current refs |existing select plus typed ingestion |radio/BT source not station role or audible playback proof |
| volume.set |exact numeric extension below |real target domain/ownership/outcome evidence; B06 guard alone insufficient |

Station alternatives remain proposals: CONTENT requires durable guarded content identity; CHANNEL requires verified channel/traversal semantics; SOURCE requires approved distinct source semantics. Do not alias roles to fill UI. TUNER native Favorites and REMOTE_RECIPE logical domains remain distinct, even when playing same content. No new CCM names.

## 7. Exact Rational volume proposal

Proposed, not approved: common[0/1,1/1], external[0/1,100/1], AFFINE, GRID first0/last100/step1, resolution common1/100/external1, rounding EXACT, allowed_error0, null ABSENT_UNKNOWN, out-of-domain REJECT. Evidence must prove real integer numeric-level domain/resolution; no assumed dB/nonlinear mapping or sentinel. UI rounding stays presentation only.

For reduced normalized n/d, checked wide x=100*n/d; EXACT accepts iff remainder zero and quotient0..100. 17/25->68;1/4->25;1/3 rejects DOMAIN with zero emission;0->0;1->100. Malformed/unreduced Rational fails schema, not silently canonicalized as input. Inverse integer k is reduced k/100; noninteger/outside invalid observation, null/sentinel precedence per actual descriptor. Requested echo never becomes observed actual.

Separate optional NEAREST_TIES_UP proposal: allowed_error1/200; q=floor(100*n/d), increment when2*remainder>=d, then enforce inverse common error.1/3->33, actual33/100,error1/300;137/200->69 tie,error1/200;17/25->68. Not a default permission to round. Xtensa wide arithmetic must be verified: do not assume host __int128; use tested checked quotient/remainder or cross-cancel/long division. No overflow wrap/clamp.

Future tests:101 integer forward/inverse roundtrips, exact nongrid rejection, endpoint/outside/d=0/unreduced/maximal integer cases, ties around endpoints, inverse fraction/sentinel/null, mapping-change stale captures, differing requested/actual state and failure before send. Numeric descriptor proof and target actual setter/state remain O11/O12 dependencies.

## 8. Local versus target prerequisites and future slices

| REMOTE-local work | Independently required TUNER evidence |
| --- | --- |
| Persistent registry/allocator/effect recovery |stable physical identity/reset continuity; name/IP insufficient |
| Reference/catalog owner bridge |durable item IDs through reorder/delete/reboot/duplicates, correct codec, causal complete/partial enumeration |
| Historical immutable correlation |target ledger/dedup/result retention, admission vs completion and per-key fence guarantees O10 |
| Context/generation validation |execution-time authorization/catalog/media guards, not recent readback |
| Exact numeric normalization |synchronized actual setter/state/domain/resolution, not just underflow fix |
| Honest state and resync |B05 network truth; B07/B08 coherent audio; B02/B09 semantics; bounded B10; checked B11 persistence |
| Sleep/reboot old-scope handling |server incarnation/acquisition resync and grant revocation, no old command replay |

A journal cannot create remote execution proof. TUNER A-002 guards do not close all B01–B11 or establish Native support.

| Order | Proposed later touchpoints | Acceptance gate |
| --- | --- | --- |
| D0 codec/storage qualification |new proposed control/journal_codec.cpp/.hpp and durable_journal.cpp, scoped tests |encoded worst profile/NVS capacity/GC/credential preservation, fault matrix, latency/endurance/resource bounds; approve revised design if3072 bank insufficient |
| D1 durable transaction/recovery |core.hpp Store/Journal/Effect; core.cpp constructor/persist/submit/schedule/evidence/purge/end |atomic consumed pairs, immutable history, no release before commit, §19 fault vectors, preserve Mock semantics |
| D2 reference/catalog boundary |ingress.hpp Sample/Event/Inbox/Boundary; ingress.cpp pack/unpack/validation/apply; core.cpp catalog/ingest/fresh/media_changed |stale/gap/slot ABA/full/cleanup/atomic OOM; H/I/E31/E32/F15; no borrowed data/generation overwrite |
| D3 query/Favorite/content execution |core.hpp AdapterEvidence/AdapterSink; core.cpp foundation_operation/capability/schema/validate/capture/schedule/fold |closed schemas, typed correlated query result, DEVICE_NATIVE only, no queued reference rebasing; target guards required |
| D4 general numeric |core.cpp numeric validator/validate/schedule; proposed exact numeric helper |§14 complete descriptor/arithmetic/inverse tests; target domain evidence; synthetic tests unchanged |
| D5 real production adapter/runtime |separately scoped adapter; control_home/runtime production bootstrap |approved Native revision/target guarantees/identity/auth/reconnect/freshness before real handover; NOT assigned here |

New file names are proposals, not existing implementation or authorization. Later source tasks require full normal/soak/clean build and measured frames/heap/PSRAM/queue plateau/artifact identity. Physical cuts require separately authorized owner procedure.

## 9. Independent evidence gaps and disposition

Current unmatched BIN SHA `7e8d77c21898014fbd7ea625cf2d25aa7f77941d5ba1d24484090f7361279afb`; ELF `61d077235a971919807bcff38aede169554f60de1ea7152625e5aa5b0b730525`. Pairing known, source/flashed identity UNKNOWN. ADC optimization residency hardware FAIL retained; rollback-control residency PENDING; causality unknown. No repair/sleep/ADC/artwork/UI/OTA work under this design. POWER covered1256-byte owner evidence/review target and current WoM PASS keep original scope, not new acceptance.

Remaining implementation blockers: NVS capacity/atomicity/endurance/physical fault qualification, complete immutable correlation/conflict persistence, measured bounds, target truthful state/catalog/context/ledger/security and relevant O approvals. None prevents delivery of design READY FOR REVIEW. PM-owned registers untouched; common gate OPEN / NOT PASSED. Pre-delivery fetch observed main8e1ab9d87c4647964bbc48d0e50982472a0b9cca with PM follow-up state updates and TUNER A-001 integration. Frozen provenance is now indexed there; this report preserves input-state facts rather than overwriting PM reconciliation.

## 10. Verification and publication boundary

New regression/soak/build/upload/hardware: NOT RUN (excluded). Performed source/hash inspection, exact rational/capacity arithmetic, source/SDK snapshot checks, entry/post hashes, archive/template/link/scope checks. Future matrices are NOT test PASS claims. [Validation](https://github.com/OviOneKenoby/remote_tuner_system/blob/bfaf9a579a41b45d7c3e7ded0ac1af5c2d956fec/references/device_evidence/remote/A-REMOTE-002/VALIDATION.md).

Already published before PM took over coordination:

- PR2 ordinary merge `32d6e4b8e3fcf309b040e5701a3823289213da68`; reviewed head2163358 verified before merge.
- PR3 exact A-REMOTE-002 assignment +original PM package: ordinary merge `dce15583873f66fd56239b3b6cb805135477719a`; assignment branch head recorded in package manifest. No PM register/TUNER files edited.
- Prior delivery package remains a historical UNPUBLISHED package. Latest owner instruction supersedes publication delegation for this result. Prepared result branch `remote/a-remote-002-publication-20261008`, based on current main `8e1ab9d87c4647964bbc48d0e50982472a0b9cca`, preserves PM/TUNER changes and byte-original archive/evidence. Final PR/commit/readback is reported separately after publication; no repeated PR2/PR3 or automatic merge/contract approval.

Recommended next action: PM reviews storage qualification/bounds with TUNER evidence and assigns D0 separately. No Native implementation before approval gates.

Publication verified: [PR5](https://github.com/OviOneKenoby/remote_tuner_system/pull/5), result commit `bfaf9a579a41b45d7c3e7ded0ac1af5c2d956fec`. READY FOR REVIEW; no merge/contract approval.
