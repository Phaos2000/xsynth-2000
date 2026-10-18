# Unison development checkpoint

Historical checkpoint: current state/LFO expansion and the subsequently approved dynamic group normalization are documented in [FOUR_LFO_CHANGES.md](FOUR_LFO_CHANGES.md). The original gain policy below describes the preceding checkpoint.

This is an unreleased development change to Xsynth 2000. The published 0.1.0 release and faithful Xsynth reference remain frozen. Product identity, vendor, version and class IDs are unchanged.

## Parameters and state

New host IDs are appended to the existing contract:

| Parameter | Host ID | Normalized meaning |
| --- | --- | --- |
| Unison | `0x1052` | `0`, `1/3`, `2/3`, `1`: OFF, 2, 4, 8 |
| Detune | `0x1053` | `value * 50` cents total spread |
| Matrix slots 1–6: Unison Detune route | `0x1054`–`0x1059` | 0 follows the existing destination representation; 1 selects Unison Detune |

The 18-choice historical destination IDs retain their `/17` mapping. The six established expanded parameters retain their 41 steps and `(index+1)/41` mapping, with zero following the historical parameter. Unison Detune is appended as effective destination index 41, bringing the GUI list to **42 destinations**. Any live write to a historical or established expanded destination releases that slot's new Detune override, including repeated writes. At the same sample, historical destination events take precedence over established expanded events, which take precedence over the new route flag. No state-version or normalized-value guessing is used for live automation.

Native state is **XSED v5**, 90 doubles / 728 bytes. V4 reads its original 82 doubles unchanged and defaults the new parameters to zero. V1/V2/V3 migration and XSV3 import retain the preceding explicit compatibility rules. All older patches load with Unison OFF and Detune zero. No historical bank/source bytes are rewritten.

## Groups and allocation

There is one existing physical voice array of 32 entries. Fixed arrays of 32 logical group records and 32 held-note intents hold ownership metadata; they do not contain additional oscillators. A group uses a contiguous block of physical voices.

Effective group size is `min(requested Unison size, selected VOICES limit)`, treating OFF as one member. Both sets contain powers of two, so there are no partial groups. Poly capacity is `VOICES / effective size`; mono modes use one complete group. The UI shows the requested and effective counts when the voice limit reduces the group size.

Allocation selects an inactive group, otherwise a released group before a gated group, then the oldest group within that class. Stealing replaces the entire group. Every member shares the logical age, note, note ID, velocity and Note Random value. Intent generations distinguish a reused intent slot from an earlier release tail. Matching Note Off releases all members of its group; unspecified IDs retain pitch-based release behavior. All Notes Off releases groups and clears held intents. Engine reset / All Sounds Off clears all groups immediately.

The fixed held-intent capacity is 32. Beyond that, the oldest held intent is discarded deterministically. Reducing capacity retains the newest held groups in poly mode; mono retains highest-note priority. Increasing capacity can restore retained held intents where space permits.

Changing VOICES, UNISON or mono/poly topology rebuilds the held groups within the new physical budget. A two-millisecond interpolation from the previous output value into the rebuilt output protects continuity without keeping extra oscillators alive. Stealing uses the same short interpolation locally to the replaced group. Rebuilt envelopes restart; ordinary mono legato retains member oscillator/envelope/filter identity and uses the existing glide/retrigger rules. Once grouping has been activated, OFF uses one-member groups until reset, avoiding a mid-note renderer handoff.

## Pitch and phase

For member `m` of `N`, pitch position is `(2*m - (N-1))/(N-1)`; OFF uses zero. The cents offset is `position * totalSpread / 2`, applied to both oscillators after the existing note/tuning/modulation pitch path. Relative offsets therefore follow the same glide, bend, tuning, EG, LFO and matrix path. Spread modulation adds to normalized Detune, clamps to 0–50 cents, and uses a sample-clock two-millisecond smoothing time constant. Members retain their ordering.

At maximum spread:

| Members | Cents positions |
| ---: | --- |
| 2 | -25, +25 |
| 4 | -25, -25/3, +25/3, +25 |
| 8 | -25, -125/7, -75/7, -25/7, +25/7, +75/7, +125/7, +25 |

New groups initialize both oscillator phases to the following cycle positions:

| Members | Positions |
| ---: | --- |
| OFF / 1 | 0 |
| 2 | 0, 0.21 |
| 4 | 0, 0.15, 0.30, 0.45 |
| 8 | 0, 0.087, 0.174, 0.261, 0.348, 0.435, 0.522, 0.609 |

These evenly spaced partial arcs balance typical waveform power without using random phase or the complete-circle cancellation of uniform full-cycle phases. Equal-pitch, matching sine/triangle oscillators in RING mode with balance above 90% and XMOD OFF use half these phase positions: that product has a doubled carrier, so its doubled phases receive the same arrangement. The choice is made on group creation; waveform/mode edits and legato do not reset ongoing member phases. Hard sync and XMOD continue to operate normally inside each physical member. Output remains one mono sample, copied identically to stereo channels by the VST3 processor.

## Gain and average level

The normalizer separates the analytically estimated coherent DC term from the audio component. If `A = sum(member outputs)/N` and `D` is the estimated one-member DC response including its envelope, the group output is:

`sqrt(N) * (A - D) + D`

The AC sum factors are 1, `1/sqrt(2)`, 1/2 and `1/sqrt(8)`; the coherent DC sum factors are 1, 1/2, 1/4 and 1/8. Thus pulse, RING and XOR offsets do not receive the AC power boost. A bounded 32-point waveform-moment calculation, updated every 128 samples, estimates nonlinear mixer means; linear mixer/PWM means are exact. Four fixed scalar states per logical group track that DC term through the existing filter response. They generate no oscillators or independent note voices. Mean changes smooth over two milliseconds.

There is no audio-level follower, compressor, new limiter, global gain reduction or normalization of the beat envelope. The original output ceiling remains unchanged. Natural beat minima and increased crest factors are preserved. Average-level measurements use whole beat cycles and record both total RMS and AC RMS; short-window RMS alone can mistake a natural beat trough or DC offset for a normalization error.

## GUI

Only the existing VOICE panel is rearranged, using `moduleGrid`:

- Three-knob row: Glide Time, Detune, Master Tune.
- Full-width Voice Mode row.
- Paired Voices / Unison row.
- Paired Glide Mode / Bend Range row.

Knob dimensions, panel footprint, margins, row baselines, paired widths/gaps, fonts, themes and all neighboring modules remain unchanged. Unison follows left-click next / right-click previous ownership; Detune uses normal knob interaction. The matrix retains one coherent destination selector and the existing Amount control.

## Atomic preset restoration

Native, factory and imported patch selection sends one complete state through the standard VST3 component connection. The audio thread consumes the fixed-size state mailbox and preserves live MIDI bend, wheel and pressure. Only each slot's active destination representation is announced as an automation edit; inactive legacy and expanded fallback values remain intact. This prevents compatibility-field precedence from silently changing a loaded route. Project state restoration retains its existing full-state semantics.

A minimal host without component messaging can still load legacy-only patches through parameter edits. Expanded-routing patches report a restore error before changing the GUI if the host cannot deliver the complete state. Linked regression checks saved DSP state, all three destination representations and live bend independently of the controller cache.

See `artifacts/unison/COMPLETION-REPORT.md` for final measurements, validation logs, preservation checks and the development bundle checksum.
