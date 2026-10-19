# Common Control Model candidate.4 freeze review

2026-10-03. Reviewed `1.0.0-candidate.4`. **UNAPPROVED / UNFROZEN.** Documentation reasoning only, not an implemented Core, executable conformance test, adapter/protocol test or hardware prediction.

Read the six requested documents before editing: current candidate, candidate.3 freeze review, correction review, original freeze review, architecture and CHANGELOG. Historical findings remain unchanged. This review addresses FR15/FR16 only, retaining FR01-FR14 unless a concrete propagation issue required a local correction.

## FR15 — exact normative correction and result

**CORRECTED — PASS.** Normative changes: §§11,12,17; conformance coverage: §19.3 R01/R02 and G01-G08/G11-G14.

- `guaranteed_no_execution` now explicitly requires valid correlated proof of BOTH no prior execution and no possible later execution across every captured affected target/resource. Past-only proof and future-only fence are insufficient for this whole-attempt flag.
- Physical barriers are per attempt/key. Valid proof releases only its covered resolved keys; unrelated/other-owner barriers remain. An incomplete/ambiguous attempt retains a barrier while physical execution on that key remains unresolved. A valid future-only fence can settle future ordering without settling past effect or enabling N fallback.
- Delivery/attempt evidence is historical. Execution/fence proof alone cannot rewrite Attempt.delivery, reduce emitted counts or manufacture ACK/CONFIRMED/state. Appending proof and deriving execution/barrier state is permitted; actual new delivery evidence can refine delivery without erasing earlier evidence.
- Historical AMBIGUOUS delivery is not itself a permanent veto on a captured N fallback when every relevant prior attempt has full past/future absence proof. Existing policy flags, timers, limits, captures, equivalence, supersession, deadline, acquisition and compound restrictions still apply.
- The proof must be available before parent terminal closure in the closing/fallback-decision event. Late proof can settle history/keys, never schedule the old parent. Independently admitted work still uses its existing FIFO/validation gate; no new intent is fabricated.

Cross-check exposed two necessary FR15 propagation repairs, applied locally in §11: definitive FAILED may retain AMBIGUOUS delivery when execution uncertainty is resolved but bytes remain unknown; and a proved-nonexecuting historical ambiguity does not dominate aggregate delivery after another known complete emission covers the same logical units/step. Otherwise a safe fallback's feedback-NONE success would produce an illegal ACCEPTED/AMBIGUOUS combination. The prior Attempt remains AMBIGUOUS; aggregate refinement comes from actual covering emission, not the absence proof. These complete FR01/FR02 interactions without changing operations/scope.

No-past/no-future proof is a condition on hypothetical verified input, not a new claim that IR/Matter/HA/TUNER supplies it. Actual contracts remain unverified.

## FR16 — exact normative correction and result

**CORRECTED — PASS.** Normative changes: §11 uncertainty definition, fold/table/closed-evidence qualification; §12 recipe stop terminology; §13 deadline closure qualification; vectors R03/R04 and G09/G10/G13/G15.

The blanket closed-attempt uncertainty sentence now applies ONLY to genuine unresolved requested-unit emission/execution extent or an explicitly declared CORRELATED_RESULT obligation. Known local feedback-NONE emission has no correlated-result obligation; merely unobserved execution cannot override the existing folds:

- Full known emission: terminal ACCEPTED/SENT (or actually derived ACKNOWLEDGED)/UNCONFIRMED; no error or synthetic confirmation.
- Known stopped partial count/recipe: FAILED with retained partial delivery, PARTIAL_EXECUTION and underlying cause; known lower bounds/progress preserved; no automatic restart/resumption.
- Unknown relevant extent or unresolved promised correlated result: UNCERTAIN_OUTCOME with derived historical delivery/outcome and normal pre-deadline ambiguity/deadline TIMEOUT error.
- Physical ordering is independent: LOCAL_ONLY full complete open-loop work releases its barrier; STRICT may intentionally retain it. Incomplete emitted work retains unresolved keys under either mode until verified proof settles them.

