"""Bounded source checks for the frozen BTX/1.0 REMOTE integration."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
client_h = (root / "components/rtcp_client/include/rtcp_client/client.hpp").read_text()
client = (root / "components/rtcp_client/client.cpp").read_text()
wire_h = (root / "components/rtcp_client/include/rtcp_client/wire.hpp").read_text()
wire = (root / "components/rtcp_client/wire.cpp").read_text()
adapter = (root / "components/control_home/rtcp_device.cpp").read_text()
api = (root / "components/control_home/include/control_home/api.hpp").read_text()

checks = {
    "ids_12_15": all(token in wire_h for token in (
        "btx_capability_response=12", "btx_state_request=13",
        "btx_state_response=14", "btx_command=15")),
    "two_query_probe": "Awaited::btx_capabilities" in client and
                       "state_=State::btx_probe" in client,
    "probe_before_snapshot": client.index("state_=State::btx_probe") <
                             client.index("state_=State::ready"),
    "legacy_fallback": "BtxProbeResult::legacy" in wire and
                       "legacy.revision!=first_value.revision" in wire,
    "session_reset": "btx_negotiation_.reset()" in client and
                     "btx_capabilities_={}" in client,
    "strict_codec_bounds": "p.size()>255" in wire and "p[6]>96" in wire and
                           "p[7]>127" in wire and "(p[5]&0xc0)" in wire and
                           "wire::permitted(btx_capabilities_,now_playing_)" in client,
    "single_lane": "pending_request_||awaiting_" in client and
                   "std::array<std::uint8_t,556>rx_{},tx_{}" in client_h,
    "command_no_retry": "pending_request_=0;snapshot_at_=0" in client and
                        "awaited_=Awaited::result" in client,
    "btx_command": "client_.bluetooth" in adapter and "btx_expected" in adapter,
    "authoritative_ui": "Event::btx_state" in adapter and
                        'text_value("Bluetooth", 9)' in adapter and
                        "Phone disconnected" in (root / "components/control_home/include/control_home/btx_policy.hpp").read_text(),
    "btx_playback_only_from_id14": "const bool legacy_playback" in adapter and
                                   "BtxSource::radio" in adapter,
    "freshness_withdrawal": "btx_authoritative_" in adapter and
                            "btx_observed_at_" in adapter,
    "home_bound": "char title[227]{}, subtitle[32]{}" in api,
    "volume_unchanged": "static constexpr std::uint8_t volume_step = 2" in api,
    "no_new_task": "xTaskCreate" not in client and "xTaskCreate" not in adapter,
}

failed = [name for name, ok in checks.items() if not ok]
print(f"BTX_INTEGRATION_POLICY: PASS={len(checks)-len(failed)} FAIL={len(failed)} assertions={len(checks)}")
if failed:
    print("FAILED:", ", ".join(failed))
    raise SystemExit(1)
