"""Isolated fake-only protocol; no NVS, production Core or physical emission."""
import copy
import hashlib
import struct
import zlib
import projection as p

MAX_TAIL = 4
MAX_STATE_BYTES = 6383
MAX_BASE_BYTES = 64 + MAX_STATE_BYTES
MAX_PROOF_BYTES = 1146
MAX_ALLOC_BYTES = 162
RESERVED_RECORD_BYTES = 2*MAX_BASE_BYTES + 2*MAX_PROOF_BYTES + 2*MAX_ALLOC_BYTES
NODE_HEADER = struct.Struct('<4sHBBQQ32sII')  # 64 bytes
ROOT = struct.Struct('<4sHHQQQII32sI')       # 76 bytes
ZERO = bytes(32)
KINDS = {'BASE': 0, 'ALLOC': 1, 'PROOF': 2}
ALLOC_SCHEMA = p.SCHEMA[1]['allocators']


class Interrupted(RuntimeError):
    pass


class Oracle:
    def __init__(self):
        self.emissions = []

    def emit(self, request):
        self.emissions.append(request)


def digest(wire):
    return hashlib.sha256(wire).digest()


def node(kind, value, seq, generation, previous):
    if kind == 'BASE':
        payload = p.encode(value)
    else:
        payload = p.pack(ALLOC_SCHEMA if kind == 'ALLOC' else p.old.PROOF,
                         value, b'C4AL' if kind == 'ALLOC' else b'C4PF')
    if not 0 < seq <= p.U64_MAX or not 0 < generation <= p.U64_MAX:
        raise p.Refused('counter exhausted')
    return NODE_HEADER.pack(b'C4LR',1,KINDS[kind],0,seq,generation,previous,
                            len(payload),zlib.crc32(payload)) + payload


def read_node(wire):
    if len(wire) < NODE_HEADER.size or len(wire) > 11000:
        raise p.Refused('record length')
    m,v,k,f,s,g,prev,n,crc = NODE_HEADER.unpack(wire[:NODE_HEADER.size])
    payload = wire[NODE_HEADER.size:]
    if m != b'C4LR' or v != 1 or k not in KINDS.values() or f or not s or not g or n != len(payload) or zlib.crc32(payload) != crc:
        raise p.Refused('record schema/migration/counter/CRC')
    kind = list(KINDS)[k]
    if kind == 'BASE':
        value = p.decode(payload)
    else:
        value = p.unpack(ALLOC_SCHEMA if kind == 'ALLOC' else p.old.PROOF,
                         payload,b'C4AL' if kind == 'ALLOC' else b'C4PF')
    if node(kind,value,s,g,prev) != wire:
        raise p.Refused('record noncanonical')
    return kind,value,s,g,prev


def root(generation, first, last, slot, count, head):
    b = ROOT.pack(b'C4RT',1,0,generation,first,last,slot,count,head,0)
    return b[:-4] + struct.pack('<I',zlib.crc32(b[:-4]))


def read_root(b):
    if not b or len(b) != ROOT.size or zlib.crc32(b[:-4]) != struct.unpack('<I',b[-4:])[0]:
        raise p.Refused('trusted head unavailable/corrupt')
    m,v,f,g,first,last,slot,count,head,_ = ROOT.unpack(b)
    if m != b'C4RT' or v != 1 or f or not g or not first or slot not in (0,1) or count > MAX_TAIL or last != first+count:
        raise p.Refused('trusted head schema/migration/bounds')
    return g,first,last,slot,count,head


def key(slot, index):
    return 's%d_%s' % (slot,'base' if index == 0 else 'r'+str(index))


