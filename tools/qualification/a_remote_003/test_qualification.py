import copy
import random
import struct
import unittest
import zlib
from codec import (encode, decode, put, Reader, PROJECTION, SCALAR, RATIONAL,
                   Refused, advance_pair, U64_MAX)
from fixtures import fixture, tagged
from backend import (Fake, Interrupted, EmissionOracle, checkpoint, bootstrap,
                     recover, anchor, issue_pair_with_refusal, capacity)

SEED = 30032026
FAULT_CASES = []
CHECKS = 0


class Qualification(unittest.TestCase):
    def check(self, predicate, message=""):
        global CHECKS
        CHECKS += 1
        self.assertTrue(predicate, message)

    def refuses(self, f):
        global CHECKS
        CHECKS += 1
        with self.assertRaises(Refused):
            f()

    def test_roundtrip_and_canonical_order(self):
        for maximum in (False, True):
            for resolution in (False, True):
                x = fixture(maximum, resolution)
                wire = encode(x)
                self.check(decode(wire) == x)
                self.check(encode(dict(reversed(list(x.items())))) == wire)
                self.check(decode(wire)["effects"][0]["request"] == x["effects"][0]["request"])
                self.check(decode(wire)["conflicts"] == x["conflicts"])

    def test_scalar_exact_types(self):
        cases = [tagged("BOOL", True), tagged("UINT32", (1 << 32)-1),
                 tagged("UINT64", U64_MAX), tagged("INT64", -(1 << 63)),
                 tagged("RATIONAL", dict(n=-1, d=3)), tagged("STRING", "\x00é😀"),
                 tagged("POWER", "STANDBY"), tagged("PLAYBACK", "NOT_PLAYING"),
                 tagged("DIRECTION", "DOWN"), tagged("REFERENCE", fixture()["effects"][0]["request"]["arguments"]["favorite"])]
        for x in cases:
            self.check(Reader(put(SCALAR, x)).get(SCALAR) == x)
        for x in [tagged("BOOL", 1), tagged("UINT32", 1 << 32), tagged("UINT64", -1),
                  tagged("INT64", -(1 << 63)-1), tagged("RATIONAL", dict(n=1, d=0)),
                  tagged("STRING", "x"*257), tagged("STRING", "\ud800"), tagged("POWER", "off")]:
            self.refuses(lambda x=x: put(SCALAR, x))

    def test_profile_bounds_and_identity(self):
        x = fixture(True)
        self.refuses(lambda: encode(x, 3072))
        mutations = [lambda x: x.update(format_version=2),
                     lambda x: x.update(required_features=1),
                     lambda x: x.update(extra=1),
                     lambda x: x.pop("allocators"),
                     lambda x: x.update(registry_id="x"*129),
                     lambda x: x["bindings"].append(copy.deepcopy(x["bindings"][0])),
                     lambda x: x["catalogs"][0]["items"].append(copy.deepcopy(x["catalogs"][0]["items"][0])),
                     lambda x: x["effects"].append(copy.deepcopy(x["effects"][0])),
                     lambda x: x["effects"][0]["request"].update(recipe_capture=dict(blocked=True)),
                     lambda x: x["effects"][0]["retry_policy"].update(automatic=True),
                     lambda x: x["effects"][0]["request"]["arguments"].update(uri="secret"),
                     lambda x: x["effects"][0]["request"]["arguments"]["favorite"].update(device_id="wrong"),
                     lambda x: x["effects"][0]["affected_keys"].pop(),
                     lambda x: x["effects"][0]["proof_history"][0].update(session_generation=8),
                     lambda x: x["effects"][0]["request"]["route_plan"][0].update(mapping_revision=8),
                     lambda x: x["conflicts"][0]["participants"][0].update(conflict_ingress=99),
                     lambda x: x["conflicts"][0]["participants"][0]["resync_observation"]["value"].update(binding_id="wrong")]
        mutations += [lambda x: x["resources"].append(copy.deepcopy(x["resources"][0])),
                      lambda x: x["deleted_ids"][0].update(object_id=x["catalogs"][0]["items"][0]["item_id"]),
                      lambda x: x["associations"][0].update(current_generations=[1]),
                      lambda x: x["bindings"][0].update(association_generation=999),
                      lambda x: x["effects"][0]["proof_history"][0].update(covered_keys=[]),
                      lambda x: x["effects"][0].update(scheduling_closed=False),
                      lambda x: x["conflicts"][0]["participants"][0].update(exclusion_evidence=[copy.deepcopy(x["devices"][0]["identity_evidence"][0])])]
        for fn in mutations:
            y = copy.deepcopy(x)
            fn(y)
            self.refuses(lambda y=y: encode(y))
        y = fixture()
        y["allocators"]["request_lifetime"] = y["allocators"]["ticket_watermark"] = U64_MAX
        self.refuses(lambda: advance_pair(y))
        self.refuses(lambda: encode(dict(y, ingress_watermark=U64_MAX+1)))

    def test_every_truncation_and_corruption(self):
        wire = encode(fixture())
        for n in range(len(wire)):
            self.refuses(lambda n=n: decode(wire[:n]))
        rng = random.Random(SEED)
        for _ in range(512):
            n = rng.randrange(len(wire))
            b = bytearray(wire)
            b[n] ^= 1 << rng.randrange(8)
            self.refuses(lambda b=b: decode(b))
        self.refuses(lambda: decode(wire+b"\0"))
        self.refuses(lambda: decode(wire, len(wire)-1))

    def test_recomputed_crc_does_not_bypass_validation(self):
        x = fixture()
        x["effects"][0]["request"]["arguments"]["favorite"]["catalog_generation"] = 999
        raw = put(PROJECTION, x)
        wire = struct.pack("<4sHHII", b"D0SC", 1, 0, len(raw), zlib.crc32(raw)) + raw
        self.refuses(lambda: decode(wire))
        raw = bytearray(put(PROJECTION, fixture()))
        raw[0:2] = b"\x02\x00"
        wire = struct.pack("<4sHHII", b"D0SC", 1, 0, len(raw), zlib.crc32(raw)) + raw
        self.refuses(lambda: decode(wire))

    def test_all_protocol_boundaries_no_recovery_replay(self):
        before = fixture()
        before["effects"] = []
        before["allocators"]["request_lifetime"] = before["allocators"]["ticket_watermark"] = 0
        initial = bootstrap(before)
        after = fixture()
        probe = Fake(initial)
        checkpoint(probe, after, oracle=EmissionOracle())
        for index, label in enumerate(probe.steps):
            for mode in ("stop", "torn", "lost", "ambiguous"):
                f, oracle = Fake(initial, (index, mode)), EmissionOracle()
                with self.assertRaises(Interrupted):
                    checkpoint(f, after, oracle=oracle)
                observed = len(oracle.emissions)
                recovered = recover(f.data)
                self.check(len(oracle.emissions) == observed and recovered["scheduling"] == [])
                if observed and recovered["status"] != "GLOBAL_BLOCK":
                    self.check(recovered["projection"]["effects"] == after["effects"], label)
                if index >= 1 and index < 18 and recovered["status"] != "GLOBAL_BLOCK":
                    # Before PREPARE is visible old state is legitimate; after seal is legitimate.
                    self.check(recovered["generation"] in (1, 2))
                FAULT_CASES.append(dict(index=index, boundary=label, fault=mode,
                                        emissions=observed, recovery=recovered["status"],
                                        generation=recovered.get("generation"), result="PASS_NO_RECOVERY_REPLAY"))

    def test_capacity_refusal_and_consumed_id(self):
        x = fixture()
        fake, oracle = Fake(bootstrap(x)), EmissionOracle()
        original = copy.deepcopy(fake.data)
        self.refuses(lambda: checkpoint(fake, x, len(encode(x))-1, oracle))
        self.check(fake.data == original and oracle.emissions == [])
        y = issue_pair_with_refusal(fake, x, 1_000_000)
        self.check(y["allocators"]["request_lifetime"] == 2)
        z = recover(fake.data)["projection"]
        self.check(z["allocators"]["ticket_watermark"] == 2 and z["effects"] == x["effects"])
        self.check(oracle.emissions == [])
        self.refuses(lambda: checkpoint(Fake(bootstrap(x)), fixture(True), 3072, oracle))
        self.check(oracle.emissions == [])

    def test_missing_corrupt_and_stale_sealed_bank(self):
        x = fixture()
        d = bootstrap(x)
        for key in ("anchor", "bank0"):
            b = copy.deepcopy(d)
            b.pop(key)
            self.check(recover(b)["status"] == "GLOBAL_BLOCK")
            b = copy.deepcopy(d)
            b[key] = b[key][:-1]
            self.check(recover(b)["status"] == "GLOBAL_BLOCK")
        b = copy.deepcopy(d)
        b["anchor"] = anchor(2, 1, 0, encode(fixture(resolution=True)))
        self.check(recover(b)["status"] == "GLOBAL_BLOCK")
        b["anchor"] = anchor(2, 0, 0, d["bank0"])
        self.check(recover(b)["status"] == "GLOBAL_BLOCK")

    def test_stale_valid_anchor_counterexample_not_certified(self):
        x = fixture()
        x["effects"] = []
        x["allocators"]["request_lifetime"] = x["allocators"]["ticket_watermark"] = 0
        initial = bootstrap(x)
        f, oracle = Fake(initial), EmissionOracle()
        checkpoint(f, fixture(), oracle=oracle)
        # A valid old root cannot be distinguished without an independent trusted watermark.
        f.data["anchor"] = initial["anchor"]
        recovered = recover(f.data)
        self.check(recovered["status"] == "RECOVERED_BLOCKED")
        self.check(recovered["projection"]["allocators"]["request_lifetime"] == 0)
        self.check(recovered["projection"]["effects"] == [] and len(oracle.emissions) == 1)
        FAULT_CASES.append(dict(boundary="valid-old-anchor-after-new-emission", fault="rollback-of-anchor-only",
                                emissions=1, recovery="OLDER_VALID_PROJECTION_INDISTINGUISHABLE",
                                result="PRODUCTION_QUALIFICATION_BLOCKED"))

    def test_resolution_growth_and_no_result_recreation(self):
        initial, resolved = fixture(True), fixture(True, True)
        budget = len(encode(resolved))
        reserve = budget-len(encode(initial))
        f, oracle = Fake(bootstrap(initial)), EmissionOracle()
        checkpoint(f, initial, budget, resolution_reserve=reserve)
        checkpoint(f, resolved, budget)
        result = recover(f.data)
        self.check(result["scheduling"] == [] and oracle.emissions == [])
        self.check(result["projection"]["effects"][0]["request"] == initial["effects"][0]["request"])
        self.check(all(k["no_past"] and k["no_future"] for k in result["projection"]["effects"][0]["affected_keys"]))
        self.check(result["projection"]["conflicts"] == initial["conflicts"])
        self.check(len(encode(resolved)) > len(encode(initial)))
        self.refuses(lambda: checkpoint(Fake(bootstrap(initial)), resolved, budget-1))

    def test_repeated_cycles_and_independent_history(self):
        x = fixture()
        f, oracle = Fake(bootstrap(x)), EmissionOracle()
        for n in range(100):
            x = advance_pair(x)
            checkpoint(f, x)
            recovered = recover(f.data)
            self.check(recovered["projection"]["allocators"]["request_lifetime"] == n+2)
            self.check(recovered["projection"]["effects"][0]["request"]["request_id"] == "intent:1")
            self.check(recovered["projection"]["conflicts"] == fixture()["conflicts"])
            self.check(oracle.emissions == [] and recovered["scheduling"] == [])

    def test_explicit_backend_errors_and_readback_mismatch(self):
        initial=bootstrap(fixture())
        for operation in ("write", "commit", "read"):
            for ambiguous in (False, True):
                class Failed(Fake):
                    def write(self,key,value):
                        if operation == "write":
                            if ambiguous: super().write(key,value)
                            raise Interrupted("WRITE_ERROR/NO_SPACE")
                        return super().write(key,value)
                    def commit(self,label):
                        if operation == "commit":
                            if ambiguous: super().commit(label)
                            raise Interrupted("COMMIT_ERROR")
                        return super().commit(label)
                    def read(self,key):
                        if operation == "read":
                            if ambiguous: return b"bad-readback"
                            raise Interrupted("READ_ERROR")
                        return super().read(key)
                f,oracle=Failed(initial),EmissionOracle()
                with self.assertRaises((Interrupted,Refused)):
                    checkpoint(f,fixture(resolution=True),oracle=oracle)
                self.check(oracle.emissions == [] and recover(f.data)["scheduling"] == [])
                FAULT_CASES.append(dict(boundary=operation+"-explicit-error",fault="ambiguous" if ambiguous else "before",
                                        emissions=0,recovery=recover(f.data)["status"],result="PASS_NO_RECOVERY_REPLAY"))
        for version,features in ((0,0),(2,0),(1,1)):
            x=fixture();x["format_version"]=version;x["required_features"]=features
            if version == 0:
                self.refuses(lambda: encode(x))
                continue
            payload=put(PROJECTION,x)
            wire=struct.pack("<4sHHII",b"D0SC",1,0,len(payload),zlib.crc32(payload))+payload
            self.refuses(lambda wire=wire: decode(wire))
        x=fixture();x["effects"][0]["affected_keys"][0]["no_future"]=True
        self.refuses(lambda: encode(x))

    def test_capacity_model_and_no_live_storage(self):
        self.check(capacity(3072)["optimistic_fits"])
        self.check(not capacity(len(encode(fixture(True, True))))["optimistic_fits"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
