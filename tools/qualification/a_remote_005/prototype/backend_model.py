"""Source-informed conservative HOST model, not ESP-IDF emulator or target code.
Actual candidate is unqualified. Unsafe laboratory mode is only for falsification.
Root, sequences, CRC and records share one rollbackable persistent dictionary.
"""
from pathlib import Path
import sys,copy,math
parent=Path(__file__).resolve().parents[2]
peer=parent/('A-REMOTE-004' if (parent/'A-REMOTE-004').exists() else 'a_remote_004')/'prototype'
sys.path.insert(0,str(peer))
import projection as p
import protocol as q

class DispatchOracle:
    """Independent external observation; never stored/restored with backend."""
    def __init__(self,allowed):self.allowed=allowed;self.events=[]
    def emit(self,request):
        assert request==self.allowed
        self.events.append(request)

class Candidate(q.Fake):
    def __init__(self,laboratory=False,fault=None):
        super().__init__(qualified=laboratory,fault=fault)
        self.laboratory=laboratory
        self.versions={} # unreachable partial writes retained until model GC
        self.gc_copy={}
        self.acknowledged=[] # independent observer, NOT restored by rollback
    def clone(self,fault=None):
        f=Candidate(self.laboratory,fault)
        f.data=copy.deepcopy(self.data);f.versions=copy.deepcopy(self.versions)
        f.acknowledged=list(self.acknowledged);return f
    def write(self,key,value):
        if sum(map(len,self.data.values()))-len(self.data.get(key,b''))+len(value)>self.byte_budget:
            raise p.Refused('NO_SPACE model budget')
        self.point('before-set-'+key,key,value)
        chunks=[]
        for n in range(0,len(value),4000):
            part=value[n:n+4000]
            def add_chunk():
                chunks.append(part);self.versions[key]=list(chunks)
            self.effect('chunk-'+key+'-'+str(n),add_chunk,key,value)
        self.effect('index-'+key,lambda:self.data.__setitem__(key,b''.join(chunks)),key,value)
        self.effect('old-version-erase-'+key,lambda:self.versions.pop(key,None),key,value)
        self.peak_bytes=max(self.peak_bytes,sum(map(len,self.data.values())))
    def effect(self,label,action,key=None,value=None):
        old_data=copy.deepcopy(self.data);old_versions=copy.deepcopy(self.versions)
        before=len(self.steps)
        try:self.point('before-'+label,key,value)
        except q.Interrupted:
            if self.fault==(before,'ambiguous-persistent'):action()
            raise
        action();after=len(self.steps)
        try:self.point('after-'+label,key,value)
        except q.Interrupted:
            if self.fault==(after,'ambiguous-not-persistent'):
                self.data=old_data;self.versions=old_versions
            raise
    def erase(self,key):
        self.effect('erase-'+key,lambda:self.data.pop(key,None))
    def cas(self,expected,new):
        # Software serialized expected comparison, NOT a native NVS transaction.
        self.point('before-software-CAS-read')
        if not self.laboratory or self.data.get('root')!=expected:raise p.Refused('unqualified/stale CAS')
        ng=q.read_root(new)[0]
        if expected is not None and ng!=q.read_root(expected)[0]+1:raise p.Refused('nonmonotonic generation')
        self.point('after-software-CAS-compare')
        self.write('root',new);self.commit('root')
        if self.read('root')!=new:raise p.Refused('root readback')
        self.acknowledged.append(new)
        self.point('after-software-CAS-ack')
    def head(self):
        if not self.laboratory:raise p.Refused('G1 missing: concrete backend GLOBAL_BLOCK')
        return self.read('root')
    def gc(self):
        # FREEING -> destination copies -> erase old page. Logical relocation
        # conservative model only; real bitmap/flash timing NOT qualified.
        self.effect('GC-FREEING',lambda:setattr(self,'gc_copy',{}))
        for key,value in sorted(self.data.items()):
            self.effect('GC-copy-'+key,lambda:self.gc_copy.__setitem__(key,value),key,value)
        def erase_page():self.data=dict(self.gc_copy);self.versions={}
        self.effect('GC-page-erase',erase_page)

def provision(state,laboratory=False):
    f=Candidate(laboratory)
    wire=q.node('BASE',state,1,1,q.ZERO)
    f.data={q.key(0,0):wire,'root':q.root(1,1,1,0,0,q.digest(wire))}
    return f

def admission():
    after=p.fixture()
    for c in after['conflicts']:
        for b in c['participants']:b['resync_observation']['value']['pending_request_id']=None
    before=copy.deepcopy(after);before['effects']=[]
    before['allocators']['request_lifetime']=before['allocators']['ticket_watermark']=0
    p.validate(before);return before,after

def entries(n,chunk=4000):return math.ceil(n/32)+math.ceil(n/chunk)+1
def capacity(occupancy=108,stranded=0,root_extra=0,chunk=4000):
    nodes={'old_new_BASE':2*entries(6447,chunk),'two_PROOF':2*entries(1146,chunk),'two_ALLOC':2*entries(162,chunk),'old_new_root':2*entries(76,chunk)}
    external=2*entries(occupancy,chunk) if occupancy else 0
    peak=sum(nodes.values())+12+126+external+stranded+root_extra
    return dict(payload_encoded_max=6383,payload_encoded_pending=5587,resolution_growth=796,
        peak_live_record_reserve_bytes=15510,logical_two_root_bytes=152,
        peak_logical_including_roots_bytes=15662,existing_remote_wifi_encoded_bound=108,
        occupancy_assumed_bytes=occupancy,old_new_external_entries=external,
        physical_record_entries=nodes,metadata_entries=12,gc_reserved_entries=126,
        stranded_fragmentation_entries=stranded,unknown_root_service_extra_entries=root_extra,
        partition_bytes=24576,physical_pool_entries=756,entry_size=32,
        physical_peak_entries_lower_bound=peak,physical_entry_bytes=peak*32,
        page_header_bitmap_bytes=6*64,physical_accounted_bytes=peak*32+6*64,
        optimistic_fit=peak<=756,margin_entries=756-peak,chunk_tailroom_assumption=chunk,
        verdict='CONDITIONAL' if peak<=756 else 'FAIL',actual_physical_free='UNKNOWN')
