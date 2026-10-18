# Expansion checkpoint — 2026-10-17

Current product name: **Xsynth 2000**, vendor **Phaos**, development **0.1.0**.
See NAMING_UPDATE.md for the naming and logo update; earlier names below describe historical checkpoints.

The first core-architecture checkpoint is complete. The later layout and digital
identity passes are included. See DIGITAL_IDENTITY_CHECKPOINT.md for current measurements.
The screenshot shows a demonstration patch, not the default/import state.

![Optimized main GUI](artifacts/expansion-layout-100.png)

## Independent products and preserved reference

The workspace root is the approved reference edition. Every source file in its
approved package was compared by SHA256 after restoration and matched. The original
bundle has been restored byte-for-byte to `../build/VST3/Release/Xsynth.vst3`.
An independent rebuild passed its original four test suites and the SDK validator;
the regenerated reference binary is kept separately under ../artifacts/reference-rebuild.
The approved reference bundle is still the one at the original output path.

The immutable reference checkpoint is `../checkpoints/reference-1.0.2.1/`:
verified original release archive, source hash manifest, and identity record.
Archive SHA256: A81D55C887890E0BC1977EDBEA5B1C340C6D71C59E877FEFFA1C5FD5B7580F66.
Approved reference DLL SHA256: FA44525BC553DB86E611E023A790F6BED86FE42F15F41B2C902EDECA64DBEF8F.

This directory is the separate descendant. `XsynthExpansionDev` is an internal
build name, not the final public name. Both products have vendor Phaos.

| Identity | Reference | Expansion |
|---|---|---|
| Bundle | Xsynth.vst3 | XsynthExpansionDev.vst3 |
| Metadata version | 1.0.2.1 | 0.1.0 |
| Processor CID | 741A5B213D184DA2B528D09FA0801021 | 7923268C1C9D43DEAA65DAAC78C5232D |
| Controller CID | 741A5B213D184DA2B528D09FA0801022 | 0C0B572A7CD34DC595F21C4122650E26 |
| Host parameter range | 0…33 | 0x1000…0x104B |
| Native processor state | XSV3 v1 | XSED v1 |
| Controller UI state | XSUI v1 | XSEU v1 |
| Native editor class | Xsynth1999Editor | XsynthExpansionDevEditor |

An integration test loaded both DLLs, instantiated both processors/controllers and
attached both editors in one process. It checked disjoint class IDs, separate
parameter IDs and state formats, rejected expanded state in the reference, imported
reference state into the expansion, and closed the reference editor while the
expanded editor continued working. No system-wide installation was performed.
Both bundles can be copied side by side into the normal Windows VST3 directory.

## Implemented synthesis

- Fixed 32-voice pool; selectable 1/2/4/8/16/32 polyphony, high-note-priority mono
  retrigger and mono legato. Deterministic oldest/released-first stealing.
- CLASSIC original oscillator shapes and CLEAN PolyBLEP saw/pulse/square;
  phase-evaluated cubic BLAMP correction for triangle. Shared original sine lookup.
- One-way audio-rate FM or PM in either direction, with explicit depth and sync.
- BAL, SUM, RING, AM, XOR and DIFF mixer modes.
- Original-style Chamberlin digital filter extended to LP/HP/BP/NOTCH and one/two
  stages. Continuous keytracking: 0% references A4, 100% follows note pitch, with
  intermediate exponents. Cutoff display is the A4 reference in Hz.
- LFO Hz/host-sync, 30 straight/dotted/triplet divisions, FREE/KEY/HOST phase,
  BI/UNI, six original shapes plus S&H and smooth random. Host sync has no
  fallback internal tempo. HOST random values are deterministic by song cycle.
- EG1/EG2 ADSR with linear, original exponential and fast curved LOG responses;
  velocity scales each envelope's output. EG2 A-D or A-D-R looping stops on release.
- Configurable glide mode, bend range, and ±100-cent master tuning.
- Six fixed per-voice matrix routes, 11 sources and 18 destinations. Destinations:
  VCO1/2 pitch, shape and level; XMOD depth; mixer balance; cutoff; resonance;
  LFO rate and dedicated depth; each EG's attack, decay and release.
