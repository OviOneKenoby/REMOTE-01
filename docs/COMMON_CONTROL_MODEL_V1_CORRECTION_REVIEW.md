# Common Control Model v1 correction review

**Historical readiness verdict superseded:** the subsequent [candidate.3 freeze review](COMMON_CONTROL_MODEL_V1_CANDIDATE_3_FREEZE_REVIEW.md) found residual FR15/FR16 blockers. The PASS statements below describe the correction pass; they are not the current freeze verdict. Candidate remains unapproved and unfrozen.

2026-10-03. Reviewed `1.0.0-candidate.3` after correcting the candidate.1/2 findings. **Not approved or frozen.** This is documentation reasoning, not executable Core, firmware, adapter or protocol testing.

Read completely before editing: [candidate](COMMON_CONTROL_MODEL_V1.md), [historical freeze review](COMMON_CONTROL_MODEL_V1_FREEZE_REVIEW.md), [architecture](CONTROL_ARCHITECTURE.md), and CHANGELOG. Architecture and historical review remain unchanged. Sections 2-18 plus section 19 expectations are normative; this report records verification, not a competing specification.

## FR01-FR14 reproduction and correction

For FR01-FR12, the original counterexample below is reproduced verbatim from the historical review. FR13/FR14 were presentation/wording findings rather than execution traces. A PASS means the written candidate gives one conservative outcome for that input, not a hardware/protocol test.

### FR01 — Lifecycle closure

Original problem: ACCEPTED/SENT awaits feedback; a verified idempotent retry is permitted. Handover of its next attempt makes aggregate delivery AMBIGUOUS, which ACCEPTED forbids, but ACCEPTED -> DISPATCHED is not permitted. An ongoing count can also report PARTIALLY_SENT, which neither nonterminal state accepts. DISPATCHED/NOT_SENT at its deadline may expire under section 13, but section 11 omits that transition.

Normative correction: The nonterminal table accepts ongoing partial/ambiguous progress. Eligible retry closure enters DISPATCHED/retry wait and handover retains all attempts. Definitively unsent DISPATCHED can expire. One priority fold determines lifecycle/delivery/outcome; emitted progress is distinct units, not attempt count.

Affected normative sections checked: §§10-13,18-19. Exercising vectors: E18-E20; N,O,P; F17-F18. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR02 — Mandatory late evidence

Original problem: Identical verified late completion of an uncertain attempt can leave one Core's visible result uncertain and change another's to confirmed. The permissive late rule also lists transitions from definitely-unsent terminal results without a contradictory-evidence guard.

Normative correction: Apply valid correlated evidence idempotently and rerun the closed fold. Only uncertain, effect-possible failed, or independently correlated open-loop accepted records may resolve. Definitely-unsent/refused and already-settled contradictory claims are quarantined. Resolve original attempt barriers before folding; never schedule.

Affected normative sections checked: §§8,10-13,18-19. Exercising vectors: C,P; E07,E21; F01,F06,F09-F10,F18. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR03 — Deadline precedence

Original problem: At the same handover check the queued request's revision is stale and its deadline elapsed: section 10 checks revision first, while section 13 requires expiry for unsent work. Separately, a terminal one-way ACCEPTED result reaches its deadline: section 13 both sets deadline_elapsed after dispatch and says time alone does not change terminal results.

Normative correction: At already-admitted gate/mutation checks, elapsed deadline precedes stale captures. Terminal deadline_elapsed updates once without lifecycle/outcome/retention changes; exact cached duplicates precede validation.

Affected normative sections checked: §§10-13,18-19. Exercising vectors: F,O,P; E01,E07,E20,E22-E23; F19. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR04 — Deterministic dedup purge

Original problem: Start with 1024 terminal records all past minimum retention. One Core evicts eligible records and admits a new request; another retains them and rejects DEDUP_CAPACITY. Retrying an eligible old record likewise returns its original result or REQUEST_HISTORY_EXPIRED depending on discretionary eviction.

Normative correction: Purge every terminal record at the first event with t>=max(first closure+60000,deadline), before lookup, sorted by purge time/ticket/ID. Active/unexpired records protected; late evidence does not extend retention. Keep pairing tombstones and barrier decision state independently.

