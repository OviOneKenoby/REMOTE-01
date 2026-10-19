"""Abstract fake storage only. No ESP-IDF, NVS, filesystem or real dispatch."""
import copy
import hashlib
import json
import struct
import zlib
from codec import Refused, encode, decode, advance_pair, U64_MAX


class Interrupted(RuntimeError):
    pass


class EmissionOracle:
    # Independent of codec/recovered state; only simulated handover calls this.
    def __init__(self):
        self.emissions = []

    def emit(self, request):
        self.emissions.append(request)


def anchor(generation, phase, bank, wire):
    p = struct.pack("<QBB", generation, phase, bank) + hashlib.sha256(wire).digest()
    return p + struct.pack("<I", zlib.crc32(p))


def read_anchor(b):
    if not b or len(b) != 46 or zlib.crc32(b[:-4]) != struct.unpack("<I", b[-4:])[0]:
        raise Refused("anchor missing/corrupt")
    g, phase, bank = struct.unpack("<QBB", b[:10])
    if not g or phase not in (0, 1) or bank not in (0, 1):
        raise Refused("anchor invalid")
    return g, phase, bank, b[10:42]


class Fake:
    # fault (index, mode), index numbers each concrete before/after/readback boundary.
    # Optional disappearance/torn writes model uncertainty, not claims about hardware.
    def __init__(self, initial=None, fault=None):
        self.data = copy.deepcopy(initial or {})
        self.fault = fault
        self.steps = []

    def point(self, label, key=None, value=None):
        n = len(self.steps)
        self.steps.append(label)
        if self.fault and self.fault[0] == n:
            mode = self.fault[1]
            if key and mode == "torn":
                self.data[key] = value[:len(value)//2]
            if key and mode == "lost":
                self.data.pop(key, None)
            # before, after and ambiguous-success differ by caller side-effect position.
            raise Interrupted(label + ":" + mode)

    def write(self, key, value):
        self.point("before-write-" + key, key, value)
        self.data[key] = bytes(value)
        self.point("after-write-" + key, key, value)

    def commit(self, label):
        self.point("before-commit-" + label)
        # SDK5.5.3 writes in setters; fake commit deliberately adds no durability.
        self.point("after-commit-" + label)

    def read(self, key):
        self.point("before-read-" + key)
        value = self.data.get(key)
        self.point("after-read-" + key)
        return value


def recover(data, budget=1_000_000):
    """No oracle/adapter argument: impossible to emit through this function."""
    try:
        g, phase, bank, digest = read_anchor(data.get("anchor"))
        wire = data.get("bank" + str(bank), b"")
        if phase != 1 or hashlib.sha256(wire).digest() != digest:
            raise Refused("new intent unsealed/stale bank")
        x = decode(wire, budget)
        return {"status": "RECOVERED_BLOCKED", "generation": g, "projection": x,
                "scheduling": [], "current_observations": [], "authorization": "UNKNOWN"}
    except Refused as exc:
        return {"status": "GLOBAL_BLOCK", "reason": str(exc), "scheduling": []}


def bootstrap(x, budget=1_000_000):
    # Synthetic provisioned initial state in fake memory; not a live fresh-NVS policy.
    wire = encode(x, budget)
    return {"bank0": wire, "anchor": anchor(1, 1, 0, wire)}


def checkpoint(fake, x, budget=1_000_000, oracle=None, resolution_reserve=0):
    wire = encode(x, budget - resolution_reserve)
    old = recover(fake.data, budget)
    if old["status"] == "GLOBAL_BLOCK" or old["generation"] == U64_MAX:
        raise Refused("prior history/generation unavailable")
    g = old["generation"] + 1
    old_bank = read_anchor(fake.data["anchor"])[2]
    bank = 1 - old_bank
    fake.write("anchor", anchor(g, 0, bank, wire))
    fake.commit("prepare")
    if fake.read("anchor") != anchor(g, 0, bank, wire):
        raise Refused("prepare readback")
    fake.write("bank" + str(bank), wire)
    fake.commit("bank")
    if fake.read("bank" + str(bank)) != wire:
        raise Refused("bank readback")
    fake.write("anchor", anchor(g, 1, bank, wire))
    fake.commit("seal")
    if fake.read("anchor") != anchor(g, 1, bank, wire):
        raise Refused("seal readback")
    # This is only an independent simulated executable handover oracle.
    if oracle is not None:
        fake.point("before-simulated-handover")
        oracle.emit(x["effects"][0]["request"]["request_id"])
        fake.point("after-simulated-handover")
    return g


def nvs_entries(blob_bytes, tailroom=4000):
    # Optimistic no fragmentation: each chunk data ceil(n/32)+one header, one index.
    chunks = (blob_bytes + tailroom - 1) // tailroom
    return (blob_bytes + 31) // 32 + chunks + 1


def capacity(bank, credentials=512, metadata=12):
    # Synthetic credential size, not read from real NVS. Old inactive/new version overlap.
    entries = (3 * nvs_entries(bank) + 2 * nvs_entries(46) +
               2 * nvs_entries(credentials) + metadata + 126)
    return {"bank_bytes": bank, "credentials_assumed_bytes": credentials,
            "blob_entries": nvs_entries(bank), "peak_entries": entries,
            "partition_entries": 6 * 126, "peak_pages_lower_bound": (entries + 125)//126,
            "optimistic_fits": entries <= 6 * 126,
            "model": "3 bank generations +2 anchors +old/new credentials +metadata +GC page"}


def issue_pair_with_refusal(fake, x, budget):
    y = advance_pair(x)
    checkpoint(fake, y, budget)
    return y  # capacity refusal consumed pair, never a simulated emission
