# Current development: transitions and expanded modulation

This document describes unreleased development changes. Published Xsynth 2000
0.1.0 archives, source package, checksums and binaries remain frozen. Product
name, vendor, version and class IDs have not changed. No release is packaged.

## Interaction

Cycling buttons use left-click for next and right-click for previous. A direct
right-click on a cycling field owns that event; the module menu does not open.
Genuine dropdowns retain their popup behavior. Unused module/header areas retain
the existing section menu where supported.

Matrix Source/Destination popups open after the opening click is released.
The release cannot accidentally select an entry. Subsequent entry clicks,
Escape and click-away use the normal Windows menu loop and existing modal
click-through suppression.

Amount fields retain their exact dimensions. Their pale opaque face has black
numeric text; the bottom six of 22 pixels hold a solid theme-accent bipolar fill.
Dragging locks to horizontal or vertical after a three-logical-pixel dead zone.
Right/up increases; left/down decreases. Ties choose horizontal. The chosen axis
stays locked until release; Shift gives the existing ten-times finer sensitivity.

## Audio transitions and LFO

Wave/engine and mixer changes blend signals over two milliseconds at the current
sample rate. Blends retarget from their current weights and sum to unity. They
do not mute/fade the whole synth. Direction/sync have four fixed oscillator phase
histories (two directions x sync on/off); only active blend paths are rendered.
Sync resets and CLASSIC waveform discontinuities remain part of steady synthesis.
Filter type/slope selection blends outputs from the existing filter histories.

The first legacy-to-expanded handoff transfers oscillator phase, envelope value/
stage, filter history and coefficient, and LFO phase. During a sounding session,
returning controls to defaults does not jump back into a frozen legacy renderer.
Reset starts again with the appropriate path. Voice-mode changes keep the selected
sounding voice and retire extra voices individually over two milliseconds.
Returning to poly mode reuses still-held retiring voices with their histories.

Continuous LFO matrix sources now reach pitch at sample rate, without an
eight-sample pitch target staircase. Host LFO frequency is integrated in double
precision instead of multiplying absolute song position by a changing rate.
Only actual seeks/host-phase activation reanchor; stopped HOST phase stays fixed.
Continuous shapes are not low-pass filtered. Square/S&H jumps remain deliberate.
Stepped controller/constant route changes retain an eight-sample ramp; modulation
does not depend on host buffer size. Phase-mode/polarity edits have short local
signal blends; ordinary continuous modulation gains no such edit crossfade lag.

Discrete modulation uses an eight-percent-of-cell hysteresis beyond half-cell
boundaries. Curve edits keep the current envelope value and restart its segment
progress; loop edits do not retrigger a released voice. Global LFO targets combine
active voice contributions by their mean, with causal feedback from the preceding
sample. Other new destinations operate per voice.

## Native-state compatibility

The six historical destination IDs retain their original 18 choices and exact
`round(normalized * 17)` meaning. Their host IDs are `0x103B`, `0x103E`,
`0x1041`, `0x1044`, `0x1047`, `0x104A` (slots 1–6).

Six new IDs `0x104C` through `0x1051` are Expanded Destination parameters.
They have 41 steps / 42 choices: zero follows the legacy parameter; value
`(destination index + 1) / 41` selects index 0–40 from the table below.
All new IDs are appended; every existing ID and all non-destination semantics
remain stable. A live write to a legacy destination releases that slot's expanded
override, including repeated writes of the same value. Otherwise an expanded
write selects its explicit destination (or releases to legacy at zero).
At the same sample, legacy destination events win over expanded events regardless
of host queue order. Conflicting simultaneous automation of both representations
is therefore deterministic. The GUI retains one selector with all 41 choices;
it records legacy choices through the historical parameter and extended choices
through the appended parameter. Returning to a legacy choice also records zero
for the expanded parameter.

