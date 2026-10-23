# A-REMOTE-013 / D-009 REMOTE handoff

Status: **ROUND 5 TEST SESSION CLOSED / PARTIAL HARDWARE PASS**
Date: 2026-10-23

## Round 5 hardware-session closure

The exact 2,578,352-byte Round 5 application was installed application-only at
`0x10000`; independent readback matched SHA-256
`6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`.
Wi-Fi, pairing, authenticated RTCP and authoritative volume were retained.

Owner-observed PASS covers the existing carousel/brightness/touch/HOME UI,
encoder volume, radio Play/Pause in both directions, station and combined
StreamTitle presentation/scrolling/invalidation, discovered-context control
withdrawal, TUNER-restart invalidation/reconnect, A2DP coexistence and return
to radio. Bluetooth metadata and AVRCP remain outside D-009. Fixed-list
Previous/Next/wrap, any forced-invalid-command Favorites proof, REMOTE
sleep/wake/restart and representative resource/stability measurements remain
unexecuted. The detailed evidence and limits are in
[the Round 5 validation report](A_REMOTE_013_D009_ROUND5_HARDWARE_VALIDATION.md).

Overall A-REMOTE-013 remains **HARDWARE ACCEPTANCE PENDING**. PR #4 remains
open and retains its intentional A-REMOTE-014 discovery dependency.

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
- Round 4 RTCP command-readiness source commit:
  `bd15cc8906c244152ccadc1a6734bb9c1d4bf998`
- Round 5 volume-step/display-activity source commit:
  `bfce47816a8b2b8a92f41d76c6a4bc4975d25361`
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

## Round 5 encoder, volume-step and display-activity result

The Round 4 application was installed at `0x10000` and independently read back
as 2,578,304 bytes with SHA-256
`1fa0432b8785bbfce8aecb22e3f5b747080898fdfd850eb4d93ac965b49a30b7`.
Boot, retained Wi-Fi and authenticated RTCP synchronization passed. It remains
the installed rollback baseline for Round 5.

### A - encoder reliability: NOT CONFIRMED

GPIO2/GPIO5 are inputs with internal pull-ups and are sampled every 2 ms by the
existing serialized input task. The quadrature model preserves partial quarter
cycles, cancels reversal algebraically, emits only at plus or minus four valid
transitions, and resets an ambiguous diagonal without generating a detent. A
deterministic fixture reproduces exactly 39 transitions, 9 detents and 1
invalid transition as nine complete cycles, one diagonal and a remaining
two-quarter partial cycle. This matches the recorded counters but cannot prove
which electrical edges corresponded to the owner's approximate mechanical
count.

No decoder, polling, GPIO, pull-up, threshold or direction change was made. A
minimal remaining diagnosis is a two-channel logic-analyzer capture on
GPIO2/GPIO5 during ten slow detents: compare every stable AB state with the
transition/detent/invalid counter deltas. A valid complete cycle missing from
firmware would demonstrate acquisition loss; an absent edge, diagonal or
incomplete cycle would locate the issue before the decoder.

### B - fixed two-percentage-point volume step

`control_home::Channel::request()` applies the fixed step at the local desired
absolute-target accumulator. Each accepted positive/negative detent contributes
plus/minus two percentage points and clamps at 0/100. No acceleration is used.
The RTCP wire still carries the existing absolute normalized volume. Round 4
command readiness, bounded coalescing, one in-flight command, strict admission,
external-authority reconciliation and lifecycle cancellation remain intact.

### C - encoder display activity

The encoder router invokes `power_management::activity()` only for a decoded
`ENCODER_CYCLE`. It increments the existing atomic physical-activity counter
used by touch. The POWER task owns the inactivity refresh and display-idle
wake, so the encoder task never calls LVGL or display hardware. Invalid
transitions and partial cycles do not refresh activity.

This applies to ACTIVE and DISPLAY_IDLE. SYSTEM_SLEEP retains its GPIO8 motion
wake policy; GPIO2/GPIO5 are not new wake sources, and encoder rotation is not
claimed to wake the sleeping MCU.

### Round 5 verification and private candidate