- Matrix targets update every 8 samples and continuous values/coefficients
  interpolate. FM/PM and oscillator generation stay at audio rate. LFO rate is
  global; per-voice matrix contributions to it are averaged across active voices.
- Fixed-storage state handoff keeps UI mutexes and allocation off the audio thread.
  Continuous VST3 automation now interpolates from the implicit sample −1 point;
  selectors and MIDI panic commands remain discrete.
- Internal oscillator/envelope copy, paste, paste-to-other, reset, randomize and
  bounded 5/10/20/40% mutation. Oscillators can also swap. No system clipboard.
- Native expanded presets plus import/export of original .Xpatch files. Expanded
  presets retain every new setting; original .Xpatch exports only legacy controls.

No effects, unison, FFT synthesis, large wavetables or global oversampling were added.

## Layout optimization

Native size is 960 × 568 (preceding layout: 960 × 596). Only layout and
control presentation changed during this focused pass; the synthesis knobs remain
36 × 36 with their original rendering. The top row has asymmetric useful widths:
VCO1 170, XMOD 138, VCO2 170, mixer 104, VCF 252, amp 126 pixels.
Wave/Engine and Mode/Direction selectors are paired horizontally. Hard Sync is a
small illuminated button beside XMOD Depth, replacing its former selector row.
Type/Slope share a row. The middle-row LFO/EG/VOICE proportions and knob sizes are retained, with
both EGs symmetric. The matrix remains two columns of three routes, now numbered
1–6 with compact signed horizontal bipolar fields. Drag those fields horizontally,
use Shift for fine adjustment, or double-click to reset to zero.

Load/save remain in the header. The approved metallic CRYPGRAU bitmap title is
used (the user's later correction supersedes the brief's older DNS wording).
Tahoma, steel bevels, teal palette, selective Floyd–Steinberg surfaces, cyan
readouts and non-antialiased text are retained. Modulated labels have consistent
small cyan markers; oscillator-level routes mark the oscillator's title strip.
Hovering a knob-label marker highlights its associated matrix rows. The four
middle modules have balanced lower controls, with Phase/Polarity aligned to the
envelope Velocity/Pitch Depth readouts; lower controls have moved up
and 28 pixels of unused height have been removed without shrinking knobs.

## Exact parameter additions and defaults

Reference IDs are unchanged in the reference edition. Within this separate product,
the 32 legacy controls have new host IDs 0x1000…0x101F with the same internal
meaning/ranges. MIDI panic commands occupy 0x1020/0x1021. The 42 additions follow.
Defaults below are normalized stored values (choice order is shown).

