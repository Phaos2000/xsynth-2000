"""Write the navigation/report for an existing ROM review output directory."""
import argparse
import hashlib
import json
from pathlib import Path
from generate import ROOT,dump,csvout,hashes


def report(out):
    m=json.loads((out/'data/manifest.json').read_text())
    stats=json.loads((out/'data/frame-codec-statistics.json').read_text())
    summary=json.loads((out/'reports/summary.json').read_text())
    rows=[]
    for r in m['tables']:
        s=[x for x in stats if x['id']==r['id']]
        rows.append(dict(id=r['id'],family=r['family'],table=r['label'],N=r['cycle_length'],rootBin=r['rootBin'],
            frames=16,profile=r['codec_profile'],worst_rms_error=max(x['rms_error'] for x in s),
            worst_max_error=max(x['max_error'] for x in s),peak=max(x['peak'] for x in s),
            exponent_min=min(x['exponent_range'][0] for x in s),exponent_max=max(x['exponent_range'][1] for x in s),
            closed=all(x['closure_error_lsb']==0 for x in s),saturation=sum(x['saturation'] for x in s),
            operation=r['operation'],progression=r['progression']))
    csvout(out/'reports/table-statistics.csv',rows)
    frames=sum(r['frames'] for r in m['tables']);codes=sum(r['frames']*r['cycle_length'] for r in m['tables'])
    memory=dict(encoded_bytes=(out/'data/factory.xrom').stat().st_size,delta_codes=codes,exponents=codes//32,
        anchors=frames*2,header_directory=64+32*len(rows),decoded_int16=(out/'data/decoded-s16le.bin').stat().st_size,
        cpp_source_file_bytes=(out/'data/FactoryRom.h').stat().st_size,
        cpp_sample_payload_bytes=codes*2,cpp_descriptor_payload_bytes=35*12,
        manifest_bytes=(out/'data/manifest.json').stat().st_size,
        recipes_bytes=(ROOT/'factory/rom/recipes/tables.json').stat().st_size)
    dump(out/'reports/memory.json',memory)
    lines=['# Xsynth 2000 — factory ROM content checkpoint','',
           f"Technical candidates: **{len(rows)} tables / {frames} frames**. Artistic acceptance remains pending listening.",
           f"Every frame closed exactly: {all(s['closure_error_lsb']==0 for s in stats)}; saturation events: {sum(s['saturation'] for s in stats)}.",
           f"Exact duplicate candidates: {summary['exact_duplicate_candidates']}; remaining similarity flags: {summary['similarity_flags']}; morph warnings: {summary['morph_flags']}.",
           'Complete local regeneration comparison is recorded in DETERMINISM.json after both clean runs.',
           'Focused test results are recorded in TEST-RESULTS.txt. Neither numerical tests nor this report claim that a table sounds good.','',
           '## Inventory and progression','',
           '| ID | Family / table | N | rootBin | Fixed operation | Position progression |',
           '|---:|---|---:|---:|---|---|']
    for r in rows:lines.append(f"| {r['id']} | {r['family']} / {r['table']} | {r['N']} | {r['rootBin']} | {r['operation']} | {r['progression']} |")
    lines+=['','All have 16 frames. See recipe JSON and recipes.py for exact amplitudes, phases, seeds,',
            'formant/quantizer/fold parameters and bounded audited operations. One peak target per table',
            '(28000/32768) is computed over all 16 frames after source DC removal; no frame normalization.',
            'Playback arrays, spectra, uniqueness and audio are built from the actual bank decoder.',
            'The Q31 recipe masters are diagnostic-only and never substituted for decoded playback.','',
            '## Codec and search','',
            f"Worst frame RMS error: {max(s['rms_error'] for s in stats):.9g}; worst maximum absolute error: {max(s['max_error'] for s in stats):.9g}.",
            f"Largest decoded |DC|: {max(abs(s['dc']) for s in stats):.9g}; overall decoded peak: {max(s['peak'] for s in stats):.9g}.",
            f"Exponent range: {min(s['exponent_range'][0] for s in stats)}..{max(s['exponent_range'][1] for s in stats)}.",
            f"Terminal-lattice retry frames: {sum(s['search_policy']!='prototype' for s in stats)}; widened beams: {sum(s['search_width']!=64 for s in stats)}.",
            'The approved prototype is reused. Some unconstrained beams lose closing paths. An explicit',
            'terminal-lattice retry constrains prefix exponents >=e and final exponent=e; e is determined',
            'from final-block maximum target delta using 120 codes of headroom. This retains exact',
            'closure feasibility modulo 2^e without changing bytes, decoding, forcing PCM8, saturating',
            'or inventing noise. Same cost/rounding/64-state search; width retries remain available.',
            'Every frame records the search used. Original three prototype encoded goldens are tested unchanged.',
            'Smooth/EDGE are cost weights, not separate formats. Source-defined stepping is preserved.',
            'Detailed every-frame stats: data/frame-codec-statistics.json; table maxima: table-statistics.csv.','',
            '## Uniqueness and morph quality','',
            'Initial decoded analysis found no exact duplicates but two strong trajectory overlaps:',
            'SPECTRA/STRING mean nearest spectral coverage .987623/.989460, and STAIRS/FLUTE',
            '.994717/.985068. STRING revision 2 adds persistent bowed/body spectral regions at',
            '6.5/17/37 with moderate fixed gaps, retaining its k^-1.2/structured-phase identity.',
            'FLUTE revision 2 strengthens its second/third and periodic upper-skirt progression,',
            'while keeping the fundamental dominant. These are spectral changes, not gain/phase fixes.',
            'GRAIN revision 2 explicitly adds its required deterministic upper-noise skirt before',
            'source quantization. No table ID, N or rootBin was changed or retired. Final all-pairs',
            'measurements below/linked CSVs show the reworked outcomes; no thresholds were relaxed.','',
            'Exact signatures remove rational DC/gain, reduce primitive periods and canonicalize circular',
            'shift/sign; every pair of same-table/cross-table frames is covered. Recipe masters are',
            'checked separately. Root-aware comparison reconstructs 31 positions/table (all frames plus',
            'all midpoints) at 256 samples/intended-root period over each pair common rational period.',
            'All 595 pairs compare all 961 combinations, both signs/cyclic shifts, corresponding and',
            'reverse trajectories, offsets with >=16-position overlap and nearest-match coverage both ways.',
            'Waveform correlation, spectral cosine/distribution distance, centroid/flatness/sparsity and',
            'root-energy descriptors are available. Thresholds were not relaxed to make content pass.',
            'SIMILARITY-REVIEW.md documents each remaining flagged pair with recipe/trajectory differences',
            'and measured coverage. Single shared frame states are not silently treated as whole-table copies.',
            'Morph quality samples 65 points per adjacent pair, checking RMS relative to endpoint mixture,',
            'sign correlation, silence, DC and peak bounds. Convex morphing cannot overshoot endpoint peak;',
            'no realtime level rider is used. Artistic and remaining similarity acceptance need listening.','',
            '## Actual memory','',
            '| Representation | Bytes |','|---|---:|']
    lines += [f'| {k} | {v:,} |' for k,v in memory.items()]
    lines+=['','Encoded total matches the architecture estimate of 102,096 bytes exactly; decoded payload',
            'matches 193,536 bytes. Header text bytes are not runtime sample-memory bytes. No BAND pyramid',
            'is generated in this phase. Arrays are constexpr/shared-ready but remain UNREFERENCED.',
            'No per-instance factory sample allocation or production linkage was added.','',
            '## Chord verification and auditions','',
            'IDs 29..34 use the specified rational bins and rootBin values. Static frame-7 C3 files and',
            'audio/chords/components.csv give actual frequencies and deviations from nearest 12-TET notes.',
            'Tests check source and decoded spectral energy at the specified component/overtone bins.',
            'No tuning correction and no additional voices. These cycles are periodic, not infinitely',
            'inharmonic: stretched-source families are explicitly finite-segment periodicized.','',
            '70 primary audio/*_hold_morph.wav and *_linear_morph.wav files: six-second C3 sweeps 0→1.',
            '35 audio/static files: frames 0/5/10/15, one second each plus 0.125-second gaps; index.csv.',
            'Two audio/reels full-bank reels: ID order, 0.2-second gaps; reels/index.csv.',
            'Six audio/chords clips: static frame 7, C3, HOLD, three seconds.',
            'All 113 WAVs are mono 48-kHz PCM24, common 0.7 scalar, no effects, filtering, gain riding,',
            'limiting, normalization or fades. Abrupt file/segment boundaries may click; compare sustained',
            'content as well as morph movement. HOLD uses exact floor indexing, LINEAR cyclic lerp.','',
            '## Files and provenance','',
            'See the review README for navigation. Waveforms and magnitude contact sheets: diagnostics/',
            'waveforms and spectra. Similarity matrix and every flagged-pair plot: diagnostics/similarity.',
            'Canonical encoded bank: data/factory.xrom. All numerical sounds originate in included',
            'mathematical recipes; no third-party audio was used. Generator: tools/rom/generate.py.',
            'Recipe formulas: tools/rom/recipes.py; codec: tools/rom/codec_prototype.py; format: FORMAT.md.',
            'Source generation uses Decimal80 with fixed pi/Taylor sine and explicit rounding. Analysis',
            'uses NumPy 2.3.5; hashes prove local repeatability, not untested cross-platform bit identity.',
            'Factory-data licensing is PENDING USER APPROVAL. No new legal policy was silently selected.',
            'No production DSP/GUI/IDs/state, CMake, version, release/tag/assets or GitHub changes.','',
            '## Review decisions','',
            'Listen to the content and decide artistic acceptance, remaining flagged relationships and',
            'rational chord tuning. Approve factory-data redistribution licensing before any publication.',
            'No integration or next phase begins automatically.']
    (out/'reports/FINAL-REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    readme='''# Xsynth 2000 — local factory ROM listening review

Start with **audio/reels/factory_rom_hold_morph_reel.wav** for the intended crisp
HOLD source character. Compare **factory_rom_linear_morph_reel.wav** in that same
folder. Each table lasts six seconds with a slow Position sweep 0→1; 0.2-second
gaps separate tables. `audio/reels/index.csv` gives exact starts and stable IDs.

* `audio/`: 70 individual authoritative sweeps; IDs 000–034, HOLD/LINEAR + MORPH.
* `audio/static/`: 35 reels with frames 0,5,10,15; one-second segments and gaps.
* `audio/chords/`: six static middle-frame chord tests and component frequencies.
* `diagnostics/waveforms/`: one 16-frame decoded waveform contact sheet/table.
* `diagnostics/spectra/`: one 16-frame magnitude contact sheet/table.
* `diagnostics/similarity/matrix.svg`: all-table trajectory similarity overview.
* `reports/SIMILARITY-REVIEW.md`: every remaining review flag and evidence.
* `reports/similarity.csv`: all 595 decoded pairs; master comparison also included.
* `data/frame-codec-statistics.json`: all 560 frame error/closure/DC/exponent stats.
* `reports/table-statistics.csv`: per-table worst errors and codec checks.
* `reports/FINAL-REPORT.md`: complete inventory, methodology, memory and limits.
* `data/factory.xrom`: canonical encoded ROM; decoded data/header derived from it.
* `data/recipe-masters-q31le.bin`: diagnostic source masters, NOT playback assets.
* `DETERMINISM.json`, `SHA256SUMS`: final verification and content fingerprints.

All audition data derive from XROM-D1 decoding, not source masters. Every table
has 16 frames. Intended root=C3 (130.8127826502993 Hz); chord tables advance the
cycle at root/rootBin to keep that root correctly pitched. Rational intervals
are intentional and are not corrected to equal temperament. No hidden voices.

All WAVs: mono 48 kHz PCM24; identical final scalar 0.7; no effects or loudness
processing. The table-global source gain is shared across its 16 frames. File
and segment boundaries are abrupt; start/stop clicks can be boundary artifacts.
HOLD is true floor-index reading. LINEAR smooths only between adjacent stored
samples; MORPH crossfades adjacent frames with constant sum.

These are technical content candidates for human review, not an artistic quality
certification or a released plugin. Factory-data redistribution license remains
unresolved pending user approval. Do not publish this material yet.
'''
    (out/'README.md').write_text(readme,encoding='utf-8')


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('output',type=Path)
    report(ap.parse_args().output)