Future-only fencing does not resolve an unknown past/correlated obligation. Complete zero-execution proof does not turn failure into success. Existing rejection-origin, contradiction quarantine, terminal metadata and late-parent no-rescheduling rules remain.

## R01-R04 — expected normative results

The inputs and unique outputs below are also in normative §19.3. R02 uses proof already available when the attempt closes; timer readiness later causes the actual fallback. No proof-after-terminal workaround is allowed.

| Vector | Input | Expected result / review |
| --- | --- | --- |
| R01 | Retained terminal UNCERTAIN_OUTCOME/AMBIGUOUS/UNKNOWN/TIMEOUT at deadline; one STRICT attempt. Valid correlated proof then establishes no past execution AND no future execution on all its captured keys; bytes remain unknown; proof is not an explicit target refusal. | PASS — guaranteed_no_execution=true; release only that attempt's resolved barriers. Closed fold => FAILED/AMBIGUOUS/UNCONFIRMED with initiating TIMEOUT retained, first_terminal_time/closure_error/deadline_elapsed unchanged. Historical delivery/evidence/lower bounds unchanged except appended proof. No execution, confirmation, field or replay inferred. If the proof also explicitly refuses the whole intent, existing REJECTED/AMBIGUOUS fold instead applies. |
| R02 | N power.toggle, captured fallback_on_no_execution=true, automatic=false, max_attempts=2, delay_ms=1; first attempt closes at t=100 with historical AMBIGUOUS delivery AND valid no-past/no-future-execution proof already available; deadline=1000; equivalent fallback READY, matching captures, no supersession/other barrier. | PASS — Keep parent nonterminal DISPATCHED/retry wait, prior AMBIGUOUS evidence unchanged. At first event t>=101 and t<1000, hand over exactly one captured fallback under normal gate/limit. Historical ambiguity alone does not veto it. If fallback later fully emits with feedback NONE, terminal ACCEPTED/SENT/UNCONFIRMED; aggregate uses the covering emission, prior Attempt remains AMBIGUOUS. Proof available only after parent closure MUST NOT allow any fallback. |
| R03 | One-way power.toggle; complete known local emission; feedback NONE; no target observation; retry disabled; all actual target/resource modes LOCAL_ONLY. | PASS — Terminal ACCEPTED/SENT/UNCONFIRMED, error ABSENT NOT_APPLICABLE, emitted=total=1, confirmed=0, uncertain_remaining=false. Release reservation and LOCAL_ONLY physical barrier. STRICT variant has identical lifecycle/outcome but retains physical ordering barrier until valid proof. No target state/confirmation inferred. |
| R04 | One-way volume.step count=3; exactly unit 1 fully emitted, units 2/3 definitely not emitted; local UNAVAILABLE failure stops work; feedback NONE; retry disabled; extent known. | PASS — FAILED/PARTIALLY_SENT/UNCONFIRMED; error PARTIAL_EXECUTION with cause UNAVAILABLE; progress={total:3,emitted:1,confirmed:0,uncertain_remaining:false}. Retain physical barrier while execution of emitted work remains unresolved, including LOCAL_ONLY; no automatic suffix/full replay and no synthetic confirmation. |

## A-T / E01-E35 / F01-F22 regression

All 77 prior vector rows are unchanged in the candidate. Each was re-evaluated against the repaired bodies; the table records its written expected result, not runtime execution. Full/open-loop, known-partial, late proof and barrier cases were checked against the revised definitions, not merely counted.

