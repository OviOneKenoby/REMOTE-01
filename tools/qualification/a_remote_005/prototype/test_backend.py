import copy,random,struct,unittest
import backend_model as m
q,p=m.q,m.p
SEED=50052026
CASES=[];CHECKS=0

class Qualification(unittest.TestCase):
    def check(self,condition):
        global CHECKS
        CHECKS+=1;self.assertTrue(condition)
    def test_actual_unqualified_candidate_blocks(self):
        before,after=m.admission();f=m.provision(before);o=m.DispatchOracle(after['effects'][0]['request']['request_id'])
        self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        with self.assertRaises(p.Refused):q.compact(f,after,o)
        self.check(not o.events);self.check(f.data==m.provision(before).data)
    def test_ambiguous_ack_both_persistence_outcomes(self):
        trace=m.Candidate(True);trace.write('example',b'new')
        for label in ('before-index-example','after-index-example'):
            index=trace.steps.index(label)
            for mode in ('ambiguous-not-persistent','ambiguous-persistent'):
                f=m.Candidate(True,(index,mode));f.data['example']=b'old'
                with self.assertRaises(q.Interrupted):f.write('example',b'new')
                self.check(f.data['example']==(b'new' if mode=='ambiguous-persistent' else b'old'))
                CASES.append(dict(test='ambiguous-index',label=label,mode=mode,visible_new=f.data['example']==b'new'))
    def test_A003_valid_old_anchor_and_candidate_whole_rollback(self):
        import backend as a3
        x=p.old_fixture();x['effects']=[];x['allocators']['request_lifetime']=x['allocators']['ticket_watermark']=0
        old=a3.bootstrap(x);f=a3.Fake(old);o=m.DispatchOracle(p.old_fixture()['effects'][0]['request']['request_id'])
        a3.checkpoint(f,p.old_fixture(),oracle=o);f.data['anchor']=old['anchor'];r=a3.recover(f.data)
        self.check(len(o.events)==1 and r['projection']['effects']==[])
        CASES.append(dict(test='A003-old-anchor',result='REPRODUCED',lost_newest=True))
        before,after=m.admission();f=m.provision(before,True);saved=copy.deepcopy(f.data);o=m.DispatchOracle(after['effects'][0]['request']['request_id'])
        q.compact(f,after,o);self.check(len(o.events)==1);self.check(len(f.acknowledged)==1)
        f.data=saved;r=q.recover(f)
        self.check(r['status']=='HISTORICAL_ONLY_BLOCKED' and r['state']['effects']==[])
        self.check(r['state']['allocators']['request_lifetime']==0)
        self.check(f.data==m.provision(before,True).data)
        CASES.append(dict(test='same-domain-root-record-rollback',result='UNDETECTABLE_OLD_VALID_STATE',independent_emissions=len(o.events),lost_effect=True,lost_watermark=True))
        f.laboratory=False;self.check(q.recover(f)['status']=='GLOBAL_BLOCK');self.check(len(o.events)==1)
    def test_fault_every_persistent_boundary(self):
        before,after=m.admission()
        operations={'admission':lambda f,o:q.compact(f,after,o),'allocation':lambda f,o:q.append(f,'ALLOC',p.old.advance_pair(q.require(f)['state'])['allocators']),
            'proof':lambda f,o:q.append(f,'PROOF',p.fixture(False,True)['effects'][0]['proof_history'][1]),'compact':lambda f,o:q.compact(f),'GC':lambda f,o:f.gc()}
        for name,operation in operations.items():
            start=m.provision(before if name=='admission' else p.fixture(),True)
            trace=start.clone();operation(trace,m.DispatchOracle(after['effects'][0]['request']['request_id']))
            for n,label in enumerate(trace.steps):
                for mode in ('before-or-after','ambiguous-not-persistent','ambiguous-persistent','torn','lost'):
                    f=start.clone((n,mode));o=m.DispatchOracle(after['effects'][0]['request']['request_id'])
                    try:operation(f,o)
                    except (q.Interrupted,p.Refused):pass
                    emitted=len(o.events);f.fault=None
                    recovered=q.recover(f)
                    self.check(len(o.events)==emitted)
                    self.check(recovered['status'] in ('GLOBAL_BLOCK','HISTORICAL_ONLY_BLOCKED'))
                    if emitted:
                        self.check(recovered['status']=='HISTORICAL_ONLY_BLOCKED')
                        self.check(bool(recovered['state']['effects']))
                        self.check(recovered['state']['allocators']['request_lifetime']>=1)
                    self.check(emitted<=1)
                    CASES.append(dict(operation=name,boundary=n,label=label,mode=mode,result=recovered['status'],dispatch=emitted,recovery_dispatch=0))
    def test_stale_root_records_and_order_violation(self):
        f=m.provision(p.fixture(),True);old=f.clone();q.append(f,'ALLOC',p.old.advance_pair(q.require(f)['state'])['allocators'])
        current=copy.deepcopy(f.data);f.data=dict(old.data,root=current['root'])
        self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        f.data=copy.deepcopy(current);f.data['root']=old.data['root']
        # Older root references old BASE which is still present: freshness cannot be inferred.
        r=q.recover(f);self.check(r['status']=='HISTORICAL_ONLY_BLOCKED');self.check(r['state']['allocators']['request_lifetime']==1)
        CASES.append(dict(test='stale-root-valid-old-records',result='UNDETECTABLE_ALLOCATION_ROLLBACK'))
        f.data=copy.deepcopy(current);f.data.pop(q.key(0,1));self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        CASES.append(dict(test='new-root-before-record-durability',result='GLOBAL_BLOCK'))
    def test_software_CAS_serialization_and_reentry(self):
        f=m.provision(p.fixture(),True);old=f.data['root'];q.append(f,'ALLOC',p.old.advance_pair(q.require(f)['state'])['allocators'])
        with self.assertRaises(p.Refused):f.cas(old,f.data['root'])
        self.check(True);f.busy=True;saved=copy.deepcopy(f.data)
        with self.assertRaises(p.Refused):q.compact(f)
        self.check(f.data==saved)
        # Individually locked get/set APIs are not a compound CAS: two writers can compare same old value.
        root=1;a_observed=b_observed=root;a_ok=a_observed==root;root=2;b_ok=b_observed==1;root=3
        self.check(a_ok and b_ok and root==3)
        CASES.append(dict(test='unserialized-read-set-race',result='BOTH_COMPARE_SAME_EXPECTED',verdict='NO_NATIVE_CAS'))
    def test_corruption_migration_counter_capacity_exhaustion(self):
        base=m.provision(p.fixture(True,True),True);rng=random.Random(SEED)
        for key in ('root',q.key(0,0)):
            wire=base.data[key]
            for n in range(len(wire)):
                f=base.clone();f.data[key]=wire[:n];self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
            for _ in range(128):
                f=base.clone();b=bytearray(wire);b[rng.randrange(len(b))]^=1;f.data[key]=bytes(b);self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        # Valid CRC unsupported root version still refuses migration.
        f=base.clone();b=bytearray(f.data['root']);b[4:6]=struct.pack('<H',2)
        import zlib
        b[-4:]=struct.pack('<I',zlib.crc32(b[:-4]));f.data['root']=bytes(b);self.check(q.recover(f)['status']=='GLOBAL_BLOCK')
        f=base.clone();f.byte_budget=1
        with self.assertRaises(p.Refused):q.compact(f)
        self.check(True)
        f=m.provision(p.fixture(),True);g,first,last,slot,count,head=q.read_root(f.data['root'])
        with self.assertRaises(p.Refused):f.cas(f.data['root'],q.root(g,first,last,slot,count,head))
        self.check(True)
        x=p.fixture();x['allocators']['request_lifetime']=x['allocators']['ticket_watermark']=p.U64_MAX
        with self.assertRaises(p.Refused):p.old.advance_pair(x)
        self.check(True)
    def test_capacity_and_profile_bounds(self):
        self.check(len(p.encode(p.fixture(True,True)))==6383)
        self.check(len(p.encode(p.fixture(True)))==5587)
        self.check(m.capacity()['physical_peak_entries_lower_bound']==662)
        self.check(m.capacity(2048)['verdict']=='FAIL')
        self.check(m.capacity(108,126)['verdict']=='FAIL')
        self.check(m.capacity(108,0,126)['verdict']=='FAIL')
        x=p.fixture();x['effects'][0]['request']['request_id']='x'*129
        with self.assertRaises(p.Refused):p.encode(x)
        self.check(True)
