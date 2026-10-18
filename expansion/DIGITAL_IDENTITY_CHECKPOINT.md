# Digital identity and balanced layout checkpoint — 2026-10-17

Current product name: **Xsynth 2000**, vendor **Phaos**, development **0.1.0**.
See NAMING_UPDATE.md for the naming and logo update; earlier names below describe historical checkpoints.

This is the current separate expansion development build, vendor **Phaos**,
temporary product name **Xsynth Expansion DEV**, version **0.1.0**. No final
public name has been selected. The reference Xsynth remains preserved separately.
The supplied GUI-lock brief and subsequent spacing instructions are implemented.

![Current demonstration patch](artifacts/expansion-layout-100.png)

## GUI

The later theme pass is documented in THEMES.md: centred EG1 Curve, unchanged
default STEEL, five exact-palette alternatives, separate UI preference, and a
fresh Release build with eight passing suites. It supersedes preview-only status.

The subsequent final optical pass is complete and geometry is now frozen. See
GUI_GEOMETRY_FROZEN.md for the subsequent requested logo shadow, strict shared
selector grid, matching Curve fields, frozen screenshots, and preview validation.

Alignment follow-up: the four middle modules now use shared constexpr row
templates with 12-pixel content margins, equal 102-pixel selector halves and
36-pixel selector row spacing. Their knob readouts are uniformly 48 × 12,
centred under the original-size knobs. Three- and four-knob rows share their
outer readout edges; both EGs use identical local coordinates. VOICE's two knobs
align with the centres of its split selector columns. Phase/Polarity, the lower
EG readouts, and Voices/Glide Mode share a field-top baseline; their labels also
align. The full panel remains 960 × 568.

This follow-up is verified in the native preview/editor tests and renders at
100/125/150/200%. Logs: artifacts/alignment-preview-build.log and
artifacts/alignment-gui-verification.log. The packaged VST3 below predates this
micro-layout follow-up; the source and displayed preview include it.

The approved single-page audio/modulation structure remains. Native dimensions
are **960 × 568**, down from 960 × 596. The top row is unchanged. All four middle
modules retain balanced lower controls: EG1 Curve, EG2 Curve/Loop, and Voice
Bend Range share a bottom selector baseline. LFO Phase/Polarity are raised to
align with the envelope Velocity/Pitch Depth readouts. LFO Wave/Clock and Voice Mode have moved
up together; the second envelope knob rows have moved up by 28 pixels. Bottom
padding is consistent. Knob diameter, horizontal panel proportions and both
six-route matrix columns are retained. The matrix has moved up by 28 pixels.

The approved **CRYPGRAU** metallic bitmap logo is retained; the user's correction
supersedes the brief's older DNS font wording. Tahoma remains crisp, with selective
surface dithering and unchanged teal/steel bevels. No text is globally dithered.
Load/save stay in the header. No controls have been added to fill space.

Tiny cyan knob-label markers now highlight the associated matrix row(s) on hover.
Leaving restores the normal display without touching any parameters. Amounts show
signed semitones for oscillator pitch, octaves for cutoff/LFO rate, and percentages
for the remaining destinations. Synced LFO divisions display compactly, including
`1/8T` and `1/8D`. No new synthesis controls or parameters were added.

## Oscillators and digital character

- **CLASSIC** retains direct digital waveform generation. Removed the expanded
  renderer's hidden ±0.45-cycle frequency clamp: high pitch and through-zero FM
  now advance/wrap the actual phase and may fold audibly.
- **CLEAN** is independently selectable per oscillator. Saw, square and pulse
  use local polynomial edge correction; triangle uses phase-evaluated cubic
  corner correction instead of a leaky square-wave integrator. Sine retains the
  shared original lookup. Pulse-width endpoints retain their exact DC limits.
- Engine selectors alone no longer force the whole synth onto a different signal
  architecture. Tests confirm that switching a muted, disconnected oscillator to
  CLEAN does not change the active oscillator's output, in either signal path.
- Actual FM/PM coupling can naturally transmit one oscillator's rendering changes
  to the other. CLEAN does not promise alias-free FM/PM or sync: no global
  oversampling, frequency limiting, warmth, drift or analog modeling was added.