| Vector | Review / retained expected result |
| --- | --- |
| A | PASS — Power remains ABSENT UNKNOWN; no present optimistic power; outcome as B |
| B | PASS — terminal ACCEPTED, SENT, UNCONFIRMED, no error; never CONFIRMED_COMPLETED |
| C | PASS — UNCERTAIN_OUTCOME, AMBIGUOUS, UNKNOWN, TIMEOUT; deadline_elapsed true; barrier retained |
| D | PASS — No second emission; original uncertain result retained; fallback blocked regardless of route availability |
| E | PASS — Older retry eligibility permanently false; newer queues behind unresolved strict barrier; fence/completion required before send |
| F | PASS — REJECTED, NOT_SENT, REJECTED, STALE_PRECONDITION; no emission |
| G | PASS — invalidated true, original attempt retained, no further attempts; result remains in-flight until evidence/deadline, then C if unresolved |
| H | PASS — Wrong scope: INVALID_REFERENCE; stale generation: STALE_PRECONDITION; REJECTED/NOT_SENT; no selection |
| I | PASS — Increment catalog/token generations; reject queued A as STALE_PRECONDITION; without verified execution guard selection support UNKNOWN |
| J | PASS — Discard; current artwork remains absent or generation-6 artwork; no rollback |
| K | PASS — STALE, not selected; arrival does not refresh age; old-session callbacks discarded; verified RESYNC required |
| L | PASS — Select authoritative ON, retain both in conflict_set, latch conflict; state-dependent emulation blocked; priority does not prove OFF false |
| M | PASS — REQUEST_ID_MISMATCH; existing result unchanged; no new emission |
| N | PASS — total=3, emitted=1 lower bound, confirmed=0, uncertain_remaining=true; UNCERTAIN_OUTCOME/AMBIGUOUS/UNKNOWN; no full replay or automatic continuation |
| O | PASS — EXPIRED_BEFORE_SEND, NOT_SENT, UNKNOWN, DEADLINE_EXPIRED; no physical execution by this request |
| P | PASS — UNCERTAIN_OUTCOME, SENT, UNCONFIRMED, TIMEOUT; possible execution preserved; late verified whole completion MUST revise the retained result to CONFIRMED_COMPLETED and release its resolved barrier; no scheduling/field rollback |
| Q | PASS — Emit external 7; observed common 7/10 with resolution 1/10, not 17/25; error 1/50 within 1/20 |
| R | PASS — Valid admission/dispatch; completion still requires actual evidence; 10000 => INVALID_ARGUMENT; missing window => UNAVAILABLE |
| S | PASS — Restore original IDs with higher generations; reject unsent as STALE_PRECONDITION; preserve dispatched historical identity/possible execution and resource barriers |
| T | PASS — Title selectable; any write rejects UNSUPPORTED_OPERATION; device class/family does not grant controls |
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
| E18 | PASS — ACCEPTED -> DISPATCHED on retry wait; next handover DISPATCHED/AMBIGUOUS/UNKNOWN. Same-event direct transition legal; retain prior attempts. |
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
| F01 | PASS — Purge full result before lookup. Pair retry => HISTORY_EXPIRED. Independently correlated proof resolves its stored barrier; no recreated result or work. |
| F02 | PASS — UNAVAILABLE/DEDUP_CAPACITY wins; no eviction/handover. Refused pair is tombstoned. |
| F03 | PASS — Duplicate returns current result before capacity. Fresh version mismatch returns INVALID_ARGUMENT/VERSION_MISMATCH before capacity; no forged result schema. |
| F04 | PASS — Qualified namespaces remain distinct, canonical order TARGET then RESOURCE; whole union or none, no cycle. |
| F05 | PASS — Release whole reservation; confirmed A proof has already released its barrier, never-sent B has none. If A was open-loop STRICT instead, its actual keys stay blocked. |
| F06 | PASS — FAILED/PARTIAL_EXECUTION remains, prefix confirmation updates progress/history; no whole completion or suffix dispatch. |
| F07 | PASS — Terminal ACCEPTED/ACKNOWLEDGED/UNCONFIRMED; emitted=2, confirmed=1, no error; STRICT open-loop barrier persists. |
| F08 | PASS — REJECTED/PARTIALLY_SENT/REJECTED if total>1; keep emitted=1, confirmed=0 and refusal origin. No execution inferred and no suffix. |
| F09 | PASS — Mandatory FAILED or proved whole-no-execution REJECTED under the closed fold; never retain success merely because local emission finished. |
| F10 | PASS — Quarantine contradictions; no outcome rewrite, replay, field rollback or fabricated execution. |
| F11 | PASS — Deletion alone leaves B participant. Clear exactly after verified fresh agreeing post-conflict A+B evidence (or explicit correction resolving B), not before. |
| F12 | PASS — Unchanged refresh does not reset conflict episode. Stale resync cannot satisfy clearing; acquire fresh verified evidence from all participants. |
| F13 | PASS — First selected ABSENT UNKNOWN; second UNAVAILABLE; never infer all-route unsupported from offline. |
| F14 | PASS — Accept external 7, inverse 1/2, exact error 1/10. EXACT rejects off-row; non-row external observation invalid; no interpolation. |
| F15 | PASS — Merge [A,B,C] and increment once; complete reorders/increments once; metadata-only preserves generation. Omitted A never deleted by partial data. |
| F16 | PASS — Construction refusal UNAVAILABLE/NO_VERIFIED_SEEK_WINDOW versus malformed-envelope INVALID_ARGUMENT/MISSING_CAPTURE; neither sends. |
| F17 | PASS — Close attempt, enter DISPATCHED/retry wait, then DISPATCHED/AMBIGUOUS; previous SENT evidence preserved; no terminal reopening. |
| F18 | PASS — Release resolved per-attempt barrier before folding, so no circular barrier/completion prerequisite. Other unresolved keys remain. |
| F19 | PASS — Retention remains protected until epoch retirement; retry timer overflow forbids replay; old tickets OLD_EPOCH, old effects/barriers persist separately. |
| F20 | PASS — Transient dormant-route availability cannot reject chosen route; changed captured revision still invalidates. Completed prefix remains historical, remaining captures revalidate; no rebase. |
| F21 | PASS — Operation argument/catalog/recipe limits remain 1024, count 65535; derived attempt history explicitly permits at most 3072 (1024*3) while preserving all prior evidence. No retry limit is raised. |
| F22 | PASS — Refusal resolves target outcome to REJECTED with historical AMBIGUOUS delivery/lower-bound progress, no replay. Whole requested effect proved plus every other attempt completed/no-execution yields CONFIRMED_COMPLETED/ACKNOWLEDGED; prior uncertain byte evidence is retained. |

