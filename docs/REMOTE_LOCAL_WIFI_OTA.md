# REMOTE//01 local Wi-Fi OTA

## Status and scope

This feature provides a local-browser updater for REMOTE//01 only. It uses the
existing station-mode HTTP server and ESP-IDF OTA APIs; it does not contact a
cloud service or load external browser assets. TUNER firmware and configuration
are outside this feature.

The current owner authorization requires authenticated upload. That explicit
requirement supersedes the older unauthenticated-LAN design note retained in
`STAGE_2B_LOCAL_OTA.md`. No shared Wi-Fi, RTCP, or provisioning secret is reused.

## Partition and boot model

The accepted 16 MiB layout is unchanged:

| Partition | Offset | Size |
| --- | ---: | ---: |
| `factory` | `0x10000` | `0x800000` |
| `otadata` | `0x810000` | `0x2000` |
| `ota_0` | `0x820000` | `0x300000` |
| `ota_1` | `0xB20000` | `0x300000` |
| `remote_ctl` | `0xE20000` | `0x10000` |

An upload is streamed to the inactive ESP-IDF OTA slot. The running image is
not overwritten. Only after the complete image passes compatibility, SHA-256,
and ESP image validation is the selected boot partition changed.

ESP-IDF automatic application rollback is **not enabled** in the installed
bootloader. Therefore a newly selected image is not automatically reverted if
it later fails to boot. The verified USB application-flash procedure and
preserved full-flash backup remain the recovery path. Secure boot and flash
encryption are also not enabled; SHA-256 provides transfer integrity, not
cryptographic publisher authentication.

## Local authentication and exposure

1. Browse directly to `http://<REMOTE-IP>/ota` from the same local subnet.
2. REMOTE displays a randomly generated six-digit PIN for two minutes.
3. The page sends a SHA-256 challenge response in a POST body. The PIN is never
   placed in a URL or log.
4. Five failed proofs invalidate the challenge. Success creates one random,
   five-minute, single-use `HttpOnly; SameSite=Strict` session cookie.
5. A second or concurrent upload is refused.

The HTTP service is station-mode only, accepts the device's literal IP in the
Host header, and retains the existing local-subnet socket gate. The page has no
external dependencies and includes a local SHA-256 implementation so it works
on Android and desktop browsers that withhold WebCrypto on plain HTTP origins.

This is still plain HTTP. Anyone able to observe or actively manipulate traffic
on the local Wi-Fi may attack the session; WPA network security and a trusted
local LAN remain prerequisites. The updater must not be port-forwarded or
exposed to an untrusted network.

## Image gates and bounded operation

Before the inactive slot is erased, the first 320 bytes must identify:

- a structurally plausible ESP application image;
- ESP32-S3 chip ID `0x0009`;
- an ESP application descriptor; and
- project identity `REMOTE01`.

The request must fit the actual inactive partition and the fixed 3 MiB maximum.
The upload uses one 4096-byte PSRAM transfer buffer, a 30-second receive-idle
deadline, and incremental SHA-256. Truncation, interruption, digest mismatch,
invalid structure, wrong chip/device, allocation failure, or OTA API failure
aborts the OTA handle and leaves the current boot selection unchanged. Sleep is
inhibited while authorization/upload/reboot is active. Network-generation loss
revokes the session.

## Owner update procedure

1. Confirm a preserved USB recovery image and the intended REMOTE candidate's
   recorded size and SHA-256.
2. Keep REMOTE awake and on trusted local Wi-Fi. Open
   `http://<REMOTE-IP>/ota`.
3. Read the PIN from REMOTE and enter it in the browser. Do not share or record
   the PIN.
4. Select the verified application `.bin`, confirm REMOTE//01 as the target,
   and press **Verify and install** once.
5. Keep power and Wi-Fi stable until the page reports successful verification.
   REMOTE then reboots after approximately two seconds.
6. Verify the new build identity, retained Wi-Fi and RTCP pairing, RTCP READY,
   HOME/touchscreen, authoritative volume, and normal radio control.

On any browser error, do not immediately repeat the update. First establish the
running build and boot slot. If REMOTE does not boot, use the preserved USB
application/full-flash recovery evidence; do not erase NVS or `remote_ctl`.

## Verification boundary

Host tests cover authorization state transitions, lockout/expiry, single-use
admission, retry after failed transfer, image identity, size boundaries, hex
and constant-time comparisons, partition policy, absence of persistent-data
erase calls, and browser SHA-256 known-answer vectors. The production build
also compiles the actual ESP-IDF OTA path.

Physical completion additionally requires one application-only USB installation
of the first OTA-capable image, exact-span readback, then one real browser OTA
to a separately identified compatible build. Destructive rollback testing is
not part of acceptance.

## Corrected USB hardware checkpoint

The first USB gate exposed internal-memory pressure before any Wi-Fi OTA was
attempted. An 8192-byte HTTP task prevented mDNS task creation; 6144 bytes let
mDNS start but left only 403 internal bytes and repeated Wi-Fi allocation
failures. Hardware high-water reporting showed 4468 bytes unused at the
6144-byte setting, so the service now uses ESP-IDF's supported 4096-byte
default (about 2420 bytes measured reserve) while retaining the fixed 4096-byte
OTA transfer buffer in PSRAM.

Corrected source `50e6da9b6d440c3205277026cc54f5d31be815de` was installed
application-only at `0x10000`. The independent 2,633,744-byte readback matched
SHA-256
`9c3233580e79fa2e4ed9264b2f48f39b77ac4454a2ed30647e1cc2a8b80355ad`.
After boot, stored Wi-Fi and RTCP pairing were retained; HTTP diagnostics were
responsive, RTCP reached READY, authoritative volume was known/set, and no
allocation failure was reported.

The real browser OTA gate then passed. The owner opened the local `/ota` page,
authorized with the six-digit PIN displayed only on REMOTE, selected the
separately identified `486037f` image and received the explicit verified-image
success response. REMOTE rebooted into project `REMOTE01`, version `486037f`,
with ELF SHA-256
`8fb4df9c36bdf341e6b2b60baae5f14139ec6c0f4e21cd44d03ac2c5710e3c1c`.
The image is 2,634,112 bytes (SHA-256
`59f293df22af137740672f50681e3e7b79104f1b2c9ba5dd152166dbbae96f96`),
leaving 511,616 bytes in the 3 MiB slot. Post-reboot diagnostics confirmed
retained Wi-Fi and pairing, authenticated RTCP READY, authoritative volume
connected/known/set, responsive HTTP and zero allocation failures.

## Offline candidate checkpoint

Implementation commit `37cabec4a314630df9590f783f5c786def699094` passed the
focused OTA model, source/partition policy, diagnostics HTTP policy and network
sleep-boundary checks. The `remote01` production build completed without a new
warning and reports 61,356 bytes static RAM and 2,633,257 bytes linked flash.

The initial USB candidate is 2,633,760 bytes, SHA-256
`6efd529c1f3a20009aa6c2082cf6c987c371398c722c45f931d2fa55b1935f3e`;
its ELF SHA-256 is
`2189fc6b12ba46921cf9a6600898dbeeb0d9084e3e7f08e0c5f5bef5c2c91c56`.
The embedded descriptor identifies project `REMOTE01`, ESP32-S3 and version
`37cabec`. Exact image headroom in either 3 MiB OTA slot is 511,968 bytes.
