# Full Control 1.3 Hardware Validation

Status: **procedure ready; all results initially NOT TESTED**
Firmware family: Full Control 1.3 (`+fc4`)
Date prepared: 2026-10-26

## Validation run — 2026-10-26 (in progress)

- Smoke A1: PASS. TUNER build `e51e39db9ab4`; Wi-Fi connected at
  `192.168.1.57`; uptime advanced continuously. Last-reset reason reported
  `watchdog` and is retained as a baseline warning, with no observed reset
  loop. Initial internal free/largest was 106,808/102,388 bytes and free PSRAM
  was 4,180,715 bytes.
- Smoke A2: PASS. Owner verified saved-station selection and stable audible
  MP3 playback.
- Smoke A3: PARTIAL PASS. Encoder volume, Pause, Play and encoder return to
  Station Select passed. Next-station behavior was NOT TESTED because only one
  saved station exists; the assembly has no Previous button.
- Smoke A4/A5: PASS. REMOTE HOME/touch/Wi-Fi passed at `192.168.1.55`;
  authenticated RTCP was complete with authoritative volume available and no
  operation in flight.
- Source B1 first observation: owner confirms Radio-to-Bluetooth transition
  itself succeeded. The subsequent return to Radio was accidental and is not
  a firmware failure. Popup text appeared clipped (`Switching to...` was the
  readable portion) and the popup did not visibly dismiss until touch; popup
  rendering/timing therefore remains under controlled retest.
- Source B1 controlled retest: **PARTIAL/FAIL**. TUNER authoritatively entered
  Bluetooth and HOME ultimately displayed `Waiting for Bluetooth device...`,
  but the progress popup rendered only `Starting`, never visibly changed to
  `Bluetooth ready`, and remained present until REMOTE entered display-idle.
  A wake touch then returned HOME. This is an OPEN REMOTE popup
  rendering/completion-lifecycle defect; source switching and RTCP transport
  themselves are not classified failed by this observation.
- Bluetooth connection/playback: PASS. Owner verified continuous audible SBC,
  correct metadata on TUNER and REMOTE, REMOTE `PLAYING`, and working REMOTE
  Play/Pause. Read-only capture showed TUNER Bluetooth active/restartable,
  18,908/16,372 bytes internal free/largest, 4,166,635 bytes free PSRAM, and
  connected Wi-Fi. REMOTE RTCP was complete with volume available and no
  operation in flight; close/attempt counters were 15/16 after the source
  transition and are retained for later comparison.
- Bluetooth phone-disconnect behavior: PASS. Audio stopped, TUNER remained in
  Bluetooth mode, REMOTE displayed `Waiting for Bluetooth device...`, stale
  metadata cleared, and no automatic Radio fallback occurred.
- Source B2 Bluetooth-to-Radio: PASS for authoritative behavior. The popup
  again rendered only `Switching to` with the remainder clipped, but HOME
  returned automatically and TUNER reached Radio/Station Select stopped
  without resuming prior audio. RTCP stayed complete and its close/attempt
  counters remained 15/16. Popup rendering remains FAIL.
- Same-source Radio selection: PASS. Tapping RADIO while already in Radio
  produced no popup, restart, navigation change or playback change; TUNER
  remained stopped at Station Select.
- First Radio Browser hardware attempt: FAIL. `Searching` dismissed, but the
  resulting panel showed only `RADIO BROWSER`; the required Any Country/A-Z
  rows did not render. TUNER and REMOTE Wi-Fi were healthy, RTCP was complete,
  authoritative volume remained available, and no allocation failure was
  reported, so this is not evidence of a permanent transport or memory
  failure. REMOTE session advanced from 5 to 7 and close/attempt counters from
  15/16 to 17/18 before the capture; whether a reconnect invalidated the
  browser session at the critical time is not yet proven. Do not credit any
  Country/Name/Genre/Results behavior from this attempt.

### Targeted-correction retest status

The defects above produced offline candidates but those candidates have not
been installed. FC-UI-01 and FC-FAV-01 are **FIXED offline / hardware retest
pending**. FC-RB-01 remains **diagnostic only**. Repeat only these bounded
checks after separately authorized TUNER-first then REMOTE installation:

