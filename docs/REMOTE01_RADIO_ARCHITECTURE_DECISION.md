# REMOTE//01 radio/control product boundary - canonical current status

2026-10-05 - [Owner decision register](REMOTE01_PRODUCT_DECISIONS.md), items1-12/28-34, supersedes earlier option comparisons below.

- DECIDED: physically separate devices; TUNER streaming/decoding/playback/audio output. REMOTE has no local internet-radio playback path.
- DECIDED: TUNER-primary product, protocol-neutral common model/adapter contracts retained. Home Assistant/openHAB optional compatibility; Matter/Smart-TV/IR historical/intended directions subordinate to TUNER Native. Zigbee EXCLUDED.
- DECIDED: mDNS discovery; manual IP/hostname fallback allowed. Actual TUNER protocol UNKNOWN / NOT RECOVERED. Transport DESIGN REQUIRED after verified target evidence; Native API candidate unapproved/unfrozen; WebSocket + HTTPS not selected.
- DECIDED: TUNER persists favorites/recent stations; REMOTE reads/presents/selects them, TUNER executes playback. REMOTE does not persist station/favorite database. TUNER limits not a REMOTE requirement without target evidence.
- DECIDED: REMOTE persists Wi-Fi/UI settings, last associated TUNER and artwork cache; reset/retention/format policy DESIGN REQUIRED. Frozen CCM logical favorite domains do not require local station persistence.
- DECIDED: REMOTE owns active-URL artwork lookup/retrieval/cache/thumbnail display, discarding obsolete context results. Source/format/dimensions/bytes/timeouts/redirects/errors/decode/cache quota/eviction/lifetime/fallback DESIGN REQUIRED. Model context safety remains frozen.

Current missing work is verified target contract, bounded adapter and artwork/storage/UI implementation, not audio-location selection. Network Settings required; backend physical validation blocked by missing UI/harness, not FAIL. SD backup purpose DECIDED, not unknown. Encoder+three buttons DECIDED; electrical implementation/verification and action design/freeze still required. Production UI replaces temporary Mock; no final layout invented.

No protocol/support/implementation acceptance inferred. Dormant REMOTE audio BSP remains inventory only, excluded until cleanup. No local stream/decoder/output milestone remains active.

## Historical superseded radio option audit - preserved verbatim

Earlier OWNER DECISION REQUIRED playback gates, comparative Local playback column and unresolved SD/persistence/artwork classifications below are historical and superseded by the register above. They are not current product options or approval requirements. Actual implementation limitations remain factual dated evidence.

# REMOTE//01 radio architecture decision gate

2026-10-05: **OWNER DECISION REQUIRED; no new architecture selected.** Preserve frozen CCM and `docs/CONTROL_ARCHITECTURE.md`. This document distinguishes possible deployments from implemented behavior; it does not replace either contract.

| Option | Verified current support | Missing evidence / decision |
| --- | --- | --- |
| REMOTE controls external TUNER directly on LAN | Existing architecture describes UI -> facade/Core -> adapter -> target, with Mock implemented. A hub is not a required dependency. | **REAL TUNER SOURCE NOT AVAILABLE IN CURRENT WORKSPACE.** Candidate native API is unapproved; actual source, target capabilities, security/session/recovery and conformance required. |
| REMOTE performs local playback | Dormant raw-PCM/I2S/codec examples only. | Owner must explicitly choose this role; physical audio resources, decoder/stream pipeline and budgets are unverified. |
| External automation integration, including documented HA/Matter directions | Architecture future adapters/integration directions only. | No verified owner deployment, real integration or external engine here. Integration direction/ownership must be selected; these names do not establish an audio engine. |

No yoRadio code, protocol, wiring, codec, station catalogue or UI assumptions are imported. REMOTE's control layer must remain protocol-neutral. Core's current bounded implementation is not a claim of complete CCM conformance; accepted Mock tests are not real-target protocol tests.

## Required owner decisions

Choose whether audio is external TUNER, local REMOTE, or an explicitly specified combination. Supply actual target source/protocol if external. Decide source/catalogue/playlist/favorites ownership, supported formats and artwork origin, discovery/pairing/security, volume semantics and unavailable-target UX. Local playback additionally requires audio output/load, storage purpose and resource budget decisions. Do not invent station management or Radio Browser requirements.

Implementation order: decision and verified source/hardware -> approved adapter/pipeline contract -> host capability/session/failure tests -> resource build -> owner end-to-end playback validation -> production UI/release. OTA and controls remain separate gated branches; no network radio code was added.

## UI inventory and boundary

Current factory Screen0 and HOME show copied authoritative STA IP, HOST TESTED/BUILD VERIFIED and owner readability pending after previous session. HOME battery voltage is POWER-derived, not SOC. Factory voltage/RTC/IMU/SD/Wi-Fi/BLE demo producers remain disabled and values are static. A static Wi-Fi footer symbol is not an independent connectivity measurement.

HOME PREVIOUS/PLAY-PAUSE/NEXT follow the accepted Core/Mock path. Volume is presentation-only; artwork remains placeholder; Favorites/Devices/Remotes rows are inert. Factory instruction text still refers to demo functions/inaccessible buttons; these are development artifacts, not final product affordances. No production UI was redesigned.