- Encoder/volume/activity routing: 10/10 PASS, 98 assertions.
- Volume admission/coalescing/lifecycle: 12/12 PASS, 323 assertions.
- Physical input: 19/19 PASS, 24,627 assertions, 1,000 cycles.
- POWER lifecycle: 20/20 PASS, 19,244 assertions, 1,000 cycles.
- HOME/Core: 42/42 PASS, 3,565 assertions.
- RTCP D-009: 8/8 PASS, 44 assertions.
- D-009 policy: 15/15 PASS.
- Encoder activity policy: 9/9 PASS.
- HOME metadata policy: 15/15 PASS.
- UI volume binding: 1/1 PASS, 11 assertions.
- Production `remote01`: PASS, one build.
- Application: 2,578,352 bytes; SHA-256
  `6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`.
- ELF SHA-256:
  `9499b13b9c4f6fc3ece4c19d75b4303c13ab06639e4b66f0e4e1cfd0128d4acf`.
- Static RAM: 56,460 bytes.
- 3 MiB slot headroom: 567,376 bytes.
- Diagnostics snapshot/storage: 1,008/8,176 bytes, unchanged.

The application and ELF remain private. Round 5 performed no hardware access,
flash, NVS, pairing, partition, credential or TUNER operation.

## Round 4 rapid-volume failure and correction

The Round 3 application was installed and independently read back at
2,578,128 bytes with SHA-256
`6ba73c80b774727cc137f5606b408f6cfc3ff0ea486ef879869453a5debb0383`.
Owner testing verified HOME scrolling: the station scrolls when long, a short
StreamTitle remains static, and a long StreamTitle scrolls. That work remains
hardware PASS and was not changed in Round 4.

Ten slow intended detents produced ten accepted intents, ten completed volume
operations, zero failures and the expected 80 to 70 authoritative result. Ten
rapid detents in the opposite direction produced ten decoded and accepted
intents, but only four completions and six definite failures, ending at 74
instead of 80. Session 4 and capability revision 63 were unchanged at the
captured boundaries; queue rejection and every cancellation counter remained
zero.

The installed read-only event ring was retrieved through
`/api/events?after=<cursor>`. Its retained volume-failure records all carried
detail 13, `GUARANTEED_NO_EXECUTION`; no retained event classified an
authenticated target refusal. `/api/status` exposes only aggregate volume
counters, so the event endpoint is required for the bounded cause detail. A
safe retrieval sequence is:

```powershell
$remoteIp = '<verified-REMOTE-LAN-IP>'
$status = Invoke-RestMethod "http://$remoteIp/api/status" -TimeoutSec 3
$cursor = [uint64]0
$events = @()
do {
  $page = Invoke-RestMethod "http://$remoteIp/api/events?after=$cursor" -TimeoutSec 3
  $events += $page.events
  $previous = $cursor
  $cursor = [uint64]$page.after
} while ($cursor -ne $previous -and $cursor -lt ([uint64]$page.next - 1))
$status.core | Select-Object session, capability_revision, dispatches, volume_diag
$events | Where-Object { $_[2] -eq 10 } |
  ForEach-Object { [pscustomobject]@{sequence=$_[0]; uptime_ms=$_[1]; event=$_[2]; detail=$_[3]} }
```

The causal sequence is source-proven. `Channel::publish()` queues the
accumulated absolute successor when the prior authoritative target arrives.
The owner loop calls `Client::poll()` before popping that successor. RTCP can
then start a read-only snapshot or D-009 now-playing exchange. With `awaiting_`
set, `Client::set_volume()` refuses the simultaneous command, the adapter
reports `RTCP_NOT_SENT`, and Core records a failed attempt with guaranteed no
execution. The six failures were REMOTE-local pre-transmission failures; they
were not TUNER refusals and were not legitimate target supersession.

Round 4 keeps the existing bounded intent queued until the RTCP client reports
that its authenticated record lane is command-ready. If a command is waiting,
polling completes the current read-only exchange without starting another
background snapshot/metadata request first. The command then follows the same
Facade capture, strict Core admission and correlated RTCP dispatch exactly
once. No sent command is retried. Disconnect, session replacement, lost
authority, sleep and external divergence retain their existing cancellation
and stale-replay protections.

