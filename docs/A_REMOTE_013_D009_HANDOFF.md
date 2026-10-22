# A-REMOTE-013 / D-009 REMOTE handoff

Status: **ROUND 3 CORRECTION SOFTWARE VERIFIED / HARDWARE PENDING**
Date: 2026-10-22

## Source and dependency

- REMOTE branch: `remote/a-remote-013-d009`
- Implementation commit:
  `1a29c4e3652fb3aeb7ef496b674691e78aca2d67`
- Regression-correction source commit:
  `5b3c7c08d06fe0395409090221594f9e5bbd8ace`
- Round 2 diagnostic/overflow source commit:
  `434b1359ba78ebe517f7dfe47423067a99b745f8`
- Round 3 volume-admission/metadata-scroll source commit:
  `3a2fda5887df8671467cf36917f607e6d1b69b11`
- Stacked base: A-REMOTE-014 discovery branch
  `remote/a-remote-014-mdns-discovery` at
  `d32945b`
- The stacked dependency is intentional: automatic authenticated TUNER
  discovery is required by the current DHCP-based deployment.
- TUNER compatibility reference: D-009 implementation commit `6a58c036e848`
  in PR #7, whose reviewed head was `79d2478`; the stacked discovery work in
  TUNER PR #8 was reviewed at `216f5`.

The coordination repository's earlier task text stating that REMOTE D-009 was
not authorized or not started is historical. The owner subsequently authorized
this private implementation, focused verification, one production build, and
preparation of a reviewable PR. It does not establish hardware acceptance.

## Installed-candidate hardware checkpoint

The original D-009 application was installed at `0x10000` and independently
read back over its exact 2,575,168-byte span. Its readback SHA-256 matched
`d9c13a6ce030b9ab2a4db02fad1c5afcf80eee364f0046665a9dcb1652698d76`.

Hardware PASS evidence covers authenticated REMOTE-to-TUNER connection,
authoritative station changes, combined ICY StreamTitle changes, and HOME
Play/Pause control. Hardware FAIL evidence on that superseded candidate covers
lost accumulated rapid-detent volume targets, temporary playback-control
dimming during each volume request, and reversed station/StreamTitle visual
placement. Previous/Next remain intentionally unavailable for Favorites under
frozen D-009.

The corrected 2,575,408-byte application was subsequently installed and read
back exactly with SHA-256
`250f55bd4ce0edab9c77e59b227c2c7c9d5b3bfbd2780795dc3dd654203b80a7`.
Boot, retained Wi-Fi/pairing, authenticated RTCP, HOME carousel index 2, and
volume/playback presentation passed. Round 2 owner testing then found:

- **FAIL:** rapid encoder rotation remains intermittent and may require many
  physical detents for one or two displayed steps;
- **PASS:** direction reversal follows the new direction;
- **PASS:** Play/Pause no longer dims during volume adjustment;
- **PASS:** station is in the large upper label and StreamTitle in the smaller
  lower label;
- **FAIL:** long StreamTitle text can exceed the visible presentation boundary;
- **PASS:** HOME Play/Pause changes TUNER playback state.

These findings supersede the earlier encoder-resolution claim. They do not
reopen the listed PASS behavior.

## Round 3 demonstrated cause and correction

Round 2 hardware evidence recorded 26 queued volume targets, 14 correlated
completions, 12 definite failures, and zero cancellations. All 14 dispatched
RTCP operations completed. The 12 additional failures therefore occurred
locally before RTCP dispatch; they were not authenticated TUNER refusals.

Source tracing and a deterministic host reproduction demonstrated the local
race. An unrelated D-009 playback-context update can advance the target-wide
capability revision after `Channel::pop()` and before `Facade::handle()`. The
old Facade rejected that queued absolute volume target before re-capturing the
current volume authority. The correction permits this revalidation only for a
same-session absolute volume intent. It captures the current volume operation,
fresh authoritative observation and precondition through Core before submit.
All non-volume operations retain exact capability-revision matching. Session
replacement, missing authority, Core admission, authenticated target refusal,
deadlines, result correlation, one-command-in-flight behavior and no automatic
retry remain enforced.