- [ ] Open Radio -> Bluetooth and Bluetooth -> Radio. Confirm complete popup
  text, visible spinner/buttons, automatic completion and the specified HOME
  return; no extra touch is required.
- [ ] Start the known Favorite while connected and confirm audible playback,
  complete feedback and automatic HOME return.
- [ ] During one separate Favorite selection, allow one natural RTCP
  interruption. Confirm no duplicate PLAY; after reconnect, fresh
  authoritative same-station/PLAYING evidence completes HOME return. If a
  different station or STOPPED is authoritative, confirm no false success.
- [ ] Keep REMOTE continuously ACTIVE, open Browse All Stations once, and
  capture both read-only diagnostic endpoints before and after the empty or
  populated Country panel. Do not retry START automatically.
- [ ] While still ACTIVE, touch an empty Browser area before inactivity to
  distinguish a UI navigation issue from the specified wake-to-HOME behavior.

This checklist validates the owner-approved behavior in
[`REMOTE_FULL_CONTROL_SPEC.md`](REMOTE_FULL_CONTROL_SPEC.md). It does not
authorize firmware changes, erasure, reprovisioning, or resets. Stop the
session at the first smoke-test failure, unexpected reset, persistent network
loss, corrupted station data, or audio failure.

## Installed and rollback evidence

- [ ] TUNER application identity recorded: 2,127,616 bytes, SHA-256
  `4d68d01fd3ccaffbd463c6ff310e54d16bb42d2e9e102426384cf1d4b63e8e6d`.
- [ ] REMOTE application identity recorded: 2,606,192 bytes, SHA-256
  `dc80ba560a3327a069c911de0af31d20eeece77d8f7681ab4d54cbce3ed1c270`.
- [ ] Exact application readback for both images is recorded PASS in
  `D:\TUNER_REMOTE_BACKUPS\20261026_175436\DEPLOYMENT_REPORT.md`.
- [ ] The matching dual full-flash backups remain present on `D:`; do not
  open, modify, rename, or overwrite them during validation.

Record current addresses without assuming historical DHCP leases:

- TUNER IP: `________________`
- REMOTE IP: `________________`
- Start time: `________________`

Verified read-only endpoints used below:

```powershell
$tuner  = Invoke-RestMethod 'http://<TUNER_IP>/api/diagnostics'
$remote = Invoke-RestMethod 'http://<REMOTE_IP>/api/status'
$lists  = Invoke-RestMethod 'http://<TUNER_IP>/api/stations'
```

`/api/diagnostics` exposes TUNER uptime, reset reason, Wi-Fi, source,
playback, codec, internal heap, PSRAM and RTCP timing. It does **not** expose a
current general DMA-heap snapshot. The existing serial `x` command exposes
additional memory evidence, but opening the CH340 has previously reset TUNER;
do not use it during this no-reset session. Record DMA as NOT MEASURED unless
a separately controlled no-DTR/no-RTS serial capture is authorized.

## A. Five-minute smoke gate

Complete these five checks in order. Stop before section B if any fails.

### A1 — TUNER boot and display (manual + API)

- [ ] Power TUNER normally and wait for its normal e-paper station-selection
  view. Do not open COM5.
- [ ] Confirm there is no boot loop, panic, watchdog message, frozen display,
  or unexpected setup portal.
- [ ] Open `http://<TUNER_IP>/api/status`; expect HTTP 200 and
  `connected=true`.
- [ ] Capture `/api/diagnostics`; expect the installed build identity,
  increasing `uptime_ms`, `wifi_connected=true`, a normal `reset_reason`, and
  nonzero `total_psram`/`free_psram`.

PASS: normal UI plus two API reads with increasing uptime and no reset.
FAIL/STOP: missing UI, API unavailable, uptime decreases, or reset/boot loop.

Observation: `____________________________________________________________`

### A2 — TUNER saved-station playback (manual)

- [ ] Rotate the physical TUNER encoder through Station Select; confirm the
  e-paper selection changes and wraps normally.
