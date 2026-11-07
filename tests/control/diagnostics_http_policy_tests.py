from pathlib import Path

root = Path(__file__).parents[2]
service = (root / "components/diagnostics/service.cpp").read_text(encoding="utf-8")
model = (root / "components/diagnostics/include/diagnostics/model.hpp").read_text(encoding="utf-8")

assert '"Diagnostics busy"' in service
assert '"Diagnostics serialization overflow"' in service
assert '"Diagnostics storage unavailable"' in service
assert "response_bytes=6248" in model
assert "sizeof(Entry)==24" in model
assert "static_assert(sizeof(Storage)<=10240)" in service
assert "httpd_resp_sendstr(r,s->response)" in service
for field in ("request_type", "reset_reason", "poll_last_ms", "poll_max_ms"):
    assert f'\\"{field}\\"' in service
assert "static_assert(sizeof(RtcpCloseEvent)==32)" in model
assert "static_assert(sizeof(RtcpCloseHistory)==168)" in model
for field in ("rtcp_first_ready_disconnect", "valid", "uptime_ms",
              "session", "network_generation", "socket_error", "request",
              "state", "reason", "awaited", "response_pending",
              "request_type", "reset_reason", "poll_last_ms", "poll_max_ms"):
    assert f'\\"{field}\\"' in (root / "components/diagnostics/model.cpp").read_text(encoding="utf-8")
print("diagnostics HTTP failure categorization: PASS")
