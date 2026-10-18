# EG1 centering and exact-palette themes — 2026-10-17

Current product name: **Xsynth 2000**, vendor **Phaos**, development **0.1.0**.
See NAMING_UPDATE.md for the naming and logo update; earlier names below describe historical checkpoints.

**STEEL remains the default and retains the approved appearance.** The five
alternatives are **98, INDUSTRIAL, MULTIMEDIA, 2000, SAND**. The superseded CLASSIC
concept is not implemented or selectable.

## Geometry and baseline

EG1 Curve was centred first: its 102 × 18 selector and label moved together to
local X=69, centred at X=120 within the existing 240-pixel module. Its Y and
dimensions stayed fixed; no knobs/readouts or EG2 controls moved. A native STEEL
snapshot was saved before theme implementation. The completed STEEL screenshot
is byte-identical to that corrected baseline PNG.

After centering, Layout.h was hashed and remained unchanged throughout theme
development. Every theme uses the same 960 × 568 geometry, labels, font metrics,
hitboxes, bitmap glyphs and logo padding. The existing approved metallic bitmap
title is retained; the briefs' historical DNS wording does not replace it.

## Selection and persistence

Click the XSYNTH logo to open About/Credits. Near the bottom, click
`THEME [STEEL]` to cycle through STEEL → 98 → INDUSTRIAL → MULTIMEDIA → 2000 → SAND.
It uses the existing compact selector drawing and immediate cycling interaction.
There is no theme control on the main faceplate.

The preference is a DWORD named `Theme` at:

`HKEY_CURRENT_USER\Software\Phaos\XsynthExpansionDev\UI`

Values 0–5 correspond to the order above. An absent, unreadable or invalid value
loads STEEL. Only explicit selection in About writes the preference. New or
reopened editor instances recall the saved choice; already-open editors retain
their current choice until changed or reopened. Preview commands do not read or
write the actual preference. Theme choice never enters processor/controller
serialization, automation, or individual patches. A save failure leaves the
current session usable and displays an explanatory message.

## Materials

One Style structure in port/Theme.h contains the authoritative supplied base
colors for each new theme. All 33 stored color entries per alternative were
checked against the final brief, including chassis, panels, section strips,
fields, bevels, text, knob materials/pointers, active colors, buttons and title.
The 98 active-border fallback uses its supplied active-blue color. No alternative
base palette was invented. Intermediate colors are interpolation/quantization
of these supplied colors for shading and gradients.

The shared renderer consumes these materials without theme-specific geometry.
Knob shading interpolates the supplied shadow/mid/highlight colors. Alternative
title colors follow the supplied dark-edge/shadow/mid/highlight ramp while
preserving each bitmap pixel's coverage, integer scaling and crisp shadow.
STEEL bypasses recoloring and retains its exact original materials and pixels.

Floyd–Steinberg diffusion remains deterministic and limited to shaded surfaces.
Strength is 0.45 for 98, 0.35 INDUSTRIAL, 1.0 MULTIMEDIA, 0.2 for 2000, 0.7 SAND;
STEEL keeps 1.0. Palette step counts vary by material/theme. Text, tiny borders,
glyph geometry and fine lines are not dithered. Styles are small shared constants;
each view retains only its current frame/cache, with no six-theme bitmap atlas.

## Previews

All previews use the same demonstration patch, control values and modulation
routes. Each individual PNG is native 960 × 568. The comparison sheet places the
five new previews without resampling, with external name captions.

- [98](artifacts/themes/98.png)
- [INDUSTRIAL](artifacts/themes/INDUSTRIAL.png)
- [MULTIMEDIA](artifacts/themes/MULTIMEDIA.png)
- [2000](artifacts/themes/2000.png)
- [SAND](artifacts/themes/SAND.png)
- [Five-theme comparison](artifacts/themes/five-theme-comparison.png)
- [Unchanged default STEEL](artifacts/themes/STEEL.png)
- [About/Credits theme selector](artifacts/themes/about-steel.png)

## Verification

- Separate expansion Release VST3 built successfully; **8/8 suites pass** and
  the official SDK validator reports **47 passed, zero failed**.
- Theme tests cover six distinct deterministic renders, exact label/readout
  glyph masks, unchanged encoded patch bytes and host gestures during selection,
  identical knob hit/drag behavior, and exact restoration when returning to STEEL.
- Preference tests use a unique temporary registry key and remove it afterward.
  They cover new-instance/reopen recall, preview isolation, invalid-value fallback,
  and the STEEL default. The actual user preference was not changed by validation.
- STEEL scale checks pass at 100/125/150/200%, including exact original title
  pixels, deterministic dithering and preserved foreground treatment.
- Hash checks confirm frozen Layout.h and the original title source; DSP,
  parameters, state exchange, Plugin.cpp, IDs and oscillator code are unchanged.
  Core/plugin/coexistence tests also pass. The reference Xsynth is unaffected.

Logs and manifests are in artifacts/themes/. The initial sandboxed persistence
attempt was denied registry access; the isolated persistence test and full suite
then passed with registry access. No system VST3 installation was performed.

No theme-specific rendering limitations were found in these native previews.
Colors still depend on the monitor; fractional scaling retains the established
integer-scaled title rather than introducing filtering. Native popup/file dialogs
remain host/Windows-owned, as before. Listening/project-recall checks in the user's
DAW remain outside this visual-only pass.