Affected normative sections checked: §§10-13,18-19. Exercising vectors: M; E07-E10; F01-F03,F19. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR05 — Unified acquisition/FIFO

Original problem: An ordinary queued command holds r2 while waiting for r1; a newly admitted recipe reserves r1 in sorted order and waits for r2. Only recipe acquisition order is specified, so ordinary acquisition can form a cycle. Admission-time recipe reservations can also block older ready work that FIFO would otherwise select.

Normative correction: Qualified TARGET(device,target)/RESOURCE(resource) keys, canonical namespace/component order, all-or-none acquisition at first dispatch for ordinary and recipe writes. Queued work holds none; older ready conflicting admission wins. Release reservation separately from actual unresolved physical keys.

Affected normative sections checked: §§10-12,17-19. Exercising vectors: E11-E12,E16-E17; F04-F05,F18. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR06 — Recipe envelope/dedup

Original problem: Different recipe steps share the parent request ID but must validate as section 10, whose first check treats a reused ID with different arguments as mismatch. The closed CommandRequest lacks the pinned recipe revision/per-step captures mentioned later. One reading exempts steps; another applies request deduplication and rejects a legitimate step.

Normative correction: Declare required immutable RecipeCapture/StepCapture and pinned revision/union in parent equality. Steps inherit parent version/ID/ticket/deadline; only outer dedup runs. Step validation starts with expiry, then typed/scope/revision/context checks.

Affected normative sections checked: §§5-6,10-12,17-19. Exercising vectors: M; E13-E15; F05-F08,F20. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR07 — Recipe result fold

Original problem: Step 1 is confirmed; step 2 loses capability before handover. REJECTED/ACKNOWLEDGED/REJECTED is permitted after all prior execution is resolved, while FAILED/ACKNOWLEDGED/UNCONFIRMED is also permitted for known incomplete compound work. The stop rule chooses neither.

Normative correction: Unstarted refusal/expiry, proved-zero refusal, known incomplete prefix, ambiguity, all-confirmed and mixed open-loop completion have unique folds. PARTIAL_EXECUTION retains cause/step; closure_error/attempt evidence preserve history. Late prefix proof cannot confirm/send suffix.

Affected normative sections checked: §§10-13,17-19. Exercising vectors: B,N,P; E14-E17; F05-F09. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR08 — Absent selection

Original problem: No eligible present sample exists, and a descriptor evidences UNSUPPORTED. The rule allows ABSENT UNKNOWN “or” the evidenced absence reason. With several bindings, it also does not choose among evidenced absence reasons.

Normative correction: Reduce all current bindings: UNKNOWN wins; else all unsupported => UNSUPPORTED; else any verified unavailability => UNAVAILABLE; else NOT_APPLICABLE. Explicit descriptors/fresh absence evidence only; offline never implies unsupported.

Affected normative sections checked: §§5,7-9,19. Exercising vectors: A,T; E24,E28; F13. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR09 — Persisted conflicts

Original problem: Fresh A/B disagree; B expires; A supplies verified RESYNC. “Still-eligible involved routes” can mean fresh candidates (A only) or retained supported conflict participants (A and B), giving cleared versus retained latch. “Clear only after” gives a necessary condition without requiring clearing when it is met.

Normative correction: Persist roster independent of diagnostic history. Expiry/removal cannot drop a participant. Clear obligatorily exactly when every non-excluded participant has current fresh agreeing verified post-conflict RESYNC and all other candidates agree, or explicit proof resolves every excluded participant. Unchanged disagreement refresh does not restart episode.

Affected normative sections checked: §§2,8-9,18-19. Exercising vectors: L; E25-E28; F11-F12. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR10 — Total TABLE mapping

Original problem: TABLE has pairs (common 0, external 0), (common 1, external 10); input common 1/2 is within domain and quantization enabled. There is no affine transfer and interpolation is forbidden. The text does not say whether to reject an off-row input or quantize among common table rows; it instead asks for nearest external value without defining that value.

Normative correction: EXACT accepts rows only. NEAREST chooses closest common row with larger-common tie; domain/error first, inverse exact external-row lookup, no interpolation. AFFINE unchanged; singleton both resolutions zero.

