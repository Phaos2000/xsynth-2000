# Xsynth 2000 0.1.3

- Added 19 EDO RISE, 24 EDO RISE and 31 EDO RISE to Harmony, bringing the bank
  to 69 tables. Position traverses every equal-division step from the played
  note to its octave. A precise pitch map applies only to these three factory
  identities; existing tables and USER playback keep their pitch behaviour.
- Preserved the exact approved opening frame of all five new harmony tables,
  adding rising major voicings, descending minor voices, returning suspended
  motion and ascending/descending pentatonic upper lines across Position.
- Made RAIL STEPS traverse six stronger digital shapes with a pitched core.
- Made the played-note component of TILT CLUSTER consistently dominant over
  its non-octave cluster texture. Its C4 reference and rootBin remain correct.
- Unified all 69 factory tables in nine shared folders across the ROM picker
  and waveform programmer. Stored table IDs and provenance families stay intact.
- Fixed destination-first matrix editing: selected destinations display and
  remain checked even with Source OFF; selecting a target never enables a source.
- Retained the other 59 complete playback records, including all original 36,
  PACKET RELAY and the three digital noise types, without changes.
- Added native motion/tuning, complete folder/menu and painted-label regressions.
- Verified full microtonal step coverage, octave endpoints, Position cache
  changes and scalar/SIMD/HOLD equality across sample modes and sample rates.
- Listed Scala/`.tun` keyboard-tuning import as future work; it is not included.

The harmony tables share some chord tones with older PAD chords; their new
voicing/scale trajectories distinguish their intended role. Audible uniqueness
and the revised motion remain subject to manual listening. No lossy storage,
runtime auto-gain or compression change is part of this revision.

---

# Xsynth 2000 0.1.2

- Added the 66-table factory ROM and waveform folders, including 30 original
  procedural additions, five stable harmony voicings and dedicated SFX with
  three digital noise characters. PACKET RELAY stays; SILK CHIRP is removed.
- Added USER waveform slots with native-patch embedding and wavetable reading
  modes. New additions are leveled offline; no runtime auto-gain is applied.
- Combined each VCO's Shape / Position modulation destination according to
  oscillator source, retaining saved destination identities.
- Fixed duplicated unison Wave-modulation control work in the supplied CPU
  patch. The paired 48 kHz / 256-sample different-note replay used about 57%
  less processing with bit-exact audio and unchanged synthesis settings.
- Includes stereo Unison width, up-to-64-member voice budgets, EG2 tempo sync,
  dual-filter routing and ten distortion algorithms with extended Drive.
- Updated the interface screenshot, development checklist and validation notes.

Offline validation does not close the pending manual harmony listening or
live Renoise CPU checks. See expansion/docs/CURRENT-DEVELOPMENT.md.

---

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
