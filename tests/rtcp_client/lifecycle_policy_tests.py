"""Bounded source-level checks for explicit RTCP teardown classification."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "components/rtcp_client/client.cpp").read_text(encoding="utf-8")

checks = {
    "suspend classified": "close(CloseReason::suspend,false)" in source,
    "network loss classified": "close(CloseReason::network_lost,false)" in source,
    "connect timeout classified": "CloseReason::connect_timeout" in source,
    "response timeout classified": "CloseReason::response_timeout" in source,
    "peer EOF classified": "CloseReason::peer_eof" in source,
    "short read classified": "CloseReason::short_read" in source,
    "socket error classified": "CloseReason::receive_failed" in source,
    "send failure classified": "CloseReason::send_failed" in source,
    "pending request cleared": "pending_request_=0" in source.split("void Client::close", 1)[1].split("bool Client::connect_start", 1)[0],
    "no unclassified close": "close();" not in source and "close(false)" not in source,
    "status unavailability retained": "NetworkDecision::unavailable)return Event::none" in source,
    "confirmed disconnect classified": "network==NetworkDecision::disconnected" in source,
    "generation change classified": "network==NetworkDecision::generation_changed" in source,
    "deadline receive helper used": "response_deadline_requires_close" in source,
    "bounded response drain used": "now+response_close_deadline_ms" in source,
    "diagnostic keeps service origin": "deadline_-response_close_deadline_ms" in source,
    "handshake deadline helper used": "handshake_deadline_requires_close" in source,
}

receive = source.index("const auto before=rx_used_;const auto reading=receive()")
server_hello = source.index("if(state_==State::server_hello&&rx_used_==56)", receive)
server_hello_accept = source.index("accept_server_hello()", server_hello)
handshake_deadline = source.index(
    "if(handshake_deadline_requires_close(now,deadline_,handshake_active,handshake_accepted))",
    server_hello_accept,
)
accept = source.index("auto e=accept_record()", receive)
deadline = source.index("if(response_deadline_requires_close", accept)
checks["server hello serviced before handshake deadline close"] = (
    receive < server_hello < server_hello_accept < handshake_deadline
)
checks["empty handshake still times out"] = (
    "DiagnosticFailure::timeout" in source[handshake_deadline:accept]
    and "CloseReason::response_timeout" in source[handshake_deadline:accept]
)
checks["partial server hello retains short-read policy"] = (
    "const bool partial=rx_used_!=0" in source[handshake_deadline:accept]
    and "CloseReason::short_read" in source[handshake_deadline:accept]
)
server_validation = source[
    source.index("bool Client::accept_server_hello"):
    source.index("bool Client::send_plain")
]
checks["invalid server hello remains rejected"] = (
    "DiagnosticFailure::invalid_frame" in server_validation
)
checks["server authentication remains enforced"] = (
    "DiagnosticFailure::proof" in server_validation
    and "derive(association_" in server_validation
)
checks["response serviced before deadline close"] = receive < accept < deadline
failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("FAIL: " + ", ".join(failed))
print(f"RTCP_LIFECYCLE_POLICY: PASS={len(checks)} FAIL=0")
