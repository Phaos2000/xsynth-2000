# Xsynth 2000 oscillator pitch extension

2026-10-17. Product Xsynth 2000, vendor Phaos, development version 0.1.0.
Source/test checkpoint; no release VST3, package or installation produced.

Both existing Pitch controls now cover **0.25 through 4.0** instead of 1 through
4. All **239 of 239** historical DSSI patches now convert, with zero unavailable
entries. All previous 62 pitch-range failures are resolved. Other documented DSSI
sonic approximations remain unchanged; genuine DSSI A/B remains outstanding.

## Mapping, controls and defaults

For either VCO, normalized value `n` maps to physical ratio:

`ratio = 0.25 * 2^(4*n)`

The inverse is `n = ln(ratio/0.25)/(4*ln(2))`, clamped by the existing normal
parameter API outside the control range. DSSI pitch conversion separately rejects
out-of-range ratios before calling this inverse; valid historical pitch is never
clamped or substituted.

| Ratio | Normalized value | Existing semitone readout |
|---:|---:|---|
| 0.25 | 0 | -24.0 st |
| 0.5 | 0.25 | -12.0 st |
| 1 | 0.5 | +0.0 st |
| 2 | 0.75 | +12.0 st |
| 4 | 1 | +24.0 st |

This is an even four-octave musical control, with unison at the center. Both
default normalized values change from 0 to .5 so their default physical ratio
remains exactly **1.0**. The existing semitone display already represents all
these values clearly, so its font, formatting, rectangle and label did not change.
Both VCOs retain their exact parameter IDs: logical 0/2 and VST3 0x1000/0x1002.
There are no new controls, octave switches, hidden multipliers or compatibility
flags. All other parameter IDs, ranges and defaults remain unchanged.

## Native state migration

Previously, native `XSED` version 1 stored 76 normalized doubles, including the old
pitch mapping `ratio=2^(2*n_old)`. Interpreting those doubles directly with the
new range would retune old patches. The existing header's version byte is now
**2** for new writes. The payload layout and size remain the same: 76 doubles,
616 bytes including the eight-byte header. Reserved header bytes remain zero.

The shared state decoder accepts:

- `XSED` version 1: read and validate all 76 values, then migrate only IDs 0 and 2
  using `n_new=.5+.5*n_old`.
- `XSV3` version 1: read the existing 32-value legacy payload, fill expansion
  defaults and apply the same two pitch conversions.
- `XSED` version 2: read all 76 values without migration.

Every unrelated normalized value is restored identically. Invalid/truncated or
unknown-version data leaves the destination State unchanged. A migrated patch
saved again writes ordinary version 2 state; playback carries no migration flag.
Both processor and controller state loading use the shared header recognizer and
decoder. The VST3 source was compiled as a test object/static target without
linking or generating a VST3 product.

Original `.Xpatch` files already contain physical values and continue to import
through the inverse physical mapping. Their binary schema was not changed.

Tests cover old minimum/default, .1/.37/.5/.9 intermediate values, old maximum,
different simultaneous VCO1/VCO2 settings, four sample rates and legacy/expanded
rendering. An isolated frozen copy of the pre-extension engine in `tests/pre-pitch/`
provides differential validation. The maximum audio difference was **zero**.
All ten actual retained on-disk native version-1 presets also migrate with
identical rendered audio and identical unrelated state.

### Earlier host automation

Unreleased-development host automation generally stores normalized parameter
points independently of the plugin's saved state. Such a point has no state
version marker. The same numerical point now refers to the new four-octave range:
for example old .5 meant ratio 2, while new .5 means ratio 1.

State migration cannot identify and rewrite external host automation. Earlier
Pitch automation curves must be remapped with **`.5+.5*n_old`** to preserve their
previous physical meaning. The endpoint 1 remains ratio 4. Existing IDs remain
unchanged; duplicate parameters and hidden runtime interpretation were not added.
This limitation applies to automation authored with the earlier unreleased builds,
including automation that a host reapplies immediately after restoring a migrated
state. Product version remains 0.1.0; state encoding version is independently 2.

