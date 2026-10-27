"""Local factory XROM-D1 generation. No production plugin imports or integration."""
from __future__ import annotations
import os
os.environ['OPENBLAS_NUM_THREADS']='1'
os.environ['OMP_NUM_THREADS']='1'
import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
import csv
import hashlib
import json
from pathlib import Path
import struct
import sys
import wave
import zlib
import numpy as np
from codec_prototype import encode, decode, nearest_div
from recipes import table_sources

ROOT=Path(__file__).resolve().parents[2]
RECIPE_PATH=ROOT/'factory/rom/recipes/tables.json'
RATE=48000
PITCH=130.8127826502993
AUDITION_GAIN=0.7


def dump(path, obj):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(obj,sort_keys=True,indent=2,allow_nan=False)+'\n',encoding='utf-8')


def csvout(path, rows):
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)


def build_table(r):
    source=table_sources(r);frames=[];stats=[];payloads=[]
    for j,row in enumerate(source):
        target=[nearest_div(v,1<<16) for v in row]
        # Reuse approved codec/search; widen only on explicit quality failure.
        search_policy='prototype'
        for width in (64,256,1024):
            try:
                payload=encode(target,1 if r['codec_profile']=='EDGE' else 4,width)
            except RuntimeError:
                # Closing-block lattice must divide the accumulated residue.
                # Constrain the prefix lattice only when unconstrained pruning fails.
                differences=[abs(target[k]-target[k-1]) for k in range(len(target)-16,len(target))]
                e=max(0,int(np.ceil(np.log2(max(1,max(differences))/120))))
                e=min(9,e)
                payload=encode(target,1 if r['codec_profile']=='EDGE' else 4,width,e,e)
                search_policy=f'terminal-lattice-{e}'
            decoded,info=decode(payload,r['cycle_length'])
            ref=np.array(row,dtype=np.float64)/(1<<31)
            values=np.array(decoded,dtype=np.float64)/32768
            err=values-ref
            rms=float(np.sqrt(np.mean(err*err)))
            if rms < .03 and np.sqrt(np.mean(values*values)) > .05*np.sqrt(np.mean(ref*ref)):break
        else:raise ValueError(f"Codec quality failure {r['id']} frame {j}")
        if info['closure_error_lsb'] or info['saturation']:raise ValueError('Invalid factory encode')
        stats.append(dict(id=r['id'],frame=j,rms_error=rms,max_error=float(np.max(np.abs(err))),
                          dc=float(np.mean(values)),peak=float(np.max(np.abs(values))),
                          search_width=width,search_policy=search_policy,**info))
        frames.append(decoded);payloads.append(payload)
    return dict(recipe=r,source=source,frames=frames,stats=stats,payload=b''.join(payloads))


def bank_bytes(tables):
    families=list(dict.fromkeys(t['recipe']['family'] for t in tables))
    offset=64+32*len(tables);directory=bytearray();payload=bytearray()
    for t in tables:
        r=t['recipe'];b=t['payload']
        directory.extend(struct.pack('<HBBHHBBHIIIII',r['id'],families.index(r['family']),0,
            16,r['cycle_length'],16,4,r['rootBin'],offset,len(b),r['id'],zlib.crc32(b),0))
        payload.extend(b);offset+=len(b)
    tail=bytes(directory+payload)
    return struct.pack('<4sHH6I',b'XROM',1,64,len(tables),64,64+32*len(tables),64+len(tail),1,0)+hashlib.sha256(tail).digest()+tail


def read_bank(data):
    if len(data)<64:raise ValueError('Truncated bank')
    magic,version,header,count,directory,start,total,schema,flags=struct.unpack_from('<4sHH6I',data)
    if (magic,version,header,directory,start,total,schema,flags)!=(b'XROM',1,64,64,64+32*count,len(data),1,0):
        raise ValueError('Bank header mismatch')
    if hashlib.sha256(data[64:]).digest()!=data[32:64]:raise ValueError('Bank checksum mismatch')
    tables=[];seen=set();expected=start
    for i in range(count):
        id,family,fl,frames,n,block,bits,root,offset,length,recipe,crc,res=struct.unpack_from('<HBBHHBBHIIIII',data,64+32*i)
        size=2+n//32+n
        if id in seen or (fl,frames,block,bits,res)!=(0,16,16,4,0) or n not in (32,64,128,256) or not 1<=root<n//2:
            raise ValueError('Invalid descriptor')
        if recipe!=id or offset!=expected or length!=16*size or offset+length>len(data):raise ValueError('Invalid range')
        payload=data[offset:offset+length]
        if zlib.crc32(payload)!=crc:raise ValueError('Payload CRC mismatch')
        decoded=[]
        for j in range(frames):
            row,info=decode(payload[j*size:(j+1)*size],n)
            if info['saturation'] or info['closure_error_lsb']:raise ValueError('Invalid frame')
            decoded.append(row)
        tables.append(dict(id=id,rootBin=root,cycle_length=n,frames=decoded));seen.add(id);expected+=length
    if expected!=len(data):raise ValueError('Trailing bank bytes')
    return tables


