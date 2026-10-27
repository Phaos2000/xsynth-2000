"""Apply approved ID20 Halo + append-only ID35 Prism; preserve other PCM8 bytes.
Full clean generation is also supported by generate.py. This targeted updater
does not rerun or round-trip unrelated recipes or their payloads.
"""
from pathlib import Path
import hashlib,json,struct,sys
import numpy as np
from generate import ROOT,read_bank,build_table,bank_bytes,header_file
from prepare_bank import prepared

def sha(data):return hashlib.sha256(data).hexdigest()
def update():
 dest=ROOT/'factory/rom';oldraw=(dest/'factory.xrom').read_bytes()
 old=read_bank(oldraw);byid={t['id']:t for t in old}
 recipes=json.loads((dest/'recipes/tables.json').read_text())['recipes']
 assert len(recipes)==36 and recipes[20]['label']=='ETHEREAL' and recipes[35]['label']=='ETHEREAL #2'
 tables=[];untouched={}
 for r in recipes:
  if r['id'] in (20,35):
   t=build_table(r)
   assert sha(t['payload'])==r['parameters']['approved_pcm8_sha256']
  else:
   o=byid[r['id']];payload=b''.join(struct.pack('<'+'b'*len(row),*(v//256 for v in row)) for row in o['frames'])
   t=dict(recipe=r,frames=o['frames'],payload=payload);untouched[r['id']]=sha(payload)
  tables.append(t)
 raw=bank_bytes(tables);decoded=read_bank(raw)
 for t in decoded:
  if t['id'] not in (20,35):assert t['frames']==byid[t['id']]['frames']
 pcm=b''.join(struct.pack('<'+'h'*len(row),*row) for t in decoded for row in t['frames'])
 manifest=json.loads((dest/'manifest.json').read_text())
 manifest.update(frames=576,tables=recipes,recipe_sha256=sha((dest/'recipes/tables.json').read_bytes()),encoded_sha256=sha(raw),decoded_sha256=sha(pcm),numpy_version=np.__version__)
 manifest['approved_ethereal_update']={'replacement':{'id':20,'candidate':'A Halo'},'append':{'id':35,'candidate':'B Prism','visible_name':'PAD ETHEREAL #2'},'other_existing_table_payloads_unchanged':True}
 manifest['generator_files_sha256']={p:sha((Path(__file__).parent/p).read_bytes()) for p in ['generate.py','recipes.py','pcm8.py','math_fixed.py','analyze.py','ethereal.py','prepare_bank.py','update_approved_ethereal.py']}
 families=list(dict.fromkeys(r['family'] for r in recipes))
 data=[dict(id=r['id'],name=('PAD ' if r['family']=='PAD' else '')+r['label'],rootBin=r['rootBin'],familyId=families.index(r['family']),pcm=np.array(t['frames'],np.float32)/32768) for r,t in zip(recipes,decoded)]
 derived=prepared(data,hashlib.sha256(raw).digest())
 fixture=prepared([data[20],data[35]],hashlib.sha256(raw).digest())
 (dest/'factory.xrom').write_bytes(raw);(dest/'decoded-s16le.bin').write_bytes(pcm)
 (dest/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n',encoding='utf-8')
 (ROOT/'port/generated/FactoryRom.h').write_text(header_file(tables),encoding='utf-8')
 lines=['// Generated from canonical PCM8. Approved ETHEREAL Halo ID20 / Prism ID35.']
 lines.extend(','.join(map(str,derived[i:i+32]))+',' for i in range(0,len(derived),32))
 (ROOT/'assets/PreparedFactory.inc').write_text('\n'.join(lines)+'\n',encoding='utf-8')
 fixturepath=ROOT/'tests/reference/approved-ethereal.p8rd';fixturepath.write_bytes(fixture)
 (fixturepath.with_suffix('.json')).write_text(json.dumps({'purpose':'Approved PCM8 source + independent prototype reader regression; selected W04 A/B content','sha256':sha(fixture),'ids':[20,35],'canonical_payload_sha256':[recipes[i]['parameters']['approved_pcm8_sha256'] for i in (20,35)],'unchanged_existing_payload_sha256':untouched},indent=2)+'\n')
 entries={name:sha((dest/name).read_bytes()) for name in ['factory.xrom','manifest.json','decoded-s16le.bin','recipes/tables.json']}
 (dest/'SHA256SUMS').write_text(''.join(v+'  '+k+'\n' for k,v in sorted(entries.items())),encoding='utf-8')
 return dict(bank_sha256=sha(raw),prepared_sha256=sha(derived),table_count=36,frame_count=576,preserved_payloads=untouched,approved_fixture_sha256=sha(fixture))

if __name__=='__main__':print(json.dumps(update(),indent=2))
