from pathlib import Path

root = Path(__file__).resolve().parents[2]
runtime = (root / "components/control_home/runtime.cpp").read_text(encoding="utf-8")
ui = (root / "components/user_app/user_app.cpp").read_text(encoding="utf-8")
model = (root / "components/diagnostics/include/diagnostics/model.hpp").read_text(
    encoding="utf-8"
)
json_model = (root / "components/diagnostics/model.cpp").read_text(encoding="utf-8")

checks = {
    "fixed 16-byte model": "static_assert(sizeof(BrowserDiagnostic)==16)" in model,
    "saturating counters": "if(value!=UINT16_MAX)++value" in model,
    "START result received": "browser_channel.diagnostic.counters[0]" in runtime,
    "page query sent": "browser_channel.diagnostic.counters[1]" in runtime,
    "page response received": "browser_channel.diagnostic.counters[2]" in runtime,
    "page accepted": "browser_channel.diagnostic.counters[3]" in runtime,
    "stale generation observed": "browser_channel.diagnostic.counters[4]" in runtime
    and "mismatch_generation_low16" in runtime
    and "mismatch_revision_low16" in runtime,
    "disconnect clears request state": "if(!connected||!supported)" in runtime
    and "browser_channel.query_in_flight=false" in runtime,
    "UI state and pending operation": "browser_ui_diagnostic" in ui
    and "pending_operation" in runtime,
    "read-only status exposure": all(
        field in json_model
        for field in (
            "browser_start_results",
            "browser_page_queries_sent",
            "browser_page_responses",
            "browser_pages_accepted",
            "browser_generation_mismatches",
            "browser_ui_state",
            "browser_pending_operation",
        )
    ),
}

for name, passed in checks.items():
    if not passed:
        raise SystemExit(f"FAIL: {name}")
print(f"REMOTE browser diagnostic policy: PASS ({len(checks)}/{len(checks)})")