## DSSI direct conversion

Each source oscillator ratio now maps directly via `normalized(id, source_ratio)`.
The importer no longer searches for a shared tune offset and no longer refuses
valid combinations below unity. All 25 pairings of .25/.5/1/2/4 are tested.

Master Tune stays at neutral .5. Source VCF cutoff, EG1 filter depth and EG2 filter
depth pass directly through their existing inverse physical mappings, without
division by an artificial tune factor. Every real corpus entry is checked for
these invariants. No source oscillator value is altered to accommodate the other.

| Bank | Discovered | Parsed | Converted | Unavailable | Patches with warnings | Warning messages |
|---|---:|---:|---:|---:|---:|---:|
| XsynthBank1 | 60 | 60 | 60 | 0 | 60 | 164 |
| XsynthBank3 | 60 | 60 | 60 | 0 | 60 | 188 |
| XsynthBank2018 | 61 | 61 | 61 | 0 | 61 | 228 |
| MoogLikeBank | 58 | 58 | 58 | 0 | 58 | 320 |
| **Total** | **239** | **239** | **239** | **0** | **239** | **900** |

Both formats remain supported: 92 format-0 and 147 format-1 records. No historical
patch remains unsupported in this corpus. Conversion warnings still describe
minBLEP versus CLEAN, variable triangle, sync, nonlinear velocity, MVCLPF-3,
per-voice versus global LFO, glide and historical sample-rate timing differences.
These warnings were not suppressed to achieve 239 conversions. General parser
behavior, name handling, import chooser and neutral extension defaults are unchanged.

## Deliberate DSSI fixture transition

The previous 54 WAVs, mapping text, README and original checksum manifest were
copied **before any changes or new recording** into
`tests/dssi/pre-pitch-extension/`. `ARCHIVE.md` clearly labels this as the old
1..4-range importer output. Original `tests/dssi/reference/` files also remain
untouched. Their original checksums still verify.

The new baseline is **separate**: `tests/dssi/reference-direct-pitch/`. It contains
66 two-second float WAVs and full parsed/native mappings for all 239 patches.
The nine previous source patches retain their exact MIDI sequences; MoogLike1 and
JunoMe add two formerly blocked cases. Each patch has low/middle/high, low-velocity,
chord and glide renders. Tests compare bytes and refuse to overwrite existing
mismatches even when recording is explicitly requested.

Of the 54 comparable old/new WAVs, **34 are byte-identical** and **20 differ**.
The maximum absolute sample difference is `1.9222497940063477e-6`; the comparison
details are in `artifacts/pitch-range/dssi-before-after.csv`. Removing the tune
factor and its inverse filter compensation eliminates extra floating-point
operations and can change rounding/phase accumulation. No intentional tuning or
filter change was made to already representable patches. The new baseline records
the final direct mapping instead of normalizing or masking those differences.
The 12 additional WAVs document previously unrepresentable patches.

**Normal native DSP reference WAVs and their saved version-1 states were never
replaced or regenerated.** Their preservation hashes still match, and their
render comparisons pass unchanged.

## Focused tests and results

`tests/PitchRangeTests.cpp` covers:

- Exact .25/.5/1/2/4 stops, both directions of conversion, 10001-point monotonic
  mapping sweeps, independent live automation sweeps, endpoints and defaults.
- Version-1 native and XSV3 state migration, version-2 round trips, atomic unknown
  version rejection, frozen pre-extension engine comparison and ten real saved
  native presets.
- **240 oscillator/rate/ratio/note cases**: both oscillators, CLASSIC/CLEAN, five
  ratios, notes 36/69/84, sample rates 44.1/48/88.2/96 kHz. Each compares block sizes
  32/128/512/1024, checks finite bounded output/internal state and measures the
  generated sine's actual frequency, energy and DC independently of filter/VCA.
