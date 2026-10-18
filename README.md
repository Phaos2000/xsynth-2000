# Xsynth 2000 v0.1.1

Developed by Phaos · [https://phaos.neocities.org/](https://phaos.neocities.org/) · [phaos2000@pm.me](mailto:phaos2000@pm.me)

Xsynth 2000 started from Steve J. Brookes' Xsynth 1.0.2 from 1999, but has grown into a substantially different digital synthesizer. This version is a Windows x64 VST3 with a new retro-style interface, expanded synthesis engine and a much larger modulation system.

This branch contains the v0.1.1 source. The original `v0.1.0` tag and release downloads are still preserved as a historical checkpoint.

## Instrument

Xsynth 2000 has two oscillators, each with CLASSIC and CLEAN modes, a −24 to +24 semitone pitch range, FM, PM and hard sync. CLASSIC keeps the direct digital character of the original-style oscillator path, including aliasing, while CLEAN reduces aliasing without changing the tuning.

The mixer has six modes, followed by LP, HP, BP and NOTCH filtering with 12 or 24 dB/oct slopes and keytracking. There are two expanded envelopes, four fully independent LFOs, and a twelve-slot modulation matrix with fourteen sources and forty-two destinations.

Voice handling includes polyphonic and monophonic modes, glide, master tuning and Unison OFF / 2 / 4 / 8. Unison works inside a fixed 32-physical-voice pool and uses dynamic group normalization to keep level changes under control while preserving natural beating.

The plugin also includes the historical factory banks, native patch support, Xsynth patch import and Xsynth-DSSI bank import.

The GUI includes six themes, remembered resizing and an original fixed bitmap wordmark. The four LFO buttons simply choose which LFO is being edited; all four continue running in the background. Each LFO has its own Pitch Depth and Filter Depth, with direct pitch modulation affecting both oscillators.

## Controls and presets

Knobs drag vertically, with Shift for fine adjustment. VCO Pitch uses exact semitone steps during normal dragging and mouse-wheel movement; Shift allows continuous fine adjustment. Double-click restores the default value.

Matrix Amount can be dragged either horizontally or vertically. The control locks to the dominant drag direction after a small dead zone, Shift gives fine adjustment, and double-click returns it to zero.

Dropdowns open on a completed click and stay open until an item is chosen or the menu is dismissed. Cycling controls use left-click for the next value and right-click for the previous one.

The modulation Source menus list LFO 1–4 together, while keeping the existing saved source identities and automation behavior intact.

The LFO Wave menu and mouse wheel use the same eight shapes:

**SINE → TRI → SAW UP → SAW DN → SQUARE → PULSE → S&H → SMOOTH**

Wheel up moves forward through the list and wheel down moves backward, clamping at the ends. All four LFOs behave the same way.

The preset header contains INIT and fifty historical records across three banks: forty-two sounds and eight informational entries, including the original intentional duplicates.

`LOAD PATCH` can also import DSSI banks. `SAVE PATCH` writes `.XseDev` files without modifying the factory data. The patch filename becomes the displayed patch name; arbitrary filenames are not stored inside host project state.

Click the logo to open About. `THEME` cycles through:

**STEEL** (default), **98**, **INDUSTRIAL**, **MULTIMEDIA**, **2000**, and **SAND**.

Theme choice is stored as a per-user preference rather than inside patches. Clicking THEME or the website/email links keeps About open; clicking elsewhere closes it.

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
