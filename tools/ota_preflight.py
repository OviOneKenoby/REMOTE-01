"""Offline integrity preflight for the current unsigned IDF app-image profile.

Not authentication, SDK address/revision verification, an updater or permission
to flash. Source: installed IDF 5.5.3 esp_app_format.h/esp_image_format.c.
Only 1024-byte data chunks are read; there is no firmware/runtime integration.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct


class InvalidImage(ValueError):
    pass


def slot_capacity(csv_path):
    slots = {}
    for line in Path(csv_path).read_text(encoding="utf-8").splitlines():
        fields = [x.strip() for x in line.split(",")]
        if line.lstrip().startswith("#") or len(fields) < 5:
            continue
        if fields[2] in ("ota_0", "ota_1") and fields[1] == "app":
            if fields[2] in slots:
                raise InvalidImage("duplicate OTA slot")
            slots[fields[2]] = int(fields[4], 0)
    if set(slots) != {"ota_0", "ota_1"} or min(slots.values()) <= 0:
        raise InvalidImage("two positive OTA slots required")
    return min(slots.values())


def verify(stream, size, capacity):
    if capacity <= 0 or size < 24 + 8 + 256 + 1 + 32 or size > capacity:
        raise InvalidImage("image size outside inactive-slot capacity")
    offset = 0
    digest = hashlib.sha256()

    def read(n, hashed=True):
        nonlocal offset
        if n < 0 or n > 1024 or n > size - offset:
            raise InvalidImage("truncated or unbounded read")
        data = stream.read(n)
        if len(data) != n:
            raise InvalidImage("truncated image")
        offset += n
        if hashed:
            digest.update(data)
        return data

    header = read(24)
    if header[0] != 0xE9 or not 1 <= header[1] <= 16:
        raise InvalidImage("invalid image magic or segment count")
    if int.from_bytes(header[12:14], "little") != 9:
        raise InvalidImage("not ESP32-S3")
    if header[23] != 1:
        raise InvalidImage("unsupported profile: appended simple SHA-256 required")
    checksum = 0xEF
    for index in range(header[1]):
        _, length = struct.unpack("<II", read(8))
        if length % 4 or length > size - offset:
            raise InvalidImage("invalid segment length")
        if index == 0 and length < 256:
            raise InvalidImage("missing app descriptor")
        remaining = length
        first = True
        while remaining:
            data = read(min(1024, remaining))
            if index == 0 and first and data[:4] != struct.pack("<I", 0xABCD5432):
                raise InvalidImage("missing app descriptor magic")
            first = False
            for value in data:
                checksum ^= value
            remaining -= len(data)
    # IDF places checksum in the final byte of the next 16-byte boundary.
    trailer = read(16 - offset % 16)
    if trailer[-1] != checksum:
        raise InvalidImage("segment checksum mismatch")
    expected = digest.digest()
    if read(32, hashed=False) != expected:
        raise InvalidImage("appended SHA-256 mismatch")
    if offset != size or stream.read(1):
        raise InvalidImage("unsupported trailer or changing input")
    return {"integrity_preflight": "PASS", "bytes": size,
            "slot_bytes": capacity, "headroom_bytes": capacity - size,
            "segments": header[1], "simple_sha256": expected.hex(),
            "authenticity": "UNCONFIRMED", "compatibility": "PARTIAL_CHIP_ONLY",
            "activation_authorized": False}


def inspect(path, capacity):
    with Path(path).open("rb") as stream:
        return verify(stream, os.fstat(stream.fileno()).st_size, capacity)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--partitions", type=Path,
                        default=Path(__file__).resolve().parents[1] / "partitions.csv")
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.image, slot_capacity(args.partitions)), sort_keys=True))
    except (InvalidImage, OSError, ValueError) as error:
        parser.exit(1, f"OTA_PREFLIGHT: FAIL {error}\n")