- FM/PM at .25:4, .5:2 and .25:.5, both directions, sync on/off, every mixed engine
  pairing, all four rates and full XMOD depth. Phase/filter state remains finite
  and bounded; through-zero FM cases do not introduce invalid wrapping. Zero depth
  remains identical to OFF.
- Direct LFO/EG pitch, matrix-to-both-VCO pitch and pitch bend at .25 and 4 bases.
  These retain their existing downstream frequency equations and safety behavior.
- Audio allocation counts remain unchanged through FM/PM/sync processing.
  DSP instance size remains **13240 bytes**, exactly equal to the frozen old engine.
  No new runtime table or per-instance compatibility storage was added.

The full native gate passes **18 groups, zero failures**, with all **seven**
supporting CTest suites passing. The preserved reference-plugin comparisons pass
all **192 cases**. CLASSIC/CLEAN, envelope precision, filters/keytrack, modulation,
timing, allocation and 32-voice stress remain valid. Existing native `1..4` physical
settings continue to produce their previous sound; only their normalized positions
change. Tests targeting retained fixtures translate their pitch coordinates rather
than changing WAVs.

The new readout test captures actual rendered text at all five stops in all six
themes, compares complete glyph masks and verifies the Tahoma text fits its
unchanged 54x12 rectangle. Existing editor checks pass at 100/125/150/200 percent,
and the six-theme/persistence suite passes with STEEL still the default.

## Preserved GUI, identity and reference product

No production GUI file or layout geometry changed. The exported coordinate table
is byte-identical to the completed geometry audit. The 960x568 main preview differs
at just **142 pixels**, confined to the two Pitch pointers: physical unison is now
the centered normalized setting. Every other main-panel pixel is identical,
including readouts, labels, title, header and matrix. About styling and text remain
unchanged. Theme palettes, theme selector and preference implementation are frozen.

Product Xsynth 2000, vendor Phaos, version 0.1.0 and both class IDs remain unchanged:

- Processor: `7923268C1C9D43DEAA65DAAC78C5232D`
- Controller: `0C0B572A7CD34DC595F21C4122650E26`

The faithful Xsynth has not been edited. All 65 preserved source checksums match.
Its binary, the prior expanded VST3 and the existing ZIP retain their previous
hashes. No VST3 target was built, no package was created and nothing was installed.

## Changed files and repeatability

Production changes are limited to:

- `port/Core.h`: two pitch specs/defaults, state version/header recognition and
  two-value migration.
- `port/DssiImport.h`: direct pitch and removal of tune/filter workaround.
- `port/Plugin.cpp`: use the shared state-header recognizer when reading streams.

Test/build changes: `CMakeLists.txt`, `validate-dsp.ps1`, new
`tests/PitchRangeTests.cpp`, frozen `tests/pre-pitch/` source, updates to
`tests/DspValidation.cpp`, `ReferencePluginDspTests.cpp`, `ExpansionTests.cpp`,
`DssiTests.cpp` and `EditorTests.cpp`, plus the deliberately separate DSSI baselines.
Documentation: this report, README, the earlier DSSI report's supersession note,
current DSSI fixture README and archive label. Research sources and banks unchanged.

Verification records are in `artifacts/pitch-range/`: `dsp-gate.log`,
`final-tests.log`, `final-core-tests.log`, `themes.log`, `preservation.log`,
`dssi-before-after.csv`, `main-steel.png`, `geometry.csv`, prior source copies and
source/fixture checksum manifests. `check.ps1 -Ui` builds only tests, preview and
the VST3 source compile-check object target. `verify.ps1` checks frozen code,
native audio, prior DSSI assets, GUI pixels, identity and existing binaries.

The native gate can be repeated with `./validate-dsp.ps1`; no recording option is
needed. `tests/dssi/reference-direct-pitch/` is now the DSSI replay target.

Remaining limits are the earlier DSSI sonic approximations and external automation
implication above. Genuine DSSI A/B and real-time DAW/listening validation remain
the next stage. No work on those separate sonic differences was included here.
