# A-REMOTE-004 bounded recovery redesign / isolated host qualification

2026-10-08. Direct owner authorization;coordination main531aa8cd0435ed9813ab39664888992583df0d28;A003 input74e4f1a681cd589625aff63e91f917076cc22586/PR8preserved. Device is Git-less,flashed/source identity UNKNOWN. No production source/config/test/artifact change,live NVS/credentials access,build/upload/hardware.

Standalone tools/qualification/a_remote_004/prototype implements exact typed string-interned profile and fake BASE/ALLOC/PROOF recovery with bounded two segments/four tail records. Full mandatory represented IDs/request/attempt/contract/evidence/independent past-future keys/conflict roster/non-reuse remain;unsupported wider inputs block before hypothetical handover,not truncated. Full intermediate proof state and future proof slots/bytes preserved;compaction copies all history before trusted-head publication,cleans inactive orphan tail before new append. Root integrity G1-G4 and single owner are explicit assumptions,not hash/readback proof.

Command:`python -B run_qualification.py` in prototype;results under a_remote_004/results,isolated from A003. PASS12tests/7805counted checks/562scenarios,0failures/errors;100compactions/400allocations and both rollback counterexamples. Actual qualified root service/importer/target proof verification NOT IMPLEMENTED. Default unqualified mode GLOBAL_BLOCK,always no recovery emission. Complete trusted-service rollback is undetectable ifG1violated;actual NVS newest integrity unknown.

Measured max states pending5587/intermediate5985/resolved6383bytes;future growth796. BASE6447/PROOF1146/ALLOC162/root76. Raw record reserve15510;max-fixture compaction peak14390. Existing24KiB optimistically uses686/756entries under synthetic512byte old/new other occupancy,718at1024,782at2048,912at4096. Actual free/fragmentation/root overhead UNKNOWN => production capacity BLOCKED;32/48KiB alternatives docs only,no partition change.

Authoritative evidence/report/task/handoff publication:
https://github.com/OviOneKenoby/remote_tuner_system/blob/remote/a-remote-004-redesign-20261008/reports/A-REMOTE-004.md

Remaining:backend newest-head/durable write/CAS/GC qualification,sanitized occupancy and controlled physical power-loss/endurance/latency/resource evidence under separate authorization,verified target proof bounds,production importer/newepoch/session/dedup/current-auth semantics. Native unapproved/unfrozen/not implemented,O01-O17open,common gate OPEN/NOT PASSED,D1unassigned. Hardware NOT RUN;no new firmware/BIN/ELF hashes or hardware acceptance. Prior A003 and sleep/ADC evidence preserved.