F18's barrier-order assertion passes. Its phrase “all attempts eventually confirmed” cannot authorize confirmation of an attempt already proved never executed: any such same-attempt contradictory claim MUST be quarantined by the existing §11 rule. Successful later attempts/steps can confirm the requested effect while a prior nonexecuting attempt stays nonexecuting. This interpretation adds no permission or historical rewrite; G14 explicitly exercises contradictory evidence.

## FR01-FR16 interaction check

| Finding | Candidate.4 result |
| --- | --- |
| FR01 | PASS: ongoing DISPATCHED/retry/expiry combinations unchanged; guarded FAILED/AMBIGUOUS completes the no-execution-proof closure; aggregate coverage prevents an illegal open-loop ACCEPTED/AMBIGUOUS after safe fallback. |
| FR02 | PASS: valid late proof updates closed history idempotently; contradiction quarantine preserved; no old-parent scheduling. Independent admitted intents may proceed only through their own normal gate after key release. |
| FR03 | PASS: expiry still precedes stale captures; deadline_elapsed, first closure and terminal no-reopening rules unchanged. |
| FR04 | PASS: purge timing/order, retention and tombstones unchanged; mandatory barrier state survives purge. |
| FR05 | PASS: qualified all-or-none reservations/FIFO unchanged; release only proved per-attempt keys, not other owners or the unresolved suffix/prefix. |
| FR06 | PASS: immutable recipe capture, shared parent and outer-only dedup unchanged. |
| FR07 | PASS: known partial count/recipe failure, cause/prefix history and mixed open-loop completion remain deterministic; no resumed prefix/suffix. |
| FR08 | PASS: absence reduction unchanged; no-execution proof creates no field value. |
| FR09 | PASS: persisted roster and mandatory verified-resync clearing unchanged; command fence proof cannot clear field conflict by implication. |
| FR10 | PASS: TABLE/AFFINE quantization/inverse/domain/error rules unchanged. |
| FR11 | PASS: incomplete catalog merge/order/generation rules unchanged. |
| FR12 | PASS: validation order/error vocabulary/capacity unchanged except exact candidate.4 version check; stale/authorization/supersession checks still veto fallback. |
| FR13 | PASS: conformance ordering and diagnostic/history distinction unchanged. |
| FR14 | PASS: shorthand/closed types/MUST NOT wording retained; no new record/operation/enum or error code. |
| FR15 | CORRECTED / PASS: whole no-past/no-future proof and per-key future fence resolution distinguished; historical delivery alone not a retry/barrier veto; terminal proof cannot reopen parent. |
| FR16 | CORRECTED / PASS: uncertainty closure only for genuine extent or declared correlation; full/known-partial feedback NONE use acceptance/failure folds. |

