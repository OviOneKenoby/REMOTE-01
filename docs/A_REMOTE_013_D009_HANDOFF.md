# A-REMOTE-013 / D-009 REMOTE handoff

Status: **SOFTWARE VERIFIED / HARDWARE NOT RUN**
Date: 2026-10-21

## Source and dependency

- REMOTE branch: `remote/a-remote-013-d009`
- Implementation commit:
  `1a29c4e3652fb3aeb7ef496b674691e78aca2d67`
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
| D-009 admission/fencing policy | 12/12 PASS |
| HOME/Core integration | 41/41 PASS, 3,562 assertions |
| Volume admission regression | 9/9 PASS, 238 assertions |
| UI volume binding regression | 1/1 PASS, 11 assertions |
| Power/lifecycle regression | 19/19 PASS, 19,236 assertions, 1,000 stress iterations |
| Ingress/UTF-8 regression | 18/18 PASS, 22,189 assertions, 1,000 stress iterations |
| Production environment | `remote01` PASS |

No previously passing pairing or TUNER suites were repeated because their
inputs did not change.

## Private candidate identity

- Source: `1a29c4e3652fb3aeb7ef496b674691e78aca2d67`
- Application size: 2,575,168 bytes
- Application SHA-256:
  `d9c13a6ce030b9ab2a4db02fad1c5afcf80eee364f0046665a9dcb1652698d76`
- ELF SHA-256:
  `3f8c0f7294742e782bdedb3e860a3af9114bae7e345027c1eafb7435a57eeee3`
- Static RAM: 56,356 bytes
- 3 MiB application-slot headroom: 570,560 bytes
- Partition image SHA-256:
  `a144343580dbdadace66fccc5969d7557de3d4c4f15a61e76db205cc26348184`

Compared with the accepted A-REMOTE-012 application (2,526,848 bytes, 47,388
static RAM bytes, and 618,880 bytes headroom), the candidate is +48,320
application bytes, +8,968 static RAM bytes, and -48,320 bytes of headroom. The
accepted rollback application remains SHA-256
`b6053c04c03114a24a59f178da4eb03d4bada77faded43b6009667579c21d5da`.

The application, ELF, backups, credentials, pairing material, and private
device data remain private and are not part of the public branch.

## Hardware validation still required

1. Reverify retained local and separate-storage rollback backups and exact
   device identity before either write.
2. Install compatible TUNER D-009/discovery firmware first, then install this
   REMOTE application only at the verified application offset; independently
   read back and hash both exact spans.
3. Verify authenticated discovery, identity/capability negotiation, IDs 1-8
   compatibility, volume control, and fresh playback/metadata authority.
4. Verify station and StreamTitle changes, malformed/absent metadata behavior,
   fixed-list previous/next, play/pause, command correlation, and explicit
   refusal in discovered and Bluetooth contexts.
5. Verify station/source generation changes, disconnect/reconnect, session
   replacement, SYSTEM_SLEEP/wake, and absence of stale command replay or stale
   metadata.
6. Recheck MP3, Bluetooth A2DP, web-manager coexistence, UI/configuration,
   motion wake, voltage telemetry, and volume responsiveness.

Rollback must use the preserved verified application or complete-flash backup
for the affected device. No rollback, flash, hardware access, or persistent
mutation was performed during this software phase.
