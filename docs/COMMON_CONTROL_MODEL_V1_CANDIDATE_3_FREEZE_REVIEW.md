# Common Control Model candidate.3 freeze review

2026-10-03. Reviewed `1.0.0-candidate.3`. **Not approved or frozen.** This review supersedes the correction review's readiness verdict, not its historical record. Findings continue the existing numbering as FR15-FR16. No normative repair is implemented by this report.

Scope: the candidate's complete sections 1-21, all A-T/E01-E35/F01-F22 vectors, the correction review and previous freeze-review findings, with architecture/evidence boundaries preserved. This is written-contract reasoning, not an executable conformance or protocol test.

## FR15 — no-execution proof versus blanket ambiguity restrictions

**BLOCKER FOR V1 FREEZE.** Affects §§11-12,17,19.2; related to FR01/FR02/FR05 and F08/F18/F22.

Exact conflicting rules in §12:

> Commit valid per-attempt completion/no-execution/fence proof and release exactly its resolved physical keys BEFORE parent-result folding

> Ambiguous/incomplete attempts always retain barriers even under LOCAL_ONLY.

F22 explicitly allows verified whole-intent refusal/no-execution proof while local byte extent remains unknown, yielding REJECTED with historical AMBIGUOUS delivery. Thus target-execution uncertainty can be resolved while delivery extent remains ambiguous.

Counterexample R01: a STRICT attempt has ambiguous local delivery and a persisted barrier. A valid correlated proof guarantees it never executed and cannot execute later, but does not resolve the historical byte extent. No other unresolved attempt exists. The proof rule requires releasing its physical keys; the blanket ambiguity rule requires retaining them. A subsequent conflicting ready request can therefore dispatch or remain ORDERING_BLOCKED under incompatible readings. This is not the intentional STRICT block when no proof exists: sufficient proof is present.

The same distinction affects fallback. §12 allows a captured equivalent fallback for N with verified no execution on every prior attempt, but also says:

> Ambiguous N or unverified C MUST NOT replay, including hidden transport retries.

Counterexample R02: before terminal closure, an N power.toggle attempt closes with unknown byte extent, then gains verified no-execution/no-future-execution proof. Its captured policy permits fallback, max_attempts=2, delay has elapsed, deadline is future and an equivalent authorized route is READY. The positive fallback rule permits it; the unqualified prohibition forbids it if the retained AMBIGUOUS delivery is used as the criterion. Do not confuse this active scheduling example with F22's terminal refusal: terminal scheduling must never reopen.

Smallest proposed repair, **not applied**: define whether “ambiguous/incomplete” in scheduling/barrier prohibitions means unresolved *possible target execution*, rather than historical delivery extent; explicitly make valid no-execution/no-further-execution/fence proof the barrier exception. Specify the same criterion for non-idempotent fallback. Retain delivery/attempt history, every other unresolved barrier, conservative default retries and the terminal no-rescheduling rule. Add R01/R02 normative vectors. Merely changing F22's expected result would not close these cross-section rules.

Status: **FAIL — unresolved**. No choice of barrier/fallback result is prescribed here as an implemented amendment.

## FR16 — closed-attempt uncertainty rule conflicts with open-loop/known-partial folds

**BLOCKER FOR V1 FREEZE.** Affects §§6,11-13,19; related to FR01/FR07 and A/B/E16/E17/F05/F07.

Exact §11 rule:

> A closed attempt with possible execution and no eligible retry closes uncertain immediately

But the priority fold and B require complete feedback-NONE emission to close terminal ACCEPTED/SENT/UNCONFIRMED. Physical execution remains possible/unconfirmed and retry is disabled by default.

Counterexample R03: a one-way power.toggle completes local emission with feedback NONE; target/all resources are LOCAL_ONLY; no target observation exists; default retry is disabled. The attempt closes. B and fold 3 require terminal ACCEPTED without error and §12 releases the LOCAL_ONLY barrier. The quoted blanket closure rule instead requires UNCERTAIN_OUTCOME, changing the lifecycle/error and potentially barrier behavior. Changing to STRICT does not remove the lifecycle contradiction; it only intentionally retains the ordering barrier.

Counterexample R04: a one-way volume.step count=3 proves exactly one unit fully emitted, then a local failure (UNAVAILABLE) stops emission before units 2/3. Progress is total=3, emitted=1, confirmed=0, uncertain_remaining=false: emission extent is known, not ambiguous. No correlated feedback is promised; target execution of the emitted unit remains unconfirmed; retry is disabled. Fold 4's known incomplete compound branch yields FAILED/PARTIALLY_SENT/UNCONFIRMED, PARTIAL_EXECUTION with UNAVAILABLE cause. The blanket closed-attempt rule yields UNCERTAIN_OUTCOME/PARTIALLY_SENT/UNCONFIRMED instead. Both preserve a physical barrier; preservation alone does not make the visible result deterministic.

