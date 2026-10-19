"""Standalone D0 qualification codec. Not a production journal or SDK binding."""
import copy
import math
import re
import struct
import zlib


class Refused(ValueError):
    pass


U64_MAX = (1 << 64) - 1


def uint(bits=64, minimum=0):
    return ("uint", bits, minimum)


def enum(*tokens):
    return ("enum", tokens)


def record(**fields):
    return ("record", fields)


def array(element, maximum):
    return ("array", element, maximum)


def optional(element):
    return ("optional", element)


def union(**variants):
    return ("union", variants)


BOOL = ("bool",)
ID = ("id", 128)
TEXT = ("text", 256)  # explicit profile restriction, never truncation
U64 = uint()
GEN = uint(minimum=1)
U32 = uint(32)
INT = ("int",)
RATIONAL = record(n=INT, d=GEN)
ABSENCE = enum("UNKNOWN", "UNSUPPORTED", "NOT_APPLICABLE", "UNAVAILABLE")


def value(element):
    return union(PRESENT=element, ABSENT=ABSENCE)


TIME = record(clock_epoch=ID, tick_ms=U64)
REF = record(kind=enum("FAVORITE"), device_id=ID, target_id=ID,
             device_association_generation=GEN, association_generation=GEN,
             item_id=ID, catalog_generation=GEN)
SCALAR = union(BOOL=BOOL, UINT32=U32, UINT64=U64, INT64=INT,
               RATIONAL=RATIONAL, STRING=TEXT,
               POWER=enum("ON", "OFF", "STANDBY"),
               PLAYBACK=enum("PLAYING", "PAUSED", "NOT_PLAYING", "BUFFERING"),
               DIRECTION=enum("UP", "DOWN"), REFERENCE=REF)
EVIDENCE = record(evidence_id=ID,
                  kind=enum("PROJECT_DESIGN", "OFFICIAL_CONTRACT", "PROTOCOL_TEST",
                            "OWNER_ASSOCIATION", "HARDWARE_TEST"),
                  reference=TEXT, verified_claims=array(TEXT, 4), version=value(TEXT))
GUARANTEE = enum("VERIFIED", "UNKNOWN", "NOT_APPLICABLE")
CONTRACT = record(feedback=enum("NONE", "CORRELATED_RESULT"), completion_claim=TEXT,
                  guaranteed_no_execution_errors=array(TEXT, 2), context_guard=GUARANTEE,
                  selection_guard=GUARANTEE, ordering_fence=GUARANTEE, mapping_reference=TEXT)
RETRY = record(automatic=BOOL, fallback_on_no_execution=BOOL,
               max_attempts=uint(32, 1), delay_ms=GEN)
KEY = union(TARGET=record(device_id=ID, target_id=ID), RESOURCE=record(resource_id=ID))
PRECONDITION = record(field_id=enum("media.title"), observation_id=ID,
                      expected_value=value(SCALAR))
ROUTE = record(binding_id=ID, mapping_revision=GEN, capability_revision=GEN,
               token_generation=optional(GEN), execution_contract=CONTRACT)
REQUEST = record(common_model_version=record(major=U32, minor=U32, patch=U32),
                 request_id=ID, admission_ticket=record(clock_epoch=ID, serial=GEN),
                 device_id=ID, target_id=ID, operation_id=enum("favorites.activate"),
                 arguments=record(favorite=REF), deadline=TIME, capability_revision=GEN,
                 binding_id=ID, mapping_revision=GEN, device_association_generation=GEN,
                 association_generation=GEN, catalog_generation=GEN, token_generation=GEN,
                 media_context_generation=optional(GEN), seek_window_generation=optional(GEN),
                 preconditions=array(PRECONDITION, 1), route_plan=array(ROUTE, 1),
                 recipe_capture=optional(record(blocked=BOOL)))
ERROR_CODE = enum("UNSUPPORTED_OPERATION", "INVALID_ARGUMENT", "INVALID_REFERENCE",
                  "STALE_PRECONDITION", "UNAUTHORIZED", "UNAVAILABLE", "TIMEOUT",
                  "AMBIGUOUS_DELIVERY", "DEADLINE_EXPIRED", "PARTIAL_EXECUTION",
                  "REQUEST_HISTORY_EXPIRED", "REQUEST_ID_MISMATCH", "ORDERING_BLOCKED", "CANCELLED")
