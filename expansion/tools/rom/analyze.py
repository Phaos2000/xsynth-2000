"""Deterministic decoded-ROM diagnostics and root-aware all-pairs triage."""
from functools import reduce
from html import escape
import math
from pathlib import Path
import struct
import numpy as np
from generate import dump,csvout,slug


def exact_signature(row):
    """Primitive cycle, rational DC removal, positive gain, sign and circular phase."""
    a=tuple(map(int,row));n=len(a)
    for p in range(1,n+1):
        if n%p==0 and all(a[i]==a[i%p] for i in range(n)):
            a=a[:p];break
    n=len(a);total=sum(a);a=tuple(v*n-total for v in a)
    divisor=reduce(math.gcd,map(abs,a))
    if not divisor:return ('DC',)
    a=tuple(v//divisor for v in a)
    return min(a[i:]+a[:i] for a in (a,tuple(-v for v in a)) for i in range(n))


def feature(row,root):
    x=np.asarray(row,dtype=np.float64);dc=float(x.mean());center=x-dc
    rms=float(np.sqrt(np.mean(center*center)));peak=float(np.max(np.abs(x)))
    mag=np.abs(np.fft.rfft(center))[1:];energy=mag*mag;total=float(energy.sum())
    frequency=np.arange(1,len(mag)+1)/root
    distribution=energy/total if total else np.zeros_like(energy)
    order=np.argsort(-energy,kind='stable');bins=int(np.searchsorted(np.cumsum(distribution[order]),.95)+1) if total else 0
    crosses=np.flatnonzero((center>=0)!= (np.roll(center,1)>=0))
    spacing=np.diff(np.r_[crosses,crosses[:1]+len(x)]).tolist() if len(crosses) else []
    harmonic=np.abs(frequency-np.rint(frequency))<1e-10
    odd=harmonic & (np.rint(frequency).astype(int)%2==1)
    even=harmonic & ~odd
    eodd=float(energy[odd].sum());eeven=float(energy[even].sum())
    return dict(dc=dc,rms=rms,peak=peak,crest_factor=peak/rms if rms else 0,
        zero_crossings=len(crosses),zero_crossing_spacing=spacing,
        spectral_centroid_root_units=float(np.sum(frequency*distribution)),
        spectral_flatness=float(np.exp(np.mean(np.log(np.maximum(energy,1e-24))))/(np.mean(energy)+1e-24)),
        odd_energy=eodd/(total or 1),even_energy=eeven/(total or 1),
        odd_even_ratio=eodd/eeven if eeven else None,
        harmonic_root_energy=float(distribution[harmonic].sum()),off_root_grid_energy=float(distribution[~harmonic].sum()),
        bins_95_energy=bins,bins_95_indices=(order[:bins]+1).tolist(),
        nonnegligible_bins=int(np.sum(distribution>1e-5)),
        normalized_magnitude=(mag/np.linalg.norm(mag)).tolist() if total else mag.tolist())


def svg(path,body,width=1000,height=620):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}"><rect width="100%" height="100%" fill="#f8f8f4"/><g font-family="sans-serif" font-size="11" fill="#182e38">'+''.join(body)+'</g></svg>\n',encoding='utf-8')


def pathline(values,x,y,w,h,color,maximum=1):
    vals=np.asarray(values,dtype=float)
    points=' '.join(f'{x+i*w/max(1,len(vals)-1):.3f},{y+h/2-v*h/(2*maximum):.3f}' for i,v in enumerate(vals))
    return f'<polyline fill="none" stroke="{color}" stroke-width="1" points="{points}"/>'


