# REMOTE Artwork Implementation

## Stage 2: bounded online album covers

Stage 2 adds album-cover lookup without changing the RTCP wire protocol,
TUNER firmware, HOME geometry, NVS or partitions. The RTCP owner publishes a
fixed protocol-neutral metadata copy containing source, station, separate BTX
artist/title fields, Radio StreamTitle, session, source generation, media
generation and metadata revision. A local request generation completes the
stale-result fence.

Radio metadata is accepted only for one unambiguous `Artist - Title`,
`Artist – Title` or `Artist — Title` separator. Missing, repeated, URL-like,
advertising, station-identification and slogan metadata stays on the generic
source placeholder. Bluetooth uses its existing separate artist and title
fields and never splits the HOME display string.

One low-priority worker performs HTTPS activity and never calls LVGL. It is
eligible only while Wi-Fi is connected, provisioning/scanning are inactive,
OTA is idle, commands are allowed and the UI is not parked for sleep. New
metadata immediately restores the generic Radio/Bluetooth placeholder. A
completed image is copied to the boot-lifetime Stage 1 surface only if every
semantic media fence still matches; track/source/session changes discard stale
work. A metadata-revision-only update with identical source, session, media
generation, station, artist and title is not a new artwork identity.

### Lookup and transport policy

- MusicBrainz release-group search uses HTTPS, `fmt=json`, a maximum of five
  results and the descriptive User-Agent
  `REMOTE01/<build> (https://github.com/OviOneKenoby/REMOTE-01)`.
- Split AVRCP metadata updates are coalesced for 1.5 seconds before a lookup.
  Heavy artwork HTTPS starts are separated by at least five seconds, exceeding
  MusicBrainz's published one-request-per-second limit and leaving bounded CPU0
  idle time between Wi-Fi/TLS bursts. Both waits recheck current-media and
  lifecycle fences. The JSON body is capped at 64 KiB and parsed without
  dynamic allocation.
- Selection requires score 90 or above plus exact normalized artist and title.
  Conflicting titles (including live/remix variants) are rejected rather than
  guessed. Ranked unique exact release groups are tried in bounded order.
- Cover Art Archive uses the accepted release-group `front-250` endpoint.
  Certificate-bundle verification remains enabled; at most three HTTPS-only
  redirects are accepted. Each request has an 8-second timeout.
- Only successful JSON or JPEG responses are accepted. The encoded image is
  capped at 128 KiB. Missing, ambiguous, oversized, malformed, truncated,
  progressive, non-JPEG or decode-failed results retain the generic artwork.
  Transport failures, HTTP 429 and HTTP 5xx receive two bounded retries after
  5 and 10 seconds while all media/lifecycle fences remain current. Transient
  exhaustion is not negative-cached; deterministic negative results use the
  bounded cache instead of generating repeated traffic.

### Decode, cache and memory bounds

The ESP32-S3 ROM Tiny JPEG decoder handles baseline JPEG only. The preflight
scanner requires SOI/SOF0/SOS/EOI, rejects progressive JPEG, and limits both
dimensions to 2,048 pixels. Decode occurs on the artwork worker, scales by a
ROM-supported power of two and center-crops proportionally to 144 x 144.

Permanent dynamic storage is explicit PSRAM: one 128 KiB encoded buffer, one
41,472-byte RGB565 staging/cache buffer and one 8 KiB JPEG work pool, in
addition to Stage 1's 41,472-byte displayed surface. The worker has one fixed
request, one fixed completion and a four-entry negative cache with a 15-minute
boot-local TTL. Allocation failure disables online covers and leaves local
fallback artwork available. No image is written to filesystem or NVS.

### Offline verification

- Artwork identity/matching/parser/cache/transport/JPEG/scheduling policy:
  **PASS** (80 assertions).
- Artwork UI/ownership/memory/TLS/worker policy: **PASS** (28 assertions).
- HOME metadata UI: **PASS** (15 assertions).
- UI navigation: **PASS** (12 assertions).
- BTX integration policy: **PASS** (15 assertions).
- OTA policy and preflight: **PASS** (8 preflight tests / 685 assertions).
- Sleep boundary and power publication: **PASS** (23 and 82,668 assertions).
- Core, HOME and RTCP ownership passed before the broad harness reached the
  separately documented, pre-existing `utf8_snapshot_bounds` assertion. That
  unrelated expectation remains unchanged.