def sample_audio(frames,root,mode,seconds=6,position=None):
    a=np.asarray(frames,dtype=np.float64)/32768
    size=int(RATE*seconds);time=np.arange(size,dtype=np.float64)
    phase=np.remainder(time*(PITCH/(RATE*root)),1)
    index=phase*a.shape[1];ix=np.floor(index).astype(np.int64);frac=index-ix
    pos=np.linspace(0,15,size) if position is None else np.full(size,float(position))
    left=np.floor(pos).astype(np.int64);right=np.minimum(left+1,15);blend=pos-left
    low=a[left,ix];high=a[right,ix]
    if mode=='linear':
        low+=(a[left,(ix+1)%a.shape[1]]-low)*frac
        high+=(a[right,(ix+1)%a.shape[1]]-high)*frac
    result=((1-blend)*low+blend*high)*AUDITION_GAIN
    if not np.isfinite(result).all() or np.max(np.abs(result))>=1:raise ValueError('Audition nonfinite/clipping')
    return result


def pcm24(values):
    ints=np.rint(values*(1<<23)).astype(np.int32)
    raw=np.empty((len(ints),3),dtype=np.uint8)
    for b in range(3):raw[:,b]=(ints>>(8*b))&255
    return raw.tobytes()


def write_wav(path,values):
    path.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1);f.setsampwidth(3);f.setframerate(RATE);f.writeframes(pcm24(values))


def slug(r):
    prefix='pad_' if r['family']=='PAD' else ''
    return f"{r['id']:03}_"+prefix+r['label'].lower().replace(' ','')


def audition(out,tables):
    audio=out/'audio';rows=[];static_index=[];chords=[]
    for mode in ('hold','linear'):
        reel=[];sample=0
        for t in tables:
            r=t['recipe'];values=sample_audio(t['frames'],r['rootBin'],mode)
            filename=slug(r)+f'_{mode}_morph.wav';write_wav(audio/filename,values)
            rows.append(dict(mode=mode,id=r['id'],family=r['family'],table=r['label'],start_seconds=sample/RATE,
                             duration_seconds=6,peak=float(np.max(np.abs(values))),file=filename))
            reel.extend((values,np.zeros(int(.2*RATE))));sample+=len(values)+int(.2*RATE)
        write_wav(audio/'reels'/f'factory_rom_{mode}_morph_reel.wav',np.concatenate(reel))
    for t in tables:
        r=t['recipe'];pieces=[];start=0
        for frame in (0,5,10,15):
            values=sample_audio(t['frames'],r['rootBin'],'hold',1,frame)
            pieces.extend((values,np.zeros(6000)))
            static_index.append(dict(id=r['id'],table=r['label'],frame=frame,start_seconds=start/RATE,
                                     duration_seconds=1,file=slug(r)+'_static_frames.wav'))
            start+=54000
        write_wav(audio/'static'/ (slug(r)+'_static_frames.wav'),np.concatenate(pieces))
        if r['operation']=='chord':
            write_wav(audio/'chords'/(slug(r)+'_frame07_C3.wav'),sample_audio(t['frames'],r['rootBin'],'hold',3,7))
            for b in r['parameters']['bins']:
                ratio=b/r['rootBin'];semitones=round(12*np.log2(ratio))
                chords.append(dict(id=r['id'],table=r['label'],frame=7,rootBin=r['rootBin'],bin=b,
                    ratio=f"{b}/{r['rootBin']}",frequency_hz=PITCH*ratio,
                    cents_from_12tet=float(1200*np.log2(ratio)-100*semitones)))
    csvout(audio/'reels/index.csv',rows);csvout(audio/'static/index.csv',static_index)
    csvout(audio/'chords/components.csv',chords)


