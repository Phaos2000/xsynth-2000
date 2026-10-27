# Current development checkpoint — 2026-10-27

The v0.1.3 release packages the current instrument. Historical packages and tags are retained.

- 69 factory tables: the original 36 plus 33 original procedural additions. The additions comprise28 musical tables and five SFX, including three distinct digital noise types. Nine shared folders serve both waveform browsers.
- Five consonant harmony voicings retain their exact approved opening frames, with rising major, descending minor, returning suspended and pentatonic melodic motion. PACKET RELAY is retained; SILK CHIRP is removed.
- Three additional Harmony tables—19 EDO RISE,24 EDO RISE,31 EDO RISE—cover every equal-division step over one octave. Position transposes only these factory identities;16 frames supply timbre independently. Keyboard tuning and Scala/`.tun` import are separate future work.
- New tables have16 frames and RMS-normalized source cycles. Revised harmony MORPH levels vary by less than0.13 dB in the measured source-cycle sweep. RAIL STEPS has stronger shape transitions, with up to1.92 dB MORPH crossfade dips; its individual STEP frames are normalized. Listening acceptance remains open.
- One Shape / Position destination per VCO: Shape in WAVE mode and Position in ROM/USER. Historical saved identities remain readable; previously dormant cross-mode routes now have the requested source-aware effect.
- Wave-modulation control sharing reduces duplicated envelope/control work across coherent unison members. Oscillator phases, histories and filters remain independent. Wave crossfades and pending route removal use the scalar fallback until settled.

## Measured CPU regression

The supplied numeric patch fixture exercises eight-way unison and EG2-to-Wave modulation. In the paired actual-plugin different-note replay at 48 kHz / 256 samples, mean callback-budget use changed from 49.91% to 21.39% (-57.15%). Polyphony, quality, release and gain were preserved.

The comparison against the previous listening build produced 6,912,000 bit-exact stereo values across four timelines and 64/128/256-sample blocks. A separate same-semantics unoptimized oracle produced another 6,912,000 exact values. The frozen native oracle covers 702 matrix traces plus the patch, with 1,725,696 exact left samples and matching right output. This is offline measurement on the development machine; live Renoise CPU and manual listening remain separate acceptance gates.

The two new self-contained regression suites are included in the portable public build. The broader local development qualification and final build results are recorded below.

The README image is captured from this source's native editor renderer using STEEL, MAJOR TRIAD and MAJOR PENTA. No commercial reference audio, archives or extracted waveform coefficients are included.

## Completed checks

- 74 local native regression suites passed, zero failures. The broader suite resumes after the source-aware routing contract was applied to three historical frozen oracles; DSP bodies were retained. The old destination-label assertion was also updated for the two combined names while retaining enum/default checks. Initial failures and resumed results are preserved in the private audit artifact.
- Final copied plugin: Steinberg validator 47 passed, zero failures.
- Fresh portable GitHub source: plugin build passed its bundled SDK validator (51 checks, zero failures); both new focused regression suites passed.
- Plugin built from the portable public source: 6,912,000 bit-exact stereo values against the final listening candidate.
- Complete 66-table encoded bank and prepared playback data regenerated exactly from baseline36 and the independent formulas.
- README screenshot rendered from the current native editor and visually checked.

The public portable suite and the broader private qualification are distinct. Neither is a live-host CPU or musical acceptance certificate.

## v0.1.2 package qualification

The release changes the version identity to 0.1.2 and refreshes the documentation and screenshot. The versioned plugin passed the bundled SDK validator (51 checks, zero failures) and produced 6,912,000 bit-exact stereo values against the qualified final listening build using the supplied patch. The 74-suite engine qualification above is inherited from that unchanged synthesis implementation; it was not rerun for a version-label change. Both the plugin metadata and the new screenshot identify 0.1.2. Release packages include matching tagged source and SHA-256 checksums.

## v0.1.3 tuning, motion and browser qualification

Seven fresh targeted suites passed: factory presets, plugin/state/automation integration, complete ROM/matrix menus, revised ROM motion, microtonal accuracy, unified Shape/Position and Wave-control sharing. The rebuilt plugin passed the bundled SDK validator (51 checks, zero failures). The supplied CPU regression patch—using unchanged tables—produced6,912,000 bit-exact stereo values against the qualified earlier build. The historical74-suite gate above is not presented as a fresh v0.1.3 full-suite run.

- The original36 and another23 complete prepared playback records remain byte-identical; seven existing tables changed and three were appended without renumbering any ID.
- All five approved harmony opening frames remain byte-identical. The new Position movement is voicing/scale movement, not merely a brightness sweep. OPEN SUS4 shares its pitch collection with PAD SUS4; the triads/pentatonics also share some tones with older PAD chords. Distinct trajectories are intentional; audible uniqueness needs listening.
- TILT CLUSTER's rootBin16 already encoded the correct played-note reference. Competing components previously overtook the root in four frames. The revised root stays at least8.862 dB above all competing bins over241 STEP/MORPH positions. This stabilizes the C4 anchor when playing C4, without hardwiring all keys to C4.
- 22,210,560 native ROM lookups were finite and bounded. Microtonal tests cover every19/24/31 degree, octave endpoints, boundary rounding, cache changes and old-ROM/USER isolation. They include157,696 bit-exact scalar/SIMD/HOLD samples and U1/U8 at44.1/48/96 kHz across all three sample modes. Stored-ratio numerical error was below1e-9 cents; engine relative-pitch checks passed a0.001-cent bound.
- All69 identities are covered exactly once by nine menu folders.138 programmer checks cover both VCOs.24 painted-label checks verify destination-first editing at100%/200% and saved-state roundtrip. Selecting a destination with Source OFF leaves modulation inactive.

The new scale pitch map is isolated to factory IDs66–68 and uses precomputed ratios, without audio-thread allocation or exponentials. No global keyboard tuning, lossy storage, runtime auto-gain or installed-size compression was added. Complete encoded/prepared bank reproduction passed for all69 tables. The README screenshot is rendered from the current native editor and identifies0.1.3.
