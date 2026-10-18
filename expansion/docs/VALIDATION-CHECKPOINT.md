# Current development checkpoint

2026-10-18. Xsynth 2000 / Phaos / development version 0.1.0. This is a source
checkpoint, not a newly packaged release. The original v0.1.0 tag is retained.

The subsequent v0.1.1 selector fix and its release validation are documented in
[LFO-WHEEL-FIX.md](LFO-WHEEL-FIX.md). The measurements below describe the preceding
development checkpoint.

- Four complete independent LFOs run continuously at sample rate. Editor tabs
  change the displayed LFO without activating, resetting or retriggering it.
- Twelve matrix routes, fourteen stable internal sources, forty-two destinations.
- XSED v6 state: 147 parameters / 1,184 bytes, with explicit older-state migration
  and preserved source/destination automation meanings.
- Fixed 32-voice pool, Unison OFF / 2 / 4 / 8, deterministic phases and approved
  dynamic group energy/crest normalization.
- Direct LFO pitch reaches both VCOs. Each LFO also has an independent global
  Filter Depth contribution. Continuous modulation remains sample-clocked.
- Corrected high-cutoff filtering transitions to a warm trapezoidal SVF before
  the old Chamberlin path's dangerous near-Nyquist coefficient region.

## Completed validation

The full local development configuration passed 24/24 suites, with the optional
faithful-reference binary and external DSSI corpus present. Steinberg validation
passed 47/0. Public source alone does not include those optional external data.
See ../../SOURCE.md and ../tests/EXTERNAL-FIXTURES.md for setup.

Optimization preserved exact samples in 3,168 comparisons. The 2,304-case
high-cutoff audit passes. Unison's worst average total/AC RMS difference against
OFF remains 0.617463 dB; all 108 long-cycle crest cases matched the current
pre-optimization checkpoint, and default-volume ceiling cases stayed 0/36.
Engine size is 56,784 bytes, shared waveform tables 1,212 bytes, with no realtime
allocation or oscillator/filter quality reduction.

The paired benchmark used 480 combinations: four sample rates, six buffer
sizes, 1/8/16/32 physical voices and five modulation/Unison workloads, warm-up
and five alternating before/after repetitions. Fully modulated 32-voice costs
fell roughly 22–25%. Over-realtime combinations fell from 35 to two. Remaining
cases were 96 kHz / 32 voices / 128 samples with four LFOs/twelve routes, without
and with Unison (1,014.08 and 1,048.82 processing ms per audio second). Actual
Renoise scheduling, headroom and listening remain manual checks.

## Latest GUI/menu cleanup

The 960×568 layout remains. Matrix headings and controls moved down four native
pixels through shared geometry. Title strip, fonts, 19-pixel row pitch, 18-pixel
field height, widths, Amount bars and interaction remain intact. Six-theme pixel
checks confirmed exact field translation and unchanged surrounding interface.

Source display order is OFF, LFO 1–4, EG1, EG2, Velocity, Key, Mod Wheel,
Pressure, Pitch Bend, Note Random, Constant. Display positions map to unchanged
internal identities `0,1,11,12,13,2,3,4,5,6,7,8,9,10`; no host IDs, normalized
values or saved-state encodings were renumbered. Native menu tests inspect all
items/checkmarks and select every item on all twelve slots at four scales.
DSP sources were unchanged by this GUI pass.

## Sources and local outputs

Audit programs and intentional comparison snapshots are source/test inputs.
Generated CSVs, logs, benchmark exports, build trees and release archives are
local outputs and are excluded from Git. Write new outputs under a build tree
or expansion/artifacts/. Dated notes may refer to local audit paths from earlier
passes; those paths are not source dependencies.

Private 0.1.0 was historical comparison material, not a sound-parity target.
Confirmed filter/direct-LFO bugs were deliberately corrected before the exact
optimization comparisons above. Historical references were not regenerated to
conceal those changes. No global installation, packaging or publication was
performed by the validation passes summarized here.