The event ring now records a bounded volume-failure cause for local validation,
capture/precondition, Core admission, authenticated target refusal, expiration,
or guaranteed non-execution. No payload, secret, dynamic allocation, or
per-transition log was added. `diagnostics::Snapshots` remains 1,008 bytes and
continues to satisfy its 1,024-byte guard; added snapshot storage overhead is
zero bytes.

Both existing 148-pixel HOME metadata labels now use LVGL's clipped,
bidirectional scroll mode. Speed is 25 pixels/second, with 1,000 ms initial and
endpoint delays. New text resets to its first character. Scrolling is stopped
outside HOME and while display commands are unavailable, so navigation and
sleep/display-idle lifecycle do not leave background label animations. Widget
identity, geometry, fonts, station/StreamTitle assignment, buffers, UTF-8
validation, touch behavior and generated UI resources are unchanged.

## Historical Round 2 evidence and changes (superseded where noted)

### Encoder: Round 2 position before counter capture

The physical GPIO2/GPIO5 sampling, quadrature model, 2 ms input task, HOME
gating, Channel accumulation, single-in-flight RTCP path, and authoritative
reconciliation were audited end to end. The decoder and sampling task are the
same imported A-REMOTE-012 implementation that previously passed owner rapid
volume testing. Deterministic host tests continue to preserve every correctly
sampled, decoded, and admitted detent across bursts, direction changes,
capability revision changes, results, snapshots, failures, disconnect and
sleep/wake fences.

At that checkpoint, the installed evidence did not show whether failing
physical turns were sampled, invalid, decoded, admitted, queued, or cancelled.
Therefore no decoder, scheduling, CCM, RTCP, timeout, or retry behavior was
changed.
Round 3 counters later demonstrated the local pre-dispatch boundary described
above; the decoder and RTCP protocol remain unchanged.

The existing `/api/status` diagnostic now adds two fixed-order arrays:

- `encoder_diag`: `[invalid transitions, positive detents, negative detents,
  rejected HOME emissions, last sample interval ms, maximum sample interval
  ms]`;
- `volume_diag`: `[targets queued, correlated completions, definite failures,
  cancellations, authoritative observations, session cancellations,
  missing-capability cancellations, lost-volume-authority cancellations,
  external-divergence cancellations]`.

Existing named fields retain their meanings, including raw transitions,
decoded detents, intents, accepted intents, admission reason, authoritative
volume, in-flight state, and dispatch count. New counters saturate at 65,535,
use fixed storage, allocate nothing, block nowhere in the encoder path, and do
not log per transition. `diagnostics::Snapshots` is 1,008 bytes, below the
existing 1,024-byte limit.

### StreamTitle overflow: Round 2 presentation change

The lower 148-pixel StreamTitle label used `LV_LABEL_LONG_CLIP`, unlike the
upper station label's bounded ellipsis mode. It now uses
`LV_LABEL_LONG_DOT`. The label remains at x=12, width=148 on the 172-pixel
screen, so its allocated right edge is x=160. Station/StreamTitle assignment,
fonts, coordinates, buffers, UTF-8 validation, carousel, artwork and navigation
are unchanged. Round 3 supersedes ellipsis with the bounded bidirectional
scrolling policy documented above. Physical display verification remains
required.

## Verified regression causes and corrections

- D-009 context updates can advance the target-wide capability revision. The
  volume channel treated any such revision as invalidation even when the same
  authenticated session retained fresh volume authority. The correction keeps
  the bounded absolute target across unrelated revisions and refreshes only an
  undispatched queued intent to the current capture fence. Confirmed network
  loss, session replacement, lost volume authority, definite failure and
  external authoritative divergence still cancel it.
- The production session presentation disabled Previous/Play-Pause/Next while
  any RTCP request was pending. It now preserves fresh playback availability
  for RTCP; CCM and RTCP continue to serialize actual commands.
- The D-009 snapshot handler had reintroduced short-circuit publication. A
  rejected volume observation could suppress the independent playback
  observation. Both observations are now attempted independently.
