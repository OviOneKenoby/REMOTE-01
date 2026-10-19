"""Offline malformed-image regression; no SDK, hardware or flash access."""
import hashlib
import io
from pathlib import Path
import struct
import sys
import unittest
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from ota_preflight import InvalidImage, slot_capacity, verify


def image():
    header = bytearray(24)
    header[0:2] = bytes((0xE9, 1)); header[12] = 9; header[23] = 1
    data = struct.pack("<I", 0xABCD5432) + bytes(252)
    body = header + struct.pack("<II", 0x3C000020, len(data)) + data
    checksum = 0xEF
    for byte in data:
        checksum ^= byte
    body += bytes(15) + bytes((checksum,))
    return body + hashlib.sha256(body).digest()


assertions = 0
class Counted(unittest.TestCase):
    def assertEqual(self, *args, **kwargs):
        global assertions
        assertions += 1
        return super().assertEqual(*args, **kwargs)
    def assertFalse(self, *args, **kwargs):
        global assertions
        assertions += 1
        return super().assertFalse(*args, **kwargs)
    def assertRaises(self, *args, **kwargs):
        global assertions
        assertions += 1
        return super().assertRaises(*args, **kwargs)

class PreflightTests(Counted):
    def test_valid_and_not_authorized(self):
        data = image(); result = verify(io.BytesIO(data), len(data), len(data))
        self.assertEqual(result["integrity_preflight"], "PASS")
        self.assertFalse(result["activation_authorized"])
        self.assertEqual(result["authenticity"], "UNCONFIRMED")
        self.assertEqual(result["headroom_bytes"], 0)

    def test_every_truncation(self):
        data = image()
        for size in range(len(data)):
            with self.subTest(size=size), self.assertRaises(InvalidImage):
                verify(io.BytesIO(data[:size]), size, 3145728)

    def test_every_single_byte_corruption(self):
        data = image()
        for index in range(len(data)):
            bad = data.copy(); bad[index] ^= 1
            with self.subTest(index=index), self.assertRaises(InvalidImage):
                verify(io.BytesIO(bad), len(bad), 3145728)

    def test_size_and_short_stream(self):
        data = image()
        for size, cap in ((len(data), len(data)-1), (len(data),0), (len(data)+1,3145728)):
            with self.assertRaises(InvalidImage): verify(io.BytesIO(data), size, cap)
        with self.assertRaises(InvalidImage): verify(io.BytesIO(data[:-1]),len(data),3145728)

    def test_trailing_bytes(self):
        data=image()+bytes(16)
        with self.assertRaises(InvalidImage): verify(io.BytesIO(data),len(data),3145728)

    def test_length_overflow_and_alignment(self):
        for length in (0xFFFFFFFF, 257, 0):
            data=image();data[28:32]=struct.pack("<I",length)
            with self.assertRaises(InvalidImage): verify(io.BytesIO(data),len(data),3145728)

    def test_bounded_reads(self):
        class Bounded(io.BytesIO):
            def read(self, count=-1):
                if not 0 <= count <= 1024: raise AssertionError("unbounded read")
                return super().read(count)
        data=image();verify(Bounded(data),len(data),3145728)

    def test_approved_capacity(self):
        self.assertEqual(slot_capacity(Path(__file__).resolve().parents[2]/"partitions.csv"),3145728)


if __name__ == "__main__":
    result = unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout), exit=False).result
    failures = len(result.errors) + len(result.failures)
    print(f"OTA_PREFLIGHT: PASS={result.testsRun-failures} FAIL={failures} assertions={assertions}")
    sys.exit(1 if failures else 0)