Current state is **XSED version 4**, 82 doubles / 664 bytes. V1/V2 read their
original 76 doubles and keep destination normalized values intact. V1 retains
its existing pitch migration. V3 reads 76 doubles, resolves destinations with
`round(value * 40)`, then stores indices below 18 in the historical parameter
on its original `/17` scale; indices 18–40 use the appended override, with a
canonical zero historical value. Non-destination values are unchanged apart
from the existing V1 pitch migration. XSV3 legacy import remains supported.
V4 serializes both representations, making recall and takeover deterministic.
Frozen 0.1.0 cannot read V3/V4 states; no downgrade compatibility is promised.

**Old 0.1.0 destination automation is compatible without manual remapping.**
This follows from preserved live parameter semantics, independently of native
state migration. Host queues and the engine are tested separately from decoding.
The unreleased V3 build's `/40` automation authored against the old IDs cannot
also retain that conflicting interpretation: those IDs now permanently honor
the published `/17` contract. V3 native states do migrate explicitly.

## Destination list

Indices 0â€“17 retain their meanings. The formerly ambiguous LFO Depth entry is
explicitly labeled **LFO Depth (Legacy Both)**: it retains the old shared
multiplicative behavior for compatibility. New Pitch Depth and Filter Depth
entries are independent normalized offsets to their respective controls.

| Index | Destination | Index | Destination |
| ---: | --- | ---: | --- |
| 0 | VCO1 Pitch | 21 | XMOD Direction |
| 1 | VCO1 Shape | 22 | Mixer Mode |
| 2 | VCO1 Level | 23 | VCF Type |
| 3 | VCO2 Pitch | 24 | VCF Slope |
| 4 | VCO2 Shape | 25 | VCF Keytrack |
| 5 | VCO2 Level | 26 | LFO Phase |
| 6 | XMOD Depth | 27 | LFO Polarity |
| 7 | Mixer Balance | 28 | LFO Pitch Depth |
| 8 | VCF Cutoff | 29 | LFO Filter Depth |
| 9 | VCF Resonance | 30 | EG1 Pitch Depth |
| 10 | LFO Rate | 31 | EG1 Filter Depth |
| 11 | LFO Depth (Legacy Both) | 32 | EG1 Velocity |
| 12 | EG1 Attack | 33 | EG1 Curve |
| 13 | EG1 Decay | 34 | EG2 Pitch Depth |
| 14 | EG1 Release | 35 | EG2 Filter Depth |
| 15 | EG2 Attack | 36 | EG2 Velocity |
| 16 | EG2 Decay | 37 | EG2 Curve |
| 17 | EG2 Release | 38 | EG2 Loop |
| 18 | VCO1 Wave | 39 | Glide Time |
| 19 | VCO2 Wave | 40 | Master Tune |
| 20 | XMOD Mode | | |

## Resonance audit

720 cases cover 44.1/48/96 kHz, CLASSIC/CLEAN, four filter types, both slopes,
low/mid/high notes and five resonance settings. Peak/RMS, pre-limiter peak/RMS,
internal states and clipping sample counts are in artifacts/pre-unison/resonance.csv.

All 720 cases match the frozen renderer's output and internal filter states
exactly. No numerical faults/non-finite states occurred. Some high-cutoff 24 dB
cases hit the existing output ceiling even at low resonance; resonant and
near-Nyquist gain can be large. This is existing finite gain into the final hard
ceiling, not a newly introduced internal clip or instability. Filter equations,
resonance range, gain and output ceiling are left unchanged.

Retained audio files are not overwritten. Unaffected reference cases remain
bit-identical. The continuous-LFO matrix reference intentionally follows the new
sample-rate modulation law; its retained old WAV is separately verified against
the frozen source. LFO-affected DSSI references receive the same explicit treatment,
while current imports still undergo mapping, determinism, stability and sample-rate
checks. This does not establish bit-identical DSSI sonic compatibility.

No unison implementation is included.