Affected normative sections checked: §§3,5,10,14,19. Exercising vectors: Q,R; E29-E30,E33-E34; F14. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR11 — Partial catalog merge

Original problem: Known complete catalog [A,B] receives verified incomplete [B], with no proven identity/token change. Implementations can treat the nonidentical refresh as a new generation or retain the known membership/generation because absence is unproven. An old A reference then rejects or remains valid. The complete=false merge/order rule is missing.

Normative correction: Retain omitted IDs and relative known order, update verified metadata, append new IDs lexically. Only proved merged membership/order/identity/token changes increment once. Complete authoritative order may replace; no omission deletion or label-only increment. References atomically republished.

Affected normative sections checked: §§2,6-8,10,15,18-19. Exercising vectors: H-J,R,S; E05-E06,E31-E32; F15. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR12 — Validation/error totality

Original problem: A request with a mismatched common-model version must fail, but no normalized error is prescribed. Duplicate ticket with a different request ID and old-epoch ticket checks lack explicit ID-ticket binding/error rules. Different error codes or lookup order are reasonable despite the fixed validation-stage order.

Normative correction: Ordered first-failure table specifies existing normalized codes, version/missing captures, malformed references, current/old epoch and ID-ticket pairing, cached/expired history, auth/context waits, deadline precedence and dedup-before-queue capacity. Core construction inability differs from supplied malformed capture.

Affected normative sections checked: §§1-3,5-6,10-13,17-19. Exercising vectors: H,M,O,R,T; E01-E09,E23,E35; F02-F03,F16,F20. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR13 — Conformance array order

Original problem: sections 8/9 leave conflict_set/history ordering and retention presentation unspecified even though `[]` is an ordered list. Selection of the primary value is deterministic; ancillary list output is not.

Normative correction: conflict_set uses exact selection order; persisted participants sort binding ID. Diagnostic history retention/order excluded from decision equality and cannot hold sole copies of causal/mandatory decision state.

Affected normative sections checked: §§1,8-9,18-19. Exercising vectors: L; E27; F11-F13. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

### FR14 — Type/wording closure

Original problem: sections 4/5/6 refer to `Version`, `Unit` and generic `Record` without explicit aliases/path-bearing unit structure; unsupported descriptors refer to `status` although the fields are support/availability. Section 7's “No field MUST be manufactured” should use the unambiguous prohibition MUST NOT. Identifier-name shorthand must likewise inherit the declared scoped type, not a new implicit namespace.

Normative correction: Define Version, Record, scoped identifier/counter shorthand and path-bearing Unit; replace phantom status with support/availability; state MUST NOT manufacture fields. Added result/capture records are explicitly typed/closed; schema list bounds distinguish operation arguments from derived attempt history.

Affected normative sections checked: §§1-7,10-12,14-15,19. Exercising vectors: A,T; E13,E33,E35; F06,F16,F21. **PASS after correction.** Remaining ambiguity: none found for this contract; actual protocol guarantees remain unverified and grant no support.

## Complete consistency pass

Cross-checked each correction against request/result/schema closure, lifecycle aggregation/transition closure, retry/fallback eligibility, expiry and dedup order, immutable recipe captures and stopped-parent folding, FIFO/reservation versus physical barriers, observed absence/conflict state, exact numeric transfer and catalog generation. Sections 4-8 retain device/target boundaries, typed operations/fields, source causality and freshness. Sections 16-18 preserve identity rollback, shared effects and serialized event decisions. Section 20 is unchanged: no additional ecosystem claim or protocol research was introduced.

Direct transitions use Q=QUEUED, D=DISPATCHED, A=nonterminal ACCEPTED, AT=terminal ACCEPTED, C=CONFIRMED_COMPLETED, R=REJECTED, F=FAILED, X=EXPIRED_BEFORE_SEND, U=UNCERTAIN_OUTCOME:

