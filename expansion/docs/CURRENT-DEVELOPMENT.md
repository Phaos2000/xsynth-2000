# Current development checkpoint — 2026-10-27

Main contains the current development instrument. Published v0.1.1 packages and tags are unchanged.

- 66 factory tables: the original 36 plus 30 original procedural additions, organized in folders. The additions comprise 25 musical tables and five SFX, including three distinct digital noise types.
- Five stable harmony voicings: MAJOR TRIAD, MINOR TRIAD, OPEN SUS4, MAJOR PENTA and MINOR PENTA. Position varies voice and octave colour within a fixed pitch collection. PACKET RELAY is retained; SILK CHIRP is removed.
- New tables have 16 frames and RMS-normalized source cycles. New harmony playback levels vary by less than 0.1 dB across the measured Position sweep. Listening acceptance remains open.
- One Shape / Position destination per VCO: Shape in WAVE mode and Position in ROM/USER. Historical saved identities remain readable; previously dormant cross-mode routes now have the requested source-aware effect.
- Wave-modulation control sharing reduces duplicated envelope/control work across coherent unison members. Oscillator phases, histories and filters remain independent. Wave crossfades and pending route removal use the scalar fallback until settled.

## Measured CPU regression

The supplied numeric patch fixture exercises eight-way unison and EG2-to-Wave modulation. In the paired actual-plugin different-note replay at 48 kHz / 256 samples, mean callback-budget use changed from 49.91% to 21.39% (-57.15%). Polyphony, quality, release and gain were preserved.

The comparison against the previous listening build produced 6,912,000 bit-exact stereo values across four timelines and 64/128/256-sample blocks. A separate same-semantics unoptimized oracle produced another 6,912,000 exact values. The frozen native oracle covers 702 matrix traces plus the patch, with 1,725,696 exact left samples and matching right output. This is offline measurement on the development machine; live Renoise CPU and manual listening remain separate acceptance gates.

The two new self-contained regression suites are included in the portable public build. The broader local development qualification and final build results are recorded below after completion.

The README image is captured from this source's native editor renderer using STEEL, MAJOR TRIAD and MAJOR PENTA. No commercial reference audio, archives or extracted waveform coefficients are included.

## Completed checks

- 74 local native regression suites passed, zero failures. The broader suite resumes after the source-aware routing contract was applied to three historical frozen oracles; DSP bodies were retained. The old destination-label assertion was also updated for the two combined names while retaining enum/default checks. Initial failures and resumed results are preserved in the private audit artifact.
- Final copied plugin: Steinberg validator 47 passed, zero failures.
- Fresh portable GitHub source: plugin build passed its bundled SDK validator (51 checks, zero failures); both new focused regression suites passed.
- Plugin built from the portable public source: 6,912,000 bit-exact stereo values against the final listening candidate.
- Complete 66-table encoded bank and prepared playback data regenerated exactly from baseline36 and the independent formulas.
- README screenshot rendered from the current native editor and visually checked.

The public portable suite and the broader private qualification are distinct. Neither is a live-host CPU or musical acceptance certificate.
