# REMOTE Full Control — Owner-Approved Specification

Status: **binding acceptance specification**  
Approved: 2026-10-25  
Scope: REMOTE//01 touchscreen and encoder control of TUNER//01 over authenticated RTCP/1

This document records approved product behavior. It is intentionally separate
from implementation choices. Future work that changes REMOTE UI, navigation,
source switching, or station management must read this specification first.
If a technical limitation conflicts with it, the limitation must be reported
for owner decision; the behavior must not be silently reinterpreted.

## 1. Authority and safety

- TUNER is authoritative for source, playback, metadata, volume, stations,
  lists, browser state, and persistent mutations.
- REMOTE is an authenticated Wi-Fi/RTCP controller. It never connects to the
  phone over Bluetooth; phone Bluetooth/A2DP/AVRCP terminates at TUNER.
- TUNER remains fully usable without REMOTE.
- Physical TUNER controls have priority. Simultaneous physical and REMOTE
  navigation is permitted. A REMOTE operation made stale by a physical action
  is rejected, authoritative state is refreshed, and the operation is not
  automatically retried. Ordinary local/remote navigation conflicts do not
  produce nuisance popups.
- Commands are semantic operations, not injected raw physical input events.
- Existing RTCP authentication, encryption, replay protection, correlation,
  generation fencing, bounded storage, and one-request-in-flight safety remain
  mandatory.

## 2. Portrait HOME and top-level navigation

The display remains portrait. The existing HOME screen remains the third
carousel page (carousel index/page 2), preserving:

- device and Wi-Fi information;
- artwork area;
- station, artist, and title metadata;
- playback-state indicator;
- Previous, Play/Pause, and Next controls;
- authoritative and provisional volume presentation;
- the accepted visual style and carousel behavior.

HOME navigation rows are exactly:

1. `FAVORITES`
2. `DEVICES`
3. `TUNER CONTROL`

`FAVORITES` opens the real Favorites list directly. `DEVICES` is reserved for
future Smart Home integration and must not be repurposed. `TUNER CONTROL`
opens one dedicated control view; individual menus must not become additional
top-level carousel pages.

## 3. TUNER CONTROL main view

The dedicated portrait view contains:

- title `TUNER CONTROL`;
- `RADIO` and `BLUETOOTH` source selectors;
- `Saved Stations`, `Favorites`, `Recent`, and `Browse All Stations`;
- `BACK` at top-left and `EXIT` at top-right.

The source highlight is derived only from authoritative TUNER state. A tap is
not itself evidence of a source change.

`BACK` moves exactly one navigation level. At the main view it returns HOME.
`EXIT` returns directly to HOME from any depth. Neither action changes audio.
When Favorites was entered from HOME, BACK returns directly to HOME; when it
was entered through TUNER CONTROL, BACK returns to the TUNER CONTROL main view.

## 4. Source switching

### Radio to Bluetooth

If Bluetooth is already authoritative, selecting it does absolutely nothing:
no command, popup, mode restart, or navigation change. Otherwise REMOTE sends
one authenticated source-selection command and shows the compact centered
popup `Starting Bluetooth...` with an animated progress indicator. Duplicate
source requests are disabled while pending.

Success requires authoritative TUNER completion, not an optimistic tap. On
success the popup says `Bluetooth ready` for approximately three seconds and
then returns HOME. Starting the restartable A2DP Sink service counts as source
switch success even if no phone is connected. HOME then displays
`Waiting for Bluetooth device...`. A later phone connection updates metadata
and only the AVRCP operations actually supported. If the phone disconnects,
TUNER remains in Bluetooth mode, HOME returns to the waiting state, and REMOTE
does not switch back to Radio automatically.

### Bluetooth to Radio

If Radio is already authoritative, selecting it does nothing. Otherwise
REMOTE sends one authenticated source-selection command and shows
`Switching to Radio...`. TUNER stops Bluetooth through its restartable
lifecycle, retains controller/Bluedroid resources needed for a later restart,
and returns to Station Select. It must not resume the previous radio station.
The authoritative post-switch playback state is stopped/selection-ready, not
PLAYING. On authoritative success, `Radio ready` remains for approximately
three seconds and REMOTE returns HOME.

### Failures, physical changes, startup, and reconnect

- Command acknowledgment alone is not success. A failure stops the progress
  indicator, retains the current page and authoritative source, shows a
  sanitized error in the same compact popup, offers BACK to dismiss, and is
  never retried automatically.
- A physical TUNER source change updates REMOTE automatically, with no popup,
  duplicate command, or forced UI navigation.
- At startup/restart/reconnect REMOTE reads actual state, starts at HOME, and
  synchronizes source, playback, metadata, and volume. It emits no automatic
  source, playback, volume, or mutation command.

## 5. Popups

Popups are compact and centered over the existing visible view, optionally
dimming it slightly. Source transitions must not use full-screen modal pages.
Success and ordinary informational popups dismiss after approximately three
seconds. Searching/loading remains visible until the authoritative operation
finishes or fails. Protocol operations use explicit bounded deadlines, not an
arbitrary three-second operation timeout. UI must never invent success.

## 6. Common station-list behavior

Saved Stations, Favorites, Recent, and Radio Browser results use a consistent
portrait, vertically scrollable list of simple station-name rows without
per-row artwork. Every entry/re-entry starts at the first row; previous scroll
position is not restored after leaving and reopening. Original TUNER ordering
is preserved. The current station is highlighted with green text and a small
play icon when a stable identity permits it; display name alone is not a safe
identity when names can collide. No station-details page is required.

An empty Saved, Favorites, or Recent list displays `No stations available`
with BACK still usable. No meaningless disabled placeholder rows are shown.

### Gestures