| Host ID | Parameter | Choices/range | Default |
|---|---|---|---|
| 0x1022 | VCO1 engine | CLASSIC|CLEAN | `0` |
| 0x1023 | VCO2 engine | CLASSIC|CLEAN | `0` |
| 0x1024 | XMOD mode | OFF|FM|PM | `0` |
| 0x1025 | XMOD direction | 1 > 2|2 > 1 | `0` |
| 0x1026 | XMOD depth | continuous 0…1 | `0` |
| 0x1027 | Mixer mode | BAL|SUM|RING|AM|XOR|DIFF | `0` |
| 0x1028 | VCF type | LP|HP|BP|NOTCH | `0` |
| 0x1029 | VCF keytrack | continuous 0…1 | `1` |
| 0x102A | LFO clock | Hz|Sync | `0` |
| 0x102B | LFO phase | FREE|KEY|HOST | `0` |
| 0x102C | LFO polarity | BI|UNI | `0` |
| 0x102D | LFO digital wave | CLASSIC|S&H|SMOOTH | `0` |
| 0x102E | EG1 curve | LINEAR|EXP|LOG | `.5` |
| 0x102F | EG1 velocity | continuous 0…1 | `0` |
| 0x1030 | EG2 curve | LINEAR|EXP|LOG | `.5` |
| 0x1031 | EG2 velocity | continuous 0…1 | `0` |
| 0x1032 | EG2 loop | OFF|A-D|A-D-R | `0` |
| 0x1033 | Voice mode | MONO RETRIG|MONO LEGATO|POLY | `0` |
| 0x1034 | Voice count | 1|2|4|8|16|32 | `.6` |
| 0x1035 | Glide mode | OFF|ALWAYS|LEGATO|NON-LEGATO | `1./3` |
| 0x1036 | Pitch bend range | 0|1|2|3|4|5|7|12|24|48 | `7./9` |
| 0x1037 | Master tune | −100…+100 cents | `.5` |
| 0x1038 | Channel pressure | continuous 0…1 | `0` |
| 0x1039 | LFO division | 8/1…1/64, straight / dotted / triplet (30 choices) | `15./29` |
| 0x103A | Matrix 1 Source | OFF / LFO / EG1 / EG2 / Velocity / Key / Mod Wheel / Pressure / Pitch Bend / Note Random / Constant | `0` |
| 0x103B | Matrix 1 Destination | 18 destinations listed below | `0` |
| 0x103C | Matrix 1 Amount | −100…+100% (zero at 0.5) | `0.5` |
| 0x103D | Matrix 2 Source | OFF / LFO / EG1 / EG2 / Velocity / Key / Mod Wheel / Pressure / Pitch Bend / Note Random / Constant | `0` |
| 0x103E | Matrix 2 Destination | 18 destinations listed below | `0` |
| 0x103F | Matrix 2 Amount | −100…+100% (zero at 0.5) | `0.5` |
| 0x1040 | Matrix 3 Source | OFF / LFO / EG1 / EG2 / Velocity / Key / Mod Wheel / Pressure / Pitch Bend / Note Random / Constant | `0` |
| 0x1041 | Matrix 3 Destination | 18 destinations listed below | `0` |
| 0x1042 | Matrix 3 Amount | −100…+100% (zero at 0.5) | `0.5` |
| 0x1043 | Matrix 4 Source | OFF / LFO / EG1 / EG2 / Velocity / Key / Mod Wheel / Pressure / Pitch Bend / Note Random / Constant | `0` |
| 0x1044 | Matrix 4 Destination | 18 destinations listed below | `0` |
| 0x1045 | Matrix 4 Amount | −100…+100% (zero at 0.5) | `0.5` |
| 0x1046 | Matrix 5 Source | OFF / LFO / EG1 / EG2 / Velocity / Key / Mod Wheel / Pressure / Pitch Bend / Note Random / Constant | `0` |
| 0x1047 | Matrix 5 Destination | 18 destinations listed below | `0` |
| 0x1048 | Matrix 5 Amount | −100…+100% (zero at 0.5) | `0.5` |
| 0x1049 | Matrix 6 Source | OFF / LFO / EG1 / EG2 / Velocity / Key / Mod Wheel / Pressure / Pitch Bend / Note Random / Constant | `0` |
| 0x104A | Matrix 6 Destination | 18 destinations listed below | `0` |
| 0x104B | Matrix 6 Amount | −100…+100% (zero at 0.5) | `0.5` |

The persistent array contains 76 logical slots, including the two reserved panic
slots, which are ignored when restoring audio-engine state. Native XSED v1 is 616 bytes (8-byte header
and 76 little-endian normalized doubles). Original XSV3 v1 and .Xpatch imports
initialize all additions to the defaults above. The LFO Wave field combines the
legacy waveform with the explicit S&H/smooth override; Rate displays the saved
Division parameter when Clock=Sync. This retains original waveform/rate semantics.

## Compatibility and intentional differences

At safe new-parameter defaults, the engine delegates to the preserved legacy
renderer. All 11 supplied patches retain this path. The original-C differential
comparison still passes: worst relative RMS 0.00948%, five patches sample-identical.
Playable range remains MIDI 21…96; MIDI is omni, with a shared latest channel
pressure value. No universal original-C bit-identity claim is made.

Selecting expanded settings uses the expanded renderer, except Engine1/Engine2
alone can select local CLEAN rendering within the legacy-compatible path without
changing the other oscillator or selecting a different signal architecture. Matrix/filter coefficient
updates and glide interpolation use the new control strategy, so expanded patches
are not claimed to match the old monophonic renderer sample for sample. LEGATO
retains envelope stages; RETRIGGER restarts attack from the current envelope value,
as in the reference priority behavior. Polyphonic output sums voices with the
existing final ±1 output bound; there is no added limiter/effects section.

