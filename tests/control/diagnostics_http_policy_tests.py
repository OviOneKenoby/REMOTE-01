from pathlib import Path

root = Path(__file__).parents[2]
service = (root / "components/diagnostics/service.cpp").read_text(encoding="utf-8")
model = (root / "components/diagnostics/include/diagnostics/model.hpp").read_text(encoding="utf-8")

assert '"Diagnostics busy"' in service
assert '"Diagnostics serialization overflow"' in service
assert '"Diagnostics storage unavailable"' in service
assert "response_bytes=6384" in model
assert "sizeof(Entry)==24" in model
assert "static_assert(sizeof(Storage)<=8192)" in service
assert "httpd_resp_sendstr(r,s->response)" in service
print("diagnostics HTTP failure categorization: PASS")
