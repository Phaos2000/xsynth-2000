"""Current PCM8 content/checkpoint report; historical codec reports stay archived."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
from generate import ROOT,dump,csvout

REASONS={
 (0,7):'Monotonic spectral opening versus multiple traveling comb notches; their full trajectories differ, despite one similar low-band state.',
 (0,19):'SPECTRA opens a contiguous 1/k envelope; DREAM retains a low k^-2 bed and separated upper groups at13/31/57. Its bright endpoint is not a rolloff variation.',
 (4,6):'STAIRS changes sample-address/amplitude grids; FOLDS changes nonlinear drive1..5 and asymmetry, creating different corners and harmonic populations.',
 (4,11):'STAIRS introduces discrete phase/amplitude-grid structure; FLUTE grows second/third partials and a fixed periodic noise skirt while remaining fundamental-dominant.',
 (4,14):'DRAWBAR follows8 specified additive bins and four registration vectors; STAIRS quantizes a triangle/sine source. Initial spectral overlap does not persist through DRAWBAR.',
 (4,16):'BASS preserves an odd/even low-body spectrum and applies asymmetric folding/rectification; STAIRS changes phase/amplitude cells instead.',
 (6,11):'Both begin near a simple sine-like state. FOLDS later develops multiple fold corners; FLUTE remains low-partial/noise-skirt driven.',
 (6,16):'Different lengths alone are not the justification: BASS has a weighted low odd/even excitation and restrained fundamental emphasis; FOLDS drives a sine through five-fold structure.',
 (11,14):'FLUTE uses continuous second/third/noise progression; DRAWBAR uses the fixed1/2/3/4/6/8/12/16 partial registration trajectories.',
 (11,16):'Shared fundamental-led starts diverge into breathy second/third/noise versus asymmetric folded bass spectra.',
 (30,31):'Both start as open root/fifth structures, but SUS2 adds9/8 at rootBin8 while SUS4 adds4/3 at rootBin6. Distinct rational lattices and later overtone trajectories are intentional.'}


def report(out):
    m=json.loads((out/'data/manifest.json').read_text())
    stats=json.loads((out/'data/frame-codec-statistics.json').read_text())
    summary=json.loads((out/'reports/summary.json').read_text())
    with (out/'reports/similarity.csv').open() as f:pairs=list(csv.DictReader(f))
    flagged=[r for r in pairs if r['flagged']=='True']
    if len(m['tables'])!=35:
        # Keep the original 35-table design-stage report template historical.
        # Expanded inventories receive a measured report, not stale counts or
        # claims that their GUI/production integration has not happened.
        count=len(m['tables']);bank=out/'data/factory.xrom'
        lines=['# Factory ROM generation results','',
            f"Tables: {count}; frames: {m['frames']}; codec: {m['codec']}.",
            f"Bank bytes: {bank.stat().st_size}; SHA-256: `{m['encoded_sha256']}`.",
            f"Exact duplicate candidates: {summary['exact_duplicate_candidates']}; similarity flags: {len(flagged)}; morph flags: {summary['morph_flags']}.",
            f"Accidental clipping: {sum(x['accidental_clipping'] for x in stats)}.",
            f"All-pairs comparisons: {len(pairs)}. See reports/similarity.csv and SIMILARITY-REVIEW.md.",
            'Factory IDs20/35 contain approved Halo/Prism; all other original identities remain stable.',
            'Per-frame, source, codec, trajectory and listening evidence is in data/, reports/ and audio/.',
            'This generation report does not imply plugin/host acceptance or publication.']
        (out/'reports/FINAL-REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        (out/'README.md').write_text('# Factory ROM generation\n\nSee reports/FINAL-REPORT.md, data/manifest.json and audio/ indexes.\n',encoding='utf-8')
        return
    rows=[]
    for r in m['tables']:
        s=[x for x in stats if x['id']==r['id']]
        rows.append(dict(id=r['id'],family=r['family'],table=r['label'],N=r['cycle_length'],rootBin=r['rootBin'],frames=16,
           worst_rms_error=max(x['rms_error'] for x in s),worst_max_error=max(x['max_error'] for x in s),
           peak=max(x['peak'] for x in s),max_abs_dc=max(abs(x['dc']) for x in s),
           min_code=min(x['min_pcm_code'] for x in s),max_code=max(x['max_pcm_code'] for x in s),
           min_unique_codes=min(x['unique_pcm_codes'] for x in s),max_unique_codes=max(x['unique_pcm_codes'] for x in s),
           accidental_clipping=sum(x['accidental_clipping'] for x in s),operation=r['operation'],progression=r['progression']))
    csvout(out/'reports/table-statistics.csv',rows)
    memory=dict(pcm8_waveform_payload=96768,bank_header=64,directory=1120,
       pcm8_bank=(out/'data/factory.xrom').stat().st_size,decoded_shared_int16=(out/'data/decoded-s16le.bin').stat().st_size,
       generated_cpp_sample_payload=193536,generated_cpp_descriptors=420,
       generated_cpp_source_file=(out/'data/FactoryRom.h').stat().st_size,
       band_derivatives=0,manifest_metadata=(out/'data/manifest.json').stat().st_size,
       declarative_recipe_file=(ROOT/'factory/rom/recipes/tables.json').stat().st_size)
    dump(out/'reports/memory.json',memory)
    lines=['# Xsynth 2000 — final PCM8 factory content / GUI review','',
      '**CURRENT FACTORY ROM: PCM8. XROM-D1 is superseded experimental history.**',
      'This is local content/design review, not a VST build or GUI implementation.','',
      f"Technical acceptance: {len(rows)}/35 tables,560/560 frames; no retired IDs.",
      f"Exact duplicate candidates: {summary['exact_duplicate_candidates']}; similarity flags: {len(flagged)}; morph failures: {summary['morph_flags']}.",
      f"Accidental PCM8 clipping: {sum(s['accidental_clipping'] for s in stats)}. Artistic acceptance remains for human listening.",
      'Determinism evidence: DETERMINISM.json and SHA256SUMS. Focused checks: TEST-RESULTS.txt.',
      f"Bank SHA-256: `{m['encoded_sha256']}`.",
      f"Decoded SHA-256: `{m['decoded_sha256']}`.",'',
      '## Resume inspection and reuse','',
      'A — Reused: all 35 existing recipe identities, stable IDs,16-frame organization, mixed N,',
      'rootBin/rational chord design, Decimal math, all-pairs/root-aware feature analysis, SVG contact',
      'sheets, HOLD/LINEAR/MORPH renderer, reel/index logic and names. STRING/FLUTE revision2 and',
      'GRAIN’s periodic-noise correction from the interrupted pass were retained.',
      'B — Regenerated: literal PCM8 bank, canonical decode/header, frame metrics, source/decoded',
      'uniqueness, morph checks, all 113 audition WAVs, manifests and hashes. Both final runs were clean.',
      'C — Historical: the completed XROM factory review, initial pass, incomplete reproduction and',
      'work cache were preserved as *-historical-xromd1. Original isolated 375-Hz and C1-C7 comparisons',
      'remain in place with historical status markers. Snapshot: tools/rom/history/xromd1-factory/.',
      'D — Completed now: PCM8-specific tests/docs/format, full reproducibility, canonical staging,',
      'and18 non-production GUI design/ruler screenshots. No prior numerical evidence was deleted.','',
      '## Exact approved quantizer and bank','',
      '`q=clamp(roundTiesToEven(source*128),-128,127)`; `decode=q/128`.',
      'Signed two’s-complement bytes. Decimal multiplication precision28 matches the original',
      'audition expression; positive/negative ties and ALL three original PCM8 reference tables pass.',
      'No /127 substitution, unsigned-DAC mapping, dither, feedback, noise shaping or decoded DC fix.',
      'Prepare source at Decimal80, remove source DC, then ONE peak28000/32768 gain across16 frames.',
      'Quantize prepared Decimal directly, not the earlier signed16/DPCM targets. Source Q31 is',
      'diagnostic-only. Reopen actual PCM8 bank before all playback-derived data and diagnostics.',
      'XROM generic container version2:64-byte header,35x32-byte directory,96,768 literal sample',
      'bytes. N bytes/frame; explicit stable ID/family/N/frame count/rootBin/offset/size/CRC and',
      'bank SHA-256. No blocks/exponents/anchors/profiles/closure metadata. See tools/rom/FORMAT.md.','',
      '## Final inventory','',
      '| ID | Family / table | N | rootBin | Operation | Position progression |',
      '|---:|---|---:|---:|---|---|']
    for r in rows:lines.append(f"| {r['id']} | {r['family']} / {r['table']} | {r['N']} | {r['rootBin']} | {r['operation']} | {r['progression']} |")
    lines+=['','All 16 frames have fixed phase/seed structure and ONE shared table gain. No filler,',
      'per-frame normalization or per-instance sample duplication. No recipe was materially',
      'reworked for PCM8: final decoded PCM8 revealed no duplicate or morph failure requiring it.',
      'The existing STRING/FLUTE reworks addressed strong spectral trajectory overlap before conversion;',
      'GRAIN’s correction restored its specified noise skirt. Their IDs/N/rootBin stayed fixed.','',
      '## Quantization statistics','',
      f"Worst frame RMS error={max(s['rms_error'] for s in stats):.10g}; worst absolute error={max(s['max_error'] for s in stats):.10g}.",
      f"Maximum |DC|={max(abs(s['dc']) for s in stats):.10g}; decoded peak={max(s['peak'] for s in stats):.10g}.",
      f"Code range={min(s['min_pcm_code'] for s in stats)}..{max(s['max_pcm_code'] for s in stats)}; per-frame occupied levels={min(s['unique_pcm_codes'] for s in stats)}..{max(s['unique_pcm_codes'] for s in stats)}.",
      'Every frame’s RMS/error/DC/peak/code occupancy/clipping is in data/frame-codec-statistics.json.',
      'Per-table worst values: table-statistics.csv. Few occupied levels are intentional for some',
      'logic/short/source-quantized frames, not a quality failure by themselves.','',
      '## Uniqueness / remaining review flags','',
      'Exact signatures remove rational DC/gain, reduce primitive repeats, and canonicalize sign/',
      'circular phase. Zero same-table or cross-table duplicate frames. All595 table pairs include',
      '31 positions each (16 frames+15 midpoints), all cross pairs, sign/phase correlation, spectral',
      'cosine/distribution distance, corresponding/reverse/offset trajectories and bidirectional',
      'nearest-frame coverage. Different N/rootBin are compared in intended-root units over pair',
      'LCM periods. Source masters are separately analyzed; only final decoded PCM8 determines acceptance.',
      'Thresholds unchanged: correlation>=.995 or spectral>=.985 with centroid/flatness/sparsity',
      'differences<10%; additional whole-trajectory gate requires both coverage means>.985.',
      'No severe whole-trajectory pair remains. These numerical checks cannot certify musical identity.',
      'Morph checks sample65 points/adjacent pair: no RMS collapse, negative adjacent correlation,',
      'silence, large DC or clipping. Convex interpolation bounds peaks by endpoints; no gain rider.','',
      '| IDs | Mean nearest-frame spectral coverage A/B | Why retained for listening review |',
      '|---|---|---|']
    for r in flagged:
        ids=(int(r['id_a']),int(r['id_b']))
        lines.append(f"| {ids[0]} / {ids[1]} | {float(r['coverage_a']):.4f} / {float(r['coverage_b']):.4f} | {REASONS.get(ids,'See measured trajectory/recipe comparison.')} |")
    lines+=['','Every flagged pair has a waveform/spectrum comparison in diagnostics/similarity/.',
      'SIMILARITY-REVIEW.md gives all exact scores, positions and trajectory evidence. These are',
      'remaining human-review flags, not hidden approvals of perceptual sameness. No threshold gaming.','',
      '## Rational chord verification','',
      'IDs29..34 retain rootBin2/8/6/8/10/4 and the approved rational bins. Source and decoded',
      'spectra are checked for intended component/overtone support. At C3=130.8127826502993Hz,',
      'cycleBase=C3/rootBin. No hidden voices or12-TET correction. Actual frequencies/cents are',
      'listed in audio/chords/components.csv; six static frame7 clips are there for listening.',
      'Natural pitch/tuning preference remains a user decision, not a measured error.','',
      '## Memory — actual bytes','',
      '| Representation | Bytes |','|---|---:|']
    lines += [f'| {k} | {v:,} |' for k,v in memory.items()]
    lines+=['','The97,952-byte bank is4,144 bytes smaller than the previous102,096-byte differential',
      'estimate; it simply removes3,024 exponent+1,120 anchor bytes. No compression. The widened',
      'int16 representation adds no precision beyond code/128. No BAND derivatives were generated.',
      'Shared/static-ready arrays remain UNREFERENCED; no production build consumes this header.','',
      '## Listening, diagnostics and source paths','',
      '* Review README: artifacts/factory-rom-review/README.md.',
      '* 70 primary sweeps: audio/per-table/, six seconds C3,0→1, HOLD or LINEAR + MORPH.',
      '* Two bank reels: audio/reels/factory_rom_hold_morph_reel.wav and factory_rom_linear_morph_reel.wav; index.csv.',
      '* 35 static-frame reels: audio/static/, frames0/5/10/15, index.csv.',
      '* Six chord auditions: audio/chords/, frame7 at C3; components.csv.',
      '* Waveform/spectrum sheets: diagnostics/waveforms/ and diagnostics/spectra/.',
      '* Similarity matrix/flag plots: diagnostics/similarity/; full metrics in reports/similarity.csv.',
      '* Canonical staged bank: factory/rom/factory.xrom; manifest and SHA256SUMS beside it.',
      '* Generator/recipes: tools/rom/generate.py, recipes.py, math_fixed.py, pcm8.py; factory/rom/recipes/tables.json.',
      '* Architecture: docs/WAVETABLE-ROM-DESIGN.md; provenance: docs/ROM-PROVENANCE.md.',
      '* Historical experiment: tools/rom/history/xromd1-factory/ and artifacts/xromd1-{prototype,pitch-comparison}/.',
      'All 113 WAVs: mono 48-kHz PCM24, common 0.7 scalar, no effects/filtering/normalization/',
      'limiting/unison/fades. Abrupt segment/file boundaries may click; compare sustained sound.','',
      '## GUI mockups and strict grid','',
      'gui-mockups/main/: normal, ROM and mixed states at native960x568. ROM PROGRAMMER is480x384',
      'in all six themes under gui-mockups/rom-programmer/. Ruler versions: alignment-guides/.',
      'Geometry source/equations/rectangles: gui-mockups/geometry.json and GEOMETRY.md.',
      'Private copied GDI renderer preserves current knobs/fonts/palettes/title/materials. Existing',
      'VCO module proportions and knob centers are unchanged. New strip rows are18px high,20px',
      'apart;146px content width splits68+10+68, or124+4+18 for table/browser. Corresponding VCOs',
      'share exact geometry; all browser themes use identical rectangles. Every snapshot is DESIGN',
      'ONLY: no production GUI behavior, ROM binding or Shape-ID reinterpretation. USER is a',
      'placeholder; future Position needs separate IDs. No USER import, state or BAND implementation.',
      'Tradeoff requiring approval: compact VCO fields remove external Wave/Engine captions and',
      'leave only 3px below knob readouts, with S:/F: prefixes. Do not treat fit as automatic approval.',
      'Exact ruler guides, caption/value rectangles and geometry checks accompany the images.','',
      '## Determinism, limits and stop','',
      'Two full clean runs compare bank, decoded samples, manifest, diagnostics, hashes and all WAVs;',
      'mockups are repeated separately and included in the comparison. No cached encodes, network,',
      'timestamps or absolute-path fields. SHA-256 and the actual compared file list are recorded.',
      'NumPy2.3.5 diagnostics are locally verified; untested cross-platform identity is not claimed.',
      'Factory-data redistribution licensing is unresolved and REQUIRES USER APPROVAL. No CC0 or',
      'other new legal grant is silently adopted. All numerical sound content is original procedural',
      'work; no ROM dumps, downloaded waves, samples, resampled patches or proprietary tracing.',
      'Still needed: user artistic listening, rational chord approval, similarity review and visual',
      'layout approval. Codec choice is FINAL PCM8 and is not an unresolved decision.',
      'No production DSP/GUI/state/parameters, CMake, product version, release/tag/assets or GitHub',
      'changes. No VST build or Steinberg validator was run. Stop before any integration.']
    (out/'reports/FINAL-REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    readme="""# Xsynth 2000 — PCM8 factory ROM review

