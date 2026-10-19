# Final adversarial freeze review

2026-10-03. Reviewed `COMMON_CONTROL_MODEL_V1.md` candidate.1 and the complete `CONTROL_ARCHITECTURE.md`. This report tests written semantics, not firmware or protocols. The previous readiness claim is falsified. Candidate.2 changes version/readiness only; the normative repairs proposed below have **not** been applied. They require a focused correction milestone and another consistency check.

## Findings

FR01-FR12 are **BLOCKER FOR V1 FREEZE**. Each has a concrete contradiction or multiple reasonable observable outcomes for the same input. The proposed repairs are local; they add no operation, protocol or transport functionality.

| ID / exact rules | Concrete counterexample | Smallest proposed correction / semantic boundary |
| --- | --- | --- |
| FR01 - sections 11-13, lifecycle table, aggregate delivery, transition list | ACCEPTED/SENT awaits feedback; a verified idempotent retry is permitted. Handover of its next attempt makes aggregate delivery AMBIGUOUS, which ACCEPTED forbids, but ACCEPTED -> DISPATCHED is not permitted. An ongoing count can also report PARTIALLY_SENT, which neither nonterminal state accepts. DISPATCHED/NOT_SENT at its deadline may expire under section 13, but section 11 omits that transition. | Define legal nonterminal aggregate combinations and explicit retry/no-handover-expiry transitions. Preserve previous attempts/progress; do not relax non-idempotent replay or confirmation requirements. |
| FR02 - section 11, late evidence MAY revise terminal records | Identical verified late completion of an uncertain attempt can leave one Core's visible result uncertain and change another's to confirmed. The permissive late rule also lists transitions from definitely-unsent terminal results without a contradictory-evidence guard. | Make valid correlated late evidence updates mandatory and idempotent; restrict eligible source states and reject contradictory no-execution evidence. Preserve history, require no remaining barrier, and never reschedule. |
| FR03 - sections 10/13, validation order and terminal deadlines | At the same handover check the queued request's revision is stale and its deadline elapsed: section 10 checks revision first, while section 13 requires expiry for unsent work. Separately, a terminal one-way ACCEPTED result reaches its deadline: section 13 both sets deadline_elapsed after dispatch and says time alone does not change terminal results. | State one precedence for already-admitted expiry versus invalidation and one exact meaning/update rule for deadline_elapsed on closed results. Keep cached duplicate precedence and possible execution intact. |
| FR04 - section 12, bounded deduplication | Start with 1024 terminal records all past minimum retention. One Core evicts eligible records and admits a new request; another retains them and rejects DEDUP_CAPACITY. Retrying an eligible old record likewise returns its original result or REQUEST_HISTORY_EXPIRED depending on discretionary eviction. | Specify exact purge timing, deterministic eviction order and the terminal timestamp used for retention; retain active/unexpired records. This changes cache decisions, not exactly-once guarantees. |
| FR05 - sections 10/12/17/18, reservations versus barriers/FIFO | An ordinary queued command holds r2 while waiting for r1; a newly admitted recipe reserves r1 in sorted order and waits for r2. Only recipe acquisition order is specified, so ordinary acquisition can form a cycle. Admission-time recipe reservations can also block older ready work that FIFO would otherwise select. | Use one qualified lock-key order and all-or-none reservation rule for ordinary commands and recipes; specify whether reservations occur at admission or dispatch and their cancellation/release order. Do not clear unresolved physical-execution barriers. |
| FR06 - sections 10/12, closed request schema and recipe validation | Different recipe steps share the parent request ID but must validate as section 10, whose first check treats a reused ID with different arguments as mismatch. The closed CommandRequest lacks the pinned recipe revision/per-step captures mentioned later. One reading exempts steps; another applies request deduplication and rejects a legitimate step. | Explicitly distinguish outer request deduplication from step validation; declare the immutable recipe/step capture structure and what belongs to full-envelope equality. Keep one user intent and no nested recipes/replay. |
| FR07 - sections 11/12, interrupted recipe result | Step 1 is confirmed; step 2 loses capability before handover. REJECTED/ACKNOWLEDGED/REJECTED is permitted after all prior execution is resolved, while FAILED/ACKNOWLEDGED/UNCONFIRMED is also permitted for known incomplete compound work. The stop rule chooses neither. | Define parent-result folding for unstarted, known-partial, ambiguous, all-confirmed and mixed open-loop steps, including underlying error retention. Keep completed prefix history and prohibit resumption. |
| FR08 - section 9, selected absence | No eligible present sample exists, and a descriptor evidences UNSUPPORTED. The rule allows ABSENT UNKNOWN “or” the evidenced absence reason. With several bindings, it also does not choose among evidenced absence reasons. | Define one absence-reason reduction over current descriptors/observations, independently of present-value selection. Do not convert absence to a value or infer unsupported from offline. |
| FR09 - section 9, conflict-latch resolution | Fresh A/B disagree; B expires; A supplies verified RESYNC. “Still-eligible involved routes” can mean fresh candidates (A only) or retained supported conflict participants (A and B), giving cleared versus retained latch. “Clear only after” gives a necessary condition without requiring clearing when it is met. | Define/persist conflict participants, their eligibility and the exact sufficient clearing event; make the resulting latch update mandatory. Mere expiry/removal still must not clear a conflict. |
| FR10 - section 14, TABLE with NEAREST_TIES_UP | TABLE has pairs (common 0, external 0), (common 1, external 10); input common 1/2 is within domain and quantization enabled. There is no affine transfer and interpolation is forbidden. The text does not say whether to reject an off-row input or quantize among common table rows; it instead asks for nearest external value without defining that value. | Specify TABLE input acceptance/quantization distance, ties, error checks and inverse lookup, or explicitly forbid non-EXACT TABLE policies. Leave the existing AFFINE rules unchanged. |
| FR11 - sections 2/15, partial catalog refresh | Known complete catalog [A,B] receives verified incomplete [B], with no proven identity/token change. Implementations can treat the nonidentical refresh as a new generation or retain the known membership/generation because absence is unproven. An old A reference then rejects or remains valid. The complete=false merge/order rule is missing. | Define precisely what partial refresh can change, how omitted items/order are retained and which proven change increments generation. Never infer deletion or positional continuity from incomplete data. |
| FR12 - sections 10/12, total validation/error contract | A request with a mismatched common-model version must fail, but no normalized error is prescribed. Duplicate ticket with a different request ID and old-epoch ticket checks lack explicit ID-ticket binding/error rules. Different error codes or lookup order are reasonable despite the fixed validation-stage order. | Provide a total decision table for each validation stage, including version, missing capture, ticket epoch/serial/ID pairing and cache/queue capacity precedence. Use existing errors; do not create a wire API or permit forged new intent. |

