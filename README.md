[![Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/phaos)

# Xsynth 2000

**Windows x64 VST3 · Developed by Phaos**

[Website](https://phaos.neocities.org/) · [Email](mailto:phaos2000@pm.me) · [Latest release](https://github.com/Phaos2000/xsynth-2000/releases/tag/v0.1.1)

Xsynth 2000 is a digital synthesizer developed from Steve J. Brookes' **Xsynth 1.0.2 (1999)**.

What began as a Windows VST3 port grew into a substantially expanded instrument, with a larger oscillator and filter architecture, four independent LFOs, a 12-slot modulation matrix, polyphony, Unison, historical patch compatibility and a new retro-style interface.

![Current development interface, STEEL theme](expansion/assets/xsynth-2000-current.png)

*Current development source on main; the published v0.1.1 release remains unchanged.*

## To-Do / Being done

Completed in the current development source on `main`:

- [x] Distortion module: ten algorithms, PRE / POST placement and extended Drive range.
- [x] Factory wavetable bank and folders: 66 tables, including 30 original additions, chord/scale voicings and dedicated SFX.
- [x] USER waveform slots and native-patch embedding.
- [x] Stereo Unison width and selectable voice budgets up to 64 members.
- [x] Combined VCO Shape / Position modulation destinations.
- [x] EG2 tempo sync and dual-filter routing.
- [x] Targeted fix for the supplied Wave-modulation CPU spike: about 57% less processing in the paired different-note replay, with identical audio.
- [x] Updated interface screenshot and development validation notes.

Still open:

- [ ] Confirm the CPU improvement in Renoise at a 256-sample buffer while playing different notes.
- [ ] Complete manual listening and level/variety review of the revised factory bank, especially its five harmony voicings.
- [ ] Continue targeted voice/CPU optimization where real-host testing identifies remaining hotspots.

Completed source work is separate from listening acceptance and a new packaged release. See [current development validation](expansion/docs/CURRENT-DEVELOPMENT.md) for measured scope.

## Instrument

Xsynth 2000 has two oscillators, each with **CLASSIC**, **CLEAN** and **RASTER** rendering.

**CLASSIC** preserves the direct digital oscillator behavior, including aliasing as part of the sound. **CLEAN** provides an alternate reduced-aliasing implementation without changing oscillator tuning. Xsynth 2000 is a digital synthesizer rather than an attempt to emulate a particular analog instrument.

### Highlights

- Two oscillators with independent **CLASSIC / CLEAN / RASTER** rendering
- **−24 to +24 semitone** oscillator range
- FM, PM and hard sync
- BAL / SUM / RING / AM / XOR / DIFF oscillator mixing
- Two VCFs with LP / HP / BP / NOTCH modes, serial/parallel routing and keytracked self-oscillation
- 12 / 24 dB/oct filter slopes and keytracking
- Two expanded envelope generators, including tempo-synchronized looping EG2
- **Four complete independent LFOs**
- Tempo-synchronized LFO operation
- **12-slot modulation matrix**
- **14 modulation sources**
- **44 visible modulation destinations**, including combined Shape / Position per VCO
- Polyphonic and monophonic voice modes
- Selectable **1 / 2 / 4 / 8 / 16 / 32 / 64 physical-voice budgets**
- Unison OFF / 2 / 4 / 8 with dynamic group normalization
- Glide, pitch bend and master tuning
- **66 factory wavetables** in folders, including five chord/scale voicings and dedicated SFX
- Sixteen wavetable frames, HOLD / LINEAR / BAND reading and STEP / MORPH Position
- Sixteen USER waveform slots with native-patch embedding
- Ten distortion algorithms with PRE / POST placement and 0–200% Drive
- Stereo Unison width
- Coherent Wave-modulation optimization with independent unison oscillator histories
- Historical factory preset browser
- Original Xsynth `.Xpatch` import
- Xsynth-DSSI format 0/1 bank import
- Native Xsynth 2000 user patches
- Six interface themes
- Resizable GUI with remembered size
- Windows x64 VST3

## Historical context

The original **Xsynth** was written by **Steve J. Brookes in 1999** as a realtime MIDI-controlled software synthesizer for Linux.

It was a standalone X11/OSS application with a deliberately compact architecture: two oscillators with pitch, balance and sync, one LFO, two envelope generators, a low-pass filter and portamento. The original instrument was monophonic with highest-note priority.

Xsynth later provided the foundation for **Xsynth-DSSI**, which extended the design into the early Linux software-instrument ecosystem.

Xsynth 2000 retains Steve Brookes' original work as its historical and code foundation, but its synthesis engine, modulation system, voice architecture, Windows VST3 implementation and graphical interface have been substantially expanded.

## Original Xsynth vs. Xsynth 2000

| Area | Xsynth 1.0.2 (1999) | Xsynth 2000 |
| --- | --- | --- |
| Platform | Linux / OSS / X11 standalone | Windows x64 VST3 |
| Voice architecture | Monophonic, highest-note priority | Polyphonic + monophonic; fixed 32-voice pool |
| Oscillators | 2 oscillators, pitch, balance and sync | 2 oscillators, extended pitch, CLASSIC/CLEAN rendering, FM, PM and sync |
| Oscillator mixing | Balance | BAL, SUM, RING, AM, XOR and DIFF |
| Filter | Low-pass | LP, HP, BP and NOTCH; 12/24 dB; keytracking |
| LFO | 1 LFO | 4 independent LFOs with tempo sync |
| Envelopes | 2 envelope generators | Expanded envelope system, including looping EG2 |
| Modulation | Fixed routing; MIDI modulation to filter cutoff | 12 routes, 14 sources and 42 destinations |
| Voice controls | Mono only | Poly/mono, glide, legato behavior, master tuning and Unison |
| Presets | Original patch format | Historical presets, native patches, Xsynth and Xsynth-DSSI import |
| GUI | X11 control panel | Resizable VST3 interface with six themes |

## Historical patch compatibility

Original Xsynth `.Xpatch` files and historical Xsynth-DSSI patch/bank formats can be imported.

A development compatibility corpus of **239 historical/community Xsynth-DSSI patches** was successfully parsed and converted.

Xsynth-DSSI import is intended for useful musical migration rather than bit-identical emulation. The two engines differ in several areas of synthesis and voice behavior.

## Controls

Knobs drag vertically. Hold **Shift** for fine adjustment and double-click to restore the default value.

VCO Pitch uses exact semitone steps for ordinary dragging and mouse-wheel movement, with Shift available for continuous fine adjustment.

Cycling controls use:

- **Left-click:** next value
- **Right-click:** previous value
- **Mouse wheel:** forward/backward where applicable

Dropdown menus open on a completed click and remain open for normal selection.

The LFO Wave selector uses the same complete list for menu and mouse wheel:

**SINE → TRI → SAW UP → SAW DN → SQUARE → PULSE → S&H → SMOOTH**

All four LFOs run independently. The `[1] [2] [3] [4]` buttons select which LFO is currently being edited.

Matrix Amount supports horizontal or vertical dragging, automatically locking to the dominant drag direction. Shift provides fine adjustment and double-click returns the amount to zero.

## Presets

The preset browser contains **INIT** plus fifty historical records across three banks:

- 42 sounds
- 8 informational entries
- original intentional duplicates preserved

`LOAD PATCH` can also import Xsynth and Xsynth-DSSI patches/banks.

`SAVE PATCH` writes native `.XseDev` patches without modifying factory data. The filename stem is used as the displayed patch name.

## Themes

Click the logo to open **About**.

The `THEME` control cycles through:

**STEEL** · **98** · **INDUSTRIAL** · **MULTIMEDIA** · **2000** · **SAND**

STEEL is the default.

Theme and GUI-size preferences are stored separately from patches, so changing the appearance does not alter the sound or preset state.

## Validation

The published v0.1.1 checkpoint passed:

- **24 / 24 regression suites**
- **Steinberg validator: 47 passed / 0 failed**
- linked-plugin integration checks
- state and automation migration tests
- multi-rate / multi-buffer DSP tests
- six-theme and GUI interaction checks

The current release also includes performance optimization of the expanded four-LFO / twelve-route engine. Extreme 96 kHz, 32-voice workloads remain intentionally documented as stress cases rather than being hidden through reduced synthesis quality.

Current main adds the factory wavetable bank, shared Shape / Position routing and an exact-audio Wave-modulation CPU fix. See [current development validation](expansion/docs/CURRENT-DEVELOPMENT.md) for the source checkpoint and measured scope. Manual listening and real-host CPU acceptance remain separate from offline validation.

## Installation

1. Download the latest Windows x64 package from [Releases](https://github.com/Phaos2000/xsynth-2000/releases).
2. Extract the ZIP.
3. Copy the complete `Xsynth 2000.vst3` bundle to:

   `C:\Program Files\Common Files\VST3\`

4. Rescan VST3 plugins in your DAW if necessary.

## Source and license

Xsynth 2000 is distributed under **GPL-2.0-or-later**.

Original Xsynth code:

**Copyright © 1999 S. J. Brookes**

Xsynth 2000 synthesis development, Windows VST3 implementation and GUI:

**Copyright © 2026 Phaos**

Matching source and SHA-256 checksums are provided with each release.

Phaos  
https://phaos.neocities.org/  
phaos2000@pm.me
