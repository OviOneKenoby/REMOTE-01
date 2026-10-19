"""Host-only exact typed projection with lossless schema-driven interning."""
import copy
import math
from pathlib import Path
import struct
import sys
import zlib

sys.path.insert(0, str(Path(__file__).parent / 'vendor_a003'))
import codec as old
from fixtures import fixture as old_fixture

Refused = old.Refused
U64_MAX = old.U64_MAX
SCHEMA = old.PROJECTION


def strings(s, x, path=''):
    k = s[0]
    if k in ('id', 'text'):
        yield k, x, path
    elif k == 'record':
        for n, t in s[1].items():
            yield from strings(t, x[n], path + '.' + n)
    elif k == 'array':
        for i, y in enumerate(x):
            yield from strings(s[1], y, path + '[' + str(i) + ']')
    elif k == 'optional' and x is not None:
        yield from strings(s[1], x, path + '?')
    elif k == 'union':
        yield from strings(s[1][x['tag']], x['value'], path + '<' + x['tag'] + '>')


def dictionaries(s, x):
    return {k: sorted({v.encode('utf-8') for t, v, _ in strings(s, x) if t == k})
            for k in ('id', 'text')}


def validate(x):
    old.put(SCHEMA,x)
    original = copy.deepcopy(x)
    if original['effects']:
        request_id = original['effects'][0]['request']['request_id']
        for conflict in original['conflicts']:
            for participant in conflict['participants']:
                o = participant['resync_observation']
                if o['tag'] == 'PRESENT' and o['value']['pending_request_id'] is None:
                    o['value']['pending_request_id'] = request_id
    # A004 adds the bounded intermediate one-proof state, without changing the
    # immutable A003 vendor or removing anything from the encoded projection.
    if original['effects'] and len(original['effects'][0]['proof_history']) == 2:
        ef = original['effects'][0]
        ef['proof_history'] = ef['proof_history'][:1]
        ef['attempt']['evidence'] = ef['attempt']['evidence'][:1]
        for k in ef['affected_keys']:
            k['no_past'] = k['no_future'] = False
        ef['attempt']['guaranteed_no_execution'] = False
        ef['uncertain_extent'] = True
    old.validate(original)
    if x['associations']:
        raise Refused('association operations outside profile: preserve existing history elsewhere/block')
    for y in old.walk(x):
        if isinstance(y, dict):
            if 'verified_claims' in y and len(y['verified_claims']) > 1:
                raise Refused('evidence claims profile')
            if 'guaranteed_no_execution_errors' in y and len(y['guaranteed_no_execution_errors']) > 1:
                raise Refused('contract error-list profile')
    if any(k == 'text' and len(v.encode('utf-8')) > 64 for k, v, _ in strings(SCHEMA, x)):
        raise Refused('text exceeds declared 64-byte profile, never truncated')
    d = dictionaries(SCHEMA, x)
    added = len(x['effects'][0]['proof_history'])-1 if x['effects'] else 0
    limits = (30+added, 10+3*added)
    if len(d['id']) > limits[0] or len(d['text']) > limits[1]:
        raise Refused('dictionary growth reserve unavailable')
    if x['effects']:
        ef = x['effects'][0]
        at = ef['attempt']
        q = ef['request']
        keys = [k['key'] for k in ef['affected_keys']]
        signals = [p['signal'] for p in ef['proof_history'][1:]]
        if len(signals) != len(set(signals)) or not set(signals) <= {'FUTURE_FENCE', 'NO_EXECUTION'}:
            raise Refused('resolution signal profile')
        if ef['proof_history'][0]['signal'] != 'PROGRESS':
            raise Refused('initial history signal')
        if at['evidence'] != [p['evidence'] for p in ef['proof_history']]:
            raise Refused('full evidence duplication mismatch')
        if len({p['evidence']['evidence_id'] for p in ef['proof_history']}) != len(ef['proof_history']):
            raise Refused('duplicate evidence identity')
        for proof in ef['proof_history'][1:]:
            if (proof['historical_request'],proof['binding_id'],proof['attempt_index'],proof['mapping_revision'],proof['session_generation']) != (q['request_id'],at['binding_id'],at['attempt_index'],at['mapping_revision'],at['session_generation']):
                raise Refused('proof historical correlation')
            if proof['covered_keys'] != keys:
                raise Refused('partial-key coverage outside declared profile')
            if proof['failure'] != ef['proof_history'][0]['failure']:
                raise Refused('later proof changed historical error')
            if proof['progress'] != ef['proof_history'][0]['progress']:
                raise Refused('later proof changed historical emitted/progress bounds')
            # New proof may add only its own evidence identity and its bounded
            # reference/claim/version. All historical correlation remains exact.
        for k in ef['affected_keys']:
            if k['no_past'] != ('NO_EXECUTION' in signals) or k['no_future'] != ('FUTURE_FENCE' in signals):
                raise Refused('independent per-key proof state')
        both = len(signals) == 2
        if at['guaranteed_no_execution'] != both or ef['uncertain_extent'] == both:
            raise Refused('derived whole-attempt extent')
    return x