CAUSE = record(code=ERROR_CODE, detail=TEXT, rejection_origin=optional(enum("CORE", "TARGET")))
ERROR = record(code=ERROR_CODE, detail=TEXT, rejection_origin=optional(enum("CORE", "TARGET")),
               cause=optional(CAUSE), step_index=optional(uint(32, 1)))
PROGRESS = record(total=uint(32, 1), emitted=U32, confirmed=U32, uncertain_remaining=BOOL)
DELIVERY = enum("NOT_SENT", "PARTIALLY_SENT", "SENT", "ACKNOWLEDGED", "AMBIGUOUS")
OUTCOME = enum("CONFIRMED", "UNCONFIRMED", "UNKNOWN", "REJECTED")
ATTEMPT = record(attempt_index=uint(32, 1), binding_id=ID, mapping_revision=GEN,
                 session_generation=GEN, dispatch_time=TIME, closed_time=optional(TIME),
                 step_index=optional(uint(32, 1)), delivery=DELIVERY, target_outcome=OUTCOME,
                 evidence=array(EVIDENCE, 3), guaranteed_no_execution=BOOL,
                 resource_ids=array(ID, 1), error=value(ERROR))
PROOF = record(evidence=EVIDENCE, signal=enum("PROGRESS", "COMPLETE", "REFUSE", "FAIL",
                                            "NO_EXECUTION", "FUTURE_FENCE"),
               covered_keys=array(KEY, 2), delivery=optional(DELIVERY),
               progress=optional(PROGRESS), closed=BOOL, failure=optional(ERROR),
               historical_request=ID, attempt_index=uint(32, 1),
               binding_id=ID, mapping_revision=GEN, session_generation=GEN)
EFFECT = record(request=REQUEST, attempt=ATTEMPT, execution_contract=CONTRACT,
                retry_policy=RETRY, semantic_variant=enum("DEVICE_NATIVE"),
                affected_keys=array(record(key=KEY, ordering=enum("STRICT", "LOCAL_ONLY"),
                                           no_past=BOOL, no_future=BOOL), 2),
                proof_history=array(PROOF, 3), progress=PROGRESS,
                complete=BOOL, refused=BOOL, uncertain_extent=BOOL,
                first_terminal_time=optional(TIME), closure_error=value(ERROR),
                invalidated=BOOL, scheduling_closed=BOOL)
HOP = record(actor_id=ID, role=enum("TARGET", "INTERMEDIARY", "ADAPTER", "CORE"), evidence_id=ID)
OBSERVATION = record(observation_id=ID, device_id=ID, target_id=ID,
                     field_id=enum("media.title"), value=value(SCALAR),
                     confidence=enum("AUTHORITATIVE", "REPORTED", "ASSUMED", "UNKNOWN"),
                     binding_id=ID, provenance=array(HOP, 4),
                     device_association_generation=GEN, association_generation=GEN,
                     mapping_revision=GEN, session_generation=GEN, receipt_time=TIME,
                     acquisition_order=GEN, origin=enum("LIVE", "CACHE_REPLAY", "RESYNC"),
                     observation_time=optional(TIME), source_time=optional(TEXT),
                     age_at_receipt_ms=optional(U64), clock_quality=enum("BOUNDED", "UNKNOWN"),
                     freshness=record(max_age_ms=GEN, acquisition=enum("VERIFIED_SEQUENCE"),
                                      source_clock_contract=value(TEXT)),
                     source_sequence=record(domain=ID, ordinal=GEN),
                     media_context_generation=optional(GEN), pending_request_id=optional(ID))
PARTICIPANT = record(binding_id=ID, conflict_ingress=GEN,
                     resync_started_ingress=value(GEN), resync_observation=value(OBSERVATION),
                     exclusion_evidence=array(EVIDENCE, 0))
CONFLICT = record(device_id=ID, target_id=ID, field_id=enum("media.title"),
                  conflict_latched=BOOL, revision=GEN, participants=array(PARTICIPANT, 2))