- [ ] Press the encoder to start one known-good saved MP3 station.
- [ ] Confirm audible, continuous playback and correct station/playback text.

PASS: selected saved station starts and remains audible for at least 30 s.
FAIL/STOP: selection is unresponsive, wrong station starts, or audio stops.

Observation: `____________________________________________________________`

### A3 — Existing TUNER physical controls (manual)

- [ ] Rotate the encoder one detent each direction while playing; volume must
  change one bounded step in the matching direction.
- [ ] Press the physical Play/Pause button once, then again; audio and display
  must show PAUSED then PLAYING.
- [ ] Press the physical Next button once; for a saved station it must select
  the next saved station. Do not require a physical Previous-button test: the
  owner-confirmed assembly has no Previous button.
- [ ] Press the encoder to return to Station Select.

PASS: each installed control performs exactly one expected action.
FAIL/STOP: duplicate, missed, stale, or unrelated action.

Observation: `____________________________________________________________`

### A4 — REMOTE boot, HOME and touch (manual + API)

- [ ] Power/wake REMOTE and confirm portrait HOME at carousel index 2.
- [ ] Confirm the existing layout, touchscreen, IP, station/title, playback
  indicator and volume presentation render without corruption.
- [ ] Open `http://<REMOTE_IP>/api/status`; expect HTTP 200,
  `network.connected=1`, no new network errors, active display, and increasing
  uptime.

PASS: HOME and touch work and the read-only status endpoint is healthy.
FAIL/STOP: blank/corrupt UI, touch failure, reboot, or Wi-Fi failure.

Observation: `____________________________________________________________`

### A5 — RTCP READY and authoritative HOME state (manual + API)

- [ ] Wait for HOME to show the station, PLAYING/PAUSED state and volume that
  TUNER actually reports.
- [ ] In REMOTE `/api/status`, record `core.session`, `rtcp_stage`,
  `rtcp_close_count`, `rtcp_attempts`, `volume_connected`, `volume_known`,
  `volume_set`, `volume_in_flight`, and `dispatches`.
- [ ] Expect an authenticated session, `volume_connected/known/set=1/1/1`, and
  `volume_in_flight=0` after settling.

PASS: HOME matches TUNER and the authoritative volume channel is ready.
FAIL/STOP: UNKNOWN/stale state, unavailable controls, or repeated reconnects.

Observation: `____________________________________________________________`

## B. Source switching and Bluetooth

### B1 — Radio to Bluetooth (manual)

- [ ] On HOME tap `TUNER CONTROL`, then `BLUETOOTH`.
- [ ] Expect the compact `Starting Bluetooth...` popup; no optimistic source
  highlight is allowed.
- [ ] On authoritative completion expect `Bluetooth ready` for about 3 s,
  then HOME. With no phone connected HOME must show
  `Waiting for Bluetooth device...`.
- [ ] Tap BLUETOOTH again while already authoritative; expect no command,
  popup, restart, or navigation change.
- [ ] Connect the phone and play audio. Confirm audible A2DP, metadata on both
  devices, and only supported Previous/Play-Pause/Next controls enabled.
- [ ] Disconnect the phone. TUNER must remain in Bluetooth mode and REMOTE
  must return to the waiting state without switching to Radio.

PASS: every observed state follows authoritative TUNER/phone state.
FAIL: optimistic source state, unintended retry/restart, stale metadata, or
automatic Radio fallback.

Observation: `____________________________________________________________`

### B2 — Bluetooth to Radio (manual)

- [ ] Reconnect/play Bluetooth if needed, open TUNER CONTROL and tap `RADIO`.
- [ ] Expect `Switching to Radio...`, then `Radio ready` for about 3 s and
  HOME.
- [ ] Confirm TUNER returns to Station Select stopped; it must not resume the
  prior radio station automatically.
- [ ] Tap RADIO again while already authoritative; expect no command or popup.

PASS: clean restartable Bluetooth shutdown and stopped Radio selection state.
FAIL: stale Bluetooth metadata, automatic station resume, or source mismatch.

Observation: `____________________________________________________________`

## C. Independent navigation and TUNER priority

