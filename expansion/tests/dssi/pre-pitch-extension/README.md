# Permanent Xsynth-DSSI compatibility fixtures

These are regression inputs and **Xsynth 2000 outputs**, not genuine DSSI audio.
Full semantics, limits and source references are in `../../DSSI_COMPATIBILITY.md`.

The four files in `banks/` are unchanged downloads from
`https://linuxsynths.com/XsynthBanksDemos/` with their original names. Source patch
names and parsed fields are retained in `reference/corpus-mapping.txt`. Synthetic
`format0.dssi` and `format1.dssi` were authored for this test and are clearly labeled.

| Render prefix | Bank | Zero-based index | Original name | Coverage |
|---|---|---:|---|---|
| strings-velocity | XsynthBank1 | 0 | Quick Strings | String/pad, EG sensitivity, detune, MVCLPF approximation |
| sync-glide | XsynthBank1 | 4 | funnyTrumpet | Hard sync, glide |
| basic-saw | XsynthBank3 | 9 | MonoSaw01 | Basic bright oscillator |
| sync-saws | XsynthBank3 | 42 | SyncSaws | Sync/brightness/high-note behavior |
| pulse-lead | XsynthBank2018 | 2 | BasicPulseMonoLd | Pulse lead |
| resonant-pluck | XsynthBank2018 | 38 | BasicRezPluck | Resonance and envelopes |
| bass | XsynthBank2018 | 49 | HappyBass | Bass |
| moog-filter-approximation | MoogLikeBank | 1 | MoogLike2 | Filter limitation, glide |
| format0-xwobbbly | XsynthBank1 | 6 | Xwobbbly | Real format 0, absent velocity defaults, LFO |

For each patch:

- Sample rate 44100; 88200 mono IEEE float samples; no normalization/dither.
- `-36`, `-60`, `-84`: note at sample 0, MIDI velocity 100/127.
- `-velocity`: note 60 at sample 0, velocity 32/127.
- `-chord`: notes 60/64/67 at sample 0 with velocities 100/127, .65, .7.
- `-glide`: note 60 at sample 0, overlapping note 67 at sample 22050,
  both at 100/127. Uses imported POLY/4/LEGATO settings as supplied.
- All notes off at sample 55125 (1.25 seconds); render through two seconds.
- Pitch wheel centered; modulation wheel/pressure zero; no tempo sync.
- Fresh engine and fixed engine seed `0x7853796e` for every render.

Names such as MonoSaw do not encode voice mode in the historical format. These
fixtures use documented import defaults, not settings inferred from names.

Normal tests compare WAV bytes and the full corpus mapping; mismatches fail.
Explicit `--record` creates only missing assets and compares any existing asset;
it cannot overwrite one. Native `../reference-audio/` is completely separate and
must not be regenerated for this task. `SHA256SUMS.txt` identifies retained files.

`render-metrics.csv` reports native peak/RMS, sample-difference RMS and release-tail
RMS; these are diagnostic measurements, not a substitute for genuine DSSI A/B.
No claim of external sound matching or listening approval accompanies these files.