The corner correction follows the integral-of-BLEP approach discussed in
[Esqueda, Välimäki and Bilbao, DAFx 2016](https://www.dafx.de/paper-archive/2016/dafxpapers/18-DAFx-16_paper_33-PN.pdf).
This implementation is a compact two-sided cubic, not that paper's four-point
algorithm. It adds no waveform bank or per-voice triangle accumulator.

## Working architecture

The existing feature set is retained: fixed 32-voice pool; mono retrigger/legato
and 1/2/4/8/16/32 polyphony; per-oscillator CLASSIC/CLEAN; OFF/FM/PM in either
direction; VCO1-to-VCO2 hard sync; BAL/SUM/RING/AM/XOR/DIFF mixing; digital
LP/HP/BP/NOTCH with one/two filter stages and keytracking; Hz/synced LFO with
FREE/KEY/HOST phase, BI/UNI polarity and random waveforms; two curved,
velocity-sensitive envelopes, EG2 looping; glide, tuning and bend settings.

The matrix retains six fixed per-voice routes, 11 sources and 18 destinations.
Active routes and velocity sensitivities are cached when their settings change.
Matrix targets update every eight samples with interpolation; unrelated gain
automation no longer restarts this cadence. FM/PM remains audio-rate. Per-voice
LFO-rate contributions are averaged only when that destination is routed.
Continuous host automation and the existing allocation-free state exchange remain.
No effects, unison, PD, FFT synthesis or speculative infrastructure were added.

Native `.XseDev` state retains the entire expansion. Original `.Xpatch` and
reference processor-state imports initialize additions to safe defaults. The
complete parameter/default table remains in EXPANSION_CHECKPOINT.md.

## Efficiency findings

Current `sizeof(xs::Engine)` is **12,088 bytes (11.80 KiB)** for the complete
fixed voice pool and DSP state, down from 12,256 bytes. Shared legacy lookup
storage is **1,212 bytes** per loaded module, down from 1,616 bytes after removing
the duplicate sine table. Audio rendering/note/release tests detect no heap
allocation. DSP remains float; host values and musical timeline retain double.

Header materials/logo/buttons are now cached along with the existing background.
Unchanged GUI frames skip rendering; unchanged resize requests skip surface
reallocation. Editor close releases its cached surfaces. Native frame plus cache
storage is **4.16 MiB**, excluding GDI/SDK overhead; 200% uses about 16.64 MiB.

Local offline profile at 48 kHz, median of three one-second renders after warmup:

| Case | Milliseconds per audio second |
|---|---:|
| Legacy-compatible | 1.50 |
| One RAW voice | 2.65 |
| One CLEAN voice | 3.27 |
| Eight hybrid PM voices, six routes | 21.61 |
| 32 RAW voices | 73.46 |
| 32 CLEAN voices | 88.94 |
| 32 CLEAN PM voices, six routes | 106.53 |
| Eight hybrid FM voices, six routes, per-sample gain automation | 35.01 |

These saw/pulse diagnostic workloads measure local elapsed render time, not DAW
CPU-meter percentages or realtime deadline guarantees. They differ from the older
mostly-sine benchmark. The existing expansion test's mostly-sine 32-CLEAN render
measured 72.26 ms/audio second on this run. See artifacts/digital-profile.csv.
Per-voice oscillator/filter work and routed coefficient updates remain the main
likely DSP costs; automation-heavy state changes also cost more than steady state.

## Validation and preserved reference

- Release build successful; official VST3 SDK validator **47 passed, zero failed**.
- **Seven of seven suites pass**: core, expansion, digital identity, original-C
  differential comparison, native editor, plugin integration, and coexistence.
- New digital tests cover independently mixed oscillator modes, both FM/PM
  directions with sync, RAW high-frequency folding, pulse endpoints, phase-based
  triangle rendering and modulation cadence under gain automation.
- Editor tests cover 100/125/150/200%, modal isolation, resize/reopen, amount
  dragging/reset, hard-sync clicks, and marker highlighting/restoration without
  parameter edits. Plugin tests cover state, host automation offsets/ramping,
  editor sizing and block sizes. Both plugins load concurrently with distinct IDs.
- Render verification passes at all four scales: expected dimensions, exact
  integer-scaled title pixels, preserved text/pointers, and deterministic dither.
- All **65 reference source files** still match the frozen source manifest.
  The approved reference DLL remains byte-identical, SHA256
  `FA44525BC553DB86E611E023A790F6BED86FE42F15F41B2C902EDECA64DBEF8F`.
  Its frozen archive also retains the recorded original hash.

Build logs and profile are under artifacts/. The packaged development bundle is
dist/XsynthExpansionDev-0.1.0-win64-checkpoint.zip. Nothing was installed into
Program Files. The preceding expansion package remains under
checkpoints/digital-identity-before/. The original reference checkpoint remains
outside expansion/, under ../checkpoints/reference-1.0.2.1/.

## Remaining review

The updated full-panel screenshot is ready for visual balance review. No clipping
was observed in the rendered scale checks. Manual DAW listening, project recall,
popup workflow and long-session checks remain for the user's host test.

CC1 retains its original direct filter-closing action alongside its matrix-source
role. The cutoff readout is the legacy A4 target, subject to the existing filter
stability cap, rather than a measured corner frequency. These behaviors have not
been silently redesigned. Public naming and the documented bitmap artwork
provenance remain unchanged decisions outside this pass.