- [ ] Open a REMOTE station list and scroll away from the first row.
- [ ] While it remains open, rotate/click the physical TUNER encoder through
  its own menu without selecting a destructive operation.
- [ ] Confirm TUNER e-paper/cursor responds while REMOTE page, row focus and
  scroll position do not follow it.
- [ ] Start a saved station using TUNER. Confirm REMOTE updates shared source,
  station, metadata and playback authoritatively.
- [ ] Change TUNER volume physically; confirm REMOTE volume follows.
- [ ] Navigate REMOTE BACK/EXIT; confirm TUNER cursor and e-paper page do not
  change merely because REMOTE navigates.

PASS: both interfaces remain usable; physical state wins, shared media state
synchronizes, and navigation remains independent.
FAIL: REMOTE drives the TUNER cursor/display, physical control stalls, or
stale REMOTE state overwrites a physical change.

Observation: `____________________________________________________________`

## D. Saved Stations, Favorites and Recent

Before destructive checks, record list order with `/api/stations`. Use only a
controlled temporary station/favorite; never delete an owner record. Station
creation remains web-only through the existing TUNER web portal.

### D1 — Read, order, paging and active highlight (manual + API)

- [ ] Open Saved Stations, Favorites and Recent from TUNER CONTROL; also open
  FAVORITES directly from HOME.
- [ ] Compare Saved/Favorites names and ordering with `/api/stations`.
- [ ] Confirm five-item page loading/scrolling, long-name scrolling, and a
  green play icon/border on the active station where identity is stable.
- [ ] Leave and reopen each list; it must start at the first row.
- [ ] HOME FAVORITES BACK must return HOME; TUNER CONTROL Favorites BACK must
  return to TUNER CONTROL. EXIT must always return HOME without changing audio.

PASS: ordering, entry point, paging, highlight and navigation match above.
FAIL: missing/duplicated rows, unsafe name-only highlight, or wrong BACK path.

Observation: `____________________________________________________________`

### D2 — Short tap and gesture separation (manual)

- [ ] Short-tap a known entry. Expect `Tuning to ...`, authoritative playback,
  `Now Playing`, and HOME after about 3 s.
- [ ] Reopen the list, press a row for at least 2 s without moving. Saved and
  Favorites must show `Delete <name>?`; Recent must offer Add to Favorites.
- [ ] Repeat while deliberately scrolling more than a few pixels; the long
  press must cancel and must not also start playback.

PASS: short tap and 2 s hold are mutually exclusive; scroll cancels hold.
FAIL: a long press also plays, or a scroll mutates a list.

Observation: `____________________________________________________________`

### D3 — Controlled add/delete and dialogs (manual + API)

- [ ] Create one clearly named temporary saved station through the TUNER web
  portal using a known-good direct stream URL; record its exact position.
- [ ] On REMOTE long-press it, choose Cancel, and confirm nothing changes.
- [ ] Long-press it again, choose Delete, expect `Deleting...` then `Deleted`,
  remain in Saved Stations, and confirm only the temporary station disappeared.
- [ ] From a Recent or Browser result not already favorited, hold 2 s. Expect
  `Adding to Favorites...` then `Added to Favorites`; playback must continue.
- [ ] Repeat the add; expect `Already in Favorites` and no duplicate.
- [ ] In Favorites, test Cancel then Delete on only that temporary favorite;
  confirm the Saved record is not deleted.

PASS: one intended mutation per action, correct confirmation semantics,
authoritative refresh, no duplicate/replay, and original owner data restored.
FAIL/STOP: wrong record/order changes or any unintended persistence mutation.

Observation: `____________________________________________________________`

### D4 — Conditional empty/full behavior (manual)

- [ ] If a collection is naturally empty, verify `No stations available` and
  working BACK. Do not empty owner data merely to force this state.
- [ ] If Favorites is naturally 10/10, attempt one controlled add and expect
  `Favorites full (10/10)` with no mutation or playback change. Do not fill the
  list solely for this test.
- [ ] Otherwise record these cases as NOT TESTED, not PASS.

Result: `PASS / FAIL / NOT TESTED`
Observation: `____________________________________________________________`