def append_proof(x, proof):
    y = copy.deepcopy(x)
    ef = y['effects'][0]
    ef['proof_history'].append(copy.deepcopy(proof))
    ef['attempt']['evidence'].append(copy.deepcopy(proof['evidence']))
    signals = {p['signal'] for p in ef['proof_history'][1:]}
    for k in ef['affected_keys']:
        k['no_past'] = 'NO_EXECUTION' in signals
        k['no_future'] = 'FUTURE_FENCE' in signals
    both = signals == {'NO_EXECUTION','FUTURE_FENCE'}
    ef['attempt']['guaranteed_no_execution'] = both
    ef['uncertain_extent'] = not both
    if both:
        ef['progress']['uncertain_remaining'] = False
        ef['attempt']['target_outcome'] = 'UNCONFIRMED'
    return validate(y)


def body_put(s, x, d):
    k = s[0]
    if k in ('id', 'text'):
        return bytes([d[k].index(x.encode('utf-8'))])
    if k == 'record':
        return b''.join(body_put(t, x[n], d) for n, t in s[1].items())
    if k == 'array':
        return struct.pack('<H', len(x)) + b''.join(body_put(s[1], y, d) for y in x)
    if k == 'optional':
        return b'\0' if x is None else b'\1' + body_put(s[1], x, d)
    if k == 'union':
        return bytes([list(s[1]).index(x['tag'])]) + body_put(s[1][x['tag']], x['value'], d)
    return old.put(s, x)


class InternReader(old.Reader):
    def __init__(self, data, d):
        super().__init__(data)
        self.d = d

    def get(self, s):
        if s[0] in ('id', 'text'):
            i = self.read(1)[0]
            if i >= len(self.d[s[0]]):
                raise Refused('invalid intern reference')
            return self.d[s[0]][i].decode('utf-8', errors='strict')
        return super().get(s)


def pack(s, x, magic=b'C4SC'):
    old.put(s, x)  # closed keys/types and structural bounds, before interning
    d = dictionaries(s, x)
    if len(d['id']) > 32 or len(d['text']) > 16:
        raise Refused('dictionary bound')
    table = b''
    for k in ('id', 'text'):
        table += bytes([len(d[k])])
        for v in d[k]:
            if len(v) > (128 if k == 'id' else 64):
                raise Refused('dictionary byte limit')
            table += bytes([len(v)]) + v
    payload = table + body_put(s, x, d)
    return struct.pack('<4sHHII', magic, 1, 0, len(payload), zlib.crc32(payload)) + payload


def unpack(s, wire, magic=b'C4SC'):
    if len(wire) < 16 or len(wire) > 10000:
        raise Refused('bounded frame length')
    m, v, f, n, crc = struct.unpack('<4sHHII', wire[:16])
    if (m, v, f) != (magic, 1, 0) or n != len(wire)-16 or zlib.crc32(wire[16:]) != crc:
        raise Refused('format/features/migration/CRC')
    r = old.Reader(wire[16:])
    d = {}
    try:
        for k, count, bound in [('id', 32, 128), ('text', 16, 64)]:
            size = r.read(1)[0]
            if size > count:
                raise Refused('dictionary count')
            values = []
            for _ in range(size):
                length = r.read(1)[0]
                if length > bound:
                    raise Refused('dictionary string length')
                raw = r.read(length)
                old.put(old.ID if k == 'id' else old.TEXT, raw.decode('utf-8', errors='strict'))
                values.append(raw)
            if values != sorted(set(values)):
                raise Refused('noncanonical dictionary')
            d[k] = values
        reader = InternReader(r.data[r.offset:], d)
        x = reader.get(s)
        if reader.offset != len(reader.data) or pack(s, x, magic) != wire:
            raise Refused('noncanonical or unused dictionary entry')
        return x
    except UnicodeError as exc:
        raise Refused('UTF8') from exc


