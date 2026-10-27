"""Focused ROM-only checks. Run after generation; no VST tests or build."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import unittest
import wave
import numpy as np
from codec_prototype import encode,decode,sources
from generate import ROOT,read_bank,sample_audio
from recipes import table_sources
from analyze import exact_signature

OUT=ROOT/'artifacts/factory-rom-review'
EXPECTED_N=[128,128,64,256,64,32,128,128,128,128,128,128,128,256,128,256,64,128,
            256,256,256,128,64,256,128,256,256,256,128,128,256,256,256,256,256]
EXPECTED_ROOT=[1]*29+[2,8,6,8,10,4]


class RomTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bank=(OUT/'data/factory.xrom').read_bytes()
        cls.tables=read_bank(cls.bank)
        cls.recipes=json.loads((ROOT/'factory/rom/recipes/tables.json').read_text())['recipes']

    def test_manifest_inventory(self):
        m=json.loads((OUT/'data/manifest.json').read_text())
        self.assertEqual([r['id'] for r in m['tables']],list(range(35)))
        self.assertEqual([r['cycle_length'] for r in m['tables']],EXPECTED_N)
        self.assertEqual([r['rootBin'] for r in m['tables']],EXPECTED_ROOT)
        self.assertTrue(all(len(t['frames'])==16 for t in self.tables))
        self.assertEqual(len(self.bank),102096)
        self.assertEqual(m['encoded_sha256'],hashlib.sha256(self.bank).hexdigest())

    def test_codec_goldens_unchanged(self):
        expected=['60fd61c74dcce10cc5bf7c7867518ef12cf6217d517a8e36d3b6e41164cc1554',
                  'e660ab804964c31ede8e2102dc8f998bfdad1f618fc69df4865d3d6cb6f7e654',
                  '44e2881d619f8b9f9815488d9c6d341ba913bacc2e504ae47ea41ae83b73023c']
        for (name,row),digest in zip(sources().items(),expected):
            target=[int((v*32768).to_integral_value(rounding='ROUND_HALF_EVEN')) for v in row]
            self.assertEqual(hashlib.sha256(encode(target,4 if name=='smooth' else 1)).hexdigest(),digest)

    def test_every_frame_codec_and_dc(self):
        stats=json.loads((OUT/'data/frame-codec-statistics.json').read_text())
        self.assertEqual(len(stats),560)
        self.assertTrue(all(s['closure_error_lsb']==0 and s['saturation']==0 for s in stats))
        self.assertTrue(all(0<=e<=9 for s in stats for e in s['exponents']))
        for t in self.tables:
            for row in t['frames']:
                self.assertLessEqual(abs(sum(row)/len(row)),.5)
                self.assertGreater(np.sqrt(np.mean(np.asarray(row,dtype=float)**2)),20)

    def test_exact_duplicate_gate(self):
        seen={}
        for t in self.tables:
            for j,row in enumerate(t['frames']):
                sig=exact_signature(row)
                self.assertNotIn(sig,seen,f"Duplicate {t['id']}/{j} vs {seen.get(sig)}")
                seen[sig]=(t['id'],j)
        self.assertEqual(json.loads((OUT/'reports/exact-duplicates.json').read_text()),[])

    def test_duplicate_detector(self):
        a=[1,8,-3,4,5,-9,0,2]
        signature=exact_signature(a)
        for v in (a[3:]+a[:3],[-x for x in a],a*4,[x*7+11 for x in a]):
            self.assertEqual(signature,exact_signature(v))
        self.assertNotEqual(signature,exact_signature(a[::-1]))

    def test_recipe_determinism(self):
        # Full-bank, two-clean-run verification is separate; this exercises math directly.
        for index in (2,5,29):
            a=table_sources(self.recipes[index]);b=table_sources(self.recipes[index])
            self.assertEqual(a,b)
            self.assertLessEqual(max(abs(x) for row in a for x in row),round(28000/32768*(1<<31)))

    def test_rational_chord_bins(self):
        for r in self.recipes[29:]:
            self.assertTrue(all(0<b<r['cycle_length']//2 for b in r['parameters']['bins']))
            self.assertEqual(r['parameters']['bins'][0],r['rootBin'])
            master=np.array(table_sources(r),dtype=float)/(1<<31)
            actual=np.array(self.tables[r['id']]['frames'],dtype=float)/32768
            allowed=sorted({b*h for b in r['parameters']['bins'] for h in range(1,r['parameters']['overtones']+1)
                            if b*h<r['cycle_length']//2})
            for original,decoded in zip(master,actual):
                spectrum=np.abs(np.fft.rfft(original))**2
                self.assertGreater(spectrum[allowed].sum()/spectrum[1:].sum(),1-1e-12)
                spectrum=np.abs(np.fft.rfft(decoded))**2
                self.assertGreater(spectrum[allowed].sum()/spectrum[1:].sum(),.995)
                for b in r['parameters']['bins']:self.assertGreater(spectrum[b]/spectrum[1:].sum(),1e-5)

    def test_render_modes_and_finite(self):
        for t in self.tables:
            for mode in ('hold','linear'):
                a=sample_audio(t['frames'],t['rootBin'],mode,.01)
                self.assertTrue(np.isfinite(a).all());self.assertLess(np.max(np.abs(a)),1)
        # A diagnostic table checks genuine floor addressing without altering ROM.
        frames=[list(range(32))]*16
        a=sample_audio(frames,1,'hold',.01)
        expected=np.floor(np.remainder(np.arange(len(a))*130.8127826502993/48000,1)*32)/32768*.7
        np.testing.assert_array_equal(a,expected)

    def test_all_wav_headers_and_clipping(self):
        paths=sorted((OUT/'audio').rglob('*.wav'));self.assertEqual(len(paths),113)
        for p in paths:
            with wave.open(str(p),'rb') as f:
                self.assertEqual((f.getnchannels(),f.getsampwidth(),f.getframerate()),(1,3,48000))
                raw=np.frombuffer(f.readframes(f.getnframes()),dtype=np.uint8).reshape(-1,3).astype(np.int32)
                values=raw[:,0]+(raw[:,1]<<8)+(raw[:,2]<<16)
                values=np.where(values>=1<<23,values-(1<<24),values)
                self.assertLess(np.max(np.abs(values)),(1<<23)-1)

    def test_container_validation(self):
        for data in (self.bank[:63],self.bank[:-1],self.bank[:64]+bytes([self.bank[64]^1])+self.bank[65:]):
            with self.assertRaises(ValueError):read_bank(data)
        with self.assertRaises(ValueError):decode(b'\0'*9,32)
        bad=bytearray(35);bad[2]=15
        with self.assertRaises(ValueError):decode(bytes(bad),32)

    def test_canonical_derived_assets(self):
        raw=b''.join(struct.pack('<'+'h'*len(row),*row) for t in self.tables for row in t['frames'])
        self.assertEqual(raw,(OUT/'data/decoded-s16le.bin').read_bytes())
        from generate import header_file
        stored=json.loads((OUT/'data/decoded.json').read_text())
        self.assertEqual(header_file(stored),(OUT/'data/FactoryRom.h').read_text())

    def test_morph_gate(self):
        import csv
        with (OUT/'reports/morph-quality.csv').open() as f:rows=list(csv.DictReader(f))
        self.assertEqual(len(rows),525)
        self.assertFalse([r for r in rows if r['flagged']=='True'])


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,default=OUT)
    args,remaining=ap.parse_known_args();OUT=args.output
    unittest.main(argv=['test_rom.py']+remaining)