- CCM retains `media.title` as combined StreamTitle and `media.subtitle` as
  station. The existing HOME presentation now binds station to the large upper
  label and StreamTitle to the smaller lower label. Existing fixed buffers,
  UTF-8 boundary truncation, geometry and generated resources are unchanged.

## Implemented scope

- Exact RTCP/1 D-009 message IDs 9-11 and capability revision 2 feature bits.
- Authenticated radio play, pause, previous, and next through existing CCM
  operations and the existing HOME controls.
- Previous/next admission only for the TUNER fixed saved-station context;
  discovered-mode favorite mutations and Bluetooth AVRCP are excluded.
- Strict command correlation, playback-state preconditions, source/session
  generation fencing, one command in flight, and no automatic retry.
- Authoritative station name, combined ICY StreamTitle, optional independently
  trustworthy artist metadata, and playback state.
- Bounded length and UTF-8 validation before metadata publication.
- Stale command and metadata invalidation on disconnect, session replacement,
  source/station generation change, and sleep/wake reconnection.
- Existing HOME carousel index 2, geometry, controls, volume indicator, fonts,
  graphics, and generated UI resources remain unchanged.
- Physical REMOTE buttons remain unassigned.

RTCP maximum plaintext remains 512 bytes and authenticated record storage
remains 556 bytes. The implementation adds no network task, metadata history,
or unbounded allocation.

## Verification

| Check | Result |
|---|---:|
| RTCP D-009 codec/capability tests | 8/8 PASS, 44 assertions |
| D-009 admission/fencing/presentation policy | 14/14 PASS |
| HOME/Core integration | 42/42 PASS, 3,565 assertions |
| Volume admission regression | 10/10 PASS, 267 assertions |
| UI volume binding regression | 1/1 PASS, 11 assertions |
| Power/lifecycle regression | 19/19 PASS, 19,236 assertions, 1,000 stress iterations |
| Ingress/UTF-8 regression | 18/18 PASS, 22,189 assertions, 1,000 stress iterations |
| Production environment | `remote01` PASS |

Round 2 focused verification:

| Check | Result |
|---|---:|
| Encoder routing/timing/classification | 6/6 PASS, 30 assertions |
| Volume admission and pipeline classification | 11/11 PASS, 290 assertions |
| Diagnostics schema/storage | 9/9 PASS, 9,096 assertions, 1,000 cycles |
| HOME/Core integration | 42/42 PASS, 3,565 assertions |
| Ingress and malformed/multibyte UTF-8 | 18/18 PASS, 22,189 assertions, 1,000 stress iterations |
| D-009 source policy | 15/15 PASS |
| HOME metadata physical bounds policy | 8/8 PASS |
| Production environment | `remote01` PASS |

Round 3 focused verification:

| Check | Result |
|---|---:|
| Encoder routing and Facade revalidation | 8/8 PASS, 47 assertions |
| Volume admission/coalescing/lifecycle | 11/11 PASS, 290 assertions |
| Diagnostics schema/storage | 9/9 PASS, 9,096 assertions, 1,000 cycles |
| HOME/Core integration | 42/42 PASS, 3,565 assertions |
| RTCP D-009 codec/capability regression | 8/8 PASS, 44 assertions |
| D-009 source policy | 15/15 PASS |
| HOME metadata scrolling/lifecycle policy | 15/15 PASS |
| UI volume binding regression | 1/1 PASS, 11 assertions |
| Production environment | `remote01` PASS |

No previously passing pairing or TUNER suites were repeated because their
inputs did not change.

## Private corrected candidate identity

- Source: `5b3c7c08d06fe0395409090221594f9e5bbd8ace`
- Application size: 2,575,408 bytes
- Application SHA-256:
  `250f55bd4ce0edab9c77e59b227c2c7c9d5b3bfbd2780795dc3dd654203b80a7`
- ELF SHA-256:
  `1e55ee1e6f06ab9ef38a20a7f05b5f53feb2e7e49ec93375f077ba070b2da508`
- Static RAM: 56,356 bytes
- 3 MiB application-slot headroom: 570,320 bytes
- Partition image SHA-256:
  `a144343580dbdadace66fccc5969d7557de3d4c4f15a61e76db205cc26348184`