Additional classifications:

- **FR13 - SHOULD FIX BEFORE V1:** sections 8/9 leave conflict_set/history ordering and retention presentation unspecified even though `[]` is an ordered list. Selection of the primary value is deterministic; ancillary list output is not. Specify conflict ordering and distinguish nondeterministic diagnostic/history retention from conformance-visible data. No need to add an observation capability.
- **FR14 - EDITORIAL / CLARIFICATION:** sections 4/5/6 refer to `Version`, `Unit` and generic `Record` without explicit aliases/path-bearing unit structure; unsupported descriptors refer to `status` although the fields are support/availability. Section 7's “No field MUST be manufactured” should use the unambiguous prohibition MUST NOT. Define the shorthand and correct the wording without changing intended meanings. Identifier-name shorthand must likewise inherit the declared scoped type, not a new implicit namespace.
- **FUTURE PROTOCOL VERIFICATION:** existing section 20 obligations remain unchanged. They are not a new reason to require every optional ecosystem before common-model freeze.
- **NON-BLOCKING FUTURE EXTENSION:** wire formats, native API, additional operations and alternate scheduling policies remain outside this review. Missing optional functionality was not counted as a blocker.

## State-machine audit

Q=QUEUED, D=DISPATCHED, A=nonterminal ACCEPTED, C=CONFIRMED_COMPLETED, R=REJECTED, F=FAILED, X=EXPIRED_BEFORE_SEND, U=UNCERTAIN_OUTCOME. `+` is a listed ordinary transition; `L` is the broadly permitted late-evidence transition needing FR02's guard; `!` is contradictory/missing despite another rule permitting it; `-` is forbidden. `=` is unchanged state, not permission to alter terminal/scheduling status.