Schema closure was checked against the new guarded FAILED/AMBIGUOUS cell and old ACCEPTED/CONFIRMED/REJECTED/UNCERTAIN cells. The existing result schema inherits the revised table; no new record, scalar, enum, operation, error code or retry flag was added. Other numbered sections remain byte-equivalent after line-ending normalization: 1,3-9,14-16,18,20. Section 13's time/precedence/terminal-flag rules are unchanged; only its closure wording is qualified by FR16. Sections 2/10 change exact candidate version only; 11/12/13/17 contain the repairs; 19 adds conformance vectors; 21/header records readiness.

## Fresh adversarial pass

The following 15 new vectors challenge more than R01-R04; all are normative §19.3 cases. Particular failure modes searched: future-only mistaken for zero-execution proof, partial proof unlocking entire resource unions, late proof reopening terminal work, purge/restart erasing necessary barriers, stale/superseded/expired retry, partial replay, false confirmation and duplicate/contradictory evidence.

| Vector | Adversarial input | Expected result / review |
| --- | --- | --- |
| G01 | Future-only fence covers all keys of an ambiguous N attempt; past execution unknown. | PASS — Release fenced future-ordering barriers only; guaranteed_no_execution remains false. Past/correlated outcome stays uncertain; no non-idempotent fallback or fabricated completion. |
| G02 | No-past/no-future proof covers resource r1 only; target/r2 remain unresolved. | PASS — Release only this attempt's r1 barrier, keep target/r2 and any other r1 owner. Whole-attempt flag remains false; no N fallback. |
| G03 | Full no-past/no-future proof arrives after terminal uncertainty, including after deadline. | PASS — Mandatory closed-result fold may become FAILED or explicit-refusal REJECTED; barriers resolve, first closure/retention unchanged. Parent scheduling never reopens. |
| G04 | Full proof arrives after dedup purge or Core restart. | PASS — Resolve only independently retained verified barrier/correlation state; do not recreate result/history cache or old intent. Old ticket remains HISTORY_EXPIRED/OLD_EPOCH as applicable. |
| G05 | Proof establishes past nonexecution but not that delayed work cannot execute later. | PASS — Do not set guaranteed_no_execution or release unresolved future barrier; no N fallback. A timeout or reconnect cannot fill the missing guarantee. |
| G06 | R02 proof is complete but retry wait reaches deadline before an eligible route can dispatch. | PASS — Close FAILED/AMBIGUOUS/UNCONFIRMED with DEADLINE_EXPIRED, historical delivery retained. Do not manufacture NOT_SENT/EXPIRED_BEFORE_SEND after handover. No fallback at/after deadline. |
| G07 | Known partial count receives complete zero-execution proof; captured fallback is enabled. | PASS — Proof can release resolved keys, but existing partial-count no-replay/no-resumption rule still prohibits retry. Preserve FAILED/PARTIAL_EXECUTION and underlying local error; proof is not permission to resume. |
| G08 | Same captured retry flags/limits, but a newer conflicting intent or stale mapping supersedes it before handover. | PASS — No old retry; normal supersession/STALE_PRECONDITION gate still wins before deadline. Proof resolves barriers only; it does not rebase captures. |
| G09 | R04 extent is unknown instead of exactly one, or feedback CORRELATED_RESULT remains genuinely unresolved. | PASS — Unresolved extent => UNCERTAIN_OUTCOME/AMBIGUOUS/UNKNOWN; known partial with unresolved declared correlation => UNCERTAIN_OUTCOME/PARTIALLY_SENT/UNCONFIRMED; error AMBIGUOUS_DELIVERY before deadline, TIMEOUT at deadline. Do not apply the feedback-NONE known-partial exception. |
| G10 | Stopped two-step recipe: first step fully emitted feedback NONE under STRICT; second never sent due UNAVAILABLE. | PASS — FAILED/PARTIALLY_SENT/UNCONFIRMED, PARTIAL_EXECUTION cause UNAVAILABLE; emitted=1,confirmed=0,total=2. Release reservation, retain only sent-prefix physical barrier; no suffix resumption. |
| G11 | Proof resolves attempt A on resource r1 but attempt B still has unresolved barrier on r1; independent ready intent needs r1. | PASS — Remove A's resolved barrier only; B still blocks dispatch. If no owner remains, independent admitted intent follows ordinary FIFO/gate, never reopening A's parent. |
| G12 | R02 fallback fully emits or confirms after prior ambiguous attempt proved incapable of past/future execution. | PASS — Covering known local emission yields aggregate SENT (ACKNOWLEDGED if actually acknowledged) and normal open-loop ACCEPTED; verified whole completion yields CONFIRMED_COMPLETED. Old Attempt.delivery/evidence stays AMBIGUOUS; no-execution proof alone never supplies ACK/CONFIRMED. |
| G13 | Late future-only fence settles delayed execution risk after a complete open-loop ACCEPTED result. | PASS — Release only fenced keys; outcome stays ACCEPTED/UNCONFIRMED without target completion proof. Merely unobserved feedback NONE does not become UNCERTAIN_OUTCOME. |
| G14 | Duplicate proof or contradictory executed-after-proved-zero-execution claim. | PASS — Duplicate is idempotent; contradiction quarantined under existing rules. No replay, field rollback, evidence erasure or new result revision for identical proof. |
| G15 | A feedback-NONE count=3 reaches deadline with exactly unit 1 emitted and units 2/3 proved never emitted/started; source stops at deadline; extent remains known. | PASS — FAILED/PARTIALLY_SENT/UNCONFIRMED, PARTIAL_EXECUTION with TIMEOUT cause; progress 3/1/0,false and deadline_elapsed=true. Retain unresolved prefix keys, send no suffix. An already-terminal full open-loop ACCEPTED only updates deadline_elapsed, never becomes uncertain by time alone. |