Final Network Settings must show current IP and offer user-accessible Wi-Fi setup/reset for router changes. Wi-Fi credential reset must stay distinct from deliberate confirmed factory/general reset. The local backend exists; user wizard and general erase policy remain unimplemented/decision-gated. Encoder plus three buttons are owner requirements; optional encoder push, physical GPIOs and action meanings are unconfirmed.

Qualified fonts12/14/16/20/28 and panel details retain physical owner evidence; they do not authorize a final layout. Remaining screen flow, navigation/focus, favorites/device selection, playback/volume/artwork error states, network/recovery feedback, battery-invalid/LOW/CRITICAL indications and accessibility require approved UI requirements. Motion/touch first-contact rules and charging/external-power UNKNOWN must remain truthful.

The [completion roadmap](REMOTE01_COMPLETION_ROADMAP.md) tracks these as gaps, not implemented features. Stage1 evidence, POWER headroom measurement, OTA trust/rollback/transfer, physical controls/SD/audio, final endurance and release acceptance remain separate gates.

## Comparative engineering implications (not measured budgets)

| Dimension | Separate TUNER | Local playback | Documented automation direction |
| --- | --- | --- | --- |
| Reuse / CCM | Existing facade/Core/Mock test machinery; new real adapter must honor frozen capability/session boundaries. | Same control boundaries remain; a local playback owner/adapter would need approval, not direct UI decoder calls. | Future integration adapters only; exact role/contracts still unspecified. |
| Network / security | Approved target authentication, discovery and stale-session handling needed; LAN diagnostics restrictions are not target authentication. | Stream server TLS/source trust, redirects, bounded reconnect and parsing needed; provisioning unchanged. | Peer trust and network failure/identity requirements depend on approved integration. |
| RAM / CPU | Transport/parser/state budgets unmeasured; no local decoding requirement established. | Decoder, encoded/PCM buffers, DMA and scheduling contend with LCD/Wi-Fi/PSRAM; no defensible numeric budget yet. | Unmeasured; no external deployment evidence or runtime implementation. |
| Audio hardware | External target owns output; target hardware absent here. | Codec/output/load and pins unverified; dormant examples insufficient. | No audio-path claim implied by HA/Matter. |
| UI | Real target availability/station/volume/artwork semantics must replace Mock only after contract approval. | Local stream/buffering/output errors need approved representation. | Integration-specific status requires owner design. |
| OTA / recovery | REMOTE and target firmware compatibility/independent recovery required; OTA trust remains unresolved. | Larger future image must fit slots; recovery must stop audio safely and respect POWER. No headroom extrapolation from examples. | Update/recovery scope depends on endpoint role; no transfer authorized. |
| Testability | Host transport/capability/lateness faults, then actual target interoperability. | Host parsers/queue/fault tests plus decoder/DMA/audio/load/endurance hardware tests. | Contract/peer failure host tests then actual integration acceptance. |

## Bounded UI requirement classification

| Item | Current classification / remaining gate |
| --- | --- |
| HOME/playback/volume | REQUIREMENT VERIFIED; UI EXISTS and BACKEND EXISTS through Mock; real target OWNER DESIGN REQUIRED. Volume is read-only in current HOME. |
| Screen0 and HOME IP/network state | REQUIREMENT VERIFIED; IMPLEMENTATION EXISTS, BACKEND EXISTS, UI EXISTS; prior-session physical readability pending. |
| Battery | REQUIREMENT VERIFIED; BACKEND EXISTS / voltage UI EXISTS; physical accuracy/endurance and LOW/CRITICAL presentation acceptance pending; SOC/charging UNKNOWN. |
| Network Settings/wizard/recovery | REQUIREMENT VERIFIED; BACKEND EXISTS partial; final UI OWNER DESIGN REQUIRED. |
| Wi-Fi reset versus factory reset | REQUIREMENT VERIFIED separation; Wi-Fi confirmed backend exists, general reset refused; erase domains/confirmation and final UI OWNER DESIGN REQUIRED. |
| OTA/update state | Roadmap requirement; offline integrity preflight only; runtime backend/transfer/UI absent, OWNER DESIGN REQUIRED after trust/recovery decisions. |
| SD/storage state | UNKNOWN product purpose; dormant examples, no product backend/UI. OWNER DESIGN REQUIRED only after purpose approval. |
| Encoder/three buttons | REQUIREMENT VERIFIED; neutral host model IMPLEMENTATION EXISTS; GPIO capture/action mapper/UI interaction absent, OWNER DESIGN REQUIRED. Push UNKNOWN. |
| Diagnostics/service information | IMPLEMENTATION EXISTS / BACKEND EXISTS / browser UI EXISTS, Stage2B owner PASS; production on-device service UI OWNER DESIGN REQUIRED. |
| Station/favorites/devices/remotes/artwork | Mock/placeholder/inert rows UI EXISTS, real semantics/backend absent or UNKNOWN; OWNER DESIGN REQUIRED, not invented features. |

**OWNER DECISION REQUIRED:** smallest next input is the selected playback location/role and, for external TUNER, its actual source plus approved protocol; for local playback, confirmed audio wiring/output and supported-source requirements.