| Source | Legal scheduling transitions / guarded closed evidence |
| --- | --- |
| Q | Q,D,R,F,X. Synchronous final result must pass through committed D handover first. |
| D | D,A,AT,C,R,F,X,U, subject to fold predicates; X requires NOT_SENT and no executable handover. |
| A | D,A,AT,C,R,F,U. Never Q/X; retry wait/handover retains historical delivery. |
| AT | AT,C,R,F,U only on valid independently correlated historical evidence; scheduling stays terminal. |
| U | U,C,R,F,AT when mandatory closed fold resolves evidence; never resumes. |
| F | F,C,R,U,AT only if historical possible execution exists and closed fold permits; known halted prefix never completes its suffix. |
| C | C only; contradictory refusal invalid, consistent evidence may append. |
| R,X / definitely-unsent F | Same state only; no executed rewrite from impossible history. |

Table cells are permissions constrained by the one fold, not choices. Proof of zero execution does not erase SENT/ACKNOWLEDGED or emitted counts. Prefix ACKNOWLEDGED with incomplete whole-request emission aggregates PARTIALLY_SENT. Correlated completion carries recipient acknowledgement; target refusal without zero-execution proof stays uncertain. Barrier proof is committed before parent folding. Mixed confirmed/feedback-NONE full recipes close AT/UNCONFIRMED; STRICT physical ordering remains blocked. No result/query schema implies field publication.

The schemas explicitly include late-proof retention timestamps/errors and immutable step captures; Record arguments remain closed and guard captures mandatory. Type primitives remain exact; argument/domain/counter limits and dedup/queue bounds remain enforced. Core capacity/validation refusals cannot forge a malformed full result. Waiting reason is nonterminal state, not a terminal error. No late reply, terminal clock tick, eviction, reboot, rollback or catalog merge grants replay.

## Original A-T vectors re-evaluated

| Vector | Candidate.3 expected result / consistency |
| --- | --- |
| A | PASS: unknown power remains absent after toggle; no present overlay. |
| B | PASS: full feedback-NONE emission => AT/SENT/UNCONFIRMED. |
| C | PASS: unresolved at deadline => U/AMBIGUOUS/UNKNOWN/TIMEOUT; barrier persists. |
| D | PASS: uncertain N cannot fallback/replay. |
| E | PASS: newer admitted intent cancels old retry; new strict conflicting work queues. |
| F | PASS: before-deadline revoked capture rejects STALE_PRECONDITION; at deadline O wins. |
| G | PASS: after-handover invalidation preserves attempt, forbids more work, waits for resolution/deadline. |
| H | PASS: wrong scope INVALID_REFERENCE; stale generation STALE_PRECONDITION. |
| I | PASS: proved token reuse increments generations; queued old ref stale. |
| J | PASS: obsolete artwork discarded. |
| K | PASS: old cache age/session cannot rejuvenate truth. |
| L | PASS: authoritative selected; full ordered conflict_set and persistent latch retained. |
| M | PASS: altered retained envelope REQUEST_ID_MISMATCH, no emission. |
| N | PASS: local closure of ambiguous compound => U, lower-bound progress, no resume. |
| O | PASS: definitely unsent deadline => X, including simultaneous stale capture. |
| P | PASS: fully emitted awaiting feedback expires U/SENT/UNCONFIRMED; valid whole late proof obligatorily C without replay. |
| Q | PASS: AFFINE 17/25 -> external 7 -> common 7/10, error 1/50. |
| R | PASS: verified matching seek window permits 4000; out-of-window invalid; unavailable window construction refused. |
| S | PASS: rollback raises generations; reject unsent, preserve in-flight identity/barriers. |
| T | PASS: readable title does not imply any writable operation. |

## E01-E35 re-evaluated

Inputs are reproduced in normative section 19.1, retaining the historical review's order/defaults. Each result below is the unique written result.

