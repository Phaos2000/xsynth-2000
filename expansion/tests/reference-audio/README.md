# Deterministic sound references

48,000 Hz, mono IEEE-float WAV, two seconds, no normalization or dither.
Native XseDev files beside the WAVs capture the initial parameter values.
Fixed engine seed: 0x7853796e (reset for every case).
128-sample reference buffer; all comparison runs also use 32/64/256/512/1024.

The complete event and automation score is in tests/DspValidation.cpp, render():
- note 60 at sample 0, velocity 0.8, ID 1;
- chord additionally uses 64/67/72 at velocities 0.65/0.7/0.6, IDs 2/3/4;
- pitch wheel 0.53 at 0.37 seconds, returns to 0.5 at 0.71 seconds;
- all notes off at 1.25 seconds;
- tempo 123 BPM, playing, song beat 0 at sample 0;
- filter-sweep cutoff automation is evaluated at every sample.
The `patch()` function specifies every case's deviations from fixed defaults.

Run expansion/validate-dsp.ps1 to verify. Missing references are an error.
-Record creates missing references only, and only if every preceding group passes.
Existing references are never overwritten, even by -Record.
An intentional algorithm change requires reviewing and separately archiving the
old reference set before recording a new one. Do not loosen thresholds to hide it.
Baseline tolerance is 1e-6 peak absolute sample difference on this compiler/platform.

Retained cases: classic-saw, clean-saw, hard-sync, pm, fm, xor,
resonant-filter-sweep, synced-lfo, looping-eg2, matrix-chord.
metrics.csv reports RMS, signed mean/DC, peak and maximum reference difference.
These are regression stimuli, not finished musical presets or mastering examples.
Raw digital harmonics/aliasing and naturally asymmetric pulse DC are intentional.
