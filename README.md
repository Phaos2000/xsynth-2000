# Xsynth 2000 v0.1.1

Developed by Phaos · https://phaos.neocities.org/ · phaos2000@pm.me

Xsynth 2000 is a digital synthesizer developed into something new from Steve J. Brookes' Xsynth
1.0.2 (1999), with a Windows x64 VST3 implementation and a retro graphical
interface. This branch contains the v0.1.1 sources. The original `v0.1.0` tag
and release downloads remain preserved as a historical checkpoint.

## Instrument

- Two CLASSIC / CLEAN oscillators, −24 to +24 semitones, FM / PM and hard sync.
- Six mixer modes; LP / HP / BP / NOTCH filtering, 12 / 24 dB/oct and keytracking.
- Two expanded envelopes and four complete, independent LFOs.
- Twelve matrix routes, fourteen sources and forty-two destinations.
- Poly/mono modes, glide, master tuning and Unison OFF / 2 / 4 / 8 within a
  fixed 32-physical-voice pool, with dynamic group normalization.
- Historical factory banks, native patches, Xsynth and Xsynth-DSSI import.
- Six themes, remembered resizing and an original fixed bitmap wordmark.

CLASSIC retains direct digital oscillator behavior, including aliasing. CLEAN
reduces aliasing without changing oscillator tuning. The faithful Xsynth VST3
remains a separate product with distinct class IDs. Four LFO editor buttons
select which LFO is shown; all four continue running. Each has independent
Pitch/Filter Depth, with direct pitch affecting both oscillators.

## Controls and presets

Drag knobs vertically; Shift gives fine adjustment. Ordinary VCO Pitch dragging
and wheel ticks use exact semitones. Double-click restores defaults. Matrix
Amount accepts horizontal or vertical dragging with dominant-axis lock, a small
dead zone, Shift fine adjustment and double-click zero. Dropdowns open on a
completed click and persist for selection. Cycling buttons use left-click next
and right-click previous. Source menus group LFO 1–4 without changing saved
source identities or normalized automation.

The LFO Wave menu and mouse wheel share all eight shapes, in this order:
SINE, TRI, SAW UP, SAW DN, SQUARE, PULSE, S&H, SMOOTH. Wheel up advances;
wheel down reverses, clamping at the ends. All four LFOs behave identically.

The header contains INIT and fifty historical records in three banks: forty-two
sounds and eight informational entries, with intentional duplicates. LOAD PATCH
also imports DSSI banks. SAVE PATCH writes `.XseDev` files without overwriting
factory data. Their filename stem provides the displayed patch name; arbitrary
filenames are not embedded in host project state.

Click the logo for About. THEME cycles STEEL (default), 98, INDUSTRIAL,
MULTIMEDIA, 2000 and SAND. Theme is a per-user preference separate from patches.
THEME and website/email clicks keep About open; clicking elsewhere dismisses it.

## Build and validation

See [SOURCE.md](SOURCE.md) for Windows build and fixture setup. The
[current checkpoint](expansion/docs/VALIDATION-CHECKPOINT.md) records the four-LFO,
performance and matrix-menu changes. Full local validation passed 24/24 suites
and Steinberg validation passed 47/0 with optional external fixtures available.

Optimization preserved exact output in 3,168 comparisons and reduced fully
modulated 32-voice processing cost roughly 22–25% in the paired local benchmark.
Two 96 kHz / 32-voice / 128-sample cases remained over realtime. Offline results
do not establish Renoise scheduling or listening quality.

Original `.Xpatch` and DSSI formats 0/1 can be imported; DSSI sound is approximate,
not bit-identical. State/automation migration preserves established meanings.
High-cutoff filtering and direct LFO routing were intentionally corrected;
historical 0.1.0 sound parity is not the current behavior target.

## Repository contents and license

`expansion/` includes production/test source, development notes, historical C
sources, factory resources and deliberate regression fixtures. Required source
snapshots, native-patch/WAV references and fixture CSVs are retained. Generated
builds, audits, benchmark exports, logs, caches and release archives are excluded.
Optional third-party DSSI banks and their audio corpus remain local-only under
the existing fixture policy; see [EXTERNAL-FIXTURES.md](expansion/tests/EXTERNAL-FIXTURES.md).

The existing `external/vst3sdk/` source subset and notices are unchanged. Revisions
are recorded in SDK-COMPONENTS.txt and expansion/SDK_REVISION.txt.

Synthesizer code and original project wordmark: GPL-2.0-or-later; see COPYING,
AUTHOR and CREDITS.md. Original Xsynth: Copyright © 1999 S. J. Brookes. Steinberg
SDK components use MIT; see VST3-SDK-LICENSE.txt. Factory notices and checksums
remain in expansion/factory/. No new release package accompanies this update.