- Production `remote01` build: **PASS**. Final candidate identity and exact
  3 MiB OTA headroom are recorded after the documentation commit.

Hardware validation remains **NOT TESTED** until the final candidate passes
the existing local OTA safety gates. Stage 3 artist photographs remain
deferred; album lookup failure therefore falls directly to Stage 1 artwork.

## Stage 2 metadata-burst watchdog correction

Build `6c5bff9` was installed by one REMOTE-only application write and verified
by an exact 2,819,680-byte readback with SHA-256
`d6f43fc9638d556a2b162f9b44ff8017279dba883d6d6bac3fe503d6f28d6d9c`.
Certificate validation and the scoped PSRAM TLS allocator continued to pass,
with zero allocation failures and a 9,728-byte largest internal block.

Rapid Bluetooth metadata revisions then caused several MusicBrainz/cover HTTPS
operations in close succession. At uptime 331,916 ms ESP-IDF reported the task
watchdog because `IDLE0` had not run. Matching-ELF decoding places the captured
CPU0 stack in the ESP32-S3 Wi-Fi `ppTask` queue receive path. REMOTE did not
reboot: later diagnostics showed continuous uptime, connected Wi-Fi,
authenticated RTCP and zero allocation failures. The verified defect is
unbounded artwork network-start density during split/transient metadata, not a
new TLS allocation failure.

The correction adds no task, buffer, protocol or timeout relaxation. It
coalesces metadata for 1.5 seconds, suppresses revision-only artwork resubmits,
and spaces heavy HTTPS starts by five seconds while retaining cancellation and
exact stale-completion fencing. Hardware must confirm that rapid track and
commercial transitions no longer produce an IDLE0 watchdog or reset. Correct
cover selection remains dependent on MusicBrainz/CAA availability and the
artist/title-only metadata available over RTCP.

The first hardware pass on that candidate proved the scheduler boundary but
also captured a lifecycle-policy mismatch. `Michael Jackson - Thriller`
lookup validated TLS, then REMOTE entered display-idle before publication.
The approved behavior is to continue read-only artwork lookup while only the
display is off and stop it only for SYSTEM_SLEEP. A power-owned background
eligibility query therefore admits ACTIVE and DISPLAY_IDLE, but rejects
SYSTEM_SLEEP, shutdown and any active park sequence. The existing stricter
`commands_allowed()` contract remains unchanged for control mutations.

If a lookup is interrupted by SYSTEM_SLEEP preparation or confirmed network
loss, the failed request is retained only while its exact media identity stays
current. It remains dormant until eligibility returns. A changed track,
source or session still replaces/cancels it; deterministic failures still
terminate normally.

## Stage 2 hardware regression and memory-placement correction

The first committed Stage 2 candidate (`2efe4a0`, 2,816,992-byte BIN,
SHA-256 `7bbd55c902d98f1dd1bbbaca7793b098e7a9b35db5316f3a79abbe74f8af3533`)
**failed hardware validation**. HOME and touch remained responsive and Wi-Fi
kept its address, but RTCP did not restore metadata/volume. Both `/api/status`
and `/ota` accepted TCP, returned HTTP 200 headers, then delivered zero body
bytes before the client timeout. The owner restored Stage 1 build `8b0b406`,
which restored HOME metadata and volume.

The source-level defect is internal-memory placement. Against the established
61,180-byte baseline, the failed image used 69,620 bytes of static RAM
(+8,440). Linker symbols identify three 632-byte identities, a 2,600-byte
negative cache, a 632-byte UI identity and two approximately 720-byte metadata
copies in internal `.bss`. Startup also created the 7,168-byte artwork worker
stack from internal heap. The last Stage 1 hardware snapshot had only a
9,728-byte largest internal block, so the new stack consumed most of that
contiguous reserve before any online lookup. This explains the shared network
resource failure: small HTTP headers could be queued while response-body and
RTCP/lwIP progress lacked internal allocation headroom. It was not caused by a
MusicBrainz TLS request: no usable RTCP metadata existed to submit such a
request while RTCP was failing.

