> Historical reference-edition audit, retained for provenance.
> Current expansion results are in EXPANSION_CHECKPOINT.md; state exchange
> and continuous automation are addressed in the separate expansion.

# Xsynth optimization audit — 2026-10-17

Scope: the Windows port's audio callback, DSP, automation, state exchange,
editor rendering, resizing/DPI, resource lifetime, tests, and release settings.
This is an audit, not an optimization patch. The release pass is still in progress.
No new release archive has been produced. The user will do the interactive DAW check.

The verification build that was already running when the audit was requested
completed compilation. Its SDK validator passed all 47 checks; core and legacy-DSP
tests passed; the integration test failed on its old size-rounding expectation.
No subsequent plugin build was started for this audit. Python probes load that
existing binary through the SDK's public interfaces; they do not compile a plugin.

## Conclusion

The synth has ample measured CPU headroom, but should not be finalized yet.
Fix state synchronization and continuous automation first. Then remove redundant
coefficient work and GUI rendering/allocation work. Preserve DSP equations,
filter/oscillator behavior, patch mappings, and pixel appearance. A wholesale
DSP rewrite, GPU renderer, oversampling, SIMD rewrite, or fast-math switch is not justified.

## Measurements

Machine: AMD Ryzen 5 PRO 5650U. Windows x64 Release binary SHA-256:
`521e86daffc61f886ddb758516c6a7138a3932987626629e53a20748cb5dbaa4`.

Native `process()` calls, median of three renders of approximately two seconds
each after one second of warm-up. Includes Python/ctypes overhead. These are
comparative elapsed timings, not deadline/jitter measurements or a DAW CPU meter.

| Rate / block | Idle, ms per audio second | Held note | Zero-sustain held note |
|---|---:|---:|---:|
| 44.1 kHz / 512 | 1.17 | 2.41 | 3.40 |
| 48 kHz / 512 | 1.24 | 2.58 | 3.42 |
| 88.2 kHz / 512 | 2.20 | 4.56 | 6.25 |
| 96 kHz / 512 | 2.59 | 4.97 | 6.81 |

Also exercised blocks of 64 and 2,048 at all four rates (36 combinations).
The final sampled buffers were finite and bounded; held-note cases produced
audio. The full output is `artifacts/runtime-audit.json`. This does not validate
every patch, automation load, multiple-instance scheduling, or all samples of a
long session. Default held-note cost at 48 kHz is about 0.26% of one serial
processing second, including probe overhead; do not extrapolate a safe instance count.

Cold GUI snapshot probes include process/GDI+ startup, full material rendering,
PNG encoding, and file writing. They **are not** warm repaint or corner-drag timings.

| Physical size | Median elapsed | Two main pixel buffers, calculated |
|---|---:|---:|
| 500 × 573 | 56 ms | 2.19 MiB |
| 1,000 × 1,146 | 138 ms | 8.74 MiB |
| 2,000 × 2,292 | 474 ms | 34.97 MiB |
| 3,000 × 3,438 | 904 ms | 78.69 MiB |

The buffer figures exclude GDI+, resource images, vector capacity overhead,
PNG buffers, and process memory. Raw measurements: `artifacts/gui-audit.json`.

## Findings, in priority order

### 1. P1 — Processor state crosses threads without synchronization

`port/Plugin.cpp:75` and `:83` set/read `engine.state` directly, while `process()`
also writes it and `Engine::next()` reads it. The SDK explicitly allows
`IComponent::setState/getState` on the UI thread while Processing
(`pluginterfaces/vst/ivstcomponent.h:199`). This is a C++ data race, including
the `dirty` flag; project saving can also capture a mixture of parameter values.

Use a bounded, coherent state handoff to the audio thread and a safely published
snapshot for serialization. Do not add a blocking mutex, allocation, or an
unbounded retry loop to `process()`. Validate concurrent save/restore and
automation with a dedicated stress test before release. Static finding; no crash
or corrupted user project was observed or induced.

### 2. P1 — Continuous automation is treated as steps