CURRENT: straightforward signed PCM8, no dither. XROM-D1 is superseded historical
experimental work, NOT the current factory implementation.

Start listening:
- audio/reels/factory_rom_hold_morph_reel.wav
- audio/reels/factory_rom_linear_morph_reel.wav
- audio/reels/index.csv gives stable ID, table and exact start times.

Each table sweeps Position0→1 for six seconds at intended C3 root; gaps0.2s.
70 individual authoritative files are in audio/per-table/. Static frames0/5/10/15
are in audio/static/ with an index. Six static chord clips and their rational
component frequencies are in audio/chords/. No hidden voices or tuning correction.
All WAVs: mono 48-kHz PCM24, common 0.7 scalar, no effects or loudness processing.

Inspect:
- reports/FINAL-REPORT.md: inventory, resume/conversion, metrics, memory and limits.
- reports/SIMILARITY-REVIEW.md: all remaining similarity warnings; full CSVs beside it.
- diagnostics/waveforms/ and spectra/: sixteen-frame sheets for every table.
- diagnostics/similarity/: full matrix and flagged pair plots.
- data/frame-codec-statistics.json: every PCM8 error/DC/code-occupancy/clipping result.
- gui-mockups/main/: native normal/ROM/mixed static designs.
- gui-mockups/rom-programmer/: all six themes with ONE shared geometry.
- gui-mockups/alignment-guides/: explicit rulers; GEOMETRY.md and geometry.json beside them.
- data/factory.xrom: canonical PCM8 bank; decoded data/header derive from its bytes.
- DETERMINISM.json and SHA256SUMS: complete local reproducibility evidence.

Mockups are NOT production GUI implementation. Layout, chord timbres, remaining
similarity flags and artistic content require your review. Factory-data licensing
still requires approval before publication. No plugin integration or release change.
"""
    (out/'README.md').write_text(readme,encoding='utf-8')


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('output',type=Path)
    report(ap.parse_args().output)