| From / to | Q | D | A | C | R | F | X | U |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Q | = | + | - | - | + | + | + | - |
| D | - | = | + | + | + | + | ! | + |
| A | - | ! | = | + | + | + | - | + |
| terminal A | - | - | = | L | L | L | - | - |
| C | - | - | - | = | L | L | - | - |
| R | - | - | - | L | = | L | - | - |
| F | - | - | - | L | L | = | - | - |
| X | - | - | - | L | L | L | = | - |
| U | - | - | - | L | L | L | - | = |

Constructible ordinary transitions: Q->D valid handover; Q->R validation rejection; Q->F local failure before send; Q->X deadline. D->A full local emission/acceptance; D->C fast correlated complete reply; D->R explicit verified target refusal; D->F proven local failure/known incomplete emission; D->U ambiguous attempt closure with no eligible replay. A->C complete correlated feedback; A->R verified refusal resolving possible execution; A->F verified incomplete work; A->U timeout. These paths still require the table's exact delivery/outcome/error/progress checks.

U->C/R/F has genuine late-resolution traces (complete execution, proven refusal/no execution, or known partial failure). Terminal open-loop A may gain later completion evidence only if that binding actually supplies independently verified correlation. There is **no valid completion trace** from locally rejected/expired definitely-unsent work; section 11's broad late permission requires restriction. Confirmed completion followed by contradictory refusal must not silently rewrite history. Thus not every syntactically permitted L cell has a sound execution trace.

All other transitions, reopening terminal scheduling and D/A->Q are forbidden. Zero/full/partial progress, aggregate attempts and query_result were checked: confirmed must cover total; emitted/confirmed may not exceed total; enumeration completion requires its scoped catalog; other lifecycle states omit query_result. These protections PASS for isolated terminal examples. They do not fix the nonterminal/recipe aggregation holes in FR01/FR07. NOT_SENT must never erase an attempt that may have emitted; strongest aggregate evidence is not by itself target confirmation.

## Existing A-T vectors

PASS means the expected outcome follows for the stated isolated case. A finding means the vector itself states an answer but surrounding rules do not uniquely generalize it, or its timing/terminal preconditions need qualification. No vector was run against an implementation.

| Vector | Review result / rule |
| --- | --- |
| A | PASS, sections 7/9: unknown power plus toggle cannot generate known power |
| B | PASS, section 11: feedback-NONE full emission closes ACCEPTED/SENT/UNCONFIRMED |
| C | PASS at deadline with unresolved delivery, sections 11/13; never NOT_SENT |
| D | PASS, section 12 forbids uncertain non-idempotent fallback |
| E | PASS for supersession/barrier; the pre-supersession retry path has FR01 |
| F | PASS when queued revocation is validated before deadline; combined expiry is FR03 |
| G | PASS for preserving execution history; FR07 governs an interrupted multi-step parent |
| H | PASS for scope/stale-generation errors, section 10; generic validation totality is FR12 |
| I | PASS for proven token reuse; incomplete refresh without proved change is FR11 |
| J | PASS, section 15 discards obsolete artwork |
| K | PASS, section 8 prevents cached age rejuvenation and old-session updates |
| L | PASS for selected authoritative value/conflict latch; resolution is FR09 |
| M | PASS for retained same-ID/ticket envelope mismatch; ticket-only variants are FR12 |
| N | FR01: expected terminal uncertainty needs attempt closure; ongoing valid partial progress has no legal nonterminal combination |
| O | PASS for expiry alone; concurrent stale revision is FR03 |
| P | Initial timeout PASS; identical late completion is discretionary under FR02 |
| Q | PASS, exact AFFINE arithmetic gives 7/10, error 1/50 |
| R | Bounded valid seek and out-of-window rejection PASS; missing window versus missing required capture/error is FR12 |
| S | PASS for generation invalidation and retaining dispatched history, section 16 |
| T | PASS, readable fields do not grant write support |

## 35 additional adversarial vectors

Unless overridden: typed current-version input, future deadline, matching captures, verified support/authorization and no preexisting barrier. Result is the current normative requirement where unique. For a finding, “unresolved” means no unique consistent answer exists; the correction target is a proposal, **not new normative text**. Events are listed in processing order, including equal-tick ingress order.