`port/Plugin.cpp:107` holds the old value until a queue point's sample offset.
VST3 represents continuous curves as linear segments, including implicit points
at sample -1 (`pluginterfaces/vst/ivstparameterchanges.h`). A host can send only
an endpoint for an entire block. That must not turn a continuous volume/filter
ramp into a jump at the block's end.

Reproduced with volume 0.5 and a single point of 0 at sample 511: samples 0–510
were exactly identical to unautomated audio; sample 511 abruptly became zero.
See `artifacts/automation-audit.json`. Existing tests cover power-switch steps,
not continuous ramps. Reconstruct continuous segments, retaining discrete
waveforms/switches as steps and preserving the existing parameter IDs.
Test ramps and explicit jumps across different block partitions. This is host
integration correctness, not a change to the synthesizer's sound design.

### 3. P2 — Every change recomputes all parameter coefficients

`port/Core.h:227–248`: even an unchanged value, waveform selection, power toggle,
pitch bend, or volume change sets one global dirty flag. The next sample converts
all 24 controls and recomputes both envelopes and glide. There are ten logarithmic
control conversions; at non-44.1-kHz rates there are seven additional power calls.
Dense per-sample automation at 48 kHz can therefore request up to 816,000 of these
power calculations per second, even for a simple volume move. This is an operation
count, not a measured dense-automation timing.

Ignore unchanged values and update only coefficients affected by each parameter.
Cache waveform indices and output gain when their controls change. Preserve
floating-point operation order wherever it affects output; compare ordinary
unautomated output with the original-C regression harness and the current binary.
This becomes more important when fixing continuous automation.

### 4. P2 — Audio callback scans empty queues for every sample

`port/Plugin.cpp:98–126`: all 34 slots are checked for every sample, including
idle blocks. This is 1,632,000 slot checks/sec at 48 kHz. Pending points and
events are fetched again on every earlier sample; queue point counts are also
virtual calls inside that loop.

Cache active queue pointers, point counts, and next point/event once per block
or consumed point. Process unchanged spans without rescanning empty slots.
Continue applying MIDI, commands, and automation at exact sample offsets.
Current idle rendering still costs roughly half the ordinary held-note case.

### 5. P2 — Subnormal envelope values are possible

`port/Core.h:288` uses an asymptotic float recurrence, with no local denormal
guard. A zero sustain while a key remains held keeps the engine active.
A float32 reproduction of decay coefficient 0.1 enters subnormal range after
829 decay samples and settles at approximately 5.605e-45 under gradual underflow.
See `artifacts/numeric-binary-audit.json`. Zero-sustain probes were about 33–41%
slower than ordinary held notes at 512 samples. That timing is consistent with
this risk, but does not isolate denormals as its only cause.

Consider a scoped x86 FTZ/DAZ guard that restores the host's floating-point
environment on exit. Validate normal audio equivalence and explicitly account
for the inaudible subnormal difference. Do not enable general fast-math or alter
envelope coefficients. Measure with both host denormal settings before claiming
the size of the benefit.

### 6. P2 — Unchanged GUI content is redrawn and re-dithered

`port/Editor.cpp:368–427`: panel/knob bodies are cached, which is good, but each
paint copies the entire background and redraws all 27 pointers/readouts, labels,
all buttons, and the title. The unchanged 496×44 title ramp is diffused on every
paint. Material calls also allocate palette/error vectors and call `GdiFlush`.
Any pitch-bend/modulation state change triggers a full paint although those
controls are not displayed. Mouse moves invalidate the whole window repeatedly.

Cache the finished static title, labels and button-state images. Redraw changed
control rectangles only; ignore nonvisual state changes for repaint purposes.
Reuse diffusion scratch storage for cold renders. Pixel-compare before/after
at each scale so the authored palette and Floyd–Steinberg pattern remain exact.
Do not replace the required dither with noise or gradients.

### 7. P2 — Resize churn, retained buffers, and unchecked allocation failure

`port/Editor.cpp:113` and `:621`: every resize destroys/recreates the DC, bitmap,
and fonts and clears the material cache, even if dimensions are unchanged.
`background.clear()` retains its largest allocation; after shrinking from the
maximum width, almost 39.35 MiB of background capacity alone can remain per view.
`close()` destroys the HWND but does not release cached graphics until the view
is destroyed. Hosts may retain view objects after closing their windows.

