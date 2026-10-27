"""Focused approved table-content/inventory regression; no audio normalization."""
import sys,unittest,json,hashlib,re
from pathlib import Path
import numpy as np
from generate import ROOT,read_bank,build_table
from prepare_bank import prepared

class ApprovedEtherealTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw=(ROOT/'factory/rom/factory.xrom').read_bytes()
        cls.tables=read_bank(cls.raw)
        cls.manifest=json.loads((ROOT/'factory/rom/manifest.json').read_text())
        cls.provenance=json.loads((ROOT/'tests/reference/approved-ethereal.json').read_text())

    def test_stable_inventory_and_exact_approved_payloads(self):
        self.assertEqual([t['id'] for t in self.tables],list(range(36)))
        self.assertEqual(self.manifest['frames'],576)
        for ident,label,key in [(20,'ETHEREAL','A'),(35,'ETHEREAL #2','B')]:
            t=self.tables[ident];r=self.manifest['tables'][ident]
            self.assertEqual((t['rootBin'],t['cycle_length'],len(t['frames'])),(1,256,16))
            self.assertEqual((r['label'],r['parameters']['candidate']),(label,key))
            payload=np.array(t['frames'],np.int16)//256
            actual=hashlib.sha256(payload.astype(np.int8).tobytes()).hexdigest()
            self.assertEqual(actual,r['parameters']['approved_pcm8_sha256'])

    def test_other_existing_payloads_preserved(self):
        hashes=self.provenance['unchanged_existing_payload_sha256']
        self.assertEqual(set(map(int,hashes)),set(range(35))-{20})
        for ident,h in hashes.items():
            t=self.tables[int(ident)];payload=(np.array(t['frames'],np.int16)//256).astype(np.int8).tobytes()
            self.assertEqual(hashlib.sha256(payload).hexdigest(),h)

    def test_recipe_regeneration_matches_listening_approval(self):
        for ident in [20,35]:
            result=build_table(self.manifest['tables'][ident])
            expected=self.manifest['tables'][ident]['parameters']['approved_pcm8_sha256']
            self.assertEqual(hashlib.sha256(result['payload']).hexdigest(),expected)
            self.assertTrue(all(s['accidental_clipping']==0 for s in result['stats']))

    def test_prepared_bank_exact_derivative_of_current_pcm8(self):
        families=list(dict.fromkeys(r['family'] for r in self.manifest['tables']))
        tables=[]
        for t,r in zip(self.tables,self.manifest['tables']):
            tables.append(dict(id=t['id'],name=('PAD ' if r['family']=='PAD' else '')+r['label'],rootBin=t['rootBin'],familyId=families.index(r['family']),pcm=np.array(t['frames'],np.float32)/32768))
        actual=prepared(tables,hashlib.sha256(self.raw).digest())
        text=(ROOT/'assets/PreparedFactory.inc').read_text().split('\n',1)[1]
        embedded=bytes(map(int,re.findall(r'\d+',text)))
        self.assertEqual(actual,embedded)

    def test_scalar_native_reference_fixture_is_pinned(self):
        fixture=ROOT/'tests/reference/approved-ethereal.p8rd'
        self.assertEqual(hashlib.sha256(fixture.read_bytes()).hexdigest(),self.provenance['sha256'])

    def test_no_exact_duplicate_frames_or_new_morph_collapse(self):
        from analyze import exact_signature
        seen=set()
        for t in self.tables:
            for row in t['frames']:
                sig=exact_signature(row)
                self.assertNotIn(sig,seen)
                seen.add(sig)
        for ident in (20,35):
            a=np.array(self.tables[ident]['frames'],dtype=float)/32768
            self.assertLess(np.abs(a.mean(axis=1)).max(),.004)
            for first,second in zip(a,a[1:]):
                floor=min(np.sqrt(np.mean(first*first)),np.sqrt(np.mean(second*second)))
                for f in np.linspace(0,1,65):
                    row=first+f*(second-first)
                    self.assertGreater(np.sqrt(np.mean(row*row)),floor*.99)

if __name__=='__main__':unittest.main()