| ID | Initial state | Ordered events | Required result / exact rule | Status |
| --- | --- | --- | --- | --- |
| E01 | Queued request, deadline future | Same tick: revoke capability/increment revision; handover check | REJECTED/NOT_SENT/REJECTED, STALE_PRECONDITION; section 10 stage 4 | PASS |
| E02 | Ready request | Same tick: handover; revoke | Preserve possible execution; invalidated=true, no further attempts; sections 10/18 | PASS |
| E03 | Queued authorized request | Authorization lost, capability revision incremented; handover check before deadline | STALE_PRECONDITION wins over UNAUTHORIZED because stage 4 precedes 6; sections 2/10 | PASS |
| E04 | Queued request | Mapping revision changes; handover check | Reject STALE_PRECONDITION, no send; sections 2/10 | PASS |
| E05 | Queued ref-bound request | Association rollback with higher generation; handover check | Reject stale captures; never retarget old ref; sections 10/16 | PASS |
| E06 | Captured media/window generation | Media or seek-window generation changes; seek handover check | Reject STALE_PRECONDITION, no seek; sections 10/14/15 | PASS |
| E07 | Retained completed record | Same full ID/ticket/envelope submitted after its deadline | Return retained original result, no new attempt; section 12 duplicate precedence | PASS |
| E08 | Ticket from prior clock epoch | Reboot; old ticket submitted | Must reject/no replay; exact error and ID/ticket lookup mapping unresolved; sections 10/12 | FR12 |
| E09 | 1024 terminal records, all retention-eligible | Clock advances; new request or old duplicate submitted | Admit versus capacity rejection, or cached result versus history-expired, is unresolved; section 12 | FR04 |
| E10 | Persisted uncertain attempt/barrier | Adapter crash; Core reboot cannot recover barrier detail | Block affected bindings/resources pending verified fence; no reconstruction/replay; section 12 | PASS; intentional safety block |
| E11 | Targets A/B share r1; A's old send uncertain | B's otherwise ready handover check | No dispatch through B; barrier spans both targets; sections 12/17 | PASS; intentional safety block |
| E12 | Ordinary A requires r2/r1; recipe B requires r1/r2/r3 | A holds r2 before gate; B reserves r1 then waits r2; A waits r1 | Current rules do not exclude the cycle; correction needs unified all-or-none reservation; sections 10/12/17 | FR05 |
| E13 | Recipe with differing step arguments | Admit parent; validate/capture first and second steps under shared ID | Step-specific validation versus duplicate mismatch is unresolved; correction must exempt step progression from outer dedup; sections 10/12 | FR06 |
| E14 | Recipe step 1 confirmed, step 2 unsent | Step-2 capability revoked; validate next step | Known prefix preserved, no step 2; FAILED versus REJECTED parent/error fold unresolved; sections 11/12 | FR07 |
| E15 | Recipe at intermediate step | Attempt closes ambiguous; next step otherwise ready | Stop remaining steps, preserve uncertain progress/barriers, no auto-resume; sections 11/12 | PASS |
| E16 | One-way command; target/all resources LOCAL_ONLY | Complete emission with feedback NONE | Terminal ACCEPTED/SENT/UNCONFIRMED; release local lock; sections 11/12 | PASS |
| E17 | Same command under STRICT | Complete emission; no execution/fence evidence ever arrives | Retain barrier; no inferred target order; section 12 | PASS; intentional permanent block |
| E18 | Nonterminal ACCEPTED/SENT; verified retry enabled | Attempt closes; retry delay elapses; next attempt handed over with unknown extent | Must retain prior evidence, but no listed nonterminal transition accepts new aggregate ambiguity; sections 11/12 | FR01 |
| E19 | Count=3, ongoing attempt | First unit is definitely emitted; two remain active | PARTIALLY_SENT is valid known progress, but no nonterminal table row accepts it; section 11 | FR01 |
| E20 | DISPATCHED/NOT_SENT with verified no executable handover | Deadline reached | Section 13 permits expiry; section 11 omits D->X; sections 11/13 | FR01 |
| E21 | Terminal uncertain record, retained | Verified late correlated completion resolves its only attempt/barrier | Visible completion versus retained uncertainty is discretionary; section 11 MAY rule | FR02 |
| E22 | Terminal ACCEPTED before deadline | Deadline tick after closure | deadline_elapsed update versus unchanged terminal record is contradictory; section 13 | FR03 |
| E23 | Queued request at deadline with stale captured revision | Handover validation | Stale rejection versus unsent expiry is contradictory; sections 10/13 | FR03 |
| E24 | No fresh present field; current descriptor evidences unsupported | Evaluate selected field | UNKNOWN versus UNSUPPORTED absence is permitted; section 9 | FR08 |
| E25 | A/B fresh conflict latch set | B expires; verified A RESYNC | Whether expired B remains a required participant and whether latch must clear are unresolved; section 9 | FR09 |
| E26 | Verified source sequence k/value ON | Same source/session/domain k/value OFF | Invalidate source current field, latch conflict, require RESYNC; section 8 | PASS |
| E27 | Equal-priority fresh authoritative A=ON/B=OFF | Both committed; evaluate | Select lexically earlier binding, retain conflict/latch; section 9. Conflict array order itself is not fixed | PASS selection; FR13 presentation |
| E28 | max_age=5000, bounded age 5000 or unknown age | Evaluate; reconnect; cache replay/old callback | Equality stale; unknown-age excluded; old session discarded; no receipt rejuvenation; section 8 | PASS |
| E29 | TABLE (0->0,1->10), NEAREST_TIES_UP, allowed error 1/2 | Common input 1/2 | Off-row rejection versus nearest-table-row quantization unresolved; section 14 | FR10 |
| E30 | AFFINE [0,1]->integer [0,10], allowed error 1/20 | Inputs 0,1,13/20; separately invalid sentinel-containing representable set | 0->0,1->10,tie->7; invalid descriptor cannot advertise support; section 14 | PASS |
| E31 | Complete known catalog A/B, generation 5 | Verified incomplete B only; use old A reference | Generation retention/invalidation decision unresolved; no deletion may be inferred; sections 2/15 | FR11 |
| E32 | Distinct devices/items with same names/IP/labels | Proposed merge; proven positional-token reuse; old ref dispatch | Names/IP insufficient; token reuse invalidates generation/ref; sections 2/15/16 | PASS |
| E33 | Exact numeric/value types | UInt64 max duration as a value; integer overflow; unreduced logical Rational; NaN/infinity; normalized endpoints | UInt64 max fits its primitive (not necessarily a mapping); overflow/invalid logical values rejected, never zero/wrap; normalized 0/1 valid; section 3 | PASS; decoder canonicalization is outside the logical input |
| E34 | Verified window [1000,9000], duration unknown | Seek 9000; then 9001; separately change generation to UInt64 max and request another increment | Boundary inclusive; 9001 invalid; counter exhaustion stops mutation, never wraps/reuses identity; sections 2/14 | PASS |
| E35 | Current registry/version and issued ticket history | Version mismatch, or different ID paired with retained ticket serial | Must reject/no execution, but normalized error/lookup contract is incomplete; sections 10/12 | FR12 |

