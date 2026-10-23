# BTX/1.0 REMOTE implementation and handoff

Status: **SOFTWARE PASS / INSTALLATION VERIFIED / HARDWARE PARTIAL**
Date: 2026-10-23

## Phase 6B installation checkpoint

The verified REMOTE ESP32-S3 candidate was installed once, application-only at
`0x10000`. The write completed successfully. An independent readback covered
exactly 2,582,432 bytes and matched the approved application SHA-256
`5d84f83b8e9e6f860e7cd807a270eeee697ed42dc361bbb1e1022f639dc4206b`.
Boot diagnostics confirmed build `5b32cbb` and ELF SHA-256
`e4139e2a50d3b6a439c19ade8792a98c2fc67ff9417e57e02df0a9977100ee57`.

Stored Wi-Fi configuration and pairing were retained. Wi-Fi connected with no
reported reconnect or error, and RTCP reached authenticated identity acceptance
without a current transport error. During the bounded automated observation,
the authoritative volume observation and `volume.set` authority were not yet
available. The existing status API does not expose a distinct BTX negotiation
result. Therefore installation and authentication are verified, while volume,
BTX negotiation, metadata, commands, physical controls and coexistence remain
pending. TUNER was not accessed or modified.

## Baseline and authority

- REMOTE branch: `remote/btx-1.0-implementation`
- Stacked base: `remote/a-remote-013-d009` at
  `43305a261d458aa648a65456649f8f9921d2977a`
- Round 5 source: `bfce47816a8b2b8a92f41d76c6a4bc4975d25361`
- BTX implementation source:
  `5b32cbbeb554cd8a4d49094a5bd7c4797fd0b403`
- Approved coordination contract and D-011:
  `6ce849fa17e949a466ed8164b15576dd1459d7aa`
- Read-only TUNER implementation reference:
  `bc9c148a942732c890fcba6e6b15769bc4cd9067`
  (review head `1ac32909f0fe1697f0a7301cd1d3c79956b1a92f`)

The complete BTX/1.0 contract, D-011, compatibility report and reference
vectors were compared with the existing REMOTE and TUNER implementations. No
contract/endpoint incompatibility was found. TUNER source and firmware were not
changed or rebuilt.

## Implemented behavior

The existing authenticated session first performs the unchanged ID 3 to legacy
ID 4 D-009 exchange. Immediately after accepting that response, REMOTE sends
the required second ID 3 before another application request. A valid ID 12
enables BTX only for the current session. A second exact legacy ID 4 selects
clean D-009 fallback. Invalid or unexpected responses, timeout and transport
failure retain fail-closed behavior. Disconnect, replacement session, restart
and sleep teardown remove extension authority.

The codec implements:

- ID 12 capability response with exact 16-byte validation;
- ID 13 one-byte Bluetooth-state request;
- ID 14 bounded authoritative Bluetooth state and UTF-8 metadata response;
- ID 15 explicit Play, Pause, Previous and Next command;
- the unchanged ID 8 correlated command-result format.

ID 14 validation covers exact length, enum and reserved-bit rules, canonical
source/connection/playback combinations, flags, current operation availability,
sample sequence, source generation, metadata revision, 96-byte artist and
127-byte title bounds, and negotiated-capability consistency. BTX plaintext is
bounded to 255 bytes and uses the existing authenticated record buffer.

HOME reuses its existing station/source and StreamTitle widgets. Bluetooth
shows `Bluetooth`; the StreamTitle mapping is artist plus ` - ` plus title,
either field alone, `--`, or `Phone disconnected` as specified. The existing
bidirectional scrolling and lifecycle cleanup handle the bounded 227-byte
presentation string. Radio still uses D-009 station and StreamTitle data.

Play/Pause/Previous/Next enter the existing CCM admission and RTCP correlation
path. A command requires fresh authenticated BTX state, the Bluetooth source,
current operation availability, a matching session and generation, and a free
single-request lane. Playback presentation changes only after authoritative
observation. Commands are not retried or replayed after disconnect, source
change, session replacement or sleep. Radio command policy and the existing
2-percentage-point volume behavior are unchanged.

## Verification inventory

