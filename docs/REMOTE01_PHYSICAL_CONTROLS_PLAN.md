# REMOTE//01 physical controls preparation

2026-10-05. Hardware-neutral software only. [Resource map](REMOTE01_HARDWARE_RESOURCE_MAP.md) controls GPIO evidence; [session report](REMOTE01_OVERNIGHT_2_REPORT.md) controls verification status.

## A. Verified product requirements

Owner session#2 specifies a rotary encoder and three physical buttons. Encoder push is required only if verified; its presence is UNKNOWN. Current owner classification: actions require explicit design/freeze (DESIGN REQUIRED); do not claim they were never approved. GPIO mapping/A-B/push presence and pin if used/buttons/active levels/pulls/electrical levels/detents/pulses/physical CW-CCW are IMPLEMENTATION + HARDWARE VALIDATION REQUIRED. Touch coexistence and existing accepted wake/control behavior must be preserved.

## B. Verified current software

HOME [Channel/facade](../components/control_home) has an8-intent bounded queue with PREVIOUS/TOGGLE/NEXT, session/capability fencing and no delayed replay. UI enqueues intents, serialized UC_owner owns Core/Mock. These are existing actions, **not a mapping for new controls**. POWER consumes first wake contact and owns idle/motion/touch coordination.

Excluded [button_bsp](../components/button_bsp) has a5ms timer, debounce3 ticks, press/release/click/double/repeat/long-start/hold facilities and BOOT/PWR event groups. Its old GPIO macros are absent from current main definitions. Those thresholds/actions apply to dormant factory code, not the three custom buttons. LVGL includes a keypad/encoder demonstration, not a configured hardware input driver or product focus model.

## C. Unknown hardware

A/B pins, electrical polarity/pulls, maximum edge rate, detent phase/number of edges, interrupt/poll feasibility, push existence, three-button wiring/IDs, shared circuitry and wake routing are UNKNOWN / UNCONFIRMED. No GPIO driver or pin assignment is implemented. Owner must supply the physical facts listed in the resource map.

## D. Explicit action design/freeze required

Design/freeze encoder CW/CCW, push if present, three button meanings, screen/focus behavior, short/long press policy if used and control-specific wake. Verify electrical direction/detents and choose sampling/debounce from real signals. Preserve existing motion-only wake, first-touch-wakes-only and no-PWR recovery contract; do not reopen it. Define overflow recovery UX. No decision is inferred from Mock or dormant factory behavior.

## E. Hardware-independent work performed

[include/physical_input/model.hpp](../include/physical_input/model.hpp) is a standalone preparation model, **not included by running firmware or added to CMake/PlatformIO dependencies**. Architecture justification is the existing separation between input, facade intent and serialized owner. Raw normalized events sit before action mapping; they cannot call LVGL, Core, Mock or a radio transport.

- One serialized task owner supplies normalized3-bit pressed state, two-bit AB and monotonic64-bit time. Owner identity is caller-supplied diagnostic discipline, not authentication or a scheduler lock. It cannot make concurrent use safe; future platform binding must enforce one actual task.
- Caller supplies nonzero debounce milliseconds. A candidate must remain unchanged for that duration before emitting PRESS/RELEASE; samples are required to confirm elapsed time. No timer, GPIO, electrical polarity or default physical delay is selected.
- Gray transitions00→01→11→10→00 produce+1 **logical full cycle**, reverse produces−1. Physical clockwise/counter-clockwise and detents remain UNKNOWN. Adjacent reversals cancel partial cycles; repeated phases do nothing; two-bit jumps invalidate the partial cycle and increment a counter. Missing edges are never reconstructed. No press duration/hold/action semantics.
- Eight fixed FIFO events have epoch/sequence/time and neutral kind/ID/delta. Same-sample ties are deterministic ID0→1→2 then encoder. Clock regression/malformed bits are rejected. Counter increment refuses wrap; diagnostic counts saturate visibly.
- Queue saturation discards the uncertain queued history plus rejected event and latches LOSS. No more event consumption/emission until an explicit baseline establishes a new epoch. Initial/wake/loss baseline produces no actions and discards old input; held-at-baseline buttons do not create a speculative press. Future routing must honor LOSS/epoch and not replay input.
- No dynamic allocation, task, socket, callback, ISR API or hardware capture queue. **Do not call the model from ISR.** Future ISR, if needed, must only capture bounded normalized data through an approved platform shim; its cache/IRAM/timing properties require separate target/hardware proof.

Host harness covers clean/bouncing/duplicate/malformed inputs, simultaneous IDs, saturation/loss recovery, positive/reverse Gray traces, noise/two-bit jumps, rapid traces, ordering, ring wrap, time/counter bounds, ownership refusal, held baseline and long runs with zero allocator calls. Trace directions do not claim physical rotation. GPIO capture, detent fidelity, short missing pulses and real-time wake are not host-validated.

Next: owner wiring/actions → approved platform sampling/capture shim → neutral events → separate explicit mapper into existing facade intents/focus → integrated normal/soak/resource build → physical controls/touch/wake acceptance. Never activate excluded BOOT/PWR code wholesale or require inaccessible PWR.


## Canonical owner reconciliation - 2026-10-05

[Register items18-21](REMOTE01_PRODUCT_DECISIONS.md): encoder+three buttons DECIDED; electrical capture IMPLEMENTATION + HARDWARE VALIDATION REQUIRED; action mapping DESIGN REQUIRED. Earlier absolute wording "No ... actions are approved" is superseded, not evidence that no historical approval ever existed. No actions, pins, push presence, debounce interval or detent convention selected by this documentation. Existing neutral host-model evidence preserved; no source/build/upload.


### Exact superseded audit wording - historical only

These statements are preserved as evidence of the earlier audit, not current decision status. Current classifications are in the register and corrected sections above.

```text
Owner session#2 specifies a rotary encoder and three physical buttons. Encoder push is required only if verified; its presence is UNKNOWN. No volume/navigation/button meanings, physical positive rotation, detent convention or short/long-press actions are approved. Touch coexistence and existing accepted wake/control behavior must be preserved.
```

```text
Approve physical-to-logical identities, encoder direction/detent convention, action mapping by UI context, optional hold/repeat semantics, sampling/debounce settings based on real signals, idle/wake first-contact rules and overflow recovery UX. No decision is inferred from Mock or dormant factory behavior.
```

