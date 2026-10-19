"""No target APIs. Run with python -B run_qualification.py."""
from pathlib import Path
import unittest,json,platform,io,hashlib
import backend_model as m
import test_backend as t
out=Path(__file__).resolve().parent.parent/'results';out.mkdir(exist_ok=True)
log=io.StringIO();result=unittest.TextTestRunner(stream=log,verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(t))
(out/'TEST_LOG.txt').write_text(log.getvalue(),encoding='utf-8',newline='\n')
(out/'FAULT_MATRIX.json').write_text(json.dumps(t.CASES,indent=2)+'\n',encoding='utf-8',newline='\n')
ledger=[m.capacity(o,f,r,c) for o in (0,108,512,1024,2048,4096) for f in (0,32,64,126,630) for r in (0,32,126) for c in (4000,512)]
(out/'CAPACITY_LEDGER.json').write_text(json.dumps(ledger,indent=2)+'\n',encoding='utf-8',newline='\n')
v={'seed':t.SEED,'environment':platform.python_version()+' / '+platform.platform(),'tests':result.testsRun,'checks':t.CHECKS,'failures':len(result.failures),'errors':len(result.errors),'fault_scenarios':len(t.CASES),'capacity_cases':len(ledger),'host_model':'PASS' if result.wasSuccessful() else 'FAIL','concrete_backend':'FAIL: G1 anti-rollback absent; G2-G4 physical durability/capacity UNKNOWN','capacity':'CONDITIONAL low-occupancy ideal; FAIL adverse occupancy/fragmentation; actual UNKNOWN','physical':'NOT RUN','production':'NOT IMPLEMENTED','task_verdict':'BLOCKED'}
(out/'MEASUREMENTS.json').write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(v,indent=2))
raise SystemExit(0 if result.wasSuccessful() else 1)
