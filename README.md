# Xsynth 2000 0.1.0

Developed by Phaos · https://phaos.neocities.org/ · phaos2000@pm.me

Xsynth 2000 is a new digital synthesizer developed from Steve J. Brookes'
Xsynth 1.0.2 (1999). It retains the original project as its historical and code
foundation while substantially developing the synthesis architecture, modulation,
oscillator interaction, filtering, voice system, workflow, Windows VST3
implementation, and graphical interface.

## Requirements and installation

Windows x64 and a VST3 host. Validation was performed on Windows 10 x64; this
release does not provide macOS, Linux or other plugin-format builds.

Copy the entire **Xsynth 2000.vst3** folder into
`C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW if needed.
The faithful **Xsynth.vst3** is a separate product and can remain installed
alongside it. Copy the whole bundle, including Contents/Resources.

## Instrument

- Two oscillators with independent CLASSIC / CLEAN rendering and a pitch range
  of -24 to +24 semitones (0.25x–4x).
- FM / PM oscillator interaction and hard sync.
- BAL / SUM / RING / AM / XOR / DIFF mixing.
- LP / HP / BP / NOTCH filtering, 12 / 24 dB/oct selection and key tracking.
- Expanded envelopes, tempo-synced LFO and six-slot modulation matrix.
- Poly / mono voice modes, glide and master tuning.
- Historical factory library, native user presets and Xsynth-DSSI bank import.
- Six themes, a resizable interface and a fixed original pixel wordmark.

CLASSIC preserves raw, direct digital oscillator behavior; aliasing is an
intentional part of its sound. CLEAN offers reduced oscillator aliasing as an
alternate rendering mode. Neither mode is presented as analog emulation.

## Controls and presets

Drag knobs vertically. Normal VCO Pitch dragging steps by one semitone;
Shift-drag provides continuous fine adjustment. Other knobs also use Shift for
fine adjustment. Double-click a knob to restore its default. Matrix amounts
drag horizontally and double-click to zero. Discrete buttons cycle their values;
dropdown fields open menus. Right-click supported oscillator/envelope modules
for their editing commands.

The header selector contains **INIT** followed by 50 historical records in three
banks. Previous/next arrows move through that list. INIT is the new instrument's
initialization patch, not historical material. The historical records include
42 sounds and eight retained informational entries; duplicates are intentional.

**LOAD PATCH** opens native/original patch loading or **Import Xsynth-DSSI Bank...**.
**SAVE PATCH** writes a user patch without replacing factory data. Native files
use `.XseDev`; their filename stem supplies the displayed name: `My Bass.XseDev`
appears as `My Bass`. Host project state retains sound parameters; an arbitrary
user filename is not embedded in that state and may reopen as CUSTOM.

Click the title to open About. THEME cycles STEEL (default), 98, INDUSTRIAL,
MULTIMEDIA, 2000 and SAND. Theme selection is a per-user UI preference, independent
of patch state and sound. Clicking THEME or a website/email link keeps About open;
click elsewhere to dismiss it.

## Historical compatibility

Original Xsynth `.Xpatch` files and Xsynth-DSSI text patch/bank formats 0 and 1
can be imported. DSSI sound is approximate, not bit-identical: oscillator and
variable-triangle rendering, sync, nonlinear velocity response, MVCLPF-3 filter
behavior, per-voice LFO behavior, glide and historical timing differ. All 239
patches in the tested external compatibility corpus converted successfully;
this does not guarantee every malformed or future file will work identically.
Community banks are external downloads, not included in this distribution.

For users of unreleased development builds only: external automation authored
before the 0.25x–4x oscillator pitch extension used an older normalized mapping.
Legacy native-state migration preserves tuning; external automation lanes may
need adjustment. New users of 0.1.0 are unaffected by that development change.

## License and source

Synthesizer code and original project wordmark: GPL-2.0-or-later. See COPYING,
AUTHOR and CREDITS.md. Original Xsynth code Copyright © 1999 S. J. Brookes.
The incorporated Steinberg SDK components use MIT; their notice is in
VST3-SDK-LICENSE.txt. Historical preset notices, bank descriptions and exact
source checksums are inside the plugin at Contents/Resources/Factory.

The matching `Xsynth-2000-0.1.0-source.zip` is provided alongside this Windows
archive and contains the corresponding source, build instructions and required
redistributable SDK source subset. Keep both downloads available together.
See SOURCE.md for rebuilding and SHA256SUMS.txt alongside the archives for hashes.
