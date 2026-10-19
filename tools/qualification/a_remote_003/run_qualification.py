"""Reproduce only host artifacts, under ../results; never opens device/NVS."""
import hashlib
import io
import json
import platform
from pathlib import Path
import unittest
import codec
import test_qualification as tests
from fixtures import fixture
from backend import capacity

def maximum(s):
    k=s[0]
    if k=='record': return sum(maximum(t) for t in s[1].values())
    if k=='array': return 2+s[2]*maximum(s[1])
    if k=='optional': return 1+maximum(s[1])
    if k=='union': return 1+max(maximum(t) for t in s[1].values())
    if k in ('id','text'): return 2+s[1]
    if k=='uint': return s[1]//8
    if k=='int': return 8
    return 1

def fields(s,path='projection'):
    k=s[0]
    if k=='record':
        for n,t in s[1].items(): yield from fields(t,path+'.'+n)
    elif k=='array':
        yield (path,'array max '+str(s[2]))
        yield from fields(s[1],path+'[]')
    elif k=='optional': yield from fields(s[1],path+'?')
    elif k=='union':
        for n,t in s[1].items(): yield from fields(t,path+'<'+n+'>')
    else: yield (path,str(s))

def main():
    out=Path(__file__).resolve().parent.parent/'results'
    out.mkdir(exist_ok=True)
    stream=io.StringIO()
    result=unittest.TextTestRunner(stream=stream,verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(tests))
    (out/'TEST_LOG.txt').write_text(stream.getvalue(),encoding='utf-8')
    ledger={}
    for name,m,r in [('typical_pending',False,False),('typical_resolved',False,True),('maximum_pending',True,False),('maximum_resolved',True,True)]:
        x=fixture(m,r); wire=codec.encode(x)
        (out/(name+'.json')).write_text(json.dumps(x,indent=2,ensure_ascii=True)+'\n',encoding='utf-8')
        (out/(name+'.bin')).write_bytes(wire)
        ledger[name]={'payload_bytes':len(wire)-16,'encoded_bytes':len(wire),'sha256':hashlib.sha256(wire).hexdigest(),
                      'sections':{n:len(codec.put(s,x[n])) for n,s in codec.PROJECTION[1].items()}}
    upper=16+maximum(codec.PROJECTION)
    # Legal-profile restrictions versus unconstrained structural schema maximum.
    deductions={'title/precondition STRING instead of larger REFERENCE':3*(maximum(codec.REF)-maximum(codec.TEXT)),
                'second key RESOURCE instead of TARGET in effect and three proofs':4*130,
                'six correlated intent IDs 27 instead of 128 bytes':6*101,
                'no seek generation':8,'no recipe capture':1,
                'no recipe step in attempt/five errors':6*4,'no rollback_of ID':130}
    derived=upper-sum(deductions.values())
    if derived!=ledger['maximum_resolved']['encoded_bytes']: raise RuntimeError(('maximum size derivation',upper,deductions,derived,ledger))
    reserve=ledger['maximum_resolved']['encoded_bytes']-ledger['maximum_pending']['encoded_bytes']
    sizes=[3072]+[v['encoded_bytes'] for v in ledger.values()]
    report={'environment':platform.python_version()+' / '+platform.platform(),'seed':tests.SEED,'tests':result.testsRun,
            'failures':len(result.failures),'errors':len(result.errors),'counted_checks':tests.CHECKS,
            'fault_cases':len(tests.FAULT_CASES),'fixtures':ledger,'structural_max_bytes':upper,
            'legal_max_deductions':deductions,'derived_max_legal_bytes':derived,'maximum_resolution_reserve_bytes':reserve,
            'capacity':[capacity(n,c) for n in sizes for c in (0,512,4096)],
            'qualification':'PRODUCTION FAIL/BLOCKED: 3KiB maximum capacity; stale-valid-anchor guarantee',
            'hardware':'NOT RUN','production_integration':'NOT IMPLEMENTED'}
    (out/'MEASUREMENTS.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    (out/'FAULT_MATRIX.json').write_text(json.dumps(tests.FAULT_CASES,indent=2)+'\n',encoding='utf-8')
    rows=['Path | Codec bound/type','--- | ---']+[p+' | '+t for p,t in fields(codec.PROJECTION)]
    (out/'FIELD_PATHS.md').write_text('# Complete codec field paths\n\n'+'\n'.join(rows)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('tests','failures','errors','counted_checks','fault_cases','derived_max_legal_bytes','maximum_resolution_reserve_bytes','qualification')}))
    print(json.dumps({k:v['encoded_bytes'] for k,v in ledger.items()}))
    return int(not result.wasSuccessful())

if __name__=='__main__': raise SystemExit(main())
