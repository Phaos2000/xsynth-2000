"""Inspect resize and controller-state behavior of the existing binary; no build."""
import ctypes as C
import json
from pathlib import Path
import struct
import sys
from audit_runtime import P, I, U, CALL, Interface, unknown, Stream, ClassInfo, method, iid, ok

class Rect(C.Structure):
    _fields_ = [(n, I) for n in ('left', 'top', 'right', 'bottom')]

class SaveStream(Interface):
    def __init__(self):
        self.data = b''
        super().__init__(unknown + [(I, (P, I, C.POINTER(I)), lambda *_: 1),
                                   (I, (P, I, C.POINTER(I)), self.write)])
    def write(self, _, src, count, actual):
        self.data += C.string_at(src, count)
        if actual: actual[0] = count
        return 0

lib = C.WinDLL(str(Path(sys.argv[1]).resolve()))
lib.GetPluginFactory.restype = P
factory = lib.GetPluginFactory()
info = ClassInfo()
ok(method(factory, 5, args=(I,P))(factory, 1, C.byref(info)))

def create():
    controller = P()
    ok(method(factory, 6, args=(P,P,C.POINTER(P)))(factory, C.byref(info),
       iid('DCD7BBE3-7742-448D-A874-AACC979C759E'), C.byref(controller)))
    ok(method(controller, 3, args=(P,))(controller, None))
    view = method(controller, 17, P, (C.c_char_p,))(controller, b'editor')
    if not view: raise RuntimeError('No view')
    scale = P()
    ok(method(view, 0, args=(P,C.POINTER(P)))(view,
       iid('65ED9690-8AC4-4525-8AADEF7A72EA703F'), C.byref(scale)))
    return controller, view, scale

def size(view):
    r = Rect()
    ok(method(view, 9, args=(P,))(view, C.byref(r)))
    return [r.right-r.left, r.bottom-r.top]

def close(controller, view, scale):
    method(scale, 2, U)(scale)
    method(view, 2, U)(view)
    method(controller, 4)(controller)
    method(controller, 2, U)(controller)

c,v,s = create()
rows = []
for factor in (1.,1.25,1.5,2.,1.):
    ok(method(s, 3, args=(C.c_float,))(s, factor))
    rows.append(dict(hostDPI=factor, actual=size(v)))
requested = Rect(0,0,825,946)
ok(method(v, 14, args=(P,))(v,C.byref(requested)))
ok(method(v, 10, args=(P,))(v,C.byref(requested)))
saved = SaveStream()
ok(method(c, 7, args=(P,))(c,saved.ptr))
dragged = size(v)
ok(method(s, 3, args=(C.c_float,))(s,1.5))
afterDPI = size(v)
close(c,v,s)
c,v,s = create()
restore = Stream(saved.data)
ok(method(c, 6, args=(P,))(c,restore.ptr))
restored = size(v)
ok(method(s, 3, args=(C.c_float,))(s,1.5))
restoredDPI = size(v)
close(c,v,s)
method(factory, 2, U)(factory)
print(json.dumps(dict(scales=rows,dragged=dragged,afterDPI=afterDPI,
    serializedLogicalWidth=struct.unpack('<d',saved.data[8:])[0],
    restored=restored,restoredDPI=restoredDPI),indent=2))