| Verification | Result |
|---|---:|
| REMOTE BTX negotiation and codec | 7/7, 52 assertions |
| REMOTE BTX integration policy | 15/15 |
| Approved reference codec vectors | 17/17 |
| HOME metadata/source policy | 15/15 |
| D-009 policy | 15/15 |
| HOME/Core regression | 42/42, 3,565 assertions |
| Encoder/volume routing regression | 10/10, 98 assertions |
| RTCP D-009 regression | 8/8, 44 assertions |
| Volume admission/lifecycle | 12/12, 323 assertions |
| RTCP freshness/admission | 2/2, 221 assertions |
| POWER lifecycle | 20/20, 19,244 assertions |
| Physical input/quadrature | 19/19, 24,627 assertions |

The approved coordination vector file and the TUNER vector file are
byte-identical. Their SHA-256 is
`b09f93919e5f91a25531e40352e29043a0dd3f31461e3dd152be6b9bf10aaf34`.
This supports wire compatibility; it does not constitute hardware validation.
The existing TUNER BTX focused evidence (183 checks/assertions) was reviewed and
not rerun because TUNER is read-only in this phase.

Compatibility conclusions:

- an old REMOTE continues to use the first D-009 exchange with the new TUNER;
- this REMOTE accepts a second legacy ID 4 and falls back with an old TUNER;
- new endpoints enable BTX only through authenticated ID 12;
- radio remains on D-009 while Bluetooth uses IDs 13-15 and correlated ID 8;
- lifecycle invalidation clears all BTX authority and pending operations.

## Production artifact and resource budget

The production environment is `remote01`. A clean build from the exact
implementation source passed with embedded build identity `5b32cbb`.

| Item | Result |
|---|---:|
| Application size | 2,582,432 bytes |
| Application SHA-256 | `5d84f83b8e9e6f860e7cd807a270eeee697ed42dc361bbb1e1022f639dc4206b` |
| ELF size | 19,817,764 bytes |
| ELF SHA-256 | `e4139e2a50d3b6a439c19ade8792a98c2fc67ff9417e57e02df0a9977100ee57` |
| PlatformIO static RAM | 56,652 / 327,680 bytes |
| 3 MiB application-slot headroom | 563,296 bytes |

Measured host layouts changed as follows:

| Bounded object | Round 5 | BTX | Delta |
|---|---:|---:|---:|
| HOME snapshot/channel | 720 | 912 | +192 |
| RTCP client | 2,464 | 2,488 | +24 |
| RTCP adapter | 1,032 | 1,048 | +16 |

Total additional persistent state is 232 bytes, within the approved 320-byte
limit. The temporary UI snapshot increase is 192 bytes, within the approved
195-byte limit. The 227-byte presentation storage reuses the existing bounded
now-playing object. There is no new task, network queue, transport buffer,
unbounded storage or new dynamic metadata-allocation path.

The firmware and ELF remain private and are not committed. The preserved Round
5 application remains the rollback baseline: 2,578,352 bytes, SHA-256
`6eb44ff4137d8c11f44dc5e0fed0a7061f77b172d4a38982e5219bbd7454e278`.

## Minimum hardware commissioning checklist

1. Verify REMOTE identity, the retained Round 5 rollback files and the exact
   candidate size/hash. Stop on any mismatch.
2. Install only the application at `0x10000`; independently read back exactly
   2,582,432 bytes and require the candidate SHA-256.
3. Verify normal boot, retained Wi-Fi/pairing/configuration, authenticated RTCP,
   authoritative volume and no outstanding command.
4. Confirm negotiated BTX with the installed new TUNER. Legacy fallback is
   software-verified and needs no destructive downgrade test.
5. In radio mode, verify station/StreamTitle, scrolling, volume, Play/Pause and
   fixed-list Previous/Next remain unchanged.
6. Switch to Bluetooth and verify the source label, all metadata combinations,
   long-text scrolling and stale-text clearing.
7. Exercise exactly one authorized Play, Pause, Previous and Next where the
   phone advertises each operation; verify correlated results and subsequent
   authoritative state without automatic retry.
8. Disconnect/reconnect the phone and switch back to radio; verify command
   withdrawal, metadata clearing and restored radio semantics.
9. Verify slow/rapid/reversal volume behavior remains fixed at 2% per accepted
   detent and remains source-independent.
10. Verify RTCP reconnect and one sleep/wake cycle do not replay a command or
    retain stale Bluetooth authority.
11. Check MP3, A2DP and web-manager coexistence and observe no panic, watchdog,
    reset or material resource regression.

Hardware BTX command execution, phone-specific AVRCP availability, metadata
behavior across real phone/player combinations, physical controls and
coexistence remain pending. Phase 6B installed and independently verified the
REMOTE application; it did not access or modify TUNER.
