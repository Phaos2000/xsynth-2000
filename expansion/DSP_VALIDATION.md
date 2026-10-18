# Xsynth 2000 — DSP validation gate

2026-10-17. Product 0.1.0, vendor Phaos.

The automated offline DSP gate PASSED: 18 validation groups and five supporting
CTest suites passed, with zero remaining failures. No new VST3 or release package
was produced during this pass. The previously delivered ZIP predates the envelope
fix described below. Auditory judgement and DAW real-time deadline testing remain
separate from this numerical gate; these results are not a claim of listening approval.

## Findings and correction

One production defect was found and fixed, solely in the expanded engine:
single-precision envelope accumulation lost accuracy at long settings. The
44.1 kHz LINEAR maximum attack was still at 0.994763 after 460,550 samples,
when it should already have transitioned. Envelope value, start, and progress
now accumulate in double precision. Rate caches and audio output remain floats.
This also avoids the rounding floor near a nonzero sustain target, important for
slow exponential EG2 loops. All curves now pass the full timing/sustain/release
checks; slow EG2 A-D/A-D-R runs include 100 seconds of envelope evolution and
40 seconds of release at 96 kHz. The original/reference renderer was not changed.

The first harness also incorrectly treated LFO phases just below 1 and just above
0 as far apart. Its comparison now uses circular phase distance. This was a test
error, not a transport/DSP change. Initial failure logs are retained for audit.
The loop reference patch was changed to a lower cutoff because the initial fully
open filter masked its envelope modulation; the original preliminary WAVs are
archived in artifacts/dsp-gate-initial-reference-set. Final saw references isolate
VCO1 so the CLASSIC/CLEAN pair is directly comparable.

## Coverage and results

| Area | Automated checks and outcome |
|---|---|
| Reference lineage | Sine, saw, pulse, balance, filter, ADSR, glide, sync. Current default/CLASSIC compatibility path versus both frozen source and preserved Xsynth VST3. **192 binary comparisons: zero RMS difference**, across 4 rates × 6 buffers × 8 patches. Original-C differential suite also passes all 11 supplied patches. |
| Oscillators | VCO1 and VCO2 independently; six shapes, CLASSIC/CLEAN, MIDI 33/69/93, four rates. Fundamental projection and off-frequency rejection, RMS, signed DC, shape geometry, pulse widths 0/10/50/90/100%, finite/bounded output. Existing mode-isolation tests cover muted-oscillator independence and all four engine combinations. |
| CLEAN versus CLASSIC | At 7.5 kHz oscillator frequency, selected folded harmonics decrease by approximately **13–24 dB** for triangle, saw up/down, square and pulse. Fundamental amplitude remains **89.6–92.1%** of CLASSIC in those high-frequency tests. This is a targeted spectral measurement, not a total-alias-energy or alias-free guarantee. RAW above-Nyquist folding still passes its existing explicit regression. |
| Sync / FM / PM | MIDI 24/60/96; oscillator pitch multipliers 1, sqrt(2), 2^0.74 and 4; four engine combinations; both directions; FM/PM depths 0, .05, .5, 1; sync enabled; four rates. Finite internal/output state, nonzero energy, exact zero-depth versus OFF, source oscillator isolation. Additional phase-equation tests validate FM's signed excursion and PM's unaltered carrier accumulator. |
| Mixers | BAL/SUM/RING/AM/XOR/DIFF at five balance values. Output matches each actual synthesis-stage equation before filtering/envelope/gain; live mode changes exercised. XOR/DIFF are also in full-polyphony stress. |
| Filter | LP/HP/BP/NOTCH, both slopes, low/middle/high notes, keytracking 0/.5/1, full cutoff/resonance sweeps, four rates. Independent low/high/band/notch response checks and internal-state bounds. These test identity/stability, not identical analogue-model response at every sample rate. |
| Envelopes | EG1/EG2; min/mid/max time controls, LINEAR/EXP/LOG, sustain convergence, release, velocity scaling. Attack wall-clock times compared across rates. EG2 OFF/A-D/A-D-R transitions, long-loop and post-loop release checks. |
| LFO | All six basic shapes plus S&H/smooth random; Hz/Sync; FREE/KEY/HOST; BI/UNI. 1/1, 1/4, 1/8, 1/16 plus dotted/triplet divisions at 60/123/180 BPM. HOST position, stopped transport, seeks, KEY reset, FREE rate, and buffer independence. |
| Matrix | All six slots, all 11 source choices (including OFF/Constant), all 18 destinations, amounts -1/0/+1. Exact source scaling/sign/destination checks; summed routes; eight-sample interpolation and OFF settling; six concurrent routes in renders/stress. Note Random is stable through a note, changes on a new trigger, and repeats from the fixed reset seed. |
| Voices | MONO RETRIGGER/LEGATO and POLY; 1/2/4/8/16/32 caps, stealing stress, high-note priority/fallback, overlapping notes, repeated fast notes, note IDs, all-notes-off/release. OFF/ALWAYS/LEGATO/NON-LEGATO glide gating and motion. All bend ranges and master-tune endpoints/midpoint. |
| Output / safety | Full polyphony, maximum resonance/depth, sync+PM, XOR/DIFF, six routes, extreme modulation. Output finite/bounded, no output denormals, no internal filter runaway above the test bound, no unexpected silence/crash/stuck voices in tested cases. Internal state is inspected so output clipping alone cannot pass an unstable engine. |
| Determinism / buffers | Ten two-second scores × four rates × six buffers, plus same-seed rerenders. Buffer differences stay below 2e-5 peak absolute error; identical-buffer rerenders are sample-identical. Baseline comparisons use 1e-6 tolerance. |