FM depth reaches ±8× carrier frequency excursion (through-zero allowed, phase
advances and wraps without the former ±0.45 phase-step clamp). PM reaches ±4 cycles. Hard sync remains VCO1→VCO2.
With reverse XMOD, slave reset uses the previous sample's master-wrap flag to avoid
recursive bidirectional evaluation. CLEAN reduces ordinary waveform aliasing;
wide-band FM/PM and hard sync can still produce aliasing without oversampling.

Mixer formulas, with a=VCO1, b=VCO2 and t=Balance: BAL=(1−t)a+tb;
SUM=0.5[min(1,2(1−t))a+min(1,2t)b]; RING=(1−t)a+tab;
AM=(1−t)a+ta(b+1)/2; XOR=(1−t)a+t(sign-bit-XOR bipolar waveform);
DIFF=(1−t)a−tb. Thus balance is still meaningful in every mode.
Matrix Amount ±100% maps pitch to ±48 semitones, cutoff/LFO-rate/time scaling to
±4 octaves, and other destinations to their bounded normalized domain.

## Memory, profiling and remaining decisions

The previous checkpoint measured `sizeof(xs::Engine)` at 12,256 bytes. The current
measurement is recorded in DIGITAL_IDENTITY_CHECKPOINT.md for the entire fixed pool,
legacy renderer, parameters and per-instance DSP state. Voice DSP uses float;
normalized host state and musical timeline use double. Shared original tables and
the sine table total 1,212 bytes per loaded module; the duplicate sine table has
been removed. The title bitmap is
shared immutable data. Engine note/render/release processing was checked for
zero heap allocation. GUI frame plus cached background total about 4.37 MiB at
the preceding native size; current native frame/cache storage is 4.16 MiB. This
is separate from DSP memory and excludes small GDI/SDK overhead.

On this Ryzen 5 PRO 5650U, 32 CLEAN voices at 48 kHz measured about **87–93 ms CPU
per audio second** in the diagnostic render loop (~8.7–9.3% of one serial second).
The earlier unoptimized diagnostic was ~305 ms. These are local elapsed samples,
not host deadline/jitter or maximum-instance guarantees. Control-rate caching
removed per-sample pitch/envelope exponentials and repeated selector decoding.
Remaining likely hotspots from code inspection: per-voice oscillator/filter work,
8-sample matrix routing/derived coefficients, and full-control GUI repaint during
rapid parameter changes. No speculative DSP rewrite was performed.

CC1 still performs the original global filter-closing action alongside its matrix
source role. Whether expanded patches should be able to disable that direct link
is an unresolved musical-workflow decision; this GUI pass adds no new switch.
The cutoff readout shows the legacy A4 target, subject to the original filter
stability cap, rather than an exact measured corner frequency.

The final public name is deliberately unresolved. Host-specific musical behavior,
listening, project recall and long-session tests remain for a real DAW. This
checkpoint has not been installed into Program Files or declared a public release.
Right-click popup workflow is implemented but still benefits from manual use.

## Build/test status

Reference rebuild: SDK validator **47/47**, original **4/4** test suites passed.
Expansion: SDK validator **47/47**, **7/7** suites passed (core, expansion,
digital identity, original-C comparison, native editor, VST3 integration and dual-product coexistence).
Expansion tests include fixed-pool bounds/release/no allocation, FM/PM/mixer/filter
combinations, velocity/curves/loops, host LFO phase/clock, matrix routing, version-1
migration, coherent concurrent state transfer and a spectral aliasing check.
The CLEAN saw folded-harmonic amplitude was 0.0064721 versus CLASSIC 0.0300482
in that targeted test (~13.3 dB reduction, not a universal aliasing guarantee).

Native editor tests cover 100/125/150/200%, modal input isolation, resize/reopen,
multiple instances and the new amount-field horizontal drag/zero reset.
VST3 integration verifies namespaced IDs, selector steps, state recall and a
sample-for-sample continuous volume-ramp reference. GUI checks at all four scales
passed dimensions, exact integer title pixels, deterministic dither and preserved
foreground. Screenshots are in artifacts/expansion-layout-{100,125,150,200}.png.

Build reference: `../build.ps1` from the parent. Build expansion: `./build.ps1`
from this directory. SDK is shared as immutable source with separate binary build
directories, or overridable using VST3_SDK_ROOT. Full identity is recorded in
PRODUCT-IDENTITY.md. See artifacts/expansion-build.log and gui-verification.log.
