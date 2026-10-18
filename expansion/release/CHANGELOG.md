# Xsynth 2000 0.1.1

- Fixed LFO Wave mouse-wheel traversal: all eight menu shapes, including S&H
  and SMOOTH, are reachable on all four LFOs. Endpoints retain clamping.
- Menu and wheel share the same existing classic/random parameter encoding.
  This interaction fix does not change DSP, parameter IDs, state or automation.
- Added exhaustive production-editor menu/wheel tests for every starting shape,
  both directions, all four LFOs and four display scales.

Also includes the development work since the historical v0.1.0 checkpoint:

- Four full LFOs, twelve modulation routes and compatible source extensions.
- Unison group normalization, corrected direct pitch routing/high-cutoff filter,
  and behavior-preserving performance optimization.
- Final matrix header spacing and LFO-grouped Source menus.
- Retained regression inputs and excluded generated/local outputs.

The existing v0.1.0 tag/history remain unchanged. See
expansion/docs/VALIDATION-CHECKPOINT.md for current validation and limitations.

---

# Xsynth 2000 0.1.0

First public Xsynth 2000 release, developed from Steve J. Brookes' Xsynth 1.0.2.

- Two oscillators with independent CLASSIC/CLEAN rendering, extended tuning,
  FM/PM and hard sync; six digital mixer modes.
- Expanded filtering, envelopes, tempo-synced LFO, six modulation routes,
  poly/mono voice control and glide.
- Native Windows x64 VST3 implementation with resizable GUI, six themes,
  original geometric wordmark and About/contact panel.
- INIT, three historical preset banks, user patch workflow and Xsynth-DSSI
  format 0/1 import. Normal pitch dragging uses semitones; Shift remains fine.

DSSI sonic compatibility is approximate. External automation from unreleased
builds predating the pitch-range extension may need adjustment; see README.md.
Host state stores sound parameters but does not preserve arbitrary user filenames.