## E. Radio Browser

### E1 — Complete filter flow (manual)

- [ ] Open Browse All Stations. Expect a compact
  `Searching stations...` popup, then Country choices: Any Country and A-Z.
- [ ] Choose a letter and country (or Any Country), then confirm Station Name
  choices: Popular Stations and A-Z.
- [ ] Choose a name filter, then Genre: Any Genre plus returned genres.
- [ ] Choose a genre and confirm Results. Audio already playing must continue
  while HTTPS search runs; TUNER physical controls and RTCP must remain usable.

PASS: exact Country -> Name -> Genre -> Results order with bounded loading.
FAIL/STOP: blocked audio/controls, reset, stale page, or wrong stage order.

Observation: `____________________________________________________________`

### E2 — Paging, result actions and highlighting (manual)

- [ ] Where more than five results exist, use `More >`; then use
  `< Previous`. Expect stable five-item pages without duplicates.
- [ ] Short-tap one known-good result. Expect `Tuning to ...`, authoritative
  `Now Playing`, and HOME after about 3 s.
- [ ] Reopen the same result set where practical and confirm active green
  play highlighting.
- [ ] Long-press a non-favorite result for 2 s; expect Add to Favorites and no
  playback interruption. Remove only that controlled addition afterward.

PASS: paging, play, highlight and favorite mutation are authoritative.
FAIL: duplicate/stale page, optimistic success, or wrong station/favorite.

Observation: `____________________________________________________________`

### E3 — BACK, EXIT, cancellation, empty and error states (manual)

- [ ] After completed pages, BACK must move Results -> Genre -> Name ->
  Country -> TUNER CONTROL while preserving filters in the current session.
- [ ] EXIT from any depth must return HOME and leave audio unchanged.
- [ ] During an active search, use EXIT (the implemented cancellation path).
  Return to Browse and confirm no stale result or command appears.
- [ ] If a filter naturally yields zero results, expect `No stations found`
  with BACK usable. Do not claim this PASS if no empty query is found.
- [ ] For a controlled error test, interrupt WAN access while keeping the LAN
  and both devices associated, start a search, then restore WAN. Expect a
  sanitized `Radio Browser error` (or `Results too large` for that bounded
  condition), no audio interruption, and BACK usable after dismissing the
  popup. If WAN isolation cannot be done safely, record NOT TESTED.
- [ ] Note: while the busy spinner is active, BACK is intentionally ignored;
  there is no CANCEL button. EXIT performs bounded cancellation.

PASS: no stale result/replay and navigation/error behavior matches above.
FAIL: stale page appears, audio stops, or a cancelled mutation later executes.

Observation: `____________________________________________________________`

## F. Audio, volume and playback synchronization

### F1 — MP3 and AAC/AAC+ (manual + API)

- [ ] Play a previously known-good MP3 saved/browser station and operate
  REMOTE menus during playback for at least 2 min.
- [ ] Play a previously known-good AAC/AAC+ station; confirm the TUNER
  diagnostic `codec` reports AAC and record audio quality separately.
- [ ] Do not classify already-known AAC choppiness/menu latency as a new Full
  Control regression unless it is materially worse than the prior baseline.
- [ ] During each stream, query both status endpoints once and confirm uptime
  continuity, Wi-Fi and RTCP responsiveness.

PASS: MP3 remains stable; AAC behavior is no worse than its documented prior
baseline; no reset or control/network regression.
FAIL: new dropout, decoder/reset failure, or Full Control blocks playback.

Observation: `____________________________________________________________`

### F2 — Volume and playback state (manual + API)

- [ ] Rotate REMOTE encoder slowly, then 10 rapid detents. Every accepted
  detent must contribute to the final authoritative target; no stuck in-flight
  state or stale replay.
- [ ] Change volume using TUNER and, in Bluetooth, the phone; verify HOME
  follows authoritative volume.
- [ ] Use REMOTE Play/Pause in Radio and Bluetooth. Confirm audio, TUNER state,
  phone state where applicable, and REMOTE label reconcile correctly.