ASSOCIATION = record(transaction_id=ID, scope=enum("TARGET"),
                     original_ids=array(ID, 1), affected_bindings=array(ID, 2),
                     previous_generations=array(GEN, 1), current_generations=array(GEN, 1),
                     evidence=array(EVIDENCE, 1), rollback_of=optional(ID),
                     phase=enum("APPLIED", "ROLLED_BACK"))
IDENTITY = record(device_id=ID, association_generation=GEN,
                  identity_evidence=array(EVIDENCE, 1))
TARGET = record(device_id=ID, target_id=ID, association_generation=GEN,
                capability_revision=GEN, media_context_generation=GEN,
                resource_ids=array(ID, 1), ordering=enum("STRICT", "LOCAL_ONLY"))
BINDING = record(binding_id=ID, device_id=ID, target_id=ID,
                 device_association_generation=GEN, association_generation=GEN,
                 mapping_revision=GEN, session_generation=GEN, adapter_version=TEXT,
                 external_version=value(TEXT), model_versions=array(record(major=U32, minor=U32, patch=U32), 1),
                 owner_priority=U32, mapping_evidence=array(EVIDENCE, 1),
                 transport_refs=array(ID, 1), private_metadata_ref=optional(ID), credential_ref=optional(ID))
CATALOG = record(device_id=ID, target_id=ID, binding_id=ID, kind=enum("FAVORITE"),
                 catalog_generation=GEN, token_generation=GEN,
                 mapping_revision=GEN, session_generation=GEN,
                 continuity_evidence=array(EVIDENCE, 1), complete=BOOL,
                 items=array(record(item_id=ID, reference=REF, label=TEXT,
                                    variant=optional(TEXT), private_token=ID), 2))
RESOURCE = record(resource_id=ID, member_targets=array(record(device_id=ID, target_id=ID), 1),
                  ordering=enum("STRICT", "LOCAL_ONLY"), evidence=array(EVIDENCE, 1))
PROJECTION = record(format_version=uint(16, 1), required_features=U32,
                    common_model_version=record(major=U32, minor=U32, patch=U32),
                    registry_id=ID,
                    allocators=record(device=U64, target=U64, binding=U64, resource=U64,
                                      item=U64, asset=U64, request_lifetime=U64, boot=U64,
                                      epoch_base=U64, ticket_watermark=U64),
                    clock_epoch=ID, ingress_watermark=U64,
                    deleted_ids=array(record(kind=enum("DEVICE", "TARGET", "BINDING", "RESOURCE", "ITEM"),
                                             owner=ID, object_id=ID), 4),
                    devices=array(IDENTITY, 1), targets=array(TARGET, 1),
                    bindings=array(BINDING, 2), resources=array(RESOURCE, 1),
                    associations=array(ASSOCIATION, 1), catalogs=array(CATALOG, 1),
                    conflicts=array(CONFLICT, 1), effects=array(EFFECT, 1))


