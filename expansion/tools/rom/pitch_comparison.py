"""SUPERSEDED / EXPERIMENTAL / NOT PRODUCTION.

Fixed original prototype tables transposed; no encoding or factory generation."""
import csv
import hashlib
import json
import math
from pathlib import Path
import wave
import numpy as np
from codec_prototype import decode

ROOT=Path(__file__).resolve().parents[2]
INPUT=ROOT/'artifacts/xromd1-prototype'
OUTPUT=ROOT/'artifacts/xromd1-pitch-comparison'
RATE=48000
DURATION=2
GAIN=.5
NULL_GAIN=16


def dump(path, obj):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(obj,indent=2,sort_keys=True,allow_nan=False)+'\n',encoding='utf-8')


def csvout(path,rows):
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)


def wav(path,values):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not np.isfinite(values).all() or np.max(np.abs(values))>=1:raise ValueError('Nonfinite/clipped audio')
    q=np.rint(values*(1<<23)).astype(np.int32)
    b=np.stack([(q>>(8*k))&255 for k in range(3)],axis=1).astype(np.uint8)
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1);f.setsampwidth(3);f.setframerate(RATE);f.writeframes(b.tobytes())


def rms(x):return float(np.sqrt(np.mean(x*x)))
def db(x):return 20*math.log10(max(1e-30,float(x)))


def phase_indices(frequency,count):
    # Actual shared phase-advance/wrap loop, not a resampled static table.
    phase=0.;increment=frequency/RATE;positions=np.empty(count)
    for i in range(count):
        phase+=increment
        phase-=math.floor(phase)
        positions[i]=phase*128
    return positions


def lookup(table,positions,mode):
    i=np.floor(positions).astype(np.int64)
    if mode=='hold':return table[i]*GAIN
    f=positions-i
    return (table[i]*(1-f)+table[(i+1)%len(table)]*f)*GAIN


def transform(values):
    window=np.hanning(len(values));raw=np.fft.rfft(values*window)
    amplitude=np.abs(raw)*2/window.sum();amplitude[[0,-1]]*=.5
    power=np.abs(raw)**2/(len(values)*float(window@window));power[1:-1]*=2
    return amplitude,power


def alias_masks(frequency,bins):
    # Finite reconstruction-harmonic candidate model, not an alias-separation oracle.
    alias=np.zeros(len(bins),bool);legit=np.zeros(len(bins),bool)
    for h in range(1,513):
        original=h*frequency
        folded=abs((original+RATE/2)%RATE-RATE/2)
        ix=int(round(folded/(bins[1]-bins[0])))
        target=legit if original<RATE/2 else alias
        target[max(1,ix-2):min(len(bins),ix+3)]=True
    return alias,alias & ~legit


def artifact(amp_a,amp_b,mask,bins,reverse=False):
    difference=(amp_b-amp_a) if reverse else (amp_a-amp_b)
    eligible=np.flatnonzero(mask & (difference>0))
    if not len(eligible):return None
    i=int(eligible[np.argmax(difference[eligible])])
    return dict(frequency_hz=float(bins[i]),pcm8_dbfs=db(amp_a[i]),xromd1_dbfs=db(amp_b[i]),
                magnitude_gap_dbfs=db(abs(amp_a[i]-amp_b[i])),
                magnitude_ratio_db=db(amp_a[i])-db(amp_b[i]),
                audible_frequency_band=bool(20<=bins[i]<=20000))


