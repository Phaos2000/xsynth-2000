"""Reproduce continuous-automation point handling in the existing binary."""
from audit_runtime import *

class Queue(Interface):
    def __init__(self):
        super().__init__(unknown + [(U, (), lambda *_: 23), (I, (), lambda *_: 1),
            (I, (I, C.POINTER(I), C.POINTER(C.c_double)), self.point)])
    def point(self, _, index, offset, value):
        offset[0], value[0] = 511, 0.
        return 0

class Changes(Interface):
    def __init__(self):
        self.queue = Queue()
        super().__init__(unknown + [(I, (), lambda *_: 1),
                                   (P, (I,), lambda *_: self.queue.ptr)])

lib=C.WinDLL(str(Path(sys.argv[1]).resolve()))
lib.GetPluginFactory.restype=P
f=lib.GetPluginFactory()
ci=ClassInfo()
ok(method(f,5,args=(I,P))(f,0,C.byref(ci)))
c=P()
ok(method(f,6,args=(P,P,C.POINTER(P)))(f,C.byref(ci),iid('E831FF31-F2D5-4301-928E-BBEE25697802'),C.byref(c)))
ok(method(c,3,args=(P,))(c,None))
p=P()
ok(method(c,0,args=(P,C.POINTER(P)))(c,iid('42043F99-B7DA-453C-A569-E79D9AAEC33D'),C.byref(p)))
process=method(p,9,args=(P,))
config=Setup(0,0,512,48000)
ok(method(p,7,args=(P,))(p,C.byref(config)))
results=[]
for automate in (False,True):
    ok(method(c,11,args=(C.c_uint8,))(c,1))
    stream=Stream(b'XSV3\x01\0\0\0'+struct.pack('<32d',*defaults()))
    ok(method(c,12,args=(P,))(c,stream.ptr))
    left,right=(C.c_float*512)(),(C.c_float*512)()
    buffers=(P*2)(C.addressof(left),C.addressof(right))
    bus=Bus(2,0,C.addressof(buffers))
    data=Data(0,0,512,0,1,None,C.addressof(bus))
    events=Events()
    data.events=events.ptr
    ok(process(p,C.byref(data)))
    data.events=None
    changes=Changes()
    if automate: data.changes=changes.ptr
    ok(process(p,C.byref(data)))
    results.append(list(left))
    method(c,11,args=(C.c_uint8,))(c,0)
baseline,automated=results
print(json.dumps(dict(parameter='output volume',queue='Single point: normalized 0 at sample 511; preceding value 0.5',
    first_511_samples_identical_to_unautomated=baseline[:511]==automated[:511],
    final_unautomated=baseline[511],final_automated=automated[511],
    implication='Holds the old value then jumps at the point; does not reconstruct the SDK continuous ramp.'),indent=2))
method(p,2,U)(p)
method(c,4)(c)
method(c,2,U)(c)
method(f,2,U)(f)
