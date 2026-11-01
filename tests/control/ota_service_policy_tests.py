from pathlib import Path
import csv
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / "components/ota_service/service.cpp").read_text(encoding="utf-8")
diagnostics_source = (root / "components/diagnostics/service.cpp").read_text(encoding="utf-8")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")

assert "project(REMOTE01)" in cmake
assert "esp_ota_get_next_update_partition(nullptr)" in source
assert "esp_ota_begin(target, request->content_len" in source
assert "esp_ota_write(handle" in source
assert source.index("esp_ota_end(handle)") < source.index("esp_ota_set_boot_partition(target)")
assert "esp_ota_abort(handle)" in source
assert "X-REMOTE-SHA256" in source and "constant_time_equal(actual_digest" in source
assert "ESP_CHIP_ID" not in source  # portable prefix parser owns the chip gate
assert "erase_flash" not in source and "nvs_erase" not in source and "nvs_set" not in source
assert "ota_session=" in source and "HttpOnly; SameSite=Strict" in source
assert "MALLOC_CAP_SPIRAM" in source and "transfer_buffer_bytes = 4096" in source
assert "c.stack_size=4096" in diagnostics_source
assert "crypto.subtle" not in source  # unavailable on ordinary local-HTTP origins

# Execute the exact browser-side SHA-256 implementation against standard
# known-answer vectors when the repository's Node runtime is available.
node = shutil.which("node")
if node:
    start = source.index("function hexDigest")
    end = source.index("\ndocument.getElementById", start)
    browser_hash = source[start:end]
    script = browser_hash + r"""
const te=new TextEncoder();
if(hexDigest(new Uint8Array())!=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")process.exit(1);
if(hexDigest(te.encode("abc"))!=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")process.exit(2);
"""
    subprocess.run([node, "-e", script], check=True)

rows = []
with (root / "partitions.csv").open(newline="", encoding="utf-8") as handle:
    for row in csv.reader(line for line in handle if not line.lstrip().startswith("#")):
        if row:
            rows.append([field.strip() for field in row])
parts = {row[0]: row for row in rows}
assert parts["otadata"][2] == "ota" and int(parts["otadata"][4], 0) == 0x2000
assert int(parts["ota_0"][3], 0) == 0x820000 and int(parts["ota_0"][4], 0) == 0x300000
assert int(parts["ota_1"][3], 0) == 0xB20000 and int(parts["ota_1"][4], 0) == 0x300000
assert parts["nvs"][3:5] == ["0x9000", "0x6000"]
assert parts["remote_ctl"][3:5] == ["0xE20000", "0x10000"]

print("LOCAL OTA SOURCE/PARTITION POLICY: PASS")
