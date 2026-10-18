> Historical pre-pitch-extension checkpoint. The pitch-range restriction and
> Master Tune workaround below have been superseded by [PITCH_RANGE.md](PITCH_RANGE.md).
> Current result: 239 parsed / 239 converted / 0 unavailable. Other sonic
> approximations and the outstanding genuine DSSI A/B remain applicable.

# Xsynth-DSSI import compatibility checkpoint

2026-10-17. Xsynth 2000 / Phaos / 0.1.0. Source and tests only.

**File parsing passes. Conversion is partial. Sonic compatibility is NOT approved.**
All 239 entries in the four requested banks parse, but only 177 can be converted
without changing oscillator tuning. The other 62 remain unavailable in the chooser.
Converted entries can still have significant filter, velocity, waveform, sync and
LFO differences. Genuine DSSI A/B and real-time DAW/listening validation remain
outstanding. These results must not be described as full DSSI sound compatibility.

## Using the importer

Click the existing LOAD PATCH button, then **Import Xsynth-DSSI Bank...**. The same
menu retains **Load Patch...** for native presets and original binary Xpatch files.
Select an extensionless or named bank, select a patch, review its conversion notes,
and click Import. Cancel and unavailable entries make no parameter changes.

The name list preserves bank order and duplicate names, with numbered entries.
Percent escapes are decoded; malformed names get a readable fallback and source
line diagnostic. Original encoded names remain in the parsed diagnostic data.
UTF-8 names display as Unicode; older byte names use a Latin-1 display fallback.
SAVE PATCH suggests the imported name, sanitized for a Windows filename.
Names are not added to the frozen native state format: save the converted patch
under that name to retain it after closing the editor.

All parsing, file reads, allocation and selection occur on the editor thread.
The file limit is 4 MiB, with at most 4096 entries and 1024 bytes per line. No bank
text or parser structures enter the audio processor or survive the import dialog.
After selection, the existing parameter-edit workflow applies a fully validated
ordinary 76-value State. Existing live MIDI controllers are retained, just as for
native loading. No permanent bank dependency, hidden import flag, locked engine,
new parameter, or compatibility table is introduced.

## Source authority

