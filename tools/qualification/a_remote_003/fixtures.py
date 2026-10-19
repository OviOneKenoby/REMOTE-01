"""Synthetic closed-schema fixtures; no target credentials or external proof."""
import copy
from codec import U64_MAX


def tagged(tag, v):
    return {"tag": tag, "value": v}


def absent():
    return tagged("ABSENT", "UNKNOWN")


def fixture(maximal=False, resolution=False):
    ids = {k: (k + ":" + "x" * (127 - len(k)) if maximal else k + ":1")
           for k in ("registry", "device", "target", "bindingA", "bindingB", "resource",
                     "itemA", "itemB", "epoch", "sequence", "obs", "private", "credential",
                     "transaction", "retiredA", "retiredB", "retiredC", "retiredD")}
    def text(label):
        return label + "x" * (256 - len(label)) if maximal else label

    def evidence(label):
        return dict(evidence_id=(label + "x" * (128 - len(label)) if maximal else label),
                    kind="PROJECT_DESIGN", reference=text("D0 synthetic fixture, not external proof"),
                    verified_claims=[text("claim" + str(i)) for i in range(4 if maximal else 1)],
                    version=tagged("PRESENT", text("fixture1")))
    def time(t):
        return dict(clock_epoch=ids["epoch"], tick_ms=t)
    def ref(item):
        return dict(kind="FAVORITE", device_id=ids["device"], target_id=ids["target"],
                    device_association_generation=1, association_generation=1,
                    item_id=ids[item], catalog_generation=1)
    contract = dict(feedback="CORRELATED_RESULT", completion_claim=text("whole-activation"),
                    guaranteed_no_execution_errors=[text("refusal" + str(i)) for i in range(2 if maximal else 1)],
                    context_guard="VERIFIED", selection_guard="VERIFIED", ordering_fence="VERIFIED",
                    mapping_reference=text("qualification-model-not-native"))
    q = dict(common_model_version=dict(major=1, minor=0, patch=0), request_id="intent:1",
             admission_ticket=dict(clock_epoch=ids["epoch"], serial=1),
             device_id=ids["device"], target_id=ids["target"], operation_id="favorites.activate",
             arguments=dict(favorite=ref("itemA")), deadline=time(60000), capability_revision=1,
             binding_id=ids["bindingA"], mapping_revision=1, device_association_generation=1,
             association_generation=1, catalog_generation=1, token_generation=1,
             media_context_generation=1, seek_window_generation=None,
             preconditions=[dict(field_id="media.title", observation_id=ids["obs"],
                                 expected_value=tagged("PRESENT", tagged("STRING", text("before conflict"))))],
             route_plan=[dict(binding_id=ids["bindingA"], mapping_revision=1, capability_revision=1,
                              token_generation=1, execution_contract=copy.deepcopy(contract))],
             recipe_capture=None)
    keys = [tagged("TARGET", dict(device_id=ids["device"], target_id=ids["target"])),
            tagged("RESOURCE", dict(resource_id=ids["resource"]))]
    progress = dict(total=1, emitted=0, confirmed=0, uncertain_remaining=True)
    error = dict(code="TIMEOUT", detail=text("unknown emission/execution"), rejection_origin="CORE",
                 cause=dict(code="UNAVAILABLE", detail=text("offline after handover"), rejection_origin="CORE") if maximal else None,
                 step_index=None)
    attempt = dict(attempt_index=1, binding_id=ids["bindingA"], mapping_revision=1,
                   session_generation=1, dispatch_time=time(20), closed_time=time(60000),
                   step_index=None, delivery="AMBIGUOUS", target_outcome="UNKNOWN",
                   evidence=[evidence("proof0")], guaranteed_no_execution=False,
                   resource_ids=[ids["resource"]], error=tagged("PRESENT", copy.deepcopy(error)))
    def proof(i, signal):
        return dict(evidence=evidence("proof" + str(i)), signal=signal, covered_keys=copy.deepcopy(keys),
                    delivery="AMBIGUOUS", progress=copy.deepcopy(progress), closed=True,
                    failure=copy.deepcopy(error) if maximal else None, historical_request="intent:1",
                    attempt_index=1, binding_id=ids["bindingA"], mapping_revision=1, session_generation=1)
    effect = dict(request=q, attempt=attempt, execution_contract=copy.deepcopy(contract),
                  retry_policy=dict(automatic=False, fallback_on_no_execution=False, max_attempts=1, delay_ms=1),
                  semantic_variant="DEVICE_NATIVE", affected_keys=[dict(key=k, ordering="STRICT", no_past=False, no_future=False) for k in keys],
                  proof_history=[proof(0, "PROGRESS")], progress=copy.deepcopy(progress),
                  complete=False, refused=False, uncertain_extent=True, first_terminal_time=time(60000),
                  closure_error=tagged("PRESENT", copy.deepcopy(error)), invalidated=False, scheduling_closed=True)
    bindings = []
    participants = []
    for n, name in enumerate(("bindingA", "bindingB")):
        bindings.append(dict(binding_id=ids[name], device_id=ids["device"], target_id=ids["target"],
                             device_association_generation=1, association_generation=1, mapping_revision=1,
                             session_generation=1, adapter_version=text("fixture-adapter"),
                             external_version=tagged("PRESENT", text("fixture-target")),
                             model_versions=[dict(major=1, minor=0, patch=0)], owner_priority=n,
                             mapping_evidence=[evidence("map" + str(n))], transport_refs=[ids["private"]],
                             private_metadata_ref=ids["private"], credential_ref=ids["credential"]))
        obs = dict(observation_id=ids["obs"], device_id=ids["device"], target_id=ids["target"],
                   field_id="media.title", value=tagged("PRESENT", tagged("STRING", text("conflicting-" + str(n)))),
                   confidence="REPORTED", binding_id=ids[name],
                   provenance=[dict(actor_id=ids["device"], role="TARGET", evidence_id=evidence("hop" + str(i))["evidence_id"])
                               for i in range(4 if maximal else 1)],
                   device_association_generation=1, association_generation=1, mapping_revision=1,
                   session_generation=1, receipt_time=time(61000), acquisition_order=5,
                   origin="RESYNC", observation_time=time(61000), source_time=text("diagnostic-time"),
                   age_at_receipt_ms=0, clock_quality="BOUNDED",
                   freshness=dict(max_age_ms=1000, acquisition="VERIFIED_SEQUENCE",
                                  source_clock_contract=tagged("PRESENT", text("bounded fixture age"))),
                   source_sequence=dict(domain=ids["sequence"], ordinal=5), media_context_generation=1,
                   pending_request_id="intent:1")
        # Distinct resync values mean the conflict remains latched; no false agreement.
        participants.append(dict(binding_id=ids[name], conflict_ingress=4,
                                 resync_started_ingress=tagged("PRESENT", 5),
                                 resync_observation=tagged("PRESENT", obs),
                                 exclusion_evidence=[]))
    x = dict(format_version=1, required_features=0, common_model_version=dict(major=1, minor=0, patch=0),
             registry_id=ids["registry"], allocators=dict(device=1, target=1, binding=2, resource=1,
                                                        item=2, asset=0, request_lifetime=1, boot=1,
                                                        epoch_base=0, ticket_watermark=1),
             clock_epoch=ids["epoch"], ingress_watermark=6,
             deleted_ids=[dict(kind="ITEM", owner=ids["target"], object_id=ids[k]) for k in
                          (("retiredA", "retiredB", "retiredC", "retiredD") if maximal else ("retiredA",))],
             devices=[dict(device_id=ids["device"], association_generation=1, identity_evidence=[evidence("identity")])],
             targets=[dict(device_id=ids["device"], target_id=ids["target"], association_generation=1,
                           capability_revision=1, media_context_generation=1, resource_ids=[ids["resource"]], ordering="STRICT")],
             bindings=bindings, resources=[dict(resource_id=ids["resource"], member_targets=[dict(device_id=ids["device"], target_id=ids["target"])],
                                               ordering="STRICT", evidence=[evidence("shared")])],
             associations=[dict(transaction_id=ids["transaction"], scope="TARGET", original_ids=[ids["target"]],
                                affected_bindings=[ids["bindingA"], ids["bindingB"]], previous_generations=[1], current_generations=[2],
                                evidence=[evidence("association")], rollback_of=None, phase="ROLLED_BACK")] if maximal else [],
             catalogs=[dict(device_id=ids["device"], target_id=ids["target"], binding_id=ids["bindingA"],
                            kind="FAVORITE", catalog_generation=1, token_generation=1, mapping_revision=1,
                            session_generation=1, continuity_evidence=[evidence("catalog")], complete=True,
                            items=[dict(item_id=ids[k], reference=ref(k), label=text("Favorite " + k),
                                        variant=text("DEVICE_NATIVE"), private_token=ids[k]) for k in ("itemA", "itemB")])],
             conflicts=[dict(device_id=ids["device"], target_id=ids["target"], field_id="media.title",
                             conflict_latched=True, revision=1, participants=participants)], effects=[effect])
    if maximal:
        # Historic association is recorded after the attempt, so its current generations
        # legitimately differ from immutable attempt captures.
        x["targets"][0]["association_generation"] = 2
        for b in x["bindings"]:
            b["association_generation"] = 2
        for i in x["catalogs"][0]["items"]:
            i["reference"]["association_generation"] = 2
        for p in x["conflicts"][0]["participants"]:
            p["resync_observation"]["value"]["association_generation"] = 2
    if resolution:
        for i, signal in ((1, "FUTURE_FENCE"), (2, "NO_EXECUTION")):
            p = proof(i, signal)
            effect["proof_history"].append(p)
            effect["attempt"]["evidence"].append(copy.deepcopy(p["evidence"]))
        for k in effect["affected_keys"]:
            k["no_past"] = k["no_future"] = True
        effect["attempt"]["guaranteed_no_execution"] = True
        effect["attempt"]["target_outcome"] = "UNCONFIRMED"
        effect["progress"]["uncertain_remaining"] = False
        effect["uncertain_extent"] = False
    if maximal:
        # Longest generated request namespace ID: intent: + 20 decimal digits.
        serial = U64_MAX - 1
        x["allocators"]["request_lifetime"] = x["allocators"]["ticket_watermark"] = serial
        q["admission_ticket"]["serial"] = serial
        q["request_id"] = "intent:" + str(serial)
        for p in effect["proof_history"]:
            p["historical_request"] = q["request_id"]
        for p in participants:
            p["resync_observation"]["value"]["pending_request_id"] = q["request_id"]
    return x