- Short tap selects and starts a station immediately.
- Long press threshold is two seconds.
- A completed long press must not also trigger short-tap playback.
- Finger movement/scroll cancels the long-press action through a bounded
  gesture state machine.

During selection REMOTE shows `Tuning to [station]...`. Success is shown only
after authoritative playback evidence where the protocol can supply it; then
`Now Playing` is displayed and HOME returns after approximately three seconds.
If tuning fails, REMOTE returns HOME, shows `Station unavailable` (or an
equivalent sanitized error) for approximately three seconds, leaves the actual
audio state intact, and does not reopen the previous list.

## 7. Saved Stations

Capacity remains 15 and the current NVS layout/order is preserved.

- Short tap plays the station.
- Two-second long press offers `Delete`.
- Delete opens a confirmation containing the station name and explicit
  `Delete` / `Cancel` choices.
- Cancel changes nothing. Confirm performs one explicit authenticated delete.
- After deletion, remain in Saved Stations, refresh the authoritative list,
  and preserve scroll position when practical.
- Deleting a saved station does not delete a Favorite unless existing TUNER
  semantics explicitly require it.
- REMOTE does not create stations. Manual name/URL entry remains web-only.

## 8. Favorites

Capacity remains 10. HOME FAVORITES is a direct shortcut to this same list.

- Short tap plays the station.
- Two-second long press offers Delete with explicit confirmation.
- Confirm removes only the Favorite, not the Saved Station record.
- After deletion, remain in Favorites and refresh the list.
- Attempting an eleventh entry shows `Favorites full (10/10)` for three
  seconds and changes neither Favorites nor playback.

## 9. Recent

Capacity remains five, preserving TUNER insertion, ordering, deduplication,
and persistence. Recent cannot be manually deleted from REMOTE.

- Short tap plays the station.
- Two-second long press offers `Add to Favorites` without a second destructive
  confirmation.
- Success shows `Added to Favorites` for three seconds, remains in the current
  list, and does not interrupt playback.
- Existing entry shows `Already in Favorites`.
- Full list shows `Favorites full (10/10)`.
- Refusal causes no unintended mutation.

## 10. Radio Browser

The existing TUNER sequence and semantics are preserved:

1. Country: `Any Country`, or A-Z then a country.
2. Station-name filter: `Popular Stations`, or A-Z.
3. Genre: `Any Genre`, or a selected genre.
4. Results: scrollable station names; short tap plays, two-second long press
   offers Add to Favorites.

BACK returns one filter stage and preserves filters within the current browse
session. Re-entering Browse from outside starts at the first item. No results
shows `No stations found` and keeps BACK available.

TUNER's existing Radio Browser HTTPS operations are blocking and have no
verified cancellation. REMOTE therefore shows `Searching stations...` in a
small centered popup with an animated indicator, no elapsed timer, and no
CANCEL button. Conflicting interactions are temporarily disabled. The popup
ends only on authoritative completion/error; errors are sanitized and BACK
returns to the filters.

The browser must use a bounded asynchronous/cooperative TUNER job with safe
ownership, completion, error, and stale-result handling. A blocking HTTPS call
must not be bridged into a synchronous RTCP command or allowed to violate RTCP
deadlines. No new large internal-stack task may be introduced without memory
safety evidence. If this cannot be verified within TUNER memory limits, this
milestone is BLOCKED while earlier working milestones remain intact.

## 11. Persistence and administrative boundaries

Saved/Favorite deletion requires confirmation. Add-to-Favorites does not.
Station creation remains exclusively in the TUNER web interface. Wi-Fi
provisioning, credentials, and timezone remain outside TUNER CONTROL and gain
no RTCP command. Existing NVS namespaces, record capacities, and serialization
formats are preserved. Timed-out/reconnected mutations are never replayed.

## 12. Volume and physical controls

The REMOTE rotary encoder controls volume on every screen through the existing
bounded authenticated CCM path. Volume is displayed only on HOME; TUNER
CONTROL has no duplicate volume widget. Existing Bluetooth phone-volume
behavior is preserved. Navigating menus never changes audio by itself.

Other approved REMOTE physical inputs remain unassigned/reserved until a later
milestone after touchscreen behavior is stable.

## 13. Sleep, wake, and disconnection

The existing inactivity timeout remains. If display-idle/sleep occurs in a
submenu, wake returns HOME, refreshes authoritative TUNER state, consumes the
first wake touch without a command, does not restore the submenu, and does not
replay pending work. Restart likewise begins at HOME and changes no TUNER
state.

If TUNER disconnects while TUNER CONTROL is visible, a compact centered
`TUNER disconnected` popup stays over the current page. It dismisses
automatically on reconnect and state refresh. It has OK; OK dismisses it and
returns HOME without repeatedly reopening it for the unchanged outage. The
same behavior applies if TUNER CONTROL opens while disconnected. Existing
Wi-Fi reconnection behavior is otherwise unchanged.

## 14. Protocol and resource acceptance criteria

- New records are capability-negotiated and never reuse D-009/BTX IDs.
- Wire payloads are bounded binary, not JSON, within the existing 512-byte
  authenticated plaintext maximum.
- Lists are paged/windowed; complete station/browser datasets are not copied
  into internal RAM.
- Generations, request IDs, stale/busy/unsupported results, correlation,
  fail-closed parsing, no replay, and one-request-in-flight behavior remain.
- PSRAM is used only for verified CPU-only storage. DMA/Wi-Fi/Bluetooth
  framework memory, timing-critical state, and relevant stacks remain internal.
- No unbounded String growth or permanent large allocation is acceptable.
- Each milestone records static DRAM impact and the remaining uncertainty in
  runtime internal/DMA headroom.

These requirements are binding even when a milestone must be reported PARTIAL
or BLOCKED.