Semantics were checked against the **unmodified Xsynth-DSSI 0.9.4 release** available
from the [Debian source archive](https://deb.debian.org/debian/pool/main/x/xsynth-dssi/xsynth-dssi_0.9.4.orig.tar.gz).
The archive SHA-256 is
`D3D24941A509C6193EAC2B2EE3C9A6057C832565A0B66E58C3BD2E89AFD8DC02`.
The local research copy is under `artifacts/dssi/upstream/`.

Relevant upstream files:

- `src/xsynth_data.c`: `xsynth_data_read_patch`, escaped names and field ordering.
- `src/xsynth_ports.c`: parameter ranges and waveform/filter enumerations.
- `src/xsynth_voice_render.c`: output gain, velocity curve, filters, envelopes,
  direct modulation and glide recurrence.
- `src/xsynth_voice_blosc.h`: bandlimited oscillators, amplitudes and sync.
- `src/xsynth_voice.c`: note initialization and per-voice LFO/glide behavior.
- `src/xsynth-dssi.c`, `src/xsynth_synth.c`, `src/xsynth_voice.h`, `src/xsynth.h`:
  four-voice polyphonic default, legato glide, bend range and 64-sample control rate.

The importer and chooser are independently written. No upstream implementation
code is compiled into Xsynth 2000. Mathematical definitions and file semantics
were used as specifications. The downloaded research archive remains unmodified
with its original GPL notices; no additional authorship claim was added to the
product credits. Steve J. Brookes and Phaos credits remain unchanged. Test banks
retain their downloaded contents and source URLs; no new ownership is asserted.

## Formats and corpus results

The accepted declarations are exactly `xsynth-dssi patch format 0 begin` and
`xsynth-dssi patch format 1 begin`, ending with `xsynth-dssi patch end`.
Field order is name, osc1, osc2, sync, balance, lfo, eg1, eg2, vcf, glide, volume.
Blank lines and comment lines beginning with `#` after whitespace are accepted.
Numbers use locale-independent decimal/scientific notation. Locale-specific comma
decimals are not accepted. NaN, infinity, hex numbers and trailing tokens fail.

Format 0 has six envelope numbers: A, D, S, R, oscillator depth, filter depth.
Format 1 inserts velocity sensitivity after R, giving seven numbers. Format 0
explicitly supplies zero velocity sensitivity. Other fields share their meanings;
filter model 2 and waveform 6 are legal regardless of envelope-format version.

| Bank | Discovered | Parsed | Converted | Unsupported conversion | Malformed |
|---|---:|---:|---:|---:|---:|
| [XsynthBank1](https://linuxsynths.com/XsynthBanksDemos/XsynthBank1) | 60 | 60 | 51 | 9 | 0 |
| [XsynthBank3](https://linuxsynths.com/XsynthBanksDemos/XsynthBank3) | 60 | 60 | 49 | 11 | 0 |
| [XsynthBank2018](https://linuxsynths.com/XsynthBanksDemos/XsynthBank2018) | 61 | 61 | 55 | 6 | 0 |
| [MoogLikeBank](https://linuxsynths.com/XsynthBanksDemos/MoogLikeBank) | 58 | 58 | 22 | 36 | 0 |
| **Total** | **239** | **239** | **177** | **62** | **0** |

92 entries are format 0; 147 are format 1. No unsupported declared versions or
malformed records occurred in these banks. All 62 conversion failures are pitch
range incompatibilities, including cases where each oscillator would fit alone
but no shared Master Tune setting can preserve both simultaneously.
Every entry's parsed values, conversion result, native normalized values and
warnings are retained in `tests/dssi/reference/corpus-mapping.txt`.
`artifacts/dssi/unsupported-patches.txt` lists the 62 blocked entries.

## Exact conversion rules

Below, `N(id, v)` is the existing inverse physical mapping `normalized(id, v)`.
No existing parameter ranges or encodings were changed.

| DSSI field | Native target / rule |
|---|---|
| osc1/osc2 pitch | Native physical pitch ratios span 1..4, versus DSSI .25..4. Find a common Master Tune offset `t` within +/-1 semitone, closest to zero, for which both `pitch / 2^(t/12)` fit 1..4. Assign IDs 0/2 via N and Master Tune `(t+1)/2`. If there is no solution, refuse conversion. |
| osc1/osc2 waveform | 0 sine, 1 triangle, 2 saw up, 3 saw down, 4 square, 5 pulse map directly to native wave index/5. DSSI 6 variable-slope triangle maps to symmetric triangle, with a warning when width differs from .5. |
| osc1/osc2 pulse width | IDs 1/3 directly. Width has no effect on the substituted symmetric triangle. |
| oscillator engine | Both CLEAN. DSSI uses minBLEP discontinuity correction and bandlimited slopes; existing CLEAN uses polyBLEP/BLAMP and is closer than raw CLASSIC. They are not identical, especially under sync or high pitches. |
| sync | ID 27 directly. Source is VCO1 and destination VCO2. DSSI sub-sample reset/discontinuity correction differs from native sync; warning shown. |
| balance | ID 7 directly: `(1-b)*osc1 + b*osc2`; Mixer Mode BAL. Verified at 0, .5 and 1. |
| lfo rate/wave/depth | ID 4=N(4, Hz); wave ID 26=index/5; pitch/filter depths IDs 5/6 directly. Hz clock, bipolar, regular waveform, KEY phase. Direct pitch modulation affects VCO2, as in DSSI. |
| EG1 A/D/S/R/pitch/filter | IDs 9/10/11/12/13/14. Rates use N, sustain/pitch are direct, filter depth uses N after the tune compensation described below. EXP curve. |
| EG2 A/D/S/R/pitch/filter | IDs 15/16/17/18/19/20, same rules as EG1. EXP curve; Loop OFF. |
| EG velocity | Format 0: zero. Format 1: least-squares fit of DSSI's nonlinear amplitude response to native linear sensitivity over MIDI velocities 1..127, clipped to 0..1. Each nonzero source sensitivity produces an approximation warning. |
| vcf cutoff | ID 21=N(21, source cutoff / `2^(t/12)`). Full keytrack=1. The same compensation is applied to EG1/2 filter depths because Master Tune also tunes the filter's tracking frequency. Values exceeding the destination range are limited with an explicit warning. |
| vcf resonance | DSSI stores resonance 0..1.995; the native physical parameter is damping. ID 22=N(22, `2-source resonance`), equivalently normalized `source resonance/1.995`. |
| vcf mode 0 | LP, 12 dB/oct. Both use the original Chamberlin state-variable family, `g=min(.825, 2*pi*cutoff/sampleRate)`. |
| vcf mode 1 | LP, 24 dB/oct: two cascaded original stages. |
| vcf mode 2 | LP, 24 dB/oct approximation with an explicit warning. DSSI's nonlinear MVCLPF-3 has a different cutoff warp, feedback and saturation; the source numbers cannot make the existing native filter equivalent. This mapping preserves only the low-pass/four-pole role. |
| glide | DSSI coefficient `g` updates at 64-sample control boundaries. Native base coefficient at 256 samples becomes `1-(1-g)^4`, then N(8, coefficient). Values below the native .01 minimum are limited with a warning. This matches exponential decay rate at 44.1 kHz, not DSSI's stepped trajectory or arbitrary event boundaries. |
| volume | Let `e=1/(2*log10(2))`. DSSI oscillators have half native amplitude, and its output gain is `(2*v)^e/4`. Native volume is therefore `2*v / 8^(1/e)`, making native gain `(2*v)^e/8`. Small DSSI gain-table interpolation differences remain; nonlinear-filter gain is not guaranteed. |

For velocity, define `v=k/127`, `knee=max(k,10)/127` and
`A(k)=2^(12.5*(knee^.32-1))*min(1,k/10)`.
DSSI's idealized amplitude at sensitivity `s` is `A(k)^s`. The imported linear
sensitivity is `clamp(sum((1-v)*(1-A(k)^s))/sum((1-v)^2),0,1)`. Native envelopes
then scale by `1-s_native+s_native*v`. This is an approximation over the range,
not an exact match at every velocity; high sensitivities can hit the native limit.
DSSI's quarter-dB lookup-table interpolation is also absent from the fit.

Envelope coefficients are copied as per-sample coefficients calibrated at
44.1 kHz. DSSI uses them directly at its current sample rate; the native engine
corrects them for consistent seconds across sample rates. Consequently, timing
equivalence at 48/88.2/96 kHz is not claimed. Native double-precision envelope
accumulation, rate caches, curves and release logic are unchanged. Release-tail
cutoff/voice termination also differs from DSSI.

The native LFO is global; DSSI has one LFO per voice. KEY matches a fresh voice's
onset most closely but retriggers the shared native LFO when another note arrives.
No host clock, HOST phase, random LFO, or extra modulation route is enabled.

## Neutral and absent-setting defaults

- XMOD OFF, depth 0, direction 1->2; Mixer BAL; filter LP; keytrack 100%.
- EXP envelopes, EG2 Loop OFF; all six matrix sources OFF, amounts zero.
- Power ON; pitch bend neutral, wheel and pressure zero in the converted State.
  Live controller parameters are not overwritten by the editor load operation.
- Four-voice POLY, LEGATO glide and two-semitone bend range match DSSI's default
  global configuration. Those global settings are not contained in these banks;
  names such as MonoSaw do not justify inventing a saved monophonic setting.
- Master Tune is neutral except for the explicit shared pitch correction above.
  All other extensions retain their existing safe defaults.

No compatibility DSP path was added. Reproducing variable triangle, MVCLPF-3,
nonlinear velocity and per-voice LFO behavior would require more than parameter
translation. Preserving the frozen native state/engines is preferable to silently
changing their meaning. Full compatibility will require an explicit future design
decision about those capabilities and the downward pitch range.

## Tests and retained material

`tests/DssiTests.cpp` independently tests every mapped category for formats 0 and
1, native encode/decode, neutral parameters, balance endpoints, pitch refusal,
common-detune/filter compensation and velocity bounds. Malformed cases cover
missing end, incomplete oscillator, NaN/overflow, unsupported version, truncation,
unknown field, trailing tokens, fractional waveform, invalid glide, malformed
header, malformed/empty names, whitespace, binary NUL, oversized file, and recovery
of a subsequent valid entry. The production UI only applies a selected converted
entry; malformed or canceled imports do not partially apply parameter values.

`tests/DssiUiTests.cpp` exercises the actual Windows chooser template, all names,
unavailable-entry blocking, conversion-note display, modal selection and Cancel.
The existing editor suite passes geometry, credits isolation, control routing,
modulation markers and resizing at 100/125/150/200 percent.

The permanent compatibility set is under `tests/dssi/`:

- Four untouched banks with source identity and hashes.
- Independent format0/format1 fixtures covering every mapped field.
- Full parsed/native mapping and warnings for all 239 entries.
- Nine named source patches covering format 0, strings/pad, velocity, bass,
  bright saw, pulse lead, sync, resonance, glide and MVCLPF approximation.
- **54 IEEE float mono WAVs**, 44.1 kHz, two seconds, without normalization:
  each selected patch has notes 36/60/84, low velocity, chord and glide renders.
- `render-metrics.csv`: peak, RMS, sample-difference RMS and final-half-second RMS.
  These describe native outputs; difference RMS is not a measured aliasing score.
- A SHA-256 manifest. Test replay compares complete WAV bytes and mapping text.
  Recording may add missing new fixtures, but existing mismatches always fail;
  it never overwrites a baseline.

All 177 converted patches additionally render low/middle/high notes for finite,
bounded output. The nine selected patches render high notes at 48, 88.2 and 96 kHz
as well. This establishes deterministic/stable execution, not sonic equivalence.
The resonant-pluck chord reaches the existing output limiter for two of 88200
samples. That retained result is flagged for listening/headroom review; it was
not normalized away or used to justify a DSP change.

### Genuine reference execution

No installed runnable Xsynth-DSSI renderer or usable Linux distribution was found.
The existing `wsl.exe` reports installation usage rather than an installed distro.
No Linux environment, VM, compatibility layer or toolchain was installed. The
upstream source was inspected but was not altered and presented as a reference
audio engine. **No genuine DSSI WAVs exist in this checkpoint, no direct A/B passed,
and listening comparison to genuine DSSI remains outstanding.**

### Native regression and preservation

The existing DSP gate passes **18 groups, zero failures**. Its five prior supporting
suites plus the new DSSI suite pass. The preserved-plugin comparison suite still
passes its 192 comparisons, and retained native WAV/parameter references match.
CLASSIC/CLEAN algorithms, envelope precision, modulation, timing, 32-voice stress
and realtime allocation checks remain unchanged. Native preset loading/saving,
original Xpatch conversion and VST3-state encoding/decoding remain compatible.

All six themes and isolated-registry persistence tests pass. STEEL remains DEFAULT.
The 960x568 main screenshot is **byte-for-byte identical** to the identity checkpoint;
control-geometry exports also match. No module, label, logo, matrix or header moved.
Only the LOAD PATCH command now presents the two file actions.

The locked-file manifest verifies unchanged DSP, parameter definitions, state,
product IDs, metadata, fonts, layout, theme implementation and native audio fixtures.
All 65 preserved reference-source files match their checkpoint manifest. Existing
reference and expanded VST3 binaries and the existing release ZIP retain their
prior hashes. No VST3 product, package or installation was produced in this pass.

## Changed files and verification

Production: new `port/DssiImport.h`, new `port/DssiDialog.h`, updated
`port/Editor.cpp` (file-action menu, chooser, validated application and save-name
suggestion). Build/test wiring: `CMakeLists.txt`, `validate-dsp.ps1`; new
`tests/DssiTests.cpp`, `tests/DssiUiTests.cpp`, `tests/dssi/` fixtures.
Documentation: this report, `tests/dssi/README.md`, README import note.
Research, prior source copies, test logs and preservation manifests are under
`artifacts/dssi/`.

Run `./validate-dsp.ps1` for the offline gate including importer regression.
`artifacts/dssi/check.ps1 -Ui` builds only test/preview targets and runs importer,
chooser and editor checks. The theme test uses its isolated temporary registry
key and removes it afterward. None of these commands builds a VST3 target.

Logs: `artifacts/dssi/final-tests.log`, `dsp-gate.log`, `theme-persistence.log`,
and `preservation.log`. Source checkpoint manifest: `SOURCE-CHECKPOINT.sha256`.

This is the requested source/test stop. Genuine DSSI listening, real-time host
validation, the 62 unsupported pitch combinations, and the documented audible
approximations remain unresolved; release production remains deferred.
