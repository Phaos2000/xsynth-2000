# Xsynth 1.0.2 port audit

The original sources and GPL-2.0-or-later license remain unchanged. The new
implementation lives in `port/`. Audit completed before selecting a framework.

## Findings

1. **DSP:** one mono voice, highest held note in MIDI notes 21–96. Two oscillators
   offer sine/triangle (101-entry interpolated tables), ascending/descending saw,
   square and pulse. VCO1 can reset VCO2. LFO and two exponential ADSRs modulate
   VCO2 pitch and a Chamberlin state-variable filter (12/24 dB). EG1 drives VCA.
   Portamento is a one-pole pitch interpolation, updated every 256 samples.
   Output volume uses a 101-entry power-curve table. Velocity is a gate only.
2. **MIDI:** omni mono; note-on velocity zero releases; pitch wheel uses the
   full 14-bit value, approximately +/-one octave; CC1 reduces the filter frequency multiplier
   from 1 to 0. No sustain, program-change or aftertouch implementation.
3. **Parameters:** 24 physical floats, three waveform bytes (1,2,4,8,16,32), two
   sound switches. Original GUI also has load/save actions and an exit button.
   Tuning 1–4x; pulsewidth/depth/mix/sustain/volume 0–1; LFO .1–10 Hz;
   glide coefficient 1–.01 per 256 samples; envelope coefficient .1–.00001 per
   sample; filter depths/cutoff 0–50; resonance damping 2–.005. GUI tapers are
   retained in normalized-to-physical mappings. IDs 0–23 continuous, 24–26
   waveforms, 27 sync, 28 slope, 29 power, 30 bend, 31 modulation are frozen.
4. **Patches:** 209-byte little-endian records: 24 interleaved float angle/value
   pairs, three float-angle/byte-waveform pairs, two switch bytes. Angles are
   redundant and may disagree with values. Physical values are authoritative.
   Import validates an entire patch before committing; export reconstructs angles.
5. **Timing:** nominal 44.1 kHz, actual OSS device rate used for oscillator/filter
   increments, but envelope and glide coefficients remain nominal. The port
   corrects envelope and glide time constants for host sample rate; this is an
   intentional difference at rates other than 44.1 kHz. The original 256-sample
   control cadence is retained independently of host block size, with MIDI and
   automation changes handled at their sample offsets.
6. **Platform:** Xlib windows/fonts/events, OSS `/dev/dsp` and `/dev/midi`, fork,
   shared memory, signals, Linux scheduler, console patch prompts and obsolete
   GCC flags all require replacement.
7. **Reusable:** oscillator/filter/envelope/mixer equations, volume and pitch
   tables, mapping coefficients, layout coordinates, all supplied patches.
8. **Required repairs:** uninitialized envelope phases/levels/LFO phase/pitch;
   oscillator can wrap only once and index beyond its table at high rates;
   no return for invalid waveform; unsafe float-to-int16 overflow; no patch I/O
   validation. Initialize state, wrap phases safely, validate input, output finite
   bounded floats. Do not add antialiasing, oversampling or redesign the filter.
9. **Architecture:** official Steinberg VST3 SDK, separate processor/controller,
   native Win32 editor with GDI bitmap rendering. No JUCE or VSTGUI dependency.
   DSP and patch codec are standard C++17 with standalone tests. State is versioned
   little-endian normalized doubles, shared by processor and controller. No
   allocation, locks, disk access or UI calls in audio processing.

## GUI milestone

Use the supplied teal/steel image as material reference while retaining the
original 500x525 control coordinates below a 48-pixel title strip (500x573 total).
The user explicitly clarified that the DNS font belongs at the top, only for the
title. The bottom mark is removed; no OS window buttons are added. Surfaces use actual
deterministic Floyd–Steinberg error diffusion. DNS glyphs are cropped from the
supplied sheet, with nearest-neighbor scaling and consistent baseline.
Power becomes a host-recalled output gate instead of terminating the host.

## Validation boundaries

Automated tests cover parameter maps, all bundled legacy patches, invalid patch
rejection, deterministic audio, block independence, MIDI priority, state and
sample-rate behavior. SDK validator results and build status are recorded in
`IMPLEMENTATION.md`. Passing these is not a substitute for listening comparisons
against the Linux original or testing project recall and DPI in Windows DAWs.

SDK source: https://github.com/steinbergmedia/vst3sdk (revision pinned in
`SDK_REVISION.txt` when fetched). Retain its license alongside COPYING when
redistributing. This port remains GPL-2.0-or-later.