def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((INPUT/'determinism.json').read_text())['sha256']
    input_hashes={};rows=[];alias_rows=[];index=[];checks={}
    frequencies={f'C{o}':440*2**((12*(o+1)-69)/12) for o in range(1,8)}
    count=RATE*DURATION;bins=np.fft.rfftfreq(count,1/RATE)
    positions={pitch:phase_indices(frequency,count) for pitch,frequency in frequencies.items()}
    for source in ('smooth','edge','complex'):
        for filename in (source+'.source.json',source+'.csv',source+'.xromd1-frame'):
            digest=hashlib.sha256((INPUT/filename).read_bytes()).hexdigest()
            if digest!=manifest[filename]:raise ValueError('Original prototype input changed: '+filename)
            input_hashes[filename]=digest
        reference=np.array([float(v) for v in json.loads((INPUT/(source+'.source.json')).read_text())])
        with (INPUT/(source+'.csv')).open() as f:previous=list(csv.DictReader(f))
        pcm=np.array([float(r['pcm8']) for r in previous])
        decoded,codec=decode((INPUT/(source+'.xromd1-frame')).read_bytes())
        xrom=np.array(decoded,dtype=float)/32768
        assert np.array_equal(xrom,np.array([float(r['xromd1']) for r in previous]))
        assert codec['closure_error_lsb']==0 and codec['saturation']==0
        for mode in ('hold','linear'):
            reel=[];null_reel=[];cursor=0;null_cursor=0
            for pitch,frequency in frequencies.items():
                a=lookup(pcm,positions[pitch],mode);b=lookup(xrom,positions[pitch],mode)
                ref=lookup(reference,positions[pitch],mode);delta=a-b
                for encoding,values in (('pcm8',a),('xromd1',b)):
                    filename=f'{source}_{pitch.lower()}_{mode}_{encoding}.wav'
                    wav(OUTPUT/'audio'/source/mode/filename,values)
                    index.append(dict(reel=f'{source}_{mode}_pitch_reel.wav',source=source,mode=mode,pitch=pitch,
                                      encoding=encoding,start_seconds=cursor/RATE,duration_seconds=DURATION,file=filename))
                    gap=.15 if encoding=='pcm8' else .25
                    reel.extend((values,np.zeros(round(gap*RATE))));cursor+=len(values)+round(gap*RATE)
                wav_delta=delta*NULL_GAIN
                null_reel.extend((wav_delta,np.zeros(12000)))
                index.append(dict(reel=f'{source}_{mode}_AMPLIFIED_DIFFERENCE_x16_null_reel.wav',source=source,
                    mode=mode,pitch=pitch,encoding='PCM8 minus XROM-D1 x16 (+24.0824 dB)',start_seconds=null_cursor/RATE,
                    duration_seconds=DURATION,file='diagnostic difference only'))
                null_cursor+=len(delta)+12000
                ampa,pa=transform(a);ampb,pb=transform(b);ampd,pd=transform(delta)
                strongest=int(np.argmax(ampd[1:])+1)
                alias,isolated=alias_masks(frequency,bins)
                row=dict(source=source,pitch=pitch,frequency_hz=frequency,mode=mode,
                    rms_difference=rms(delta),max_abs_difference=float(np.max(np.abs(delta))),
                    difference_relative_pcm8_db=db(rms(delta)/rms(a)),
                    waveform_correlation=float(np.corrcoef(a,b)[0,1]),peak_difference_dbfs=db(np.max(np.abs(delta))),
                    rms_magnitude_spectral_difference=rms(ampa-ampb),
                    max_magnitude_bin_difference=float(np.max(np.abs(ampa-ampb))),
                    strongest_null_component_hz=float(bins[strongest]),strongest_null_component_dbfs=db(ampd[strongest]),
                    pcm8_energy_below10k=float(pa[bins<10000].sum()),pcm8_energy_above10k=float(pa[bins>=10000].sum()),
                    xromd1_energy_below10k=float(pb[bins<10000].sum()),xromd1_energy_above10k=float(pb[bins>=10000].sum()),
                    null_energy_below10k=float(pd[bins<10000].sum()),null_energy_above10k=float(pd[bins>=10000].sum()),
                    null_energy_alias_candidate_bins=float(pd[alias].sum()),
                    null_energy_alias_nonoverlap_bins=float(pd[isolated].sum()),
                    pcm8_reference_rms_error=rms(a-ref),xromd1_reference_rms_error=rms(b-ref),
                    pcm8_peak=float(np.max(np.abs(a))),xromd1_peak=float(np.max(np.abs(b))))
                rows.append(row)
                if pitch in ('C5','C6','C7'):
                    alias_rows.append(dict(source=source,pitch=pitch,mode=mode,
                       pcm8_stronger=artifact(ampa,ampb,isolated,bins),
                       xromd1_stronger=artifact(ampa,ampb,isolated,bins,True)))
                assert np.max(np.abs(wav_delta))<1
            wav(OUTPUT/'reels'/f'{source}_{mode}_pitch_reel.wav',np.concatenate(reel))
            wav(OUTPUT/'nulls'/f'{source}_{mode}_AMPLIFIED_DIFFERENCE_x16_null_reel.wav',np.concatenate(null_reel))
    csvout(OUTPUT/'measurements.csv',rows);csvout(OUTPUT/'reels/index.csv',index)
    dump(OUTPUT/'high-pitch-alias-details.json',alias_rows)
    dump(OUTPUT/'frequencies.json',frequencies);dump(OUTPUT/'input-hashes.json',input_hashes)
    csvout(OUTPUT/'difference-vs-pitch.csv',[{k:r[k] for k in ('source','pitch','frequency_hz','mode','rms_difference','difference_relative_pcm8_db','peak_difference_dbfs')} for r in rows])
    report(rows,alias_rows,frequencies)
    artifacts={p.relative_to(OUTPUT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(OUTPUT.rglob('*')) if p.is_file() and p.name not in ('SHA256SUMS','CHECKS.json')}
    (OUTPUT/'SHA256SUMS').write_text(''.join(v+'  '+k+'\n' for k,v in artifacts.items()),encoding='utf-8')
    dump(OUTPUT/'CHECKS.json',dict(primary_wavs=84,primary_reels=6,amplified_null_reels=6,
         phase_start=0,advance_before_read=True,all_finite=True,no_clipping=True,
         fixed_input_hashes_verified=True,no_encoding_performed=True,output_gain=GAIN,null_gain=NULL_GAIN))
    print('Ready: 84 comparison WAVs, 6 A/B reels, 6 clearly amplified null reels; no encoding/factory generation')


def report(rows,aliases,frequencies):
    lines=['# PCM8 vs XROM-D1 — pitch/interpolation comparison','',
       '**Technical classification: MEASURABLE BUT SUBTLE.** This is a numerical classification,',
       'not an artistic or controlled-listening verdict. The product codec decision remains yours.','',
       'Only the original three fixed prototype tables are read. Their recorded input hashes are',
       'verified; PCM8 is loaded from the original CSV, XROM is decoded once from the original',
       'saved bytes. No source generation, quantizer/encoder changes or factory generation occurs.','',
       '## Frequencies — A4=440 Hz, MIDI C1=24','']
    lines += [f'- {p}: {f:.15g} Hz' for p,f in frequencies.items()]
    lines+=['','## Direct null comparison','',
       'Metrics below use actual common-gain output (0.5), before 24-bit container rounding.',
       'Delta=PCM8−XROM. Relative dB uses PCM8 RMS; peak dBFS uses full-scale amplitude 1.',
       '| Source | Pitch | Mode | RMS delta | Max delta | Relative dB | Correlation | Peak delta dBFS |',
       '|---|---|---|---:|---:|---:|---:|---:|']
    for r in rows:lines.append(f"| {r['source']} | {r['pitch']} | {r['mode']} | {r['rms_difference']:.7g} | {r['max_abs_difference']:.7g} | {r['difference_relative_pcm8_db']:.3f} | {r['waveform_correlation']:.8f} | {r['peak_difference_dbfs']:.3f} |")
    lines+=['','## Trends measured, not assumed','']
    summary=[]
    for source in ('smooth','edge','complex'):
        for mode in ('hold','linear'):
            subset=[r for r in rows if r['source']==source and r['mode']==mode];middle=subset[2]
            levels=[r['difference_relative_pcm8_db'] for r in subset]
            text=f"{source}/{mode}: relative null {min(levels):.3f}..{max(levels):.3f} dB; C1 vs C3 RMS-null change {db(subset[0]['rms_difference']/middle['rms_difference']):+.3f} dB; C7 vs C3 {db(subset[-1]['rms_difference']/middle['rms_difference']):+.3f} dB."
            lines.append('- '+text);summary.append(text)
    lines+=['','HOLD/LINEAR difference by source (RMS null, averaged over pitches):','']
    for source in ('smooth','edge','complex'):
        h=np.mean([r['rms_difference'] for r in rows if r['source']==source and r['mode']=='hold'])
        l=np.mean([r['rms_difference'] for r in rows if r['source']==source and r['mode']=='linear'])
        lines.append(f'- {source}: HOLD null is {db(h/l):.3f} dB above LINEAR null.')
    lines+=['','## Interpretation','',
       '1. Low pitch: longer sample dwell does not amplify the fixed table error. The measured C1/C2',
       '   HOLD nulls stay close to C3, with small finite-record/phase-occupancy variation.',
       '2. High pitch: the null changes spectral placement, but does not acquire a large systematic',
       '   RMS increase. See all seven measured levels above, not only selected spectral peaks.',
       '3. HOLD exposes more quantization residual than LINEAR, whose local interpolation attenuates',
       '   some difference components. That mode contrast is not evidence of a new codec mechanism.',
       '4. High-pitch alias candidates differ repeatably in audible frequency bands, but differing',
       '   absolute levels remain low. A high dB ratio between two tiny bins does not establish',
       '   audible distinctness. Check normal A/B before the amplified diagnostic nulls.',
       '5. Across these tests the direct differences remain tens of dB below the source. None of the',
       '   measured ranges supports claiming a clearly different oscillator solely from transposition.',
       '6. Pitch does not materially overturn the original subtle-quantization conclusion. No codec',
       '   architecture change is made; human listening, not this classification, chooses the product.','',
       '## Spectral/alias method and limits','',
       'Two-second Hann FFT, 0.5-Hz bins. Amplitude is single-sided coherent-gain corrected.',
       'RMS spectral difference means RMS of the two magnitude spectra’s difference over bins.',
       'Strongest null component uses the FFT of the signed time-domain null (different from',
       'magnitude-only subtraction). Band energies are one-sided periodogram mean-square power,',
       'normalized by N*sum(window²); below/above split is at 10 kHz, including DC below.',
       'Aliasing candidates model folded m*f for m=1..512 with m*f>24 kHz; ±1-Hz neighborhoods',
       'are used. Bins near unaliased harmonics are excluded from the nonoverlap artifact list.',
       'The finite model cannot separate coincident harmonics/images, capture every infinite HOLD',
       'image, or prove an isolated bin is exclusively aliasing. Frequencies below are identifiable',
       'folded-harmonic candidates; “stronger” never means the other encoding has zero energy.',
       'Reference errors use the same finite original reference table and lookup mode, not an',
       'alias-free continuous waveform. They isolate encoding error; they do not certify alias-free sound.','',
       '| Source | Pitch | Mode | PCM8 stronger: Hz / gap dBFS | XROM stronger: Hz / gap dBFS |',
       '|---|---|---|---|---|']
    for r in aliases:
        fmt=lambda a:f"{a['frequency_hz']:.1f} / {a['magnitude_gap_dbfs']:.2f}" if a else 'none identified'
        lines.append(f"| {r['source']} | {r['pitch']} | {r['mode']} | {fmt(r['pcm8_stronger'])} | {fmt(r['xromd1_stronger'])} |")
    lines+=['','Exact bin levels, ratios and 20–20000-Hz flags: high-pitch-alias-details.json.',
       'Every comparison’s band energy, spectral differences and reference errors: measurements.csv.','',
       '## Listening/navigation','',
       '`audio/{smooth,edge,complex}/{hold,linear}/`: 84 primary two-second mono 48-kHz PCM24 WAVs.',
       '`reels/`: six A/B pitch reels, C1→C7; PCM8, .15s silence, XROM, .25s silence per pitch.',
       '`nulls/`: six AMPLIFIED_DIFFERENCE_x16 diagnostic reels (+24.0824 dB), .25s gaps.',
       '`reels/index.csv`: exact segment times for normal and null reels.',
       '`difference-vs-pitch.csv`: compact summary curves; no styled plotting infrastructure.',
       'All normal audio uses the identical 0.5 scalar; no method-specific gain or normalization.',
       'Phase starts at zero, advances before the first read, wraps every sample; identical increments',
       'and indices for both codecs. HOLD floor lookup; LINEAR ordinary cyclic lerp. No filtering,',
       'BLEP, smoothing, effects, noise shaping or BAND. Null amplification is diagnostic only.',
       'Abrupt boundaries may produce start/stop clicks; judge the sustained portions.','',
       'Factory generation was halted on receipt of this replacement comparison brief. Prior local',
       'work is preserved but no factory source assets were staged or integrated. No production',
       'DSP, GUI, state, parameters, CMake, version, release/tag/assets or GitHub changes were made',
       'for this comparison. No encoder was run or modified for it. Stop here for the codec decision.']
    (OUTPUT/'REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    (OUTPUT/'README.md').write_text('# Fixed-table pitch comparison\n\nStart with the six A/B WAVs in reels/.\n'
       'Each reel plays C1 through C7: PCM8 then XROM-D1, with silence between.\n'
       'The reels/index.csv identifies exact times.\n\n'
       'nulls/ contains AMPLIFIED differences x16 (+24.0824 dB), not normal instrument output.\n'
       'REPORT.md contains the measured conclusions; measurements.csv contains all 42 cases.\n'
       '84 individual WAVs are organized by source and HOLD/LINEAR under audio/.\n'
       'All normal playback is fixed-gain, without effects or level matching.\n',encoding='utf-8')


if __name__=='__main__':main()