class Fake:
    """qualified=True explicitly ASSUMES G1-G4; not an NVS implementation.

    trusted_head lives outside rollbackable record data. Acknowledged writes
    precede durable atomic CAS. Unknown/error CAS preserves old OR new head.
    Whole trusted-cell rollback is outside this assumed guarantee, not detectable.
    """
    def __init__(self, qualified=False, fault=None, byte_budget=20000):
        self.qualified = qualified
        self.data = {}
        self.trusted_head = None
        self.head_available = True
        self.fault = fault
        self.steps = []
        self.byte_budget = byte_budget
        self.peak_bytes = 0
        self.busy = False

    def clone(self, fault=None):
        f = Fake(self.qualified,fault,self.byte_budget)
        f.data = copy.deepcopy(self.data)
        f.trusted_head = self.trusted_head
        f.head_available = self.head_available
        return f

    def point(self,label,k=None,value=None):
        index = len(self.steps)
        self.steps.append(label)
        if self.fault and self.fault[0] == index:
            mode = self.fault[1]
            if k is not None and mode == 'torn' and value is not None:
                self.data[k] = value[:len(value)//2]
            elif k is not None and mode == 'lost':
                self.data.pop(k,None)
            raise Interrupted(label+':'+mode)

    def read(self,k):
        self.point('before-read-'+k)
        v = self.data.get(k)
        self.point('after-read-'+k)
        return v

    def write(self,k,value):
        self.point('before-write-'+k,k,value)
        if sum(map(len,self.data.values())) - len(self.data.get(k,b'')) + len(value) > self.byte_budget:
            raise p.Refused('fake NO_SPACE; real NVS footprint separately modeled')
        self.data[k] = value
        self.peak_bytes = max(self.peak_bytes,sum(map(len,self.data.values())))
        self.point('after-write-'+k,k,value)

    def commit(self,label):
        self.point('before-commit-'+label)
        # As in the installed NVS source, not an additional transaction barrier.
        self.point('after-commit-'+label)

    def erase(self,k):
        self.point('before-erase-'+k)
        self.data.pop(k,None)
        self.point('after-erase-'+k)

    def cas(self,expected,new):
        self.point('before-trusted-CAS')
        if not self.qualified or not self.head_available or expected != self.trusted_head:
            raise p.Refused('unqualified/stale/unavailable trusted head')
        ng,*_ = read_root(new)
        if expected is not None and ng != read_root(expected)[0]+1:
            raise p.Refused('monotonic head generation')
        self.trusted_head = new
        self.point('after-trusted-CAS')

    def head(self):
        self.point('before-read-trusted-head')
        if not self.qualified or not self.head_available:
            raise p.Refused('newest-intent guarantee NOT QUALIFIED: GLOBAL_BLOCK')
        v = self.trusted_head
        self.point('after-read-trusted-head')
        return v


def recover(f):
    # No oracle/adapter parameter; only historical state, NEVER a replay queue.
    try:
        b = f.head()
        g,first,last,slot,count,head = read_root(b)
        previous = ZERO
        base_generation = None
        state = None
        for i in range(count+1):
            wire = f.read(key(slot,i))
            if not wire:
                raise p.Refused('referenced record missing')
            kind,value,seq,gen,prev = read_node(wire)
            if seq != first+i or prev != previous:
                raise p.Refused('stale record/link/sequence')
            if i == 0:
                if kind != 'BASE' or gen != g-count:
                    raise p.Refused('stale base generation')
                state,base_generation = value,gen
            else:
                if kind == 'BASE' or gen != base_generation+i:
                    raise p.Refused('record generation/type')
                if kind == 'ALLOC':
                    expected = p.old.advance_pair(state)['allocators']
                    if value != expected:
                        raise p.Refused('allocator non-reuse regression/mutation')
                    state['allocators'] = value
                    p.validate(state)
                else:
                    state = p.append_proof(state,value)
            previous = digest(wire)
        if previous != head:
            raise p.Refused('trusted head mismatch: rollback/corruption')
        # The following is not implementation of Core boot/identity/epoch logic.
        return dict(status='HISTORICAL_ONLY_BLOCKED',state=state,root=b,
                    scheduling=[],authorization='UNKNOWN',current_observations=[],
                    old_epoch_lookup='HISTORY_EXPIRED; import not implemented')
    except (p.Refused,Interrupted,TypeError,ValueError,KeyError) as exc:
        return dict(status='GLOBAL_BLOCK',reason=str(exc),scheduling=[])


def provision_fake(x, qualified=False):
    f = Fake(qualified)
    wire = node('BASE',x,1,1,ZERO)
    f.data[key(0,0)] = wire
    f.trusted_head = root(1,1,1,0,0,digest(wire))
    return f


def require(f):
    result = recover(f)
    if result['status'] != 'HISTORICAL_ONLY_BLOCKED':
        raise p.Refused(result['reason'])
    return result


def erase_slot(f,slot):
    for i in range(MAX_TAIL+1):
        f.erase(key(slot,i))


def serialized(operation):
    def wrapped(f,*args,**kwargs):
        if f.busy:
            raise p.Refused('single serialized writer required; reentry blocked')
        f.busy = True
        try:
            return operation(f,*args,**kwargs)
        finally:
            f.busy = False
    return wrapped


@serialized
def compact(f, replacement=None, oracle=None):
    old = require(f)
    state = old['state'] if replacement is None else replacement
    p.validate(state)
    if f.byte_budget < RESERVED_RECORD_BYTES:
        raise p.Refused('full update/compaction/future-resolution reserve unavailable')
    # Replacement is a fake initial handover history qualification only. All
    # existing history must be preserved; no effect/barrier/allocator eviction.
    if replacement is not None:
        if old['state']['effects'] or old['state']['allocators']['request_lifetime'] != 0:
            raise p.Refused('new effect capacity exhausted; no active eviction')
        expected = copy.deepcopy(old['state'])
        expected['effects'] = copy.deepcopy(replacement['effects'])
        expected['allocators'] = p.old.advance_pair(old['state'])['allocators']
        if expected != replacement:
            raise p.Refused('replacement changes unrelated registry/captured state')
    g,first,last,slot,count,head = read_root(old['root'])
    if g == p.U64_MAX or last == p.U64_MAX:
        raise p.Refused('persistent generation/record sequence exhausted')
    inactive = 1-slot
    # Unreachable orphan versions are deleted BEFORE writing replacement: two
    # complete snapshots, not silently three. Interrupted deletion is harmless.
    erase_slot(f,inactive)
    wire = node('BASE',state,last+1,g+1,ZERO)
    f.write(key(inactive,0),wire)
    f.commit('compaction')
    if f.read(key(inactive,0)) != wire:
        raise p.Refused('compaction readback')
    f.cas(old['root'],root(g+1,last+1,last+1,inactive,0,digest(wire)))
    if oracle is not None:
        f.point('before-simulated-emission')
        oracle.emit(state['effects'][0]['request']['request_id'])
        f.point('after-simulated-emission')
    # If any cleanup fails, new trusted head remains sole recovery authority.
    erase_slot(f,slot)
    return require(f)


@serialized
def append(f,kind,value):
    if f.byte_budget < RESERVED_RECORD_BYTES:
        raise p.Refused('full update/compaction/future-resolution reserve unavailable')
    old = require(f)
    state = copy.deepcopy(old['state'])
    if kind == 'ALLOC':
        expected = p.old.advance_pair(state)['allocators']
        if value != expected:
            raise p.Refused('allocation must atomically consume next pair')
        state['allocators'] = value
    elif kind == 'PROOF':
        state = p.append_proof(state,value)
    else:
        raise p.Refused('unknown record type')
    p.validate(state)
    g,first,last,slot,count,head = read_root(old['root'])
    if g == p.U64_MAX or last == p.U64_MAX:
        raise p.Refused('persistent counter exhausted')
    remaining = 2-(len(state['effects'][0]['proof_history'])-1) if state['effects'] else 2
    if count+1+remaining > MAX_TAIL:
        raise p.Refused('compact before consuming future proof slots')
    # A power loss after compaction CAS may leave the old segment's BASE/tail.
    # Remove that unreachable set before accumulating another active tail;
    # otherwise two tails would silently exceed the stated reserve.
    erase_slot(f,1-slot)
    k = key(slot,count+1)
    # Remove unreachable failed-write orphan; never erase a referenced node.
    f.erase(k)
    wire = node(kind,value,last+1,g+1,head)
    f.write(k,wire)
    f.commit('append')
    if f.read(k) != wire:
        raise p.Refused('append readback')
    f.cas(old['root'],root(g+1,first,last+1,slot,count+1,digest(wire)))
    return require(f)