The correction keeps the same bounded worker and matching behavior but places
its large fixed state, 7,168-byte stack and UI artwork state explicitly in
PSRAM. ESP-IDF's enabled `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` support is
used through `xTaskCreatePinnedToCoreWithCaps`; the task control block and
small synchronization/diagnostic state remain internal. Worker priority is 1,
below the RTCP owner. Allocation failure still leaves generic local artwork.

The corrected offline build uses 63,764 bytes static RAM, recovering 5,856
bytes from the failed candidate before accounting for the 7,168-byte runtime
stack recovery. On recovered Stage 1 firmware, independent predeployment
checks received the complete 3,071-byte `/api/status` JSON body and complete
5,196-byte `/ota` HTML body with HTTP 200.

Corrected build `070321a` (2,817,600-byte BIN, SHA-256
`5f4367c14994cb22f631ac812eb26ffa954ce7b70769636ad9601ff06a5f7820`)
was then installed through the authenticated local OTA path. The device
reported verified installation. After wake, build identity and ELF hash
matched; Wi-Fi was connected at -52 dBm, RTCP reached stage 13 after a
35-45-second delay, authoritative volume was connected/known/set, and both
HTTP endpoints returned complete bodies. The snapshot reported 20,175 bytes
free internal heap, a 9,728-byte largest internal block, 7,343,960 bytes free
PSRAM and zero allocation failures. HOME/touch and generic Radio and Bluetooth
artwork all passed initially.

Hardware validation nevertheless remained **FAIL**. Initial observations
associated the restart with touching Bluetooth HOME, followed by loss of
Wi-Fi. A controlled passive COM3 reproduction later separated the conditions:
Bluetooth mode and HOME touch without a phone were stable, while connecting a
phone and starting a metadata-bearing track reproduced the reset without a
REMOTE touch.

The captured panic is exact: FreeRTOS reported `A stack overflow in task
artwork has been detected`, followed by reset reason 4/PANIC. Matching-ELF
disassembly shows compiled local frames of 704 bytes in `worker`, 2,928 bytes
in `lookup` and 1,296 bytes in `https_get`, totaling 4,928 nested bytes before
the ESP HTTP client/TLS call chain. The 7,168-byte PSRAM task stack therefore
left only 2,240 bytes for those library calls. The backtrace enters the
FreeRTOS stack-overflow hook and is otherwise corrupted, as expected after
overflow.

The focused correction keeps every artwork buffer and the worker stack in
PSRAM, changes only that worker's stack capacity to 16 KiB and appends the
post-lookup high-water margin to its existing bounded result log. This leaves
11,456 bytes beyond the measured application frames for HTTP/TLS while adding
no static or internal stack allocation. Actual MusicBrainz/CAA lookup,
source/stale-image handling, HOME recreation and sleep/wake require retest on
the corrected candidate. No TUNER or RTCP protocol change is included.

Focused verification passes: Artwork 64 behavioral / 26 placement-policy
assertions, HOME metadata 15/15, BTX integration 15/15, RTCP lifecycle 17/17,
RTCP discovery 10/10, diagnostics HTTP, OTA source/partition policy and OTA
preflight 8 tests / 685 assertions. The single production `remote01` build at
implementation `d00af9c` passes with 63,748 bytes static RAM and 2,816,845
bytes linked flash. Its 2,817,344-byte BIN has SHA-256
`ac5476470747ef597d407200ca6ee2bf78003c0d8e1314df9c42bfeb065d1596`
and retains 328,384 bytes exact headroom in a 3 MiB OTA slot. The matching ELF
SHA-256 is
`eae112ac6a367a3dcd4cfee643ef6a1193a2211ef9904d73b753cc36e3c0d10e`.
This candidate is not hardware-validated until the previously failing
Bluetooth metadata lookup completes without reset and reports a safe stack
margin.