def contacts(out,t):
    r=t['recipe']
    for spectral in (False,True):
        body=[f'<text x="12" y="18">{r["id"]:02} {escape(r["family"])} / {escape(r["label"])} — {"magnitude spectrum" if spectral else "decoded cycle"}; rootBin={r["rootBin"]}</text>']
        for j,row in enumerate(t['frames']):
            x=12+(j%4)*247;y=34+(j//4)*145;v=np.array(row,dtype=float)/32768
            body += [f'<rect x="{x}" y="{y}" width="236" height="132" fill="white" stroke="#bbc4c8"/>',f'<text x="{x+5}" y="{y+13}">frame {j:02}</text>']
            if spectral:
                v=np.abs(np.fft.rfft(v-v.mean()))[1:];v/=max(1e-12,float(v.max()))
                for k,a in enumerate(v):
                    xx=x+5+k*226/len(v);yy=y+123-a*100
                    body.append(f'<path d="M {xx:.3f} {y+123} V {yy:.3f}" stroke="#216c81"/>')
            else:
                body.append(f'<path d="M {x+5} {y+77} H {x+231}" stroke="#d7d7d7"/>')
                body.append(pathline(v,x+5,y+27,226,100,'#216c81'))
        svg(out/'diagnostics'/('spectra' if spectral else 'waveforms')/(slug(r)+'.svg'),body)


def compare_frames(frames,root,period):
    """31 positions, LINEAR periodic reconstruction, 256 samples/intended-root period."""
    a=np.asarray(frames,dtype=float)
    poses=[]
    for j in range(16):
        poses.append(a[j])
        if j<15:poses.append((a[j]+a[j+1])*.5)
    a=np.asarray(poses)
    phase=np.remainder(np.arange(period*256,dtype=float)/(256*root),1)*a.shape[1]
    ix=np.floor(phase).astype(int);frac=phase-ix
    values=a[:,ix]*(1-frac)+a[:,(ix+1)%a.shape[1]]*frac
    values-=values.mean(axis=1,keepdims=True)
    norms=np.linalg.norm(values,axis=1,keepdims=True)
    return np.fft.rfft(values/np.maximum(norms,1e-30),axis=1)


def pair_metrics(aa,bb,length):
    a=np.abs(aa[:,1:]);b=np.abs(bb[:,1:]);an=np.linalg.norm(a,axis=1);bn=np.linalg.norm(b,axis=1)
    spectral=np.einsum('ik,jk->ij',a,b,optimize=False)/np.maximum(an[:,None]*bn,1e-30)
    correlations=np.empty_like(spectral)
    for i in range(len(aa)):
        correlations[i]=np.max(np.abs(np.fft.irfft(aa[i]*np.conj(bb),n=length,axis=1)),axis=1)
    best=np.unravel_index(np.argmax(correlations),correlations.shape)
    bestspec=np.unravel_index(np.argmax(spectral),spectral.shape)
    corresponding=float(np.diag(spectral).mean());reverse=float(np.diag(spectral[:,::-1]).mean())
    offsets=[]
    for off in range(-24,25):
        if 31-abs(off)>=16:offsets.append(float(np.diag(spectral,k=off).mean()))
    ea=a[bestspec[0]]**2;eb=b[bestspec[1]]**2;ea/=ea.sum();eb/=eb.sum()
    bins=np.arange(1,len(ea)+1)/ (length/256)
    ca=float(bins@ea);cb=float(bins@eb)
    fa=float(np.exp(np.mean(np.log(np.maximum(ea,1e-24))))/np.mean(ea))
    fb=float(np.exp(np.mean(np.log(np.maximum(eb,1e-24))))/np.mean(eb))
    sa=int(np.searchsorted(np.cumsum(np.sort(ea)[::-1]),.95)+1)
    sb=int(np.searchsorted(np.cumsum(np.sort(eb)[::-1]),.95)+1)
    relative=lambda x,y:abs(x-y)/max(abs(x),abs(y),1e-12)
    near=relative(ca,cb)<.1 and relative(fa,fb)<.1 and relative(sa,sb)<.1
    return dict(max_waveform_correlation=float(correlations.max()),max_spectral_cosine=float(spectral.max()),
        spectral_distribution_distance=float(np.abs(ea-eb).sum()/2),
        best_waveform_position_a=best[0]/2,best_waveform_position_b=best[1]/2,
        best_spectral_position_a=bestspec[0]/2,best_spectral_position_b=bestspec[1]/2,
        coverage_a=float(spectral.max(axis=1).mean()),coverage_b=float(spectral.max(axis=0).mean()),
        worst_coverage_a=float(spectral.max(axis=1).min()),worst_coverage_b=float(spectral.max(axis=0).min()),
        corresponding=corresponding,reverse=reverse,best_offset=max(offsets),
        centroid_relative_difference=relative(ca,cb),flatness_relative_difference=relative(fa,fb),
        sparsity_relative_difference=relative(sa,sb),
        flagged=bool(correlations.max()>=.995 or (spectral.max()>=.985 and near)
                     or min(float(spectral.max(axis=1).mean()),float(spectral.max(axis=0).mean()))>.985))


def analyze(out,tables):
    reports=out/'reports';reports.mkdir(exist_ok=True)
    duplicates=[];features=[];master_features=[];morph=[];seen={};master_seen={}
    for t in tables:
        r=t['recipe'];contacts(out,t)
        for j,row in enumerate(t['frames']):
            key=exact_signature(row)
            if key in seen:duplicates.append(dict(id=r['id'],frame=j,other=seen[key],kind='decoded sign/shift/primitive/DC/gain'))
            seen.setdefault(key,[r['id'],j])
            key=exact_signature(t['source'][j])
            if key in master_seen:duplicates.append(dict(id=r['id'],frame=j,other=master_seen[key],kind='recipe-master sign/shift/primitive/DC/gain'))
            master_seen.setdefault(key,[r['id'],j])
            features.append(dict(id=r['id'],frame=j,N=r['cycle_length'],rootBin=r['rootBin'],**feature(np.array(row)/32768,r['rootBin'])))
            master_features.append(dict(id=r['id'],frame=j,N=r['cycle_length'],rootBin=r['rootBin'],**feature(np.array(t['source'][j])/(1<<31),r['rootBin'])))
        a=np.array(t['frames'],dtype=float)/32768
        for j in range(15):
            ratios=[]
            for fraction in np.linspace(0,1,65):
                v=a[j]*(1-fraction)+a[j+1]*fraction
                rms=float(np.sqrt(np.mean(v*v)))
                expected=(1-fraction)*np.sqrt(np.mean(a[j]**2))+fraction*np.sqrt(np.mean(a[j+1]**2))
                ratios.append(rms/max(expected,1e-30))
            rho=float(np.corrcoef(a[j],a[j+1])[0,1])
            morph.append(dict(id=r['id'],frame_a=j,frame_b=j+1,min_rms_ratio=min(ratios),
                adjacent_correlation=rho,max_peak=float(np.max(np.abs(a[j:j+2]))),
                dc_change=float(abs(a[j].mean()-a[j+1].mean())),
                flagged=bool(min(ratios)<.7 or rho<0 or np.min(np.sqrt(np.mean(a[j:j+2]**2,axis=1)))<.005
                    or np.max(np.abs(a[j:j+2].mean(axis=1)))>.01
                    or abs(a[j].mean()-a[j+1].mean())>.01 or np.max(np.abs(a[j:j+2]))>=1)))
    dump(reports/'exact-duplicates.json',duplicates)
    dump(reports/'decoded-frame-features.json',features);dump(reports/'recipe-frame-features.json',master_features)
    csvout(reports/'morph-quality.csv',morph)
    print(f'Exact duplicate candidates: {len(duplicates)}; morph warnings: {sum(m["flagged"] for m in morph)}',flush=True)
    pairs=[];masters=[];cache={}
    for i,ta in enumerate(tables):
        for tb in tables[i+1:]:
            ra,rb=ta['recipe'],tb['recipe'];period=math.lcm(ra['rootBin'],rb['rootBin'])
            results=[]
            for kind in ('frames','source'):
                arrays=[]
                for t in (ta,tb):
                    key=(t['recipe']['id'],period,kind)
                    if key not in cache:cache[key]=compare_frames(t[kind],t['recipe']['rootBin'],period)
                    arrays.append(cache[key])
                results.append(pair_metrics(*arrays,period*256))
            row=dict(id_a=ra['id'],id_b=rb['id'],table_a=ra['family']+' / '+ra['label'],table_b=rb['family']+' / '+rb['label'],**results[0])
            pairs.append(row);masters.append(dict(id_a=ra['id'],id_b=rb['id'],**results[1]))
            if row['flagged']:
                pa=row['best_waveform_position_a'];pb=row['best_waveform_position_b']
                def at(t,p):
                    a=int(p);return np.array(t['frames'][a])*(1-(p-a))+np.array(t['frames'][min(a+1,15)])*(p-a)
                va=at(ta,pa)/32768;vb=at(tb,pb)/32768
                body=[f'<text x="12" y="20">{escape(row["table_a"])} position {pa} vs {escape(row["table_b"])} position {pb}</text>',
                      '<text x="12" y="38">Own-cycle waveforms (not phase aligned); root-aware scores are in similarity.csv</text>',
                      pathline(va,20,60,950,160,'#216c81'),pathline(vb,20,235,950,160,'#a84b31')]
                # Root-normalized spectra visualized against the same frequency grid.
                for v,r,color in ((va,ra,'#216c81'),(vb,rb,'#a84b31')):
                    mag=np.abs(np.fft.rfft(v-v.mean()))[1:];mag/=max(1e-20,float(mag.max()))
                    for k,a in enumerate(mag,1):
                        xx=20+(k/r['rootBin'])*950/128
                        body.append(f'<path d="M {xx:.3f} 595 V {595-a*150:.3f}" stroke="{color}"/>')
                svg(out/'diagnostics/similarity'/f"flag_{ra['id']:03}_{rb['id']:03}.svg",body)
        print(f'All-pairs analysis: {i+1}/{len(tables)} table rows',flush=True)
    csvout(reports/'similarity.csv',pairs);csvout(reports/'recipe-similarity.csv',masters)
    nearest=[]
    for t in tables:
        id=t['recipe']['id'];options=[r for r in pairs if id in (r['id_a'],r['id_b'])]
        for row in sorted(options,key=lambda r:-(r['coverage_a']+r['coverage_b']))[:3]:
            nearest.append(dict(id=id,neighbor=row['id_b'] if id==row['id_a'] else row['id_a'],
                coverage_a=row['coverage_a'],coverage_b=row['coverage_b'],max_spectral_cosine=row['max_spectral_cosine']))
    csvout(reports/'nearest-neighbors.csv',nearest)
    matrix=np.eye(len(tables))
    for r in pairs:matrix[r['id_a'],r['id_b']]=matrix[r['id_b'],r['id_a']]=(r['coverage_a']+r['coverage_b'])/2
    body=[f'<text x="12" y="18">Mean bidirectional nearest-frame spectral coverage (0..1); IDs 0..{len(tables)-1}</text>']
    for i in range(len(tables)):
        body.append(f'<text x="{45+i*15}" y="37" font-size="8">{i}</text><text x="18" y="{53+i*15}" font-size="8">{i}</text>')
        for j in range(len(tables)):
            v=matrix[i,j];c=int(255*(1-v));body.append(f'<rect x="{43+j*15}" y="{43+i*15}" width="14" height="14" fill="rgb({c},{c},220)"/>')
    svg(out/'diagnostics/similarity/matrix.svg',body,650,590)
    flags=[r for r in pairs if r['flagged']]
    lines=['# Similarity review — measured flags, not artistic acceptance','',
           f'All {len(pairs)} table pairs include all 31 × 31 frame/midframe combinations, signs/circular shifts,',
           'corresponding/reversed/offset trajectories and bidirectional nearest-frame coverage.',
           'Root-aware LINEAR reconstruction uses 256 points per intended-root period over the pair LCM.',
           'Thresholds: |wave correlation| ≥ .995 OR spectral cosine ≥ .985 with centroid, flatness and',
           '95%-energy-bin-count differences all <10%. No threshold was adjusted to reduce warnings.',
           'Additional whole-trajectory review gate: both mean nearest-frame coverages > .985, even',
           'if a phase-only difference prevents a single-frame waveform flag.',
           'Master and decoded analyses are separate. See CSVs for every pair and feature JSON for every frame.','',
           f'Exact duplicate candidates: {len(duplicates)}. Similarity flags: {len(flags)}.',
           'Remaining flags below are retained for human audition, with measured whole-trajectory evidence.','']
    for row in flags:
        ra=tables[row['id_a']]['recipe'];rb=tables[row['id_b']]['recipe']
        lines += [f"## {row['id_a']:02} {row['table_a']} vs {row['id_b']:02} {row['table_b']}",
          f"Peak waveform correlation {row['max_waveform_correlation']:.6f}; spectral cosine {row['max_spectral_cosine']:.6f}.",
          f"Mean nearest-frame coverage {row['coverage_a']:.6f}/{row['coverage_b']:.6f}; worst {row['worst_coverage_a']:.6f}/{row['worst_coverage_b']:.6f}; corresponding/reverse/best-offset {row['corresponding']:.6f}/{row['reverse']:.6f}/{row['best_offset']:.6f}.",
          f"Recipe distinction: {ra['operation']}: {ra['progression']}; versus {rb['operation']}: {rb['progression']}.",
          'A shared local spectral state is not evidence of identical full movement. These reported differences',
          'support retaining the candidate for audition; they do not certify perceptual distinctness.','']
    (reports/'SIMILARITY-REVIEW.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    summary=dict(tables=len(tables),frames=sum(len(t['frames']) for t in tables),exact_duplicate_candidates=len(duplicates),
                 similarity_flags=len(flags),morph_flags=sum(m['flagged'] for m in morph),
                 severe_trajectory_pairs=[dict(id_a=r['id_a'],id_b=r['id_b'],coverage_a=r['coverage_a'],coverage_b=r['coverage_b'])
                     for r in pairs if min(r['coverage_a'],r['coverage_b'])>.985])
    dump(reports/'summary.json',summary)
