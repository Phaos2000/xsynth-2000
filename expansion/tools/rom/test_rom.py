"""Focused ROM-only checks. Run after generation; no VST tests or build."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import unittest
import wave
import numpy as np
from pcm8 import quantize,decode_codes
from decimal import Decimal
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
        cls.recipes=json.loads((OUT/'data/manifest.json').read_text())['tables']

    def test_manifest_inventory(self):
        m=json.loads((OUT/'data/manifest.json').read_text())
        self.assertEqual([r['id'] for r in m['tables']],list(range(len(self.tables))))
        self.assertEqual([r['cycle_length'] for r in m['tables']],EXPECTED_N+[256]*(len(self.tables)-35))
        self.assertEqual([r['rootBin'] for r in m['tables']],EXPECTED_ROOT+[1]*(len(self.tables)-35))
        self.assertEqual([t['id'] for t in self.tables],list(range(len(self.tables))))
        self.assertEqual([t['cycle_length'] for t in self.tables],EXPECTED_N+[256]*(len(self.tables)-35))
        self.assertEqual([t['rootBin'] for t in self.tables],EXPECTED_ROOT+[1]*(len(self.tables)-35))
        self.assertTrue(all(len(t['frames'])==16 for t in self.tables))
        self.assertEqual(len(self.bank),64+32*len(self.tables)+sum(16*t['cycle_length'] for t in self.tables))
        self.assertEqual(m["codec"],"SIGNED-PCM8")
        self.assertTrue(all("codec_profile" not in r and "block_length" not in r for r in m["tables"]))
        self.assertEqual(m['encoded_sha256'],hashlib.sha256(self.bank).hexdigest())

    def test_approved_pcm8_mapping(self):
        import csv
        for name in ('smooth','edge','complex'):
            original=ROOT/'artifacts/xromd1-prototype'
            precise=json.loads((original/(name+'.source.json')).read_text())
            with (original/(name+'.csv')).open() as f:expected=list(csv.DictReader(f))
            self.assertEqual([quantize(Decimal(v))[0]/128 for v in precise],[float(r['pcm8']) for r in expected])
        for v,code,clip in (('0',0,0),('0.00390625',0,0),('0.01171875',2,0),('-0.00390625',0,0),
                            ('-0.01171875',-2,0),('-1',-128,0),('1',127,1),('2',127,1),('-2',-128,1)):
            self.assertEqual(quantize(Decimal(v)),(code,clip))
        with self.assertRaises(ValueError):quantize(Decimal('NaN'))
        self.assertEqual(decode_codes(bytes(range(256))),[(i if i<128 else i-256)*256 for i in range(256)])

    def test_every_frame_pcm8(self):
        stats=json.loads((OUT/'data/frame-codec-statistics.json').read_text())
        self.assertEqual(len(stats),16*len(self.tables))
        self.assertTrue(all(s['accidental_clipping']==0 for s in stats))
        self.assertTrue(all(-128<=s['min_pcm_code']<=s['max_pcm_code']<=127 for s in stats))
        self.assertTrue(all(1<=s['unique_pcm_codes']<=256 and s['max_error']<=1/256+1e-14 for s in stats))
        for t in self.tables:
            for row in t['frames']:
                self.assertTrue(all(v%256==0 for v in row))
                self.assertLessEqual(abs(sum(row)/len(row)/32768),1/256+1e-12)
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
        for r in self.recipes[29:35]:
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
        self.assertEqual(decode_codes(bytes([128,0,127])),[-32768,0,32512])

    def test_canonical_derived_assets(self):
        raw=b''.join(struct.pack('<'+'h'*len(row),*row) for t in self.tables for row in t['frames'])
        self.assertEqual(raw,(OUT/'data/decoded-s16le.bin').read_bytes())
        from generate import header_file
        stored=json.loads((OUT/'data/decoded.json').read_text())
        self.assertEqual(header_file(stored),(OUT/'data/FactoryRom.h').read_text())

    def test_morph_gate(self):
        import csv
        with (OUT/'reports/morph-quality.csv').open() as f:rows=list(csv.DictReader(f))
        self.assertEqual(len(rows),15*len(self.tables))
        self.assertFalse([r for r in rows if r['flagged']=='True'])

    def test_mockup_grid_and_native_sizes(self):
        g=json.loads((OUT/'gui-mockups/geometry.json').read_text())
        self.assertEqual(g['main_size'],[960,568]);self.assertEqual(g['browser_size'],[480,384])
        self.assertEqual(g['column_width']*2+g['column_gap'],g['vco_width']-2*g['margin'])
        self.assertEqual(g['table_width']+g['browser_button_width']+g['table_browser_gap'],146)
        self.assertEqual(g['table_y_body']-g['source_y_body'],g['row_step'])
        self.assertEqual(g['reader_y_body']-g['table_y_body'],g['row_step'])
        self.assertEqual(g['reader_y_body']+g['selector_height'],g['vco_height']-2)
        for name,x,y,w,h in g['browser_fields']:
            self.assertGreaterEqual(x,12);self.assertLessEqual(x+w,468)
            self.assertEqual(h,112 if name=='WAVEFORM' else 18)
        paths=list((OUT/'gui-mockups').rglob('*.png'));self.assertEqual(len(paths),18)
        for p in paths:
            size=struct.unpack('>II',p.read_bytes()[16:24])
            self.assertEqual(size,(480,384) if p.name.startswith('browser-') else (960,568))


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,default=OUT)
    args,remaining=ap.parse_known_args();OUT=args.output
    unittest.main(argv=['test_rom.py']+remaining)
