"""Compare two complete clean local generations; stage approved local source assets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from generate import ROOT,hashes,dump
from report import report


def main():
    p=argparse.ArgumentParser();p.add_argument('first',type=Path);p.add_argument('second',type=Path)
    p.add_argument('--stage-source-assets',action='store_true')
    args=p.parse_args()
    report(args.first);report(args.second)
    ignored={'DETERMINISM.json','SHA256SUMS','reports/TEST-RESULTS.txt'}
    a={k:v for k,v in hashes(args.first).items() if k not in ignored}
    b={k:v for k,v in hashes(args.second).items() if k not in ignored}
    mismatch=sorted(k for k in a.keys()|b.keys() if a.get(k)!=b.get(k))
    if mismatch:raise RuntimeError('Determinism differences: '+repr(mismatch))
    result=dict(clean_generations=2,exact_byte_equality=True,compared_files=len(a),
                wav_files=sum(k.endswith('.wav') for k in a),sha256=a,
                scope='Full local generation; no resume cache; timing-bearing execution logs excluded')
    dump(args.first/'DETERMINISM.json',result)
    sums=''.join(v+'  '+k+'\n' for k,v in sorted(a.items()))
    (args.first/'SHA256SUMS').write_text(sums,encoding='utf-8')
    if args.stage_source_assets:
        dest=ROOT/'factory/rom';dest.mkdir(exist_ok=True)
        for name in ('factory.xrom','manifest.json','decoded-s16le.bin'):
            shutil.copyfile(args.first/'data'/name,dest/name)
        generated=ROOT/'port/generated';generated.mkdir(exist_ok=True)
        shutil.copyfile(args.first/'data/FactoryRom.h',generated/'FactoryRom.h')
        entries={name:hashlib.sha256((dest/name).read_bytes()).hexdigest()
                 for name in ('factory.xrom','manifest.json','decoded-s16le.bin','recipes/tables.json')}
        (dest/'SHA256SUMS').write_text(''.join(v+'  '+k+'\n' for k,v in sorted(entries.items())),encoding='utf-8')
    print(f'PASS: {len(a)} generated files identical; {result["wav_files"]} WAVs; all categories compared')


if __name__=='__main__':main()