| Vector | Result after correction |
| --- | --- |
| E01 | PASS — REJECTED/NOT_SENT/REJECTED, STALE_PRECONDITION; before-deadline stage 4. |
| E02 | PASS — Original attempt preserved; invalidated=true; no further attempts. |
| E03 | PASS — STALE_PRECONDITION wins over UNAUTHORIZED before deadline. |
| E04 | PASS — STALE_PRECONDITION; no handover. |
| E05 | PASS — STALE_PRECONDITION; original scope never retargeted. |
| E06 | PASS — STALE_PRECONDITION; no seek. |
| E07 | PASS — Return current cached CONFIRMED_COMPLETED, deadline_elapsed=true; no new work. |
| E08 | PASS — REQUEST_HISTORY_EXPIRED/OLD_EPOCH; no replay. |
| E09 | PASS — Purge all eligible records first. Fresh valid request can enter; old matching pair => REQUEST_HISTORY_EXPIRED/HISTORY_EXPIRED. |
| E10 | PASS — Block affected targets/resources pending verified reconciliation; no reconstruction. |
| E11 | PASS — Queue B with ORDERING_BLOCKED; shared barrier retained. |
| E12 | PASS — A's partial acquisition is forbidden; both requests acquire their whole qualified union or nothing. Older ready A gets it first; no cycle. |
| E13 | PASS — Outer dedup once; both StepCaptures validate independently under parent ID/ticket/deadline. No mismatch from step argument differences. |
| E14 | PASS — FAILED/PARTIALLY_SENT/UNCONFIRMED, PARTIAL_EXECUTION cause STALE_PRECONDITION, step_index=2; confirmed=emitted=1, total=captured recipe length; no suffix. |
| E15 | PASS — UNCERTAIN_OUTCOME/AMBIGUOUS/UNKNOWN, AMBIGUOUS_DELIVERY; suffix halted, barriers retained. |
| E16 | PASS — Terminal ACCEPTED/SENT/UNCONFIRMED; release scheduling reservation and LOCAL_ONLY physical barrier. |
| E17 | PASS — Terminal ACCEPTED/SENT/UNCONFIRMED; release reservation but keep STRICT physical barrier indefinitely without evidence. |
| E18 | PASS — ACCEPTED -> DISPATCHED retry wait, then DISPATCHED/AMBIGUOUS at new handover; same-event direct transition legal; prior attempt retained. |
| E19 | PASS — Nonterminal DISPATCHED/PARTIALLY_SENT/UNCONFIRMED; total=3, emitted=1, confirmed=0, uncertain_remaining=false for known remainder. |
| E20 | PASS — DISPATCHED/NOT_SENT -> EXPIRED_BEFORE_SEND/NOT_SENT/UNKNOWN, DEADLINE_EXPIRED. |
| E21 | PASS — Mandatory CONFIRMED_COMPLETED/ACKNOWLEDGED/CONFIRMED; result revision increases, original barrier released; no replay. |
| E22 | PASS — Same terminal ACCEPTED lifecycle/outcome/error; deadline_elapsed=true once; first closure/retention unchanged. |
| E23 | PASS — EXPIRED_BEFORE_SEND/NOT_SENT/UNKNOWN, DEADLINE_EXPIRED wins over stale revision. |
| E24 | PASS — ABSENT UNSUPPORTED iff all current relevant binding descriptors explicitly prove unsupported; UNKNOWN if any unknown contributor. |
| E25 | PASS — Latch persists with B in roster; A-only RESYNC insufficient. Clear obligatorily after fresh agreeing post-conflict verified A+B RESYNC. |
| E26 | PASS — Invalidate that source's current sample; latch single participant; verified later RESYNC required. |
| E27 | PASS — Select lexically earlier binding; conflict_set in authoritative/priority/binding/observation order; diagnostic history excluded. |
| E28 | PASS — Age equality STALE; unknown age excluded; cache cannot rejuvenate; old session discarded. |
| E29 | PASS — TABLE picks common 1/external 10; actual inverse 1, error=1/2 accepted. EXACT or tighter error rejects. |
| E30 | PASS — AFFINE 0->0,1->10,13/20->7; sentinel-bearing representable descriptor invalid. |
| E31 | PASS — Known merged [A,B], generation 5; incomplete returned [B]; A reference remains current. |
| E32 | PASS — No name/IP identity inference; proved token reuse increments catalog/token generation; old ref stale. |
| E33 | PASS — UInt64 max primitive valid; overflow/unreduced logical Rational/NaN/infinity invalid; normalized 0 and 1 valid. |
| E34 | PASS — 9000 valid; 9001 INVALID_ARGUMENT/DOMAIN; exhausted generation stops mutation without wrap. |
| E35 | PASS — Version mismatch INVALID_ARGUMENT/VERSION_MISMATCH; retained swapped pair REQUEST_ID_MISMATCH/ID_TICKET_PAIR; no emission. |

