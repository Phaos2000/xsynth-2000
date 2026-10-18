# Xsynth 2000 — product naming update

Product: **Xsynth 2000**. Vendor: **Phaos**. Development version: **0.1.0**.
The faithful/reference product remains **Xsynth**, preserved independently.

Xsynth 2000 is a new synthesizer developed by Phaos from Xsynth 1.0.2 by
Steve J. Brookes. The factory processor/controller names, VST3 bundle, Windows
file metadata, GUI/preview title, About/Credits, README and release package now
use the approved product name.

## Logo and small version

The header reads **XSYNTH 2000 v0.1.0**. XSYNTH and 2000 use the same existing
approved CRYPGRAU bitmap charset. The existing XSYNTH pixels are untouched;
the 2 and 0 were extracted directly from that sheet's original numeric glyphs.
No substitute font, filtering or reconstructed approximation was used.

The full bitmap is 336 × 38: six original 32-pixel letter cells, a 16-pixel
word gap, and four original 32-pixel numeric cells. It retains integer scaling,
the crisp shadow, theme recoloring and equal header padding. The development
label moved into the remaining header space to accommodate the longer name.

**v0.1.0 remains the original small Tahoma text**, immediately after 2000 and
on the original lower baseline. Its 51 native foreground pixels are identical
to the previous version text, translated 144 pixels right. This was checked
against the prior screenshot. The version is not part of the bitmap lettering.

About uses the full XSYNTH 2000 bitmap at the largest integer scale fitting the
existing credits panel. It identifies Phaos as developer of the new synthesizer,
and credits Steve J. Brookes and Xsynth 1.0.2 as the source. Normal UI fonts and
all synth controls retain their geometry. STEEL remains the default.

## Compatibility and outputs

The existing expansion processor/controller CIDs, host parameter IDs, state
formats, preset extension and theme preference key remain stable. Internal
XsynthExpansionDev window/registry names are deliberately retained for continuity.
The public product name is Xsynth 2000. DSP and modulation are unchanged.

- Bundle: `build/VST3/Release/Xsynth 2000.vst3`
- Package: `dist/Xsynth-2000-0.1.0-win64-checkpoint.zip`
- Main preview: `artifacts/themes/STEEL.png`
- Credits preview: `artifacts/themes/about-steel.png`
- Updated theme comparison: `artifacts/themes/five-theme-comparison.png`

Earlier development archives remain historical checkpoints. No system installation
was performed. The renamed bundle continues to coexist with reference Xsynth.

## Validation

Release build succeeds. **8/8 test suites pass; SDK validator 47/47.** The plugin
integration test explicitly checks the new processor/controller names and Phaos
vendor. Module metadata confirms Xsynth 2000 version 0.1.0 and unchanged CIDs.
Title checks verify unchanged original letters, exact source digits and the small
version's unchanged glyph mask/baseline. Scale checks pass at 100/125/150/200%.

All 65 reference source files still match the frozen manifest, and the approved
reference DLL retains SHA256
`FA44525BC553DB86E611E023A790F6BED86FE42F15F41B2C902EDECA64DBEF8F`.
See artifacts/naming-*.log for build, GUI, title and reference checks.
