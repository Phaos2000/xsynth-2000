# Four-LFO development checkpoint

This pass changes the current development instrument only. The private 0.1.0 release and faithful Xsynth reference remain comparison material, with their original bytes preserved. Their sound is not a gate that prevents intended corrections. No release is packaged or published.

## Audible corrections

VCO pitch wheels accumulate high-resolution wheel events into complete 120-unit increments. A normal increment rounds to the nearest semitone and advances one semitone, clamped to -24..+24. Positive wheel motion increases pitch. Shift retains continuous fine adjustment. Paint/readouts never quantize imported or automated pitch values.

Direct LFO pitch modulation previously multiplied VCO2's frequency alone, in both synthesis paths. Both VCOs now share the direct frequency multiplier. EG pitch depth retains its established routing. Each LFO contributes its own signed value times its own Pitch Depth. The summed direct multiplier is `max(0, 1 + sum)`; Filter Depth similarly sums all four contributions before the nonnegative cutoff multiplier and the final legal cutoff range. Matrix contributions retain their separate target-range rules. Continuous shapes are evaluated at sample rate.

The original Chamberlin filter at zero resonance uses damping 2 and a maximum coefficient .825. Its Nyquist denominator is `4 - 2*d*g - g*g`; the zero-damping-margin boundary at d=2 is `sqrt(8)-2 = .828427...`. At g=.825, the single-stage LP gain at Nyquist is about 35.13; cascading two stages magnifies this without necessarily producing NaN/Inf. Keytracking pushes high notes into this region, which explains the host-listening distortion.

The low-frequency path remains intact. Between .08 and .12 times the sample rate, output transitions to a trapezoidal state-variable path with prewarped `g=tan(pi*cutoff/sampleRate)`. The transition completes before the old coefficient's dangerous region. The high path supports the same LP/HP/BP/NOTCH and one/two-stage controls. Its cutoff is bounded to .49 times sample rate, below the bilinear Nyquist singularity. Filter histories are independent and carried through the legacy/expanded handoff. This is neither a fixed 7 kHz cap nor output limiting. The Unison DC model follows the same two filter paths.

## Group normalization

The base DC-preserving equal-power law is unchanged: for group average A and modeled coherent DC response D, `AC=sqrt(N)*(A-D)`; DC is added once. Unison OFF remains untouched. There is no fixed per-count attenuation.

Following approval for gentle dynamic group normalization, member output powers provide a reference: `R=max(0,mean(member^2)-2*D*A+D^2)`. Twenty-millisecond power estimates track R and ACÂ². A partial gain target `(R/P)^.35`, bounded to .35..2, softens reinforcement while retaining beat minima. Gain smooths with 3 ms attack and 35 ms release. Two-second reference/output power estimates provide bounded .8..2 makeup, smoothed over 35 ms, to retain average level. A further group-relative crest guard tracks the individual members' peak envelope with a 50 ms release. The group output, including DC, stays within sqrt(2) times that reference (or its modeled DC magnitude). Guard gain falls immediately when required and recovers over 8 ms. This is intentional dynamic group peak/energy control, not a master output ceiling. Epsilon 1e-14 prevents silent-state division. Gain targets update every 8 samples, makeup targets every 128; smoothing and power estimates advance every sample. There is no new output brickwall limiter, random phase initialization or additional oscillator cloud.

Fresh 4/8-member phase positions now use `fraction(m*m/(2*N))`, a deterministic quadratic arrangement that reduces initial coherence without attenuating average level. Two-member phase positions remain 0 and .21. Both oscillators share the arrangement, with the established half-phase rule for equal-pitch sine/triangle RING. Cents mapping, 32 physical voices, whole-group ownership/stealing, mono/legato/glide behavior and mono output remain as at the Unison checkpoint. Fresh groups reset all normalization metadata. The abandoned fixed-attenuation trial and earlier phase candidates remain audit material. The quadratic arrangement is retained alongside the dynamic normalizer; phases alone cannot prevent late reconvergence.

## Four LFOs and twelve routes

Every LFO has nine independent fields: Rate, Pitch Depth, Filter Depth, Wave, Clock, Phase, Polarity, Digital Wave and Division. Each has a double-precision phase accumulator, phase/polarity transitions, host anchoring and independent deterministic random state. All four run at sample rate, including hidden editor tabs. Selection is GUI state only and never sends a synthesis edit or retriggers/rephases an LFO.

LFO1 retains its original parameter IDs. New LFOs default to .1 Hz sine, Hz clock, free phase, bipolar polarity, CLASSIC digital-wave mode and the existing division default; both direct depths are zero. Old states never duplicate LFO1 depths into LFO2â€“4.

Existing source IDs keep their eleven `/10` meanings. Twelve appended override parameters select LFO2/3/4 without rescaling those IDs. Zero follows the ordinary source, and 1/3, 2/3, 1 select the additional LFOs. Any ordinary source edit releases the override, including repeated writes. Same-offset source edits take precedence over overrides regardless of queue insertion order. Existing destination IDs keep the established /17 and /41 contracts; all 42 destinations remain available. Existing LFO destination indices still address LFO1.

New host IDs:

- `0x105A..0x1074`: three consecutive nine-field LFO state sets.
- `0x1075..0x1086`: six new Source/Destination/Amount triplets.
- `0x1087..0x1092`: source overrides for routes 1â€“12.

There are 147 parameters. Native state is XSED v6, 147 doubles / 1184 bytes. V5 reads exactly its 90 fields; V4 reads 82; V1â€“V3 and XSV3 retain explicit earlier migrations. New route amounts default to centered zero, sources OFF. The generated factory table is extended from unchanged historical source banks so its new fields match importer defaults.

## Approved geometry

The existing 960x568 window and module shells remain intact. Four 18x18 square LFO editor buttons form a centered group with 2 px gaps: native x=81/101/121/141, y=343 in body coordinates. Their group center is x=120 and their top/bottom edges match the adjacent Curve selector row. The usual bevel, palette and Tahoma treatment are reused.

The existing matrix footprint contains two equal 480 px columns and six rows. Body y=400+19*row; field heights are 18 px. Source/Destination/Amount x offsets are 30/150/366 and widths 96/202/102. Amount retains pale fill, black centered text and a separate 3 px bipolar bar below the text. Labels, numbers, margins and hitboxes use shared geometry. Font size is unchanged. All twelve source/destination menus retain completed-click opening and modal event ownership; Amount keeps axis lock, dead zone, both drag directions and Shift fine adjustment.

See `docs/VALIDATION-CHECKPOINT.md` for measured results, validation, preservation and the final development checksum. Host listening remains necessary; automated checks cannot establish subjective sound quality.
