# REMOTE//01 dormant audio inventory - canonical product scope

2026-10-05 - [Canonical register](REMOTE01_PRODUCT_DECISIONS.md), items1-4/9. DECIDED: TUNER owns internet-radio streaming/decoding/playback/output; REMOTE does not locally decode/play internet radio. Dormant codec/I2S/PCM examples are hardware/software inventory, excluded until final cleanup. Their absence from running REMOTE is expected, not an unfinished radio pipeline or architecture choice.

Target source/protocol/capabilities still need verification and transport DESIGN REQUIRED. REMOTE artwork handling is required separately and is not audio decoding. No REMOTE audio-output hardware validation is a prerequisite to its controller role. Dormant pin/resource observations remain inventory facts, not product assignments.

## Historical audio audit - superseded local-playback hypotheses

Original audit retained below verbatim. The section Missing pieces if local playback is selected and its conditional role/pipeline approval are SUPERSEDED/outside current product scope. For external TUNER playback is the decided arrangement, not an alternative. No historical failure or absent-code observation is deleted.

# REMOTE//01 audio and radio audit

Evidence date: 2026-10-05. No audio hardware activated. Classifications below distinguish application implementation from SDK/dependency declarations. See [hardware map](REMOTE01_HARDWARE_RESOURCE_MAP.md) and [architecture decision gate](REMOTE01_RADIO_ARCHITECTURE_DECISION.md).

## Findings

| Area | Classification | Repository evidence / limitation |
| --- | --- | --- |
| Core/HOME accepted intent and presentation path | IMPLEMENTED | Active Core/Mock/HOME components; owner Stage2B evidence is preserved. This is control/state behavior, not playback. |
| Track, playback and volume responses | MOCK_ONLY | Mock Adapter profiles, scripted completion/capability/session tests. Radio Paradise/Mellow Mix and TUNER//01 labels do not prove an actual station/device. |
| Local decoding/output | NOT_PRESENT in active application | `main/main.cpp` does not initialize audio. Root CMake excludes `audio_bsp` and `codec_board`. |
| I2S codec examples | DORMANT | `components/audio_bsp/user_audio_bsp.c`, `components/codec_board/codec_init.c`; raw embedded `canon.pcm`, capture/echo, codec read/write. |
| Codec definitions | DORMANT | Selected example S3_LCD_3_49 profile: ES8311 output, ES7210 input, MCLK enabled, PA pin=-1, pa_gain=6. Other ES8388/ES7243/dummy alternatives are not current hardware proof. |
| Codec dependency | BUILD_DEPENDENCY_ONLY | esp_codec_dev declared by excluded components; it is absent from current build_components and current managed-components source. SDK I2S availability does not mean active driver/channel. |
| Amplifier, speaker/headphone, mute circuitry | UNKNOWN | EXIO7 NSMODE is a recorded schematic/reference signal; no active product audio ownership or physical load/amplifier validation. |
| MP3/AAC/AAC+ pipeline | NOT_PRESENT in active application | Reviewed application has raw PCM examples, no wired compressed-stream decoder. LVGL image/font decoders and vendor text matches are unrelated. No supported-format claim. |
| HTTP/HTTPS audio acquisition, playlists | NOT_PRESENT | Network services provision STA/read-only diagnostics; no active streaming/audio pipeline. |
| Station management / Radio Browser | NOT_PRESENT | Mock state is not a catalogue, persistent station store or directory client. |
| Real TUNER communication | DOCUMENTED_ONLY / NOT_PRESENT implementation | `docs/TUNER_NATIVE_API_V1.md` is candidate1, UNAPPROVED/UNFROZEN. No verified real TUNER source is available in this workspace. |
| HA/Matter integrations | DOCUMENTED_ONLY | Future architecture directions in `docs/CONTROL_ARCHITECTURE.md`; no implemented radio engine or verified external installation. |

Answers: REMOTE currently neither decodes nor outputs audio locally. An I2S path exists only as excluded example source. Codec/profile pin definitions exist, but **audio hardware pin mapping is not owner verified**. Stream acquisition, station management, Radio Browser and real TUNER communication are not implemented. Mock is the available target behavior; it is not radio playback.

## Dormant resource and safety review

S3 profile expects I2S DIN6/MCLK7/BCLK15/DOUT45/WS46 and shared I2C47/48. Alternate profiles have conflicts with current display/touch/POWER resources and cannot be copied. Exact pin table and conditional conflicts are in the hardware map. Codec helper creates I2S0 with master channels, auto-clear, standard MSB 32-bit stereo/16kHz defaults; examples request 24kHz with different sample widths. Defaults are not a product audio format contract.

The codec helper's own I2C initialization would need coordination with the existing I2C0 owner; its IDF>=5.4 get-bus helper can retrieve an existing bus, but this alone does not establish safe lifecycle/serialization. Dormant TCA9554 driver probes OUTPUT registers using codec-control addresses0x70/0x40; **probe is not a blanket expander reset**. Address encoding and subsequent direction/level operations must be checked against the selected dependency before any activation. No active SYS_EN overwrite is inferred.

Examples loop indefinitely, allocate 512/1024-byte buffers without a complete allocation/cleanup policy, ignore some read/write results and partial tails, and provide no integrated underrun/reconnect/decoder/task/CPU/RAM measurements. DMA configuration in SDK defaults is not proof of bounded product playback. No codec/library is enabled by this audit.

## Missing pieces if local playback is selected

An explicit owner architecture decision; verified codec/amplifier/load wiring and bus arbitration; approved audio formats/source transport; bounded stream acquisition, decoder and PCM queues; task/DMA/CPU/PSRAM budget; output/mute/volume lifecycle; station/playlist/storage contracts; disconnect/error recovery; controls/power coordination; host fault/stress tests and physical audio/endurance acceptance. This list is conditional, not authorization or a claim that local playback is required.

For external TUNER playback, missing pieces instead include verified target source, approved protocol/security/capability/session semantics, adapter integration, network failure/lateness fencing and end-to-end hardware acceptance. Do not implement the candidate protocol from labels or another radio project's design.
