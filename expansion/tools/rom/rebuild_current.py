"""Verify/rebuild the current bank from baseline36 and original formulas.
Writes only to the requested output directory. Python 3 and NumPy required.
"""
from pathlib import Path
import argparse, hashlib, json, struct, zlib
import numpy as np
from original_expansion import source_table
from prepare_bank import prepared

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=Path(__file__).resolve().parents[2]
    raw=(root/'factory/rom/baseline36.xrom').read_bytes()
    recipes=json.loads((root/'factory/rom/recipes/tables.json').read_text())['recipes']
    tables=[];payloads=[]
    for r in recipes:
        i=r['id']
        if i<36:
            identity,family,_,frames,n,rootbin,_,offset,size,_,_,_=struct.unpack_from('<HBBHHHHIIIII',raw,64+i*32)
            assert identity==i
            payload=raw[offset:offset+size]
            pcm=np.frombuffer(payload,dtype=np.int8).reshape(frames,n).astype('<f4')/128
        else:
            n=r['cycle_length'];rootbin=r['rootBin'];family=18+['CORE MOTION','MODULATION','SPECTRAL MOTION','DIGITAL TONES','METAL & KEYS','HARMONY','SFX'].index(r['family'])
            # Family identities start at 19 in the approved bank.
            family+=1
            a=source_table(r['parameters']['folder'],r['parameters']['voice'],n)
            payload=np.rint(a*128).astype(np.int8).tobytes()
            pcm=np.frombuffer(payload,dtype=np.int8).reshape(16,n).astype('<f4')/128
        tables.append(dict(id=i,name=('PAD ' if r['family']=='PAD' else '')+r['label'],familyId=family,rootBin=rootbin,pcm=pcm));payloads.append(payload)
    offset=64+32*len(tables);directory=[]
    for t,payload in zip(tables,payloads):
        frames,n=t['pcm'].shape
        directory.append(struct.pack('<HBBHHHHIIIII',t['id'],t['familyId'],0,frames,n,t['rootBin'],0,offset,len(payload),t['id'],zlib.crc32(payload),0));offset+=len(payload)
    tail=b''.join(directory+payloads)
    count=len(tables)
    bank=struct.pack('<4sHH6I',b'XROM',2,64,count,64,64+32*count,64+len(tail),2,0)+hashlib.sha256(tail).digest()+tail
    expected=(root/'factory/rom/factory.xrom').read_bytes()
    assert bank==expected,'Generated bank differs from the checked-in bank'
    data=prepared(tables,hashlib.sha256(bank).digest())
    embedded=bytes(int(x) for line in (root/'assets/PreparedFactory.inc').read_text().splitlines() if not line.startswith('//') for x in line.split(',') if x.strip())
    if data!=embedded:
        at=next((i for i,(a,b) in enumerate(zip(data,embedded)) if a!=b),-1)
        raise AssertionError(f'Prepared playback data differs at {at}: {data[at:at+48]!r} / {embedded[at:at+48]!r}')
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'factory.xrom').write_bytes(bank)
    (args.output/'prepared.p8rd').write_bytes(data)
    print(f'Verified {count} tables and exact prepared playback data; SHA256 '+hashlib.sha256(bank).hexdigest())

if __name__=='__main__':main()
