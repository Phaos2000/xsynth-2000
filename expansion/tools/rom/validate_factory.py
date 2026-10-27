"""Reproducible factory-integrity evidence. Reports artistic/seam warnings;
does not force duplicate endpoints, zero crossings, smoothing or normalization.
Run against a generated bank/manifest, optionally with its Q31 recipe masters.
"""
from pathlib import Path
import argparse,csv,json,hashlib,math
import numpy as np
from generate import read_bank
from cyclic_integrity import metrics

def csvout(path,rows):
 with path.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

def validate(bank,manifest,out,source_masters=None):
 bank,manifest,out=Path(bank),Path(manifest),Path(out);out.mkdir(parents=True,exist_ok=True)
 raw=bank.read_bytes();m=json.loads(manifest.read_text(encoding='utf-8-sig'));tables=read_bank(raw)
 assert hashlib.sha256(raw).hexdigest()==m['encoded_sha256'],'manifest bank hash'
 assert [r['id'] for r in m['tables']]==[t['id'] for t in tables],'identity/order mismatch'
 assert m['frames']==sum(len(t['frames']) for t in tables),'frame count mismatch'
 sources=np.fromfile(source_masters,dtype='<i4').astype(float)/(1<<31) if source_masters else None
 offset=0;rows=[];pairs=[];inventory=[];root=[]
 for t,r in zip(tables,m['tables']):
  x=np.array(t['frames'],float)/32768;n=t['cycle_length'];frames=len(x)
  name=('PAD ' if r['family']=='PAD' and not r['label'].startswith('PAD ') else '')+r['label']
  assert (n,frames,t['rootBin'])==(r['cycle_length'],r['frames'],r['rootBin']),'descriptor mismatch'
  assert np.isfinite(x).all() and np.max(np.abs(x))<=1,'nonfinite/range error'
  source=None
  if sources is not None:
   source=sources[offset:offset+x.size].reshape(x.shape);offset+=x.size
   assert np.array_equal(np.rint(source*128).clip(-128,127)/128,x),'source/canonical PCM8 mismatch'
  operation=r['operation'];periodicity='sampled periodic playback; generator law requires review'
  if operation in ('metal','pad_metal'):periodicity='finite partial segment with sin² endpoint window; value and first derivative close analytically'
  elif operation in ('pulse','stairs','grain','logic','fold','bass','keys'):periodicity='explicit periodic phase/counter construction; hard edges may be intentional'
  else:periodicity='integer-harmonic additive construction; analytic periodicity'
  if operation=='chord':
   bins=r['parameters']['bins'];assert bins[0]==t['rootBin'] and all(isinstance(k,int) and 0<k<n/2 for k in bins),'invalid rational chord root/bins'
   allowed={b*h for b in bins for h in range(1,r['parameters']['overtones']+1) if b*h<n/2}
   for j,row in enumerate(source if source is not None else x):
    power=np.abs(np.fft.rfft(row)/n)**2;unwanted=power.copy();unwanted[0]=0
    for k in allowed:unwanted[k]=0
    root.append(dict(id=t['id'],frame=j,rootBin=t['rootBin'],allowed_max_bin=max(allowed),off_recipe_energy=float(unwanted.sum()/max(power.sum(),1e-30)),source_or_pcm8='Q31 source' if source is not None else 'PCM8 includes quantization harmonics'))
  inventory.append(dict(id=t['id'],canonical_name=name,frames=frames,length=n,rootBin=t['rootBin'],operation=operation,periodicity=periodicity))
  for j,row in enumerate(x):
   q=metrics(row);slopes=np.roll(row,-1)-row
   src_error=abs(row-source[j]).max() if source is not None else None
   rows.append(dict(id=t['id'],name=name,frame=j,**q,derivative_join_difference=float(slopes[0]-slopes[-1]),wrap_step_rank=float(np.mean(abs(slopes)<=abs(slopes[-1]))),source_max_quant_error=float(src_error) if src_error is not None else '',warning_is_not_proof_of_bad_cycle=True,periodicity=periodicity))
  for j in range(frames-1):
   a,b=x[j:j+2];mix=a[None,:]+np.linspace(0,1,65)[:,None]*(b-a)
   rms=np.sqrt(np.mean(mix*mix,axis=1));floor=min(np.sqrt(np.mean(a*a)),np.sqrt(np.mean(b*b)))
   pairs.append(dict(id=t['id'],name=name,frame_a=j,frame_b=j+1,step_delta_peak=float(abs(b-a).max()),step_delta_rms=float(np.sqrt(np.mean((b-a)**2))),correlation=float(np.corrcoef(a,b)[0,1]),minimum_morph_rms=float(rms.min()),morph_relative_to_quieter_endpoint_db=float(20*np.log10(max(1e-30,rms.min()/floor))),morph_constant_sum_identity_max_error=float(np.abs((a+.5*(b-a))-(a+b)*.5).max())))
 if sources is not None:assert offset==len(sources),'extra source master samples'
 csvout(out/'frames.csv',rows);csvout(out/'frame-transitions.csv',pairs);csvout(out/'inventory.csv',inventory)
 if root:csvout(out/'rational-root-support.csv',root)
 result=dict(tables=len(tables),frames=len(rows),adjacent_pairs=len(pairs),all_finite=True,descriptor_identity_checks_pass=True,source_quantization_exact=sources is not None,source_masters_sha256=hashlib.sha256(Path(source_masters).read_bytes()).hexdigest() if source_masters else None,wrap_warning_frames=sum(r['suspicious'] for r in rows),max_abs_dc=max(abs(r['dc']) for r in rows),max_source_quant_error=max((r['source_max_quant_error'] for r in rows),default=0) if sources is not None else None,warning_policy='diagnostic, not automatic repair/failure; high slopes and unequal endpoints can be valid',bank_sha256=hashlib.sha256(raw).hexdigest())
 (out/'SUMMARY.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');return result

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('bank',type=Path);p.add_argument('manifest',type=Path);p.add_argument('output',type=Path);p.add_argument('--source-masters',type=Path)
 a=p.parse_args();print(json.dumps(validate(a.bank,a.manifest,a.output,a.source_masters),indent=2))