Smallest proposed repair, **not applied**: qualify the blanket uncertainty rule to distinguish unresolved extent or an unresolved *declared correlated-result obligation* from ordinary feedback-NONE unconfirmed execution. Make full open-loop acceptance and known partial failure explicit priority exceptions; retain uncertain outcomes for genuinely unresolved extent/correlation and all unresolved physical barriers. Add R03/R04 with unique lifecycle/error outcomes and recheck count/recipe folds, late evidence and deadlines. No synthetic target confirmation or partial resumption is permitted.

Status: **FAIL — unresolved**.

## Coverage and unaffected rules

All 77 existing vectors were inspected against the normative bodies; presence is not proof of consistency.

| Vectors | Review result |
| --- | --- |
| A,B | Unknown-state prohibition intact; shared full open-loop outcome has FR16. |
| C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T | No new contradiction found for their stated isolated inputs; general closure/barrier rules remain subject to FR15/FR16. |
| E01-E15 | Scope/revision/expiry, dedup, qualified all-or-none acquisition and stated recipe examples retained; proof-based barrier generalization has FR15. |
| E16,E17 | Explicit open-loop expected lifecycle conflicts with the unqualified closure sentence (FR16). STRICT ordering without proof remains an intentional safety block. |
| E18-E35 | No new contradiction found for the stated isolated inputs; they do not cover R01-R04 combinations. |
| F01-F04 | Purge/tombstone/capacity/qualified-key rules intact; independent proof-based barrier resolution has FR15. |
| F05-F10 | Stated prefix/full/open-loop/late-proof folds inspected; general proof and closure rules have FR15/FR16. |
| F11-F17 | Persisted conflict roster, ordered conflict selection, TABLE quantization, partial-catalog merge, capture errors and retry-wait traces retained. |
| F18 | Proof-based release contradicts the blanket ambiguity/incompleteness rule for R01 (FR15). |
| F19-F21 | Overflow/epoch retirement, captured-route validation and derived attempt-list bounds retained. |
| F22 | Its intended distinction between delivery uncertainty and proved execution absence is not propagated into all §12 prohibitions (FR15). |

FR01-FR14 received concrete candidate.3 corrections; this review does not wholesale reopen all historical findings. However FR15/FR16 show that the earlier PASS/no-blockers assertion was too broad. The priority fold does not resolve this: separate unconditional prose obligations and §12 barrier/fallback rules still conflict with it. Determinism requires the bodies and vectors to agree, not choosing one rule silently.

Schema/type/scoping, immutable parent-step capture, exact numeric bounds, deterministic purge, persistent ID-ticket pairing, expiry-before-invalidation, catalog omission/order rules, observation age/sequence and persisted conflict participants were also examined. No additional blocker is asserted for those rules. Matter/HA/IR/TUNER compatibility remains informative/unverified; no protocol guarantee or implementation support was added. Owner physical HOME/display/navigation PASS remains separate from control-path verification.

## Freeze-gate recheck — 2026-10-03

Rechecked the repeated freeze-review request against the current file. Candidate.3 is byte-equivalent after line-ending normalization to the previously reviewed candidate; no normative correction has intervened. FR15's proof-release/blanket-retention conflict and FR16's closed-attempt/open-loop conflict remain present. Verdict unchanged: FR15/FR16 remain open; no approval or freeze.

R02 requires verified no-execution proof to be available in the attempt-closing/fallback-decision event before parent terminal closure. Proof arriving only after terminal closure cannot authorize fallback; that path is already forbidden and is not an additional contradiction. R01 independently establishes FR15 without relying on fallback timing.

This recheck changes only this review record and CHANGELOG. A protected-file fingerprint verifies the candidate, architecture, prior reports, firmware and configuration remain unchanged. No build or upload.

## Change scope and verdict

Changed this review, candidate header/§21 readiness only, correction-review supersession notice only, and CHANGELOG. Candidate remains `1.0.0-candidate.3`: no operation, schema, normative §§1-20 rule/vector or logical model version changed. Architecture and historical candidate.1/2 freeze review remain unchanged. File-hash comparison checks the exact documentation scope; structural checks verify all existing vector IDs and local review links.

No firmware/UI/hardware/GPIO/rotation/touch/carousel/backlight, PlatformIO/library, API, adapter, networking or protocol changes. No build, upload, implementation, tag, approval or freeze. Proposed repairs require a subsequent correction pass and re-review.

Open blockers: **FR15 and FR16**, reproduced by R01-R04. The prior readiness verdict is withdrawn.

BLOCKERS REMAIN — NOT READY FOR V1 FREEZE