Other inspected boundaries: TABLE singleton exact row/inverse is meaningful, but its off-row policy joins FR10; exact rational intermediate arithmetic must be wide enough before final representation checks; external ms/seconds precision cannot be invented; wrong-target or rollback-stale references cannot be silently rebound; identical complete verified catalog refresh preserves generation; binding replacement cannot reuse the old binding ID; media-generation rollover stops mutation; stale correlated old-request observations cannot bypass source/session/context watermarks. These rules were preserved, not rewritten for style.

## Compatibility and scope boundary

Section 20 remains informative and explicitly grants no support. Its TUNER rows remain design targets with unknown API; Matter/HA rows remain evidence-bounded candidates; IR remains profile-specific/open-loop; Smart-TV remains UNKNOWN. No fresh protocol research or new protocol assertion was needed. Any required external context/fencing/identity guarantee is still a future verification gate, not a solution to the internal contradictions above.

Inspected files: candidate, architecture, and changelog convention. Changed files: this review record, candidate version/readiness notice only, and CHANGELOG. No architecture, firmware, configuration, UI, networking, API, adapter or library changes. No build/upload, tag or freeze. The review is document reasoning and structural/file-change validation, not executable conformance testing.

Remaining blockers: **FR01-FR12**, all still open. FR13/FR14 are secondary documentation improvements. The candidate's existing safety rules reduce risk but do not make its total decision function internally consistent.

BLOCKERS REMAIN — NOT READY FOR V1 FREEZE