## Fresh adversarial review after correction

These cases challenge the corrected rules rather than repeating only FR counterexamples. F01/F08/F09/F17/F18 exposed missing cross-boundary wording during the pass; it was repaired before the final re-evaluation: purged cache versus retained barrier state, emitted-but-proved-refused progress, late failure precedence over open-loop acceptance, retry-wait timing, and barrier proof before the result fold. F21 exposed an internal evidence-list bound clarification. F22 then separated proved no-execution/refusal from unresolved historical byte extent: reject with retained ambiguous delivery when all effects are proved absent, or acknowledge whole completion when all other attempts are completed/proved nonexecuting. No new operation/transport or external guarantee was added.

| Case | Counterexample searched | Unique result / final status | Sections |
| --- | --- | --- | --- |
| F01 | Late completion arrives exactly at purge_at; the uncertain physical barrier remains. | PASS — Purge full result before lookup. Pair retry => HISTORY_EXPIRED. Independently correlated proof resolves its stored barrier; no recreated result or work. | 11-13,18 |
| F02 | All 1024 protected dedup slots and all 64 queue slots occupied; fresh valid intent. | PASS — UNAVAILABLE/DEDUP_CAPACITY wins; no eviction/handover. Refused pair is tombstoned. | 10,12 |
| F03 | Same capacity state, but matching retained duplicate or mismatched version. | PASS — Duplicate returns current result before capacity. Fresh version mismatch returns INVALID_ARGUMENT/VERSION_MISMATCH before capacity; no forged result schema. | 10-12 |
| F04 | Request acquires TARGET(A,X) and RESOURCE(X); another needs reversed declaration order. | PASS — Qualified namespaces remain distinct, canonical order TARGET then RESOURCE; whole union or none, no cycle. | 10,12,17-18 |
| F05 | Recipe halts after confirmed A prefix; unsent B suffix has unrelated keys. | PASS — Release whole reservation; confirmed A proof has already released its barrier, never-sent B has none. If A was open-loop STRICT instead, its actual keys stay blocked. | 11-12,17 |
| F06 | Late evidence confirms a halted recipe's only sent prefix; suffix never sent. | PASS — FAILED/PARTIAL_EXECUTION remains, prefix confirmation updates progress/history; no whole completion or suffix dispatch. | 11-13 |
| F07 | Recipe completes one confirmed and one feedback-NONE step. | PASS — Terminal ACCEPTED/ACKNOWLEDGED/UNCONFIRMED; emitted=2, confirmed=1, no error; STRICT open-loop barrier persists. | 11-12 |
| F08 | First recipe step is locally emitted then explicitly refused with verified no execution. | PASS — REJECTED/PARTIALLY_SENT/REJECTED if total>1; keep emitted=1, confirmed=0 and refusal origin. No execution inferred and no suffix. | 11-12 |
| F09 | Terminal open-loop ACCEPTED later receives valid correlated proof of failure/refusal. | PASS — Mandatory FAILED or proved whole-no-execution REJECTED under the closed fold; never retain success merely because local emission finished. | 11 |
| F10 | Proof of no-execution/definitely-unsent EXPIRED receives claimed completion; confirmed request receives contradictory refusal. | PASS — Quarantine contradictions; no outcome rewrite, replay, field rollback or fabricated execution. | 11,18 |
| F11 | A/B conflict; B expires/deletes; A resyncs. Later B resyncs with matching current generations/value. | PASS — Deletion alone leaves B participant. Clear exactly after verified fresh agreeing post-conflict A+B evidence (or explicit correction resolving B), not before. | 8-9 |
| F12 | Repeated unchanged conflicting samples while two RESYNC acquisitions run; one RESYNC becomes stale. | PASS — Unchanged refresh does not reset conflict episode. Stale resync cannot satisfy clearing; acquire fresh verified evidence from all participants. | 8-9 |
| F13 | No current sample; unsupported A plus unknown B, then unsupported A plus verified unavailable B. | PASS — First selected ABSENT UNKNOWN; second UNAVAILABLE; never infer all-route unsupported from offline. | 5,8-9 |
| F14 | Singleton TABLE row (1/2->7), common domain [0,1]; input 3/5, NEAREST, error=1/10. | PASS — Accept external 7, inverse 1/2, exact error 1/10. EXACT rejects off-row; non-row external observation invalid; no interpolation. | 3,14 |
| F15 | Partial catalog [C,B] follows known [A,B], then complete [B,A,C]; metadata-only update follows. | PASS — Merge [A,B,C] and increment once; complete reorders/increments once; metadata-only preserves generation. Omitted A never deleted by partial data. | 2,10,15 |
| F16 | Core has no verified seek window versus supplied envelope omits required capture despite a known window. | PASS — Construction refusal UNAVAILABLE/NO_VERIFIED_SEEK_WINDOW versus malformed-envelope INVALID_ARGUMENT/MISSING_CAPTURE; neither sends. | 5-6,10,14 |
| F17 | Retry closure from ACCEPTED occurs before delay; handover later becomes ambiguous. | PASS — Close attempt, enter DISPATCHED/retry wait, then DISPATCHED/AMBIGUOUS; previous SENT evidence preserved; no terminal reopening. | 10-13 |
| F18 | Early target no-execution proof releases an attempt barrier while parent incomplete; all attempts eventually confirmed. | PASS — Release resolved per-attempt barrier before folding, so no circular barrier/completion prerequisite. Other unresolved keys remain. | 11-12,17 |
| F19 | Clock is near UInt64 max; retention/delay arithmetic would overflow; restart follows. | PASS — Retention remains protected until epoch retirement; retry timer overflow forbids replay; old tickets OLD_EPOCH, old effects/barriers persist separately. | 2-3,10,12-13 |
| F20 | Fallback authorization unavailable while chosen route remains valid; completed recipe-prefix generation later changes. | PASS — Transient dormant-route availability cannot reject chosen route; changed captured revision still invalidates. Completed prefix remains historical, remaining captures revalidate; no rebase. | 10-12 |
| F21 | Recipe length 1024 with up to three allowed attempts per step would exceed an undifferentiated 1024 LIST bound. | PASS — Operation argument/catalog/recipe limits remain 1024, count 65535; derived attempt history explicitly permits at most 3072 (1024*3) while preserving all prior evidence. No retry limit is raised. | 5,10-12 |
| F22 | A terminal ambiguous send receives a verified whole-intent refusal/no-execution proof but local byte extent stays unknown; alternatively a verified retry completes after prior attempt is fenced as no execution. | PASS — Refusal resolves target outcome to REJECTED with historical AMBIGUOUS delivery/lower-bound progress, no replay. Whole requested effect proved plus every other attempt completed/no-execution yields CONFIRMED_COMPLETED/ACKNOWLEDGED; prior uncertain byte evidence is retained. | 11-12 |