def encode(x):
    validate(x)
    return pack(SCHEMA, x)


def decode(wire):
    return validate(unpack(SCHEMA, wire))


def fixture(maximum=False, resolution=False):
    x = old_fixture(maximum, False)
    x['associations'] = []
    # Undo A003 association in the newly constructed synthetic fixture only.
    x['targets'][0]['association_generation'] = 1
    for b in x['bindings']:
        b['association_generation'] = 1
    for i in x['catalogs'][0]['items']:
        i['reference']['association_generation'] = 1
    for p in x['conflicts'][0]['participants']:
        p['resync_observation']['value']['association_generation'] = 1
    for y in old.walk(x):
        if isinstance(y, dict):
            if 'verified_claims' in y:
                y['verified_claims'] = y['verified_claims'][:1]
            if 'guaranteed_no_execution_errors' in y:
                y['guaranteed_no_execution_errors'] = y['guaranteed_no_execution_errors'][:1]
    # Legal equality-reused strings, not compression of a discarded input.
    pool = ['sample%02d' % i for i in range(16)]
    if maximum:
        pool = [v + 'x'*(64-len(v)) for v in pool]
    pending_text = 10
    count = [0]
    def change(s, y):
        k = s[0]
        if k == 'text':
            i = count[0] % pending_text
            count[0] += 1
            return pool[i]
        if k == 'record':
            return {n: change(t, y[n]) for n, t in s[1].items()}
        if k == 'array':
            return [change(s[1], v) for v in y]
        if k == 'optional':
            return None if y is None else change(s[1], y)
        if k == 'union':
            return {'tag': y['tag'], 'value': change(s[1][y['tag']], y['value'])}
        return y
    x = change(SCHEMA, x)
    ef = x['effects'][0]
    ef['execution_contract'] = copy.deepcopy(ef['request']['route_plan'][0]['execution_contract'])
    ef['closure_error'] = copy.deepcopy(ef['attempt']['error'])
    ef['proof_history'][0]['failure'] = (copy.deepcopy(ef['attempt']['error']['value'])
                                       if maximum else None)
    ef['attempt']['evidence'][0] = copy.deepcopy(ef['proof_history'][0]['evidence'])
    if resolution:
        ef = x['effects'][0]
        for i, signal in enumerate(('FUTURE_FENCE','NO_EXECUTION')):
            proof = copy.deepcopy(ef['proof_history'][0])
            proof['signal'] = signal
            label = 'proof'+str(i+1)
            proof['evidence']['evidence_id'] = label.ljust(128,'x') if maximum else label
            proof['evidence']['reference'] = pool[10+i*3]
            proof['evidence']['verified_claims'] = [pool[11+i*3]]
            proof['evidence']['version']['value'] = pool[12+i*3]
            x = append_proof(x,proof)
    # Different observations are a real retained conflict, not an agreement.
    for i, p in enumerate(x['conflicts'][0]['participants']):
        p['resync_observation']['value']['value']['value']['value'] = pool[i]
    if maximum:
        # Add independent bounded private/provenance identities until saturated.
        need = 32 if resolution else 30
        candidates = []
        for b in x['bindings']:
            for key in ('private_metadata_ref', 'credential_ref'):
                candidates.append((b, key))
            candidates.append((b['transport_refs'], 0))
        for p in x['conflicts'][0]['participants']:
            candidates.append((p['resync_observation']['value'], 'observation_id'))
            for h in p['resync_observation']['value']['provenance']:
                candidates.append((h, 'evidence_id'))
        for i, (obj, key) in enumerate(candidates):
            if len(dictionaries(SCHEMA, x)['id']) >= need:
                break
            obj[key] = ('extra%02d:' % i).ljust(128, 'x')
        if len(dictionaries(SCHEMA, x)['id']) != need:
            raise RuntimeError('fixture dictionary saturation')
    validate(x)
    return x
