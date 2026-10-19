# A-REMOTE-003 standalone host qualification

Not firmware-selected; no ESP-IDF/NVS calls, no production Core imports, no physical dispatch.
Run `python -B run_qualification.py` here; output is written to ../results. The canonical dated delivery fixtures/logs are in coordination references/device_evidence/remote/A-REMOTE-003. No target build/upload needed.

Production qualification FAIL/BLOCKED: complete profile/resolution exceeds3KiB; maximal profile exceeds existing NVS; valid-old-anchor safety gap. Passing host assertions include the counterexample and are not production acceptance. The projected bounds and unsupported states are documented in docs/A_REMOTE_003_D0_QUALIFICATION.md and coordination PROJECTION_PROFILE.md.
