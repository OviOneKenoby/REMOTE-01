import copy
import random
import struct
import unittest
import projection as p
import protocol as q

CHECKS = 0
CASES = []
SEED = 40042026


def pair(f):
    return p.old.advance_pair(q.require(f)['state'])['allocators']


def admission():
    after = p.fixture()
    for c in after['conflicts']:
        for b in c['participants']:
            b['resync_observation']['value']['pending_request_id'] = None
    before = copy.deepcopy(after)
    before['effects'] = []
    before['allocators']['request_lifetime'] = before['allocators']['ticket_watermark'] = 0
    p.validate(before)
    return before,after


class Redesign(unittest.TestCase):
    def check(self,v,message=''):
        global CHECKS
        CHECKS += 1
        self.assertTrue(v,message)

    def refuse(self,fn):
        global CHECKS
        CHECKS += 1
        with self.assertRaises(p.Refused):
            fn()

    def test_roundtrip_exact_fields_and_history(self):
        for maximum in (False,True):
            for resolved in (False,True):
                x = p.fixture(maximum,resolved)
                wire = p.encode(x)
                self.check(p.decode(wire)==x)
                self.check(p.encode(dict(reversed(list(x.items()))))==wire)
                self.check(q.read_node(q.node('BASE',x,1,1,q.ZERO))[1]==x)
        before = p.fixture(True)
        resolved = p.fixture(True,True)
        mid = p.append_proof(before,resolved['effects'][0]['proof_history'][1])
        self.check(len(p.encode(before))==5587)
        self.check(len(p.encode(mid))==5985)
        self.check(len(p.encode(resolved))==6383)
        self.check(p.decode(p.encode(mid))==mid)
        self.check(mid['effects'][0]['affected_keys'][0]['no_future'] and not mid['effects'][0]['affected_keys'][0]['no_past'])
        self.check(mid['effects'][0]['request']==before['effects'][0]['request'])
        self.check(mid['conflicts']==before['conflicts'])

    def test_every_truncation_corruption_and_migration(self):
        wire = p.encode(p.fixture(True,True))
        for n in range(len(wire)):
            self.refuse(lambda n=n:p.decode(wire[:n]))
        rng = random.Random(SEED)
        for _ in range(512):
            b = bytearray(wire)
            i = rng.randrange(len(b))
            b[i] ^= 1 << rng.randrange(8)
            self.refuse(lambda b=b:p.decode(b))
        for version in (0,2):
            b = bytearray(wire)
            b[4:6] = struct.pack('<H',version)
            self.refuse(lambda b=b:p.decode(b))
        b = bytearray(wire);b[6]=1
        self.refuse(lambda:p.decode(b))
        self.refuse(lambda:p.decode(wire+b'0'))

    def test_input_bounds_no_truncation_or_emission(self):
        before,after = admission()
        f = q.provision_fake(before,True);oracle=q.Oracle()
        mutations=[lambda x:x['bindings'][0].update(adapter_version='x'*65),
                   lambda x:x['effects'][0]['request'].update(recipe_capture={'blocked':True}),
                   lambda x:x['effects'][0]['request']['arguments'].update(extra='no'),
                   lambda x:x['devices'][0]['identity_evidence'][0].update(verified_claims=['a','b']),
                   lambda x:x['effects'][0]['request']['route_plan'][0]['execution_contract'].update(guaranteed_no_execution_errors=['a','b']),
                   lambda x:x['effects'][0]['affected_keys'].pop(),
                   lambda x:x['effects'].append(copy.deepcopy(x['effects'][0]))]
        for mutate in mutations:
            x=copy.deepcopy(after);mutate(x)
            self.refuse(lambda x=x:q.compact(f,x,oracle))
            self.check(oracle.emissions==[] and q.require(f)['state']==before)
        f.byte_budget=q.RESERVED_RECORD_BYTES-1
        self.refuse(lambda:q.compact(f,after,oracle))
        self.check(oracle.emissions==[])

    def test_dictionary_reserve_partial_proof_and_bad_correlation(self):
        x=p.fixture(True)
        for kind in ('id','text'):
            y=copy.deepcopy(x)
            if kind=='id':
                y['conflicts'][0]['participants'][0]['resync_observation']['value']['provenance'][0]['actor_id']='new_actor'
            else:
                y['devices'][0]['identity_evidence'][0]['reference']='unique extra text'
            self.refuse(lambda y=y:p.encode(y))
        pending=p.fixture();resolved=p.fixture(False,True)
        f=q.provision_fake(pending,True)
        for mutate in (lambda v:v.update(session_generation=99),
                       lambda v:v.update(historical_request='intent:99'),
                       lambda v:v['covered_keys'].pop(),
                       lambda v:v['evidence'].update(verified_claims=['a','b']),
                       lambda v:v.update(progress={'total':1,'emitted':1,'confirmed':0,'uncertain_remaining':True})):
            v=copy.deepcopy(resolved['effects'][0]['proof_history'][1]);mutate(v)
            old=copy.deepcopy(f.data)
            self.refuse(lambda v=v:q.append(f,'PROOF',v))
            self.check(f.data==old)
        # Past proof is not a future fence or a retry authorization.
        q.append(f,'PROOF',resolved['effects'][0]['proof_history'][2])
        ef=q.require(f)['state']['effects'][0]
        self.check(ef['affected_keys'][0]['no_past'] and not ef['affected_keys'][0]['no_future'])
        self.check(not ef['attempt']['guaranteed_no_execution'])
        q.compact(f)
        q.append(f,'PROOF',resolved['effects'][0]['proof_history'][1])
        self.check(q.require(f)['state']['effects'][0]['attempt']['guaranteed_no_execution'])

    def matrix(self,label,initial,operation):
        probe=initial.clone();oracle=q.Oracle();operation(probe,oracle)
        for index,boundary in enumerate(probe.steps):
            for mode in ('stop','torn','lost','ambiguous'):
                f=initial.clone((index,mode));oracle=q.Oracle()
                try:
                    operation(f,oracle)
                except (q.Interrupted,p.Refused):
                    pass
                else:
                    self.fail('fault not reached '+boundary)
                f.fault=None
                observed=list(oracle.emissions)
                recovered=q.recover(f)
                self.check(oracle.emissions==observed and recovered['scheduling']==[])
                if recovered['status']!='GLOBAL_BLOCK':
                    p.validate(recovered['state'])
                    if oracle.emissions:
                        self.check(len(recovered['state']['effects'])==1 and recovered['state']['allocators']['request_lifetime']==1,boundary)
                CASES.append(dict(operation=label,index=index,boundary=boundary,fault=mode,
                                  emissions=len(observed),recovery=recovered['status'],result='PASS_CONDITIONAL_G1_G4'))

    def test_every_admission_append_and_compaction_boundary(self):
        before,after=admission()
        self.matrix('initial-history-handover',q.provision_fake(before,True),lambda f,o:q.compact(f,after,o))
        pending=q.provision_fake(p.fixture(),True)
        proof=p.fixture(False,True)['effects'][0]['proof_history'][1]
        self.matrix('future-proof',pending,lambda f,o:q.append(f,'PROOF',proof))
        self.matrix('refused-allocation',pending,lambda f,o:q.append(f,'ALLOC',pair(f)))
        q.append(pending,'PROOF',proof)
        self.matrix('compaction-with-live-barrier',pending,lambda f,o:q.compact(f))

    def test_compaction_retains_all_proofs_nonreuse_and_conflict(self):
        pending=p.fixture();resolved=p.fixture(False,True)
        f=q.provision_fake(pending,True)
        oracle=q.Oracle()
        q.append(f,'PROOF',resolved['effects'][0]['proof_history'][1])
        mid=copy.deepcopy(q.require(f)['state'])
        q.compact(f)
        self.check(q.require(f)['state']==mid)
        self.check(not mid['effects'][0]['attempt']['guaranteed_no_execution'])
        q.append(f,'PROOF',resolved['effects'][0]['proof_history'][2])
        q.compact(f)
        expected=q.require(f)['state']
        for _ in range(100):
            for _ in range(4):
                q.append(f,'ALLOC',pair(f))
            expected['allocators']=copy.deepcopy(q.require(f)['state']['allocators'])
            q.compact(f)
            self.check(q.require(f)['state']==expected)
            self.check(oracle.emissions==[] and len(f.data)==1)
        self.check(expected['effects'][0]['request']['request_id']=='intent:1')
        self.check(expected['allocators']['request_lifetime']==401)

    def test_post_CAS_cleanup_loss_does_not_accumulate_two_tails(self):
        f=q.provision_fake(p.fixture(),True)
        proof=p.fixture(False,True)['effects'][0]['proof_history'][1]
        q.append(f,'ALLOC',pair(f));q.append(f,'PROOF',proof)
        oldslot=q.read_root(f.trusted_head)[3]
        probe=f.clone();q.compact(probe)
        index=probe.steps.index('after-trusted-CAS')
        interrupted=f.clone((index,'ambiguous'))
        with self.assertRaises(q.Interrupted):q.compact(interrupted)
        interrupted.fault=None
        self.check(q.key(oldslot,0) in interrupted.data)
        self.check(q.require(interrupted)['state']==q.require(f)['state'])
        q.append(interrupted,'ALLOC',pair(interrupted))
        self.check(not any(q.key(oldslot,i) in interrupted.data for i in range(q.MAX_TAIL+1)))
        self.check(interrupted.peak_bytes<=q.RESERVED_RECORD_BYTES)

    def test_future_resolution_slots_and_bytes_reserved(self):
        f=q.provision_fake(p.fixture(),True)
        q.append(f,'ALLOC',pair(f));q.append(f,'ALLOC',pair(f))
        self.refuse(lambda:q.append(f,'ALLOC',pair(f)))
        z=p.fixture(False,True)
        q.append(f,'PROOF',z['effects'][0]['proof_history'][1])
        q.append(f,'PROOF',z['effects'][0]['proof_history'][2])
        self.check(q.require(f)['state']['allocators']['request_lifetime']==3)
        self.check(q.require(f)['state']['effects'][0]['attempt']['guaranteed_no_execution'])
        self.refuse(lambda:q.append(f,'PROOF',z['effects'][0]['proof_history'][2]))

    def test_unqualified_backend_and_stale_root_record(self):
        before,after=admission();f=q.provision_fake(before,False);oracle=q.Oracle()
        self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        self.refuse(lambda:q.compact(f,after,oracle))
        self.check(not oracle.emissions)
        f=q.provision_fake(p.fixture(),True)
        original=f.clone()
        q.append(f,'ALLOC',pair(f))
        # Rollback all ordinary record data, with newest trusted head intact.
        f.data=original.data
        self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        f=q.provision_fake(p.fixture(),True);oldhead=f.trusted_head
        q.append(f,'ALLOC',pair(f))
        self.refuse(lambda:f.cas(oldhead,oldhead))
        self.check(q.require(f)['state']['allocators']['request_lifetime']==2)
        f.head_available=False
        self.check(q.recover(f)['status']=='GLOBAL_BLOCK')

    def test_a003_and_whole_trusted_backend_rollback_counterexample(self):
        import backend as a3
        x=p.old_fixture();x['effects']=[]
        x['allocators']['request_lifetime']=x['allocators']['ticket_watermark']=0
        initial=a3.bootstrap(x);f=a3.Fake(initial);o=a3.EmissionOracle()
        a3.checkpoint(f,p.old_fixture(),oracle=o);f.data['anchor']=initial['anchor']
        result=a3.recover(f.data)
        self.check(len(o.emissions)==1 and result['projection']['effects']==[])
        CASES.append(dict(operation='A003-counterexample',result='REPRODUCED_OLD_VALID_ANCHOR',emissions=1))
        before,after=admission();f=q.provision_fake(before,True);old=f.clone();o=q.Oracle()
        q.compact(f,after,o)
        # Deliberately violate G1: roll back trusted service as well as records.
        f.data=old.data;f.trusted_head=old.trusted_head
        recovered=q.recover(f)
        self.check(len(o.emissions)==1 and recovered['state']['effects']==[])
        CASES.append(dict(operation='G1-contract-violation',result='UNDETECTABLE_WHOLE_TRUSTED_BACKEND_ROLLBACK',emissions=1))
        # No pretend anti-rollback proof: actual unqualified backend always blocks.
        f.qualified=False
        self.check(q.recover(f)['status']=='GLOBAL_BLOCK')

    def test_exhaustion_and_explicit_errors(self):
        f=q.provision_fake(p.fixture(),True)
        g,first,last,slot,count,head=q.read_root(f.trusted_head)
        f.trusted_head=q.root(p.U64_MAX,first,last,slot,count,head)
        self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        wire=q.node('BASE',p.fixture(),p.U64_MAX,p.U64_MAX,q.ZERO)
        f.data={q.key(0,0):wire}
        f.trusted_head=q.root(p.U64_MAX,p.U64_MAX,p.U64_MAX,0,0,q.digest(wire))
        self.check(q.recover(f)['status']=='HISTORICAL_ONLY_BLOCKED')
        self.refuse(lambda:q.compact(f))

        self.refuse(lambda:q.append(f,'ALLOC',pair(f)))
        x=p.fixture();x['allocators']['request_lifetime']=x['allocators']['ticket_watermark']=p.U64_MAX
        f=q.provision_fake(x,True)
        self.refuse(lambda:q.append(f,'ALLOC',pair(f)))
        for op in ('write','commit','read','cas'):
            class Fails(q.Fake):
                pass
            def error(*args): raise q.Interrupted('explicit '+op+' failure')
            f=q.provision_fake(p.fixture(),True)
            setattr(f,op,error)
            try:q.compact(f)
            except (q.Interrupted,p.Refused):pass
            else:self.fail(op)
            self.check(True)
        f=q.provision_fake(p.fixture(),True);f.byte_budget=1
        self.refuse(lambda:q.compact(f))

    def test_single_owner_reentry_refuses_without_mutation(self):
        f=q.provision_fake(p.fixture(),True)
        original=copy.deepcopy(f.data)
        original_head=f.trusted_head
        f.busy=True
        self.refuse(lambda:q.compact(f))
        self.refuse(lambda:q.append(f,'ALLOC',p.old.advance_pair(p.fixture())['allocators']))
        self.check(f.data==original and f.trusted_head==original_head)
        f.busy=False
        q.compact(f)
        self.check(not f.busy and q.require(f)['state']==p.fixture())


if __name__=='__main__': unittest.main(verbosity=2)