New propagation findings during drafting: resolved by the guarded FAILED/AMBIGUOUS cell and aggregate covering-emission rule described under FR15, and the section 13 deadline qualification under FR16 (otherwise known partial feedback-NONE work at deadline could select both uncertainty and failure). No new unresolved blocker found. Historical byte uncertainty remains in Attempt/evidence even when aggregate/request outcome is settled. Neither a future-only fence nor absence proof supplies target completion/acknowledgement. Proof on one key never releases another owner's barrier.

## Validation, remaining gates and verdict

Review evidence: all R01-R04, all 77 unchanged prior vectors, G01-G15 and FR01-FR16 interactions; transition/schema/fold/retry/deadline/barrier/history checks; numbered-section/vector coverage; exact version tuple `{1,0,0,4}`; local Markdown links; whole-workspace file-hash comparison. These are document checks only, not executable conformance tests.

Exact changed files:

- `docs/COMMON_CONTROL_MODEL_V1.md`
- `docs/COMMON_CONTROL_MODEL_V1_CANDIDATE_4_FREEZE_REVIEW.md`
- `CHANGELOG.md`

All historical review documents and architecture remain unchanged. No firmware/UI/hardware/rotation/touch/GPIO/backlight/carousel, PlatformIO/library, Core/API/adapter/networking/protocol implementation; no build, upload, release, tag, approval or freeze.

FR15: PASS. FR16: PASS. Remaining internal freeze blockers: **none found**. Actual endpoint support, proof/fence semantics, contexts, precision, identity, ordering, source sequencing and physical control-path validation remain separate future verification gates; no external guarantee follows from this correction. Owner physical HOME/display/navigation PASS is not control-model/protocol validation.

NO BLOCKERS FOUND — CANDIDATE READY FOR OWNER FREEZE REVIEW