Compared with the installed D-009 application, the corrected candidate is
+240 application bytes, unchanged static RAM, and -240 bytes of headroom. The
accepted rollback application remains SHA-256
`b6053c04c03114a24a59f178da4eb03d4bada77faded43b6009667579c21d5da`.

The application, ELF, backups, credentials, pairing material, and private
device data remain private and are not part of the public branch.

## Private Round 2 candidate identity

- Source: `434b1359ba78ebe517f7dfe47423067a99b745f8`
- Application size: 2,577,504 bytes
- Application SHA-256:
  `8c529cc5c16ca3e0a3487f02b975187285eddcdd348ba7ae703d5ccc93b2b4c2`
- ELF SHA-256:
  `978a36934c5106d01a7d61870ec92cf355fec6222f8d2556fd9c18b82ea35d7e`
- Static RAM: 56,460 bytes
- 3 MiB application-slot headroom: 568,224 bytes
- Diagnostics snapshot: 1,008 bytes of the 1,024-byte limit

This Round 2 application was installed and independently read back before the
owner collected the four counter snapshots. Relative to the earlier
2,575,408-byte correction it is +2,096 application bytes, +104 static RAM
bytes, and -2,096 bytes headroom. It remains the installed rollback baseline;
the earlier correction and accepted A-REMOTE-012 application are also
preserved.

## Private Round 3 candidate identity

- Source: `3a2fda5887df8671467cf36917f607e6d1b69b11`
- Application size: 2,578,128 bytes
- Application SHA-256:
  `6ba73c80b774727cc137f5606b408f6cfc3ff0ea486ef879869453a5debb0383`
- ELF SHA-256:
  `e85165c88f1ec3410c1e9f304a4cd224a466bb3f95225045a2aeccb37d074ead`
- Static RAM: 56,460 bytes
- 3 MiB application-slot headroom: 567,600 bytes
- Diagnostics snapshot: 1,008 bytes of the 1,024-byte limit

Relative to the preserved Round 2 candidate, this is +624 application bytes,
unchanged static RAM, and -624 bytes of slot headroom. The Round 2 binary and
its SHA-256
`8c529cc5c16ca3e0a3487f02b975187285eddcdd348ba7ae703d5ccc93b2b4c2`
remain the immediate diagnostic rollback reference. All application images,
ELFs, backups, credentials, pairing material and private device data remain
private and are not part of the public branch.

## Hardware validation still required

1. Reverify retained local and separate-storage rollback backups and exact
   REMOTE identity, install only the Round 3 application at the
   verified application offset, and independently hash its exact readback. The
   compatible TUNER D-009/discovery firmware is already installed and must not
   be rewritten for this correction.
2. Reproduce the original checkpoint: authenticated discovery, identity and
   capabilities, then verify station/StreamTitle placement and Play/Pause.
3. Before input, save `/api/status`; then execute known counts of slow and
   rapid detents in each direction and save status immediately. Compare raw
   transitions/detents, both diagnostic arrays, intents/accepted intents,
   admission, dispatches, authoritative volume, and in-flight state. This
   distinguishes sampling/decoder loss from admission, queue, result or
   cancellation loss without per-transition logging.
4. Display long ASCII and multibyte station/StreamTitle values and confirm each
   existing label scrolls back and forth within its HOME boundary, pauses at
   both endpoints, restarts on new metadata, and stops across navigation and
   sleep/display-idle transitions. Malformed metadata must remain rejected.
5. Verify an actual station/source change still invalidates stale playback and
   metadata, and verify disconnect and SYSTEM_SLEEP/wake cancel pending local
   volume targets without replay.
6. Complete the still-unrun D-009 scope: fixed saved-list Previous/Next and the
   documented refusal in Favorites/discovered and Bluetooth contexts, plus
   MP3/A2DP/web-manager coexistence.

Rollback must use the preserved installed D-009 application, accepted
A-REMOTE-012 application, or complete-flash backup for the affected device.
No Round 3 flash, hardware access, or persistent mutation was performed.