- [ ] Exercise REMOTE Previous/Next only where the source advertises support;
  unsupported controls must remain disabled or be refused without side effect.

PASS: volume, playback, metadata and control availability remain synchronized.
FAIL: UNKNOWN overwrite, optimistic/stale display, lost detents, or replay.

Observation: `____________________________________________________________`

## G. Disconnect, reconnect, sleep and wake

### G1 — RTCP/TUNER disconnect (manual + API)

- [ ] Record REMOTE session/close/attempt counters, then power off or disconnect
  only TUNER while TUNER CONTROL is visible.
- [ ] Expect compact `TUNER disconnected` over the current page. Press OK;
  expect HOME and no repeated popup for the unchanged outage.
- [ ] Restore TUNER. Expect automatic discovery/authentication/state refresh
  and no automatic source, playback, volume or catalog mutation.
- [ ] Confirm a new session becomes ready and no stale pending action executes.

PASS: bounded disconnect UI, automatic recovery, authoritative refresh,
no replay.
FAIL: pairing loss, popup loop, stale command, or manual IP reconfiguration.

Observation: `____________________________________________________________`

### G2 — REMOTE inactivity and wake (manual + API)

- [ ] Open a submenu and stop touching REMOTE. At about 30 s expect
  DISPLAY_IDLE; after about 5 min total inactivity expect SYSTEM_SLEEP when
  safe.
- [ ] Wake by touch. The first touch must be consumed, HOME must be restored,
  and the prior submenu/pending operation must not reopen or replay.
- [ ] After refresh, confirm source, station, metadata, playback and volume
  match TUNER and RTCP reconnects automatically.
- [ ] Motion wake is a separate known intermittent QMI8658 limitation; record
  its result independently and do not fail Full Control solely for that known
  issue unless behavior regresses beyond the recorded baseline.

PASS: touch wake-to-HOME, state refresh and no replay.
FAIL: first touch issues a command, stale submenu/action returns, or pairing is
lost.

Observation: `____________________________________________________________`

## H. Stability and resource evidence

### H1 — 10-minute mixed-use observation (manual + automatic sampling)

- [ ] Record initial TUNER `/api/diagnostics` and REMOTE `/api/status`.
- [ ] For 10 min, keep audio playing and alternate non-destructive REMOTE list
  navigation, one Browser search, TUNER physical navigation and HOME viewing.
- [ ] At approximately 5 and 10 min capture both endpoints again; avoid rapid
  diagnostic polling.
- [ ] Compare uptime/reset reason, Wi-Fi disconnect/error counters, RTCP
  session/close/attempt counters, failed allocations, internal free/largest
  block and free PSRAM. Record TUNER RTCP response maxima.
- [ ] Confirm no panic, watchdog, unexpected reset, HTTP/RTCP outage, audio
  stop, corrupted list, or stale mutation replay.

PASS: continuous uptime/audio/network, no new failures, and bounded memory.
FAIL/STOP: any reset, allocation failure, persistent service loss, or data
corruption.
DMA result: `NOT MEASURED / separately captured with authorized serial x`.

Initial evidence: `_________________________________________________________`

Final evidence: `___________________________________________________________`

## Final result

| Area | Result | Notes |
| --- | --- | --- |
| Smoke gate | NOT TESTED | |
| Source switching/Bluetooth | NOT TESTED | |
| Independent navigation/TUNER priority | NOT TESTED | |
| Saved/Favorites/Recent | NOT TESTED | |
| Radio Browser | NOT TESTED | |
| MP3 and AAC/AAC+ | NOT TESTED | |
| Volume/playback synchronization | NOT TESTED | |
| Disconnect/reconnect and sleep/wake | NOT TESTED | |
| Stability/resources | NOT TESTED | |

Overall: `PASS / PARTIAL PASS / FAIL / BLOCKED`
Tester/date: `______________________________________________________________`
Unperformed conditional cases: `___________________________________________`
New defects with reproduction steps: `_____________________________________`

Full Control hardware acceptance is PASS only when every mandatory section is
PASS, conditional cases are honestly marked NOT TESTED where their safe setup
was unavailable, owner data is restored, and no stop condition occurred.
