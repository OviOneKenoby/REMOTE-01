"""Reproducible host evidence only; never calls ESP-IDF/NVS or production Core."""
import hashlib
import io
import json
import platform
from pathlib import Path
import unittest
import projection as p
import protocol as q
import test_redesign as tests


def max_body(s,path='',added=2):
    k=s[0]
    if k in ('id','text','enum','bool'):
        return 1
    if k=='uint':return s[1]//8
    if k=='int':return 8
    if k=='optional':return 1+max_body(s[1],path,added)
    if k=='union':return 1+max(max_body(t,path,added) for t in s[1].values())
    if k=='record':return sum(max_body(t,path+'.'+n,added) for n,t in s[1].items())
    if k=='array':
        count=s[2]
        if path.endswith(('.verified_claims','.guaranteed_no_execution_errors')):count=1
        if path.endswith('.associations'):count=0
        if path.endswith(('.proof_history','.attempt.evidence')):count=1+added
        return 2+count*max_body(s[1],path+'[]',added)
    raise RuntimeError(k)


def entries(n):
    # Lower bound: full4000byte chunks; fragmentation increases real overhead.
    return (n+31)//32+(n+3999)//4000+1


def ledger(other):
    nodes=[2*q.MAX_BASE_BYTES,2*q.MAX_PROOF_BYTES,2*q.MAX_ALLOC_BYTES]
    snapshot_entries=2*entries(q.MAX_BASE_BYTES)
    tail_entries=2*entries(q.MAX_PROOF_BYTES)+2*entries(q.MAX_ALLOC_BYTES)
    root_entries=2*entries(q.ROOT.size)
    others=2*entries(other) if other else 0
    total=snapshot_entries+tail_entries+root_entries+others+12+126
    return dict(other_occupancy_assumed_bytes=other,other_old_new_entries=others,
                snapshot_overlap_entries=snapshot_entries,tail_entries=tail_entries,
                root_old_new_entries=root_entries,metadata_entries=12,GC_entries=126,
                peak_entries_lower_bound=total,partition_entries=756,
                lower_bound_pages=(total+125)//126,optimistic_fits=total<=756,
                free_entries_if_exact=756-total,record_blob_bytes=sum(nodes))


def main():
    out=Path(__file__).parent.parent/'results';out.mkdir(exist_ok=True)
    stream=io.StringIO()
    result=unittest.TextTestRunner(stream=stream,verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(tests))
    (out/'TEST_LOG.txt').write_text(stream.getvalue(),encoding='utf-8',newline='\n')
    fixtures={}
    for name,maximum,resolution in [('typical_pending',False,False),('typical_resolved',False,True),('maximum_pending',True,False),('maximum_resolved',True,True)]:
        x=p.fixture(maximum,resolution);wire=p.encode(x)
        (out/(name+'.json')).write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8',newline='\n')
        (out/(name+'.bin')).write_bytes(wire)
        d=p.dictionaries(p.SCHEMA,x)
        fixtures[name]=dict(bytes=len(wire),sha256=hashlib.sha256(wire).hexdigest(),
                            dictionary_counts={k:len(v) for k,v in d.items()},
                            typed_body_bytes=len(p.body_put(p.SCHEMA,x,d)))
    bounds={}
    for added in (0,1,2):
        theoretical=max_body(p.SCHEMA,added=added)
        # Enforced title-only Scalar; exact TARGET+RESOURCE key pair; excluded
        # seek/recipe/error-step captures. All other legal maxima saturated.
        deductions={'3 STRING instead of REF scalars':3*27,
                    'RESOURCE second key in effect and each proof':2+added,
                    'seek absent':8,'recipe absent':1,'step absent in attempt and errors':4*(4+added)}
        body=theoretical-sum(deductions.values())
        ids=30+added;texts=10+3*added
        total=16+2+ids*129-101+texts*65+body
        bounds[str(added)]=dict(structural_body_max=theoretical,deductions=deductions,
                              legal_body_max=body,ID_table_bytes=ids*129-101,
                              TEXT_table_bytes=texts*65,encoded_max=total)
    if bounds['0']['encoded_max']!=5587 or bounds['2']['encoded_max']!=6383:
        raise RuntimeError(('max derivation',bounds))
    z=p.fixture(True,True);initial=p.fixture(True)
    middle=p.append_proof(initial,z['effects'][0]['proof_history'][1])
    assert len(p.encode(middle))==bounds['1']['encoded_max']==5985
    (out/'maximum_intermediate.json').write_text(json.dumps(middle,indent=2)+'\n',encoding='utf-8',newline='\n')
    (out/'maximum_intermediate.bin').write_bytes(p.encode(middle))
    proof=z['effects'][0]['proof_history'][1]
    assert len(q.node('PROOF',proof,1,1,q.ZERO))==q.MAX_PROOF_BYTES
    assert len(q.node('ALLOC',z['allocators'],1,1,q.ZERO))==q.MAX_ALLOC_BYTES
    assert len(q.node('BASE',z,1,1,q.ZERO))==q.MAX_BASE_BYTES
    # Measured maximal-state compaction includes old base + two proof nodes +
    # new full base; model adds conservative two allocation slots too.
    f=q.provision_fake(initial,True)
    for v in z['effects'][0]['proof_history'][1:]:q.append(f,'PROOF',v)
    q.compact(f)
    report=dict(environment=platform.python_version()+' / '+platform.platform(),seed=tests.SEED,
                tests=result.testsRun,counted_checks=tests.CHECKS,failures=len(result.failures),errors=len(result.errors),
                fault_cases=len(tests.CASES),fixtures=fixtures,maximum_derivation=bounds,
                maximum_state_bytes=q.MAX_STATE_BYTES,maximum_future_growth_bytes=6383-5587,
                BASE_node_max=q.MAX_BASE_BYTES,PROOF_node_max=q.MAX_PROOF_BYTES,ALLOC_node_max=q.MAX_ALLOC_BYTES,
                trusted_root_bytes=q.ROOT.size,maximum_tail_nodes=q.MAX_TAIL,
                conservative_record_reserve_bytes=q.RESERVED_RECORD_BYTES,
                measured_max_fixture_compaction_record_peak=f.peak_bytes,
                capacity=[ledger(n) for n in (0,512,1024,2048,4096)],
                qualification='CONDITIONAL HOST PASS; actual backend newest-intent integrity NOT QUALIFIED / PRODUCTION BLOCKED',
                hardware='NOT RUN',production_integration='NOT IMPLEMENTED')
    (out/'MEASUREMENTS.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    (out/'FAULT_MATRIX.json').write_text(json.dumps(tests.CASES,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({k:report[k] for k in ('tests','counted_checks','failures','errors','fault_cases','maximum_state_bytes','maximum_future_growth_bytes','conservative_record_reserve_bytes','measured_max_fixture_compaction_record_peak')}))
    print(json.dumps(report['capacity']))
    return int(not result.wasSuccessful())


if __name__=='__main__':raise SystemExit(main())
