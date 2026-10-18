"""Read-only performance probes of an already built Windows x64 VST3. No compilation.

Uses the checked-in SDK's public ABI. Timings include Python/ctypes call overhead;
they are comparative diagnostics, not a DAW realtime certification.
"""
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import statistics
import struct
import sys
import time
import uuid

P = C.c_void_p
I = C.c_int32
U = C.c_uint32
CALL = C.WINFUNCTYPE

def method(obj, slot, result=I, args=()):
    address = C.cast(obj, C.POINTER(C.POINTER(P))).contents[slot]
    return CALL(result, P, *args)(address)

def iid(text):
    return C.create_string_buffer(uuid.UUID(text).bytes_le, 16)

def ok(result):
    if result != 0:
        raise RuntimeError(f"VST3 result {result:#x}")

class Interface:
    def __init__(self, methods):
        self.callbacks = [CALL(result, P, *args)(fn) for result, args, fn in methods]
        self.table = (P * len(methods))(*[C.cast(f, P).value for f in self.callbacks])
        self.obj = (P * 1)(C.addressof(self.table))
        self.ptr = C.addressof(self.obj)

unknown = [(I, (P, P), lambda *_: -2147467262),
           (U, (), lambda *_: 1), (U, (), lambda *_: 1)]

class Stream(Interface):
    def __init__(self, data):
        self.data, self.pos = data, 0
        super().__init__(unknown + [(I, (P, I, C.POINTER(I)), self.read)])
    def read(self, _, dst, count, actual):
        b = self.data[self.pos:self.pos + count]
        C.memmove(dst, b, len(b))
        self.pos += len(b)
        if actual: actual[0] = len(b)
        return 0

class ClassInfo(C.Structure):
    _fields_ = [('cid', C.c_char * 16), ('cardinality', I),
                ('category', C.c_char * 32), ('name', C.c_char * 64)]

class Setup(C.Structure):
    _fields_ = [('mode', I), ('format', I), ('maximum', I), ('rate', C.c_double)]

class Bus(C.Structure):
    _fields_ = [('channels', I), ('silent', C.c_uint64), ('buffers', P)]

class Data(C.Structure):
    _fields_ = [(n, I) for n in ('mode','format','samples','inputCount','outputCount')] + [
        (n, P) for n in ('inputs','outputs','changes','outChanges','events','outEvents','context')]

class NoteOn(C.Structure):
    _fields_ = [('channel', C.c_int16), ('pitch', C.c_int16), ('tuning', C.c_float),
                ('velocity', C.c_float), ('length', I), ('noteID', I)]

class Event(C.Structure):
    _fields_ = [('bus', I), ('offset', I), ('ppq', C.c_double),
                ('flags', C.c_uint16), ('kind', C.c_uint16), ('padding', I),
                ('note', NoteOn), ('tail', C.c_ubyte * 4)]

class Events(Interface):
    def __init__(self):
        self.event = Event()
        self.event.note = NoteOn(0, 60, 0, 1, 0, -1)
        super().__init__(unknown + [(I, (), lambda *_: 1),
                                   (I, (I, P), self.get)])
    def get(self, _, index, output):
        C.memmove(output, C.byref(self.event), C.sizeof(self.event))
        return 0

def defaults():
    s = [0.] * 32
    for i in (1, 3, 7, 23, 30): s[i] = .5
    for i in (11, 17, 21, 29): s[i] = 1.
    return s

def main():
    binary = Path(sys.argv[1]).resolve()
    lib = C.WinDLL(str(binary))
    if hasattr(lib, 'InitDll'):
        lib.InitDll.restype = C.c_bool
        assert lib.InitDll()
    lib.GetPluginFactory.restype = P
    factory = lib.GetPluginFactory()
    info = ClassInfo()
    ok(method(factory, 5, args=(I, P))(factory, 0, C.byref(info)))
    component = P()
    ok(method(factory, 6, args=(P, P, C.POINTER(P)))(factory, C.byref(info),
       iid('E831FF31-F2D5-4301-928E-BBEE25697802'), C.byref(component)))
    ok(method(component, 3, args=(P,))(component, None))
    processor = P()
    ok(method(component, 0, args=(P, C.POINTER(P)))(component,
       iid('42043F99-B7DA-453C-A569-E79D9AAEC33D'), C.byref(processor)))
    process = method(processor, 9, args=(P,))
    state = method(component, 12, args=(P,))
    setup = method(processor, 7, args=(P,))
    active = method(component, 11, args=(C.c_uint8,))
    processing = method(processor, 8, args=(C.c_uint8,))
    method(component, 10, args=(I, I, I, C.c_uint8))(component, 0, 1, 0, 1)
    method(component, 10, args=(I, I, I, C.c_uint8))(component, 1, 0, 0, 1)
    rows = []
    for rate in (44100, 48000, 88200, 96000):
        for block in (64, 512, 2048):
            for scenario in ('idle', 'held_note', 'zero_sustain'):
                s = defaults()
                if scenario == 'zero_sustain': s[11] = s[17] = 0.
                stream = Stream(b'XSV3\x01\0\0\0' + struct.pack('<32d', *s))
                ok(state(component, stream.ptr))
                config = Setup(0, 0, block, rate)
                ok(setup(processor, C.byref(config)))
                ok(active(component, 1))
                ok(processing(processor, 1))
                left, right = (C.c_float * block)(), (C.c_float * block)()
                buffers = (P * 2)(C.addressof(left), C.addressof(right))
                bus = Bus(2, 0, C.addressof(buffers))
                data = Data(0, 0, block, 0, 1, None, C.addressof(bus))
                events = Events()
                if scenario != 'idle': data.events = events.ptr
                ok(process(processor, C.byref(data)))
                data.events = None
                # Warm up past a full second, including decay into subnormals.
                for _ in range(math.ceil(rate / block)):
                    ok(process(processor, C.byref(data)))
                loops = math.ceil(rate * 2 / block)
                samples = []
                for repeat in range(3):
                    start = time.perf_counter()
                    for _ in range(loops): process(processor, C.byref(data))
                    samples.append(time.perf_counter() - start)
                duration = loops * block / rate
                assert all(math.isfinite(v) and abs(v) <= 1 for v in left)
                if scenario == 'held_note': assert max(abs(v) for v in left) > .001
                rows.append(dict(rate=rate, block=block, scenario=scenario,
                                 median_ms_per_audio_second=statistics.median(samples)*1000/duration,
                                 max_final_amplitude=max(abs(v) for v in left)))
                processing(processor, 0)
                active(component, 0)
    method(processor, 2, U)(processor)
    method(component, 4)(component)
    method(component, 2, U)(component)
    method(factory, 2, U)(factory)
    if hasattr(lib, 'ExitDll'): lib.ExitDll()
    result = dict(binary=str(binary), sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  method='Native VST3 process through ctypes; median of three two-second renders; includes FFI overhead; no DAW scheduling',
                  rows=rows)
    print(json.dumps(result, indent=2))

if __name__ == '__main__': main()