def header_file(tables):
    lines=['// Generated exclusively by decoding factory.xrom. UNREFERENCED development asset.',
           '// Factory data licensing pending approval; see docs/ROM-PROVENANCE.md.',
           '#pragma once','#include <cstdint>','namespace xs::rom_generated {',
           'struct Table {uint16_t id,length,frames,rootBin;uint32_t offset;};',
           'inline constexpr int16_t Samples[] = {']
    offset=0;descriptors=[]
    for t in tables:
        r=t['recipe'];descriptors.append(f"{{{r['id']},{r['cycle_length']},16,{r['rootBin']},{offset}}}")
        for row in t['frames']:
            lines.append(','.join(map(str,row))+',');offset+=len(row)
    lines+=['};','inline constexpr Table Tables[] = {'+','.join(descriptors)+'};','}']
    return '\n'.join(lines)+'\n'


def hashes(path):
    return {p.relative_to(path).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(path.rglob('*')) if p.is_file()}


def generate(out,workers):
    if out.exists() and any(out.iterdir()):raise ValueError('Use a CLEAN output directory')
    out.mkdir(parents=True,exist_ok=True)
    recipes=json.loads(RECIPE_PATH.read_text(encoding='utf-8'))['recipes']
    if [r['id'] for r in recipes]!=list(range(35)):raise ValueError('Stable inventory mismatch')
    tables={}
    cache=ROOT/'artifacts/factory-rom-work-cache';cache.mkdir(exist_ok=True)
    implementation=hashlib.sha256(b''.join((Path(__file__).parent/p).read_bytes() for p in ('recipes.py','codec_prototype.py'))).hexdigest()
    with ProcessPoolExecutor(max_workers=workers) as pool:
        futures={}
        for r in recipes:
            key=hashlib.sha256((implementation+json.dumps(r,sort_keys=True)).encode()).hexdigest()
            cached=cache/(key+'.json')
            if '--resume-work-cache' in sys.argv and cached.exists():
                t=json.loads(cached.read_text());t['payload']=bytes.fromhex(t['payload']);tables[r['id']]=t
            else:futures[pool.submit(build_table,r)]=(r['id'],cached)
        for job in as_completed(futures):
            t=job.result();tables[t['recipe']['id']]=t
            dump(futures[job][1],{**t,'payload':t['payload'].hex()})
            print(f"Encoded {len(tables)}/35: {t['recipe']['id']:02} {t['recipe']['label']}",flush=True)
    tables=[tables[i] for i in range(35)]
    data=out/'data';data.mkdir()
    bank=bank_bytes(tables);(data/'factory.xrom').write_bytes(bank)
    canonical=read_bank((data/'factory.xrom').read_bytes())
    for t,c in zip(tables,canonical):
        assert t['frames']==c['frames'];t['frames']=c['frames']
    source=b''.join(struct.pack('<'+'i'*len(row),*row) for t in tables for row in t['source'])
    decoded=b''.join(struct.pack('<'+'h'*len(row),*row) for t in tables for row in t['frames'])
    (data/'recipe-masters-q31le.bin').write_bytes(source)
    (data/'decoded-s16le.bin').write_bytes(decoded)
    (data/'FactoryRom.h').write_text(header_file(tables),encoding='utf-8')
    dump(data/'decoded.json',[dict(recipe=t['recipe'],frames=t['frames']) for t in tables])
    dump(data/'frame-codec-statistics.json',[s for t in tables for s in t['stats']])
    dump(data/'manifest.json',dict(schema=1,codec='XROM-D1',format_version=1,generator_schema=1,
         frames=560,tables=recipes,recipe_sha256=hashlib.sha256(RECIPE_PATH.read_bytes()).hexdigest(),
         encoded_sha256=hashlib.sha256(bank).hexdigest(),decoded_sha256=hashlib.sha256(decoded).hexdigest(),
         generator_files_sha256={p:hashlib.sha256((Path(__file__).parent/p).read_bytes()).hexdigest()
              for p in ('generate.py','recipes.py','codec_prototype.py','analyze.py')},
         numpy_version=np.__version__,
         factory_data_license='PENDING USER APPROVAL - NOT FOR PUBLICATION'))
    print('Decoded bank verified; rendering 70 sweeps, static frames, reels and chord clips',flush=True)
    audition(out,tables)
    from analyze import analyze
    analyze(out,tables)
    print('Generation complete',flush=True)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,required=True);ap.add_argument('--workers',type=int,default=4)
    ap.add_argument('--resume-work-cache',action='store_true',help='Development iteration only; do not use for clean determinism runs')
    args=ap.parse_args();generate(args.output,args.workers)


if __name__=='__main__':main()