Focused tests cover rapid +10, rapid -10, rapid direction reversal, slow
single-detent behavior, multiple intents accumulated during one in-flight
command, coalescing, transport-busy retention, same-session capability changes,
external authority, authenticated refusal, definite local failure,
disconnect/sleep cancellation, stale replay and final authoritative
reconciliation. Results: encoder/Facade/transport 9/9 (94 assertions), volume
admission/lifecycle 11/11 (290 assertions), D-009 policy 15/15, and HOME
metadata policy 15/15.

The production build passed. The private application is 2,578,304 bytes,
SHA-256
`1fa0432b8785bbfce8aecb22e3f5b747080898fdfd850eb4d93ac965b49a30b7`;
the ELF SHA-256 is
`7fe9aaaebd2b61dddfc3a414fd41f89fc8f89d1500ae3bcef9ee7b9425cd9cd7`.
Static RAM is 56,460 bytes, slot headroom is 567,424 bytes,
`diagnostics::Snapshots` is 1,008 bytes and total diagnostic storage is 8,176
bytes. No hardware operation occurred. Installation and physical slow/rapid,
reversal, boundary, disconnect and sleep/wake validation remain owner-gated.

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
scrolling policy documented above. Owner testing of the Round 3 application
verified long station and StreamTitle scrolling and static short StreamTitle
presentation; that display scope is hardware PASS.

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
bytes, and -2,096 bytes headroom. It remains a preserved rollback candidate;
the earlier correction and accepted A-REMOTE-012 application are also
preserved. Round 3 subsequently superseded it on hardware.

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
remain a preserved secondary rollback reference. All application images,
ELFs, backups, credentials, pairing material and private device data remain
private and are not part of the public branch.

## Private Round 4 candidate identity

- Source: `bd15cc8906c244152ccadc1a6734bb9c1d4bf998`
- Application size: 2,578,304 bytes
- Application SHA-256:
  `1fa0432b8785bbfce8aecb22e3f5b747080898fdfd850eb4d93ac965b49a30b7`
- ELF SHA-256:
  `7fe9aaaebd2b61dddfc3a414fd41f89fc8f89d1500ae3bcef9ee7b9425cd9cd7`
- Static RAM: 56,460 bytes
- 3 MiB application-slot headroom: 567,424 bytes
- Diagnostics snapshot/storage: 1,008 / 8,176 bytes

This is +176 application bytes and -176 bytes of slot headroom versus Round 3,
with unchanged static RAM and diagnostic storage. Installation and exact-span
readback passed; Round 4 is the installed baseline and Round 3 remains a
preserved secondary rollback reference. This private artifact is not published
in the repository.

## Round 5 hardware validation still required

1. Reverify rollback backups and REMOTE identity, install only the Round 5
   application at `0x10000`, and independently hash the exact readback. Do not
   rewrite the compatible TUNER.
2. Confirm retained Wi-Fi/pairing, authenticated discovery/RTCP, authoritative
   volume/playback and the accepted HOME metadata behavior.
3. From a safe mid-range value, verify one clockwise/counterclockwise detent is
   exactly plus/minus two points; then verify ten slow and ten rapid detents,
   reversal, 1-to-0 and 99-to-100 clamping with before/after counters.
4. Keep REMOTE ACTIVE and rotate at intervals below 30 seconds: the display
   must remain on. Stop and verify the normal existing timeout. From
   DISPLAY_IDLE, rotate once and verify the existing display wake behavior and
   one volume intent. Do not claim or test encoder wake from SYSTEM_SLEEP.
5. If mechanical and decoded detent counts still differ, collect the bounded
   two-channel GPIO2/GPIO5 logic-analyzer trace described above; do not tune the
   decoder without that evidence.
6. Preserve lifecycle safety by confirming disconnect and SYSTEM_SLEEP/wake
   cancel pending volume targets without stale replay. Complete the remaining
   D-009 saved-list Previous/Next/refusal and MP3/A2DP/web-manager checks.

Rollback may use the installed Round 4 application, preserved Round 3 or
A-REMOTE-012 applications, or the verified full-flash backup. Round 5 has no
hardware PASS until owner commissioning completes these checks.