def put(schema, x):
    k = schema[0]
    if k == "record":
        if not isinstance(x, dict) or set(x) != set(schema[1]):
            raise Refused("missing/unknown field")
        return b"".join(put(s, x[n]) for n, s in schema[1].items())
    if k == "array":
        if not isinstance(x, list) or len(x) > schema[2]:
            raise Refused("array bound")
        return struct.pack("<H", len(x)) + b"".join(put(schema[1], y) for y in x)
    if k == "optional":
        return b"\0" if x is None else b"\1" + put(schema[1], x)
    if k == "union":
        if not isinstance(x, dict) or set(x) != {"tag", "value"} or x["tag"] not in schema[1]:
            raise Refused("union")
        return bytes([list(schema[1]).index(x["tag"])]) + put(schema[1][x["tag"]], x["value"])
    if k == "bool":
        if type(x) is not bool:
            raise Refused("bool")
        return bytes([x])
    if k in ("uint", "int"):
        low, high = (schema[2], (1 << schema[1]) - 1) if k == "uint" else (-(1 << 63), (1 << 63) - 1)
        if type(x) is not int or not low <= x <= high:
            raise Refused("integer range")
        return x.to_bytes(schema[1] // 8 if k == "uint" else 8, "little", signed=k == "int")
    if k == "enum":
        if x not in schema[1]:
            raise Refused("enum")
        return bytes([schema[1].index(x)])
    if k in ("text", "id"):
        if not isinstance(x, str):
            raise Refused("text type")
        try:
            b = x.encode("utf-8", errors="strict")
        except UnicodeError as exc:
            raise Refused("utf8") from exc
        if len(b) > schema[1] or (k == "id" and not re.fullmatch(r"[A-Za-z0-9._:-]{1,128}", x)):
            raise Refused("string bound/id")
        return struct.pack("<H", len(b)) + b
    raise Refused("unknown schema")


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def read(self, n):
        if n < 0 or n > len(self.data) - self.offset:
            raise Refused("truncated")
        b = self.data[self.offset:self.offset + n]
        self.offset += n
        return b

    def get(self, s):
        k = s[0]
        if k == "record":
            return {n: self.get(t) for n, t in s[1].items()}
        if k == "array":
            n = int.from_bytes(self.read(2), "little")
            if n > s[2]:
                raise Refused("array bound")
            return [self.get(s[1]) for _ in range(n)]
        if k in ("optional", "bool", "enum", "union"):
            n = self.read(1)[0]
            if k == "optional":
                if n > 1:
                    raise Refused("optional tag")
                return self.get(s[1]) if n else None
            if k == "bool":
                if n > 1:
                    raise Refused("bool tag")
                return bool(n)
            tokens = list(s[1])
            if n >= len(tokens):
                raise Refused("enum/union tag")
            return tokens[n] if k == "enum" else {"tag": tokens[n], "value": self.get(s[1][tokens[n]])}
        if k in ("uint", "int"):
            return int.from_bytes(self.read(s[1] // 8 if k == "uint" else 8), "little", signed=k == "int")
        if k in ("text", "id"):
            n = int.from_bytes(self.read(2), "little")
            if n > s[1]:
                raise Refused("string length")
            try:
                return self.read(n).decode("utf-8", errors="strict")
            except UnicodeError as exc:
                raise Refused("utf8") from exc
        raise Refused("unknown schema")


def walk(x):
    yield x
    if isinstance(x, dict):
        for y in x.values():
            yield from walk(y)
    elif isinstance(x, list):
        for y in x:
            yield from walk(y)


def validate(x):
    # Exact structural validation first; limits checked before encoding.
    put(PROJECTION, x)
    if x["format_version"] != 1 or x["required_features"] != 0 or x["common_model_version"] != {"major": 1, "minor": 0, "patch": 0}:
        raise Refused("migration/features/model blocked")
    for y in walk(x):
        if isinstance(y, dict) and set(y) == {"n", "d"}:
            if not y["d"] or math.gcd(abs(y["n"]), y["d"]) != 1:
                raise Refused("noncanonical Rational")
    a = x["allocators"]
    if a["epoch_base"] + a["ticket_watermark"] != a["request_lifetime"] or a["request_lifetime"] > U64_MAX:
        raise Refused("allocator pairing")
    ds = {d["device_id"]: d for d in x["devices"]}
    ts = {(t["device_id"], t["target_id"]): t for t in x["targets"]}
    bs = {b["binding_id"]: b for b in x["bindings"]}
    rs = {r["resource_id"] for r in x["resources"]}
    if len(ds) != len(x["devices"]) or len(bs) != len(x["bindings"]) or len(ts) != len(x["targets"]):
        raise Refused("duplicate identity")
    if len(rs) != len(x["resources"]):
        raise Refused("duplicate resource")
    deleted = [(d["kind"],d["owner"],d["object_id"]) for d in x["deleted_ids"]]
    if len(set(deleted)) != len(deleted):
        raise Refused("duplicate deleted identity")
    live = set(ds) | set(bs) | rs | {t["target_id"] for t in ts.values()} | {i["item_id"] for c in x["catalogs"] for i in c["items"]}
    if any(d["object_id"] in live for d in x["deleted_ids"]):
        raise Refused("deleted identity reused in narrow profile")
    for b in x["bindings"]:
        if (b["device_id"], b["target_id"]) not in ts:
            raise Refused("binding ownership")
        if b["device_association_generation"] != ds[b["device_id"]]["association_generation"] or b["association_generation"] != ts[(b["device_id"],b["target_id"])]["association_generation"]:
            raise Refused("current binding generations")
    for t in x["targets"]:
        if t["device_id"] not in ds or not set(t["resource_ids"]) <= rs:
            raise Refused("target ownership")
    for c in x["catalogs"]:
        if c["binding_id"] not in bs or (c["device_id"], c["target_id"]) not in ts:
            raise Refused("catalog scope")
        ids = [i["item_id"] for i in c["items"]]
        if len(set(ids)) != len(ids):
            raise Refused("duplicate item")
        for i in c["items"]:
            if i["reference"]["item_id"] != i["item_id"] or i["reference"]["catalog_generation"] != c["catalog_generation"]:
                raise Refused("catalog reference")
    for conflict in x["conflicts"]:
        ids = [p["binding_id"] for p in conflict["participants"]]
        if len(ids) != len(set(ids)) or not set(ids) <= set(bs):
            raise Refused("conflict participant identity")
        if conflict["conflict_latched"] != bool(ids):
            raise Refused("conflict roster")
        if (conflict["device_id"], conflict["target_id"]) not in ts:
            raise Refused("conflict scope")
        for participant in conflict["participants"]:
            if participant["conflict_ingress"] > x["ingress_watermark"]:
                raise Refused("conflict watermark")
            obs = participant["resync_observation"]
            if obs["tag"] == "PRESENT":
                o = obs["value"]
                if o["binding_id"] != participant["binding_id"] or o["device_id"] != conflict["device_id"] or o["target_id"] != conflict["target_id"]:
                    raise Refused("resync scope")
                if o["value"]["tag"] == "PRESENT" and o["value"]["value"]["tag"] != "STRING":
                    raise Refused("title type")
                start = participant["resync_started_ingress"]
                if start["tag"] != "PRESENT" or not participant["conflict_ingress"] < start["value"] <= x["ingress_watermark"]:
                    raise Refused("resync watermark")
    for a_record in x["associations"]:
        if a_record["rollback_of"] is not None or a_record["phase"] != "ROLLED_BACK":
            raise Refused("association profile blocked")
        if len(ts) != 1 or a_record["original_ids"] != [next(iter(ts.values()))["target_id"]] or set(a_record["affected_bindings"]) != set(bs):
            raise Refused("ownership-changing association blocked")
        if len(a_record["previous_generations"]) != 1 or len(a_record["current_generations"]) != 1 or not a_record["previous_generations"][0] < a_record["current_generations"][0] == next(iter(ts.values()))["association_generation"]:
            raise Refused("association generation history")
    for ef in x["effects"]:
        q, at = ef["request"], ef["attempt"]
        if not ef["scheduling_closed"] or ef["complete"] or ef["refused"]:
            raise Refused("closed uncertain-history profile only")
        if ef["uncertain_extent"]:
            if len(ef["proof_history"]) != 1 or len(at["evidence"]) != 1:
                raise Refused("reserve two later proofs before handover")
        else:
            if len(ef["proof_history"]) != 3 or len(at["evidence"]) != 3:
                raise Refused("full two-proof resolution required")
        if any(y.get("step_index") is not None for y in walk(ef) if isinstance(y, dict)):
            raise Refused("recipe error step blocked")
        if q["recipe_capture"] is not None or q["seek_window_generation"] is not None or at["step_index"] is not None:
            raise Refused("recipe/seek profile blocked")
        if not q["route_plan"] or q["binding_id"] != q["route_plan"][0]["binding_id"] or q["binding_id"] != at["binding_id"]:
            raise Refused("route correlation")
        if q["common_model_version"] != x["common_model_version"] or q["request_id"] != "intent:" + str(q["admission_ticket"]["serial"] + a["epoch_base"]):
            raise Refused("request allocation/version")
        if q["admission_ticket"]["serial"] > a["ticket_watermark"]:
            raise Refused("unallocated pair")
        if ef["retry_policy"] != {"automatic": False, "fallback_on_no_execution": False, "max_attempts": 1, "delay_ms": 1}:
            raise Refused("retry profile blocked")
        if at["attempt_index"] != 1 or q["mapping_revision"] != at["mapping_revision"]:
            raise Refused("attempt correlation")
        if q["device_id"] not in ds or (q["device_id"], q["target_id"]) not in ts or q["binding_id"] not in bs:
            raise Refused("effect ownership")
        if ef["execution_contract"] != q["route_plan"][0]["execution_contract"]:
            raise Refused("historical contract mismatch")
        if q["deadline"]["clock_epoch"] != q["admission_ticket"]["clock_epoch"] or not at["dispatch_time"]["tick_ms"] < q["deadline"]["tick_ms"]:
            raise Refused("historical time")
        if q["route_plan"][0]["mapping_revision"] != q["mapping_revision"] or q["route_plan"][0]["capability_revision"] != q["capability_revision"]:
            raise Refused("route capture revisions")
        for p in q["preconditions"]:
            if p["expected_value"]["tag"] == "PRESENT" and p["expected_value"]["value"]["tag"] != "STRING":
                raise Refused("precondition title type")
        f = q["arguments"]["favorite"]
        if f["device_id"] != q["device_id"] or f["target_id"] != q["target_id"] or f["catalog_generation"] != q["catalog_generation"]:
            raise Refused("captured reference scope")
        expected = [{"tag": "TARGET", "value": {"device_id": q["device_id"], "target_id": q["target_id"]}}]
        expected += [{"tag": "RESOURCE", "value": {"resource_id": v}} for v in at["resource_ids"]]
        if [k["key"] for k in ef["affected_keys"]] != expected:
            raise Refused("missing/extra effect key")
        if not set(at["resource_ids"]) <= rs:
            raise Refused("historical resource unknown")
        if any(p["historical_request"] != q["request_id"] or p["binding_id"] != at["binding_id"] or p["mapping_revision"] != at["mapping_revision"] or p["session_generation"] != at["session_generation"] or p["attempt_index"] != at["attempt_index"] for p in ef["proof_history"]):
            raise Refused("proof correlation")
        if len({p["evidence"]["evidence_id"] for p in ef["proof_history"]}) != len(ef["proof_history"]):
            raise Refused("duplicate proof")
        for p in ef["proof_history"]:
            if p["covered_keys"] != expected:
                raise Refused("proof key coverage")
        for conflict in x["conflicts"]:
            for participant in conflict["participants"]:
                o = participant["resync_observation"]
                if o["tag"] == "PRESENT" and o["value"]["pending_request_id"] != q["request_id"]:
                    raise Refused("historical pending correlation")
        for k in ef["affected_keys"]:
            past = any(p["signal"] == "NO_EXECUTION" and k["key"] in p["covered_keys"] for p in ef["proof_history"])
            future = any(p["signal"] == "FUTURE_FENCE" and k["key"] in p["covered_keys"] for p in ef["proof_history"])
            if k["no_past"] != past or k["no_future"] != future:
                raise Refused("unproved per-key derived state")
        if at["guaranteed_no_execution"] != all(k["no_past"] and k["no_future"] for k in ef["affected_keys"]):
            raise Refused("whole-attempt proof mismatch")
        for pr in (ef["progress"],):
            if pr["total"] != 1 or pr["emitted"] > 1 or pr["confirmed"] > pr["emitted"]:
                raise Refused("progress")
    return x


def encode(x, budget=1_000_000):
    validate(x)
    payload = put(PROJECTION, x)
    wire = struct.pack("<4sHHII", b"D0SC", 1, 0, len(payload), zlib.crc32(payload)) + payload
    if len(wire) > budget:
        raise Refused("checkpoint capacity before handover")
    return wire


def decode(wire, budget=1_000_000):
    if len(wire) < 16 or len(wire) > budget:
        raise Refused("frame length")
    magic, version, features, n, crc = struct.unpack("<4sHHII", wire[:16])
    if magic != b"D0SC" or version != 1 or features or n != len(wire) - 16 or zlib.crc32(wire[16:]) != crc:
        raise Refused("frame/schema/checksum")
    reader = Reader(wire[16:])
    x = reader.get(PROJECTION)
    if reader.offset != len(reader.data) or encode(x) != wire:
        raise Refused("noncanonical")
    return x


def advance_pair(x):
    y = copy.deepcopy(x)
    a = y["allocators"]
    if a["request_lifetime"] == U64_MAX or a["ticket_watermark"] == U64_MAX:
        raise Refused("allocator exhausted")
    a["request_lifetime"] += 1
    a["ticket_watermark"] += 1
    return y