Hardware reproduction on the corrected stack build subsequently passed the
original reset boundary: repeated Spotify metadata lookups completed without
panic and reported 8,880 bytes minimum artwork-task stack margin. Spotify also
confirmed that the earlier `Unavailable - Not provided` observation belonged
to a phone application that did not publish AVRCP metadata, not to REMOTE or
TUNER metadata handling.

The first real cover lookup then exposed a separate, precisely bounded TLS
allocation failure. `mbedtls_ssl_setup` returned `-0x7F00`
(`MBEDTLS_ERR_SSL_ALLOC_FAILED`) while internal heap had a 9,728-byte largest
block. The pinned configuration allocates a 16,717-byte inbound TLS record
buffer during setup, so the request cannot succeed regardless of total free
bytes. The correction uses a link-time wrapper already validated in the TUNER
project: only the exact 16,717-byte input-record request is PSRAM-first. The
original ESP-IDF internal allocator remains the fallback and handles every
other mbedTLS allocation, preserving certificate/session/key placement and
RTCP security behavior. A compile-time guard pins the 16,384-byte configured
input content length, host tests cover exact selection and overflow, and an ELF
check verifies the allocator pointer reaches the wrapper while the original
allocator remains linked. Actual MusicBrainz and Cover Art Archive retrieval
still requires hardware validation.

The first post-TLS functional check then identified an independent response
bound: MusicBrainz returned HTTP 200 with 47,216 bytes for the exact
`Daft Punk - Within` query, exceeding the original 32 KiB search allowance.
The worker therefore rejected the response before parsing despite successful
certificate validation. The existing encoded download storage is already a
fixed 128 KiB PSRAM allocation. Raising only `search_limit` to 64 KiB admits
the measured response without allocating another byte and retains a finite
bound, the exact artist/title/score policy and multi-release ambiguity
rejection. Hardware retrieval and JPEG decode require retest.

The clean `remote01` production build embeds implementation `7837137`, uses
63,748 bytes static RAM and 2,816,913 bytes linked flash, and produces a
2,817,408-byte BIN with 328,320 bytes headroom in either 3 MiB OTA slot. BIN
SHA-256 is
`d67f03325fa8761ae968be271274c8f1f1930cac877cc5a8aa081a6556259e70`;
the 21,653,176-byte ELF SHA-256 is
`e0a2c0be19338869a47e9193d8dca9c6a83d53ccf7ee804500c109509c0d99de`.
The final ELF binds `mbedtls_calloc_func` to the wrapper and retains the
original allocator symbol for the internal-memory fallback. Hardware status is
still pending.

## Stage 1 scope

Artwork Stage 1 supplies a local, hardware-testable image path for the HOME
screen. It implements source-specific Radio and Bluetooth placeholders and a
neutral disconnected/unknown placeholder. Station logos, Radio Browser
favicons, album lookup, artist lookup, HTTPS downloads, image caches and
runtime PNG/JPEG decoding are outside this stage.

The approved future selection order remains:

1. A reliable album cover.
2. A reliable artist photograph.
3. The generic placeholder for the authoritative playback source.

## Architecture and ownership

- `artwork::Model` accepts only the existing authoritative Full Control
  connection/source observation. Disconnected, unknown and invalid source
  values select the neutral placeholder; station names and metadata are never
  used as source heuristics.