Skip identical dimensions, bound physical sizes, coalesce redundant resize work,
and release/reduce large resources when the editor closes or shrinks substantially.
Check DC/DIB/font allocation and reject a failed resize while retaining the last
valid frame. Currently a failed DIB allocation can leave a null pixel pointer
that the next render writes through. Do not temporarily blur/stretch the GUI
as a resize optimization without the user's agreement.

### 8. P2 — New sizing needs updated tests and host confirmation

Bulkhead's useful model is separate logical UI size and host DPI; Xsynth now
implements that model with continuous aspect-constrained host resizing.
There is no size menu. A direct interface probe restored an 825×945 size after
destroying/recreating the controller/view. At 150% DPI it became 1238×1419;
serialized logical width remained exactly 825. See `artifacts/size-audit.json`.

The old integration test at `tests/PluginTests.cpp:184` truncates 573×1.5 to 859;
the new aspect rounding returns 860. Update the expectation and test rejected
host resize requests, repeated same-size calls, bounds, repeated monitor changes,
state restore with an open editor, and simultaneous instances. Current probes
have no real host `IPlugFrame`, so they do not prove corner-drag usability in a
DAW. The user will perform the actual host test.

### 9. P3 — Build/package work remains; do not publish the candidate as final

The current binary is x64 Release, named Xsynth, vendor Phaos, version 1.0.2.1.
Its imports are Windows system DLLs only: GDI+, common dialogs, kernel/user/GDI,
and OLE. No dynamic MSVC runtime/debug-runtime dependency was found. No absolute
drive-path strings were found by the binary scan. The symbol directory is absent.
See `artifacts/release-binary-audit.txt`.

The build script still forces one compilation job. This affects development
time, not audio performance. Avoid compiler fast-math and architecture flags
that unnecessarily narrow CPU compatibility. Static runtime increases size
but removes the separate VC++ redistributable requirement for this binary.

Packaging still uses 1.0.2 paths and stale runtime/test-success text. README,
CREDITS, third-party notices, clean staging, and validation-gated packaging remain
unfinished release tasks. Keep development probes/logs/previews separate from
the lean binary package. Preserve upstream copyright/license files. The supplied
DNS charset has no accompanying redistribution/license statement in the provided
materials; document its provenance and resolve distribution rights before public
publication. No license files have been deleted or rewritten.

## What is already sound

- The ordinary `process()`/`Engine::next()` paths contain no explicit heap
  allocation, file/network I/O, blocking locks, or GUI calls.
- DSP tables and note storage are fixed-size; engine initialization is outside
  normal audio processing. The synth is monophonic, so there is no voice allocator.
- Oscillator wrap and finite/clipped output protection are retained.
- Idle UI polls at about 30 Hz but does not repaint unchanged state; major
  panel/knob materials are already cached.
- Mouse gestures balance begin/end edits; native window classes are cleaned up
  when the last view closes.
- Original-C comparison still passes for the 11 patches at 44.1 kHz.

## Before the next release build

1. Complete the user's direct-charset title reconstruction. Only Xsynth uses the
   charset; the visible 1.0.2 remains crisp Tahoma, bottom aligned. Release metadata
   stays 1.0.2.1. Keep this separate from sound/performance changes.
2. Fix state exchange and continuous automation with focused regressions.
3. Apply dependency-specific coefficient caching and queue traversal changes;
   preserve exact normal audio and sample-offset behavior.
4. Cache static graphics, guard allocations, and eliminate redundant resize work;
   preserve exact pixels, nearest-neighbour logo scaling and unsmoothed labels.
5. Re-run measured probes and the corrected/expanded tests, including simultaneous
   instances, dense automation, pitch/envelope timing across rates, MIDI panic,
   rapid retrigger, process stop/start and host resize acceptance/rejection.
6. Finish attribution and clean release packaging, then hand it to the user for
   their DAW discovery, corner-drag, save/quit/reopen and listening checks.

The audit does not certify Windows-host behavior or claim a speedup that has not
been implemented and measured. It identifies concrete targets without changing
the original synthesizer architecture or GUI styling.