Numeric boundary reasoning used exact fractions: AFFINE 17/25 maps to 7/10 with error 1/50; TABLE midpoint selects larger common row with error 1/2; singleton 3/5 to 1/2 error 1/10; seek 9000 is inclusive and 9001 invalid. Retention equality purges before duplicate lookup; freshness equality is stale; overflow never wraps. No interpolation, clamp, silent font/UI change or external guarantee was introduced.

## Validation, scope and verdict

Validated complete numbered-section coverage, all 14 findings, every A-T and E01-E35 row, 22 fresh cases, exact candidate revision, repaired type/wording links and local Markdown links. Rechecked transition and schema closure and every first-failure/queue/purge branch. File hashes verify exactly three documentation files changed: `docs/COMMON_CONTROL_MODEL_V1.md`, this review, and `CHANGELOG.md`; architecture and historical freeze review are unchanged. No firmware/API/adapter/networking/UI/hardware/configuration/library edits; no build, upload, tag or freeze. These checks prove document coverage/change scope, not implementation conformance.

FR01-FR14: corrected and PASS in the written-contract review. Remaining internal ambiguity/blocker: none found after this adversarial pass. Required actual binding/protocol/context/fencing verification is explicitly still UNKNOWN and remains a separate future gate. Owner physical navigation, display qualification and HOME observations are unrelated hardware evidence, not protocol validation.

NO BLOCKERS FOUND — CANDIDATE READY FOR OWNER FREEZE REVIEW