- `artwork::Surface` owns one 144 x 144 RGB565 buffer allocated explicitly from
  PSRAM (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`). Its fixed cost is 41,472
  bytes. Allocation is attempted once and the buffer is retained until reboot.
- The corresponding `lv_img_dsc_t` has the same boot lifetime. HOME widgets may
  be destroyed and recreated without retaining a dangling descriptor or pixel
  pointer.
- The existing LVGL/UI owner creates the image widget, observes source changes,
  renders the local placeholder and invalidates the widget. There is no artwork
  task and no LVGL access from networking or RTCP code.
- Unchanged source observations cause no redraw and no allocation. If the
  one-time PSRAM allocation fails, the panel displays its local text fallback.
- The panel remains at `(12, 96)`, 148 x 148 pixels. The 144 x 144 image is
  centered and does not alter adjacent HOME controls, metadata or navigation.

The exposed UI write buffer is intentionally limited to the UI owner. A later
asynchronous decoder/downloader must use a separate bounded staging buffer and
handoff; it must not write the displayed surface concurrently.

## Lifecycle behavior

Initial boot and RTCP disconnect select neutral. Authenticated Radio and
Bluetooth states select their matching images. Reconnection and Radio/Bluetooth
transitions update deterministically. HOME exit deletes only widget pointers;
HOME re-entry reuses the valid surface and current authoritative state. Display
sleep does not delete or transfer buffer ownership, and wake refreshes from the
same state path. Metadata changes do not redraw artwork unless the source also
changes.

## Verification checkpoint

- Artwork model and fallback behavior: **PASS** (20 assertions).
- UI ownership/layout/memory/decoder policy: **PASS** (15 assertions).
- HOME metadata policy: **PASS** (15 assertions).
- UI navigation layout: **PASS** (12 assertions).
- BTX integration policy: **PASS** (15 assertions).
- D-009 policy: **PASS** (16 assertions).
- Local OTA source/partition policy: **PASS**.
- Core: **PASS** (69 cases, 2,425 assertions).
- HOME: **PASS** (42 cases, 3,565 assertions).
- RTCP ownership: **PASS** (4 cases, 92 assertions).
- Broad hardening: 25 PASS / 1 pre-existing FAIL. The separately tracked
  `utf8_snapshot_bounds` length expectation is unchanged and unrelated to the
  artwork files; no speculative correction is included here.
- Production `remote01`: **PASS** after one transient compiler internal error
  in an unchanged generated LVGL image compiled successfully on the single
  incremental retry.

Final firmware identity and partition headroom are recorded in the private
candidate manifest after the source commit.

## Hardware validation

The exact candidate was installed through the previously accepted local Wi-Fi
OTA workflow. The updater reported successful REMOTE//01 image verification
before reboot. Post-reboot diagnostics independently identified project
`REMOTE01`, version `8b0b406` and the candidate ELF SHA-256.

Owner-observed checks passed:

- Radio mode displayed the Radio placeholder.
- Bluetooth mode displayed the Bluetooth placeholder and HOME remained
  responsive.
- Bluetooth to Radio restored the Radio placeholder without a stale image or
  persistent error.
- Leaving and recreating HOME retained the correct source image, metadata and
  touch behavior.
- Display idle followed by SYSTEM_SLEEP/wake restored HOME, artwork, metadata,
  volume and responsive touch without unintended playback behavior.

The final read-only snapshot reported Wi-Fi connected at -52 dBm, RTCP stage
13 with no current failure, authoritative volume connected/known/set, no
outstanding operation, zero allocation/network/power errors, 29,311 bytes free
internal heap (9,728-byte largest block), and 7,533,728 bytes free PSRAM.
Artwork Stage 1 hardware validation is therefore **PASS**. TUNER was not
flashed, reset or otherwise modified by this milestone.

### Stage 2 display-idle scheduling checkpoint

Clean build `9084580` proved that the power-owned lifecycle gate permits an
unchanged artwork lookup while the display is off and keeps SYSTEM_SLEEP
ineligible. An ABBA lookup completed as `cover_ready`. A subsequent lookup
during `DISPLAY_IDLE` exposed two task-watchdog warnings: the installed ELF
decoded both CPU0 stacks to the artwork worker's synchronous HTTPS TLS
handshake, including X.509/ECDSA verification and the scoped mbedTLS allocator.
The device remained up with Wi-Fi, RTCP and volume synchronized and zero
allocation failures, so this is verified CPU scheduling starvation rather than
heap exhaustion or a transport failure.

The artwork worker is consequently scheduled at `tskIDLE_PRIORITY`. This keeps
online cover retrieval as background work and allows the monitored CPU0 idle
task to run while mbedTLS is CPU-bound. TLS verification, protocol behavior,
lookup fencing and task placement are unchanged. Hardware acceptance of this
priority correction remains pending.

## Stage 1 next-milestone boundary (historical)

The next artwork milestone may add one bounded decoder and network worker only
after this lifecycle and memory ownership have passed hardware validation. It
must preserve the authoritative-source model, explicit buffer lifetimes and
LVGL UI ownership established here.
