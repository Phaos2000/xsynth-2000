# DSSI direct-pitch regression set

Current output: `reference-direct-pitch/` (66 WAVs plus complete 239-entry mapping).
Historical output: `pre-pitch-extension/` (54 WAVs and the old 177-conversion mapping).
The previous `reference/` files remain untouched as an additional preserved copy.

All four unchanged banks are downloaded from
https://linuxsynths.com/XsynthBanksDemos/ with their original filenames.
Format 0/1 synthetic fixtures are independently authored and labeled.
Current mapping semantics are in ../../PITCH_RANGE.md. Earlier sonic limitations
remain documented in ../../DSSI_COMPATIBILITY.md.

The first nine render cases keep the source bank/index/name and MIDI sequence
listed in pre-pitch-extension/README.md. Two formerly blocked patches were added:
MoogLikeBank index 0 (MoogLike1), and XsynthBank1 index 11 (JunoMe).

Each case has six renders: notes 36/60/84 at velocity 100/127, note 60 at velocity
32/127, chord 60/64/67 with velocities 100/127/.65/.7, and glide from 60 to 67.
Glide note 67 starts at sample 22050. All notes off is sample 55125. Every file
is 88200 mono IEEE float samples at 44100 Hz, with no normalization or dither.
Fresh engine seed is 0x7853796e; wheel/pressure zero, bend centered.

Tests verify all 239 ratios directly, neutral Master Tune, unchanged historical
cutoff/EG depths, native state recall, deterministic audio, and low/mid/high notes.
Recording can add new files only; existing mismatches fail and are never replaced.
The normal native DSP gate's ../reference-audio is separate and unchanged.

No genuine DSSI reference audio or sonic A/B approval is implied by these assets.