Sample rates: **44,100; 48,000; 88,200; 96,000 Hz**.
Block sizes: **32, 64, 128, 256, 512, 1024 samples**.
The direct binary comparisons exercise actual VST3 event offsets. Expanded render
comparisons exercise the current DSP and per-block transport input; they do not
replace real-time testing of a newly linked VST3 in a DAW.

## Intentional differences and remaining limits

- Default compatibility patches retain the reference renderer. Expanded patches
  use the newer voice/envelope/control implementation and are not expected to be
  bit-identical to the legacy mono engine. The envelope precision correction is
  an intentional sound/timing change for expanded patches.
- CLASSIC stays direct digital and aliased. CLEAN is local waveform correction;
  it does not promise alias-free hard sync or FM/PM. No oversampling, frequency
  cap, DC blocker, normalization, or new output processing was introduced.
- FM is through-zero, up to +/-8 times carrier-frequency excursion. PM depth
  reaches +/-4 cycles. Hard sync remains VCO1 to VCO2; with reverse XMOD it uses
  the previous sample's master-wrap flag to avoid recursive feedback. This
  existing one-sample dependency remains documented and unchanged.
- Natural pulse DC and mode-dependent loudness remain. Retained renders are not
  normalized. The hard-sync clip's signed mean is about .008; the matrix chord's
  is about .023. These are measured properties, not silently filtered away.
- Existing MIDI range 21–96, highest-note mono priority, omni/shared controllers,
  and eight-sample interpolated matrix cadence remain unchanged.
- No serious DSP instability remains **in the tested cases**. Discrete patch/mode
  switches and extreme digital sounds still need listening judgement; the gate
  does not assert that every transient is subjectively pleasing. Long DAW sessions,
  host deadline/jitter, project recall and live automation remain host checks.

## CPU and RAM

Local optimized Windows x64 offline timing, 48 kHz, six routes configured in every
case, one-second timed windows after warm-up. These are elapsed measurements,
not DAW CPU-meter readings or guaranteed instance counts.

| Active voices | CLASSIC, OFF/FM/PM range (ms per audio second) | CLEAN, OFF/FM/PM range |
|---|---:|---:|
| Idle | 1.97–1.98 | 1.97–2.12 |
| 1 | 4.00–4.03 | 4.65–4.81 |
| 8 | 18.22–19.16 | 22.81–23.86 |
| 32 | 70.48–73.18 | 88.69–94.38 |

DSP instance: **13,240 bytes**, previously 12,088; increase **1,152 bytes** for
accurate envelope accumulation across the fixed 32-voice pool. Shared legacy
lookup tables: **1,212 bytes**, not duplicated per instance. No large oscillator
banks or per-note heap storage. Allocation instrumentation reports zero C++ heap
allocations in measured render/note/release paths. Repeated note churn releases
all voice slots. This supports fixed-memory behavior; it is not an OS-wide leak
profiler or proof about unrelated GUI/host allocations.

## Permanent regression assets and rerunning

`tests/reference-audio/` contains ten 48 kHz, two-second mono float WAVs, their
initial `.XseDev` states, `metrics.csv`, a SHA256 manifest, and the score/readme:

1. classic-saw.wav
2. clean-saw.wav
3. hard-sync.wav
4. pm.wav
5. fm.wav
6. xor.wav
7. resonant-filter-sweep.wav
8. synced-lfo.wav
9. looping-eg2.wav
10. matrix-chord.wav

The set is approximately 3.85 MB. Fixed note/velocity/event/automation/tempo and
seed details are in its README and `tests/DspValidation.cpp`. Tests compare
against existing WAVs; they never overwrite an existing baseline. `-Record`
creates missing references only after earlier groups pass. An intentional sound
change requires explicit review and archival of old references.

Run `./validate-dsp.ps1` from expansion. It compiles test executables in the
separate `build-dsp-gate` directory with `XSYNTH_PLUGIN=OFF`, then runs the gate.
It does not compile a VST3. `build.ps1` now runs this gate before a product build;
`package.ps1` rejects a binary older than source and reruns validation before
copying a release. Preview-only builds do not produce a plugin.

Evidence:
- artifacts/dsp-gate-final.log — passing gate and recorded CPU measurements.
- artifacts/dsp-gate-replay.log — final source replay: 18 groups, all existing WAVs match.
- artifacts/dsp-gate-supporting-tests.log — original-C, allocation, mode isolation
  and direct preserved-binary comparison details.
- artifacts/dsp-gate-first.log and dsp-gate-second.log — initial failure evidence.
- artifacts/dsp-gate-reference-integrity.txt — preservation hashes.
- tests/reference/Core.h — frozen, hashed source fixture; original unchanged.

All 65 reference source files match the preservation manifest. Reference VST3
SHA256 remains FA44525BC553DB86E611E023A790F6BED86FE42F15F41B2C902EDECA64DBEF8F.
The existing expansion ZIP remains
6E910FC09C6C14022F17328D6B9C64F39DE9FFBB1D0A3B260806A985813F3E12.
Neither package was replaced, and nothing was installed in the system VST3 folder.
