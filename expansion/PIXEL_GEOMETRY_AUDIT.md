# Xsynth 2000 — pixel geometry audit

2026-10-17. GUI-only checkpoint after the DSP validation gate.
All coordinates below are integer native-window pixels, origin at the top left.
Layout.h body coordinates have the existing 48-pixel header offset added here.
Rectangles use x, y, width, height; right/bottom endpoints are exclusive.

## Measured inconsistencies and corrections

| Group | Old knob center X | Corrected center X | Reason |
|---|---|---|---|
| VCO1 Pitch / Shape | 39 / 119 | **46 / 124** | Align each knob/readout with its existing Wave/Engine column; pair center now 85, the module center. |
| VCO2 Pitch / Shape | 347 / 427 | **354 / 432** | Same relative template; pair center now 393, the module center. |
| Mixer Balance | 529 | **530** | Module and Mode selector center are 530. |
| VCF Cutoff / Resonance / Keytrack | 619 / 703 / 787 | **624 / 708 / 792** | Retains 84-pixel center spacing; complete row centered on module X=708. |
| Matrix route numbers 1–6 | Baselines 464 / 504 / 544 | **463 / 503 / 543** | Number text boxes were height 20 versus the shared 18-pixel row grammar. Height 18 centers the unchanged Tahoma font on the same baseline as arrows, selector values and amount values. |

Moved knob labels and readouts follow their knob centers; no independent text
nudges. Every top knob center remains Y=93, label baseline Y=123 and readout
outer top Y=127. All knob material remains 36 x 36; readouts retain their sizes.

Readout X coordinates moved with the knobs:

| Group | Old readout X | New readout X | Width/height (unchanged) |
|---|---|---|---|
| VCO1 | 12 / 92 | 19 / 97 | 54 x 12 |
| VCO2 | 320 / 400 | 327 / 405 | 54 x 12 |
| Mixer | 502 | 503 | 54 x 12 |
| VCF | 592 / 676 / 760 | 597 / 681 / 765 | 54 x 12 |

These are the only visible changes. No selector moved, no module changed size,
and no middle-row control needed a position correction. XMOD's compact knob and
Hard Sync arrangement and AMP's sparse layout were retained.

## The questioned row: already exactly flush, now protected

| Control | Old outer rectangle | Current outer rectangle | Label box Y / baseline |
|---|---|---|---|
| EG1 Filter Depth | (420,355,48,12) | **(420,355,48,12)** | 341 / 351 |
| EG2 Filter Depth | (660,355,48,12) | **(660,355,48,12)** | 341 / 351 |
| VOICE Voices | (732,355,102,18) | **(732,355,102,18)** | 341 / 351 |
| VOICE Glide Mode | (846,355,102,18) | **(846,355,102,18)** | 341 / 351 |

All four outer top edges are **Y=355**, not an estimate from text positions.
The existing readout height of 12 and selector height of 18 explains the different
bottom edges and value-text centers. The labels above them share baseline 351.
The readout text baseline is 365; selector value text baseline is 368, as expected
from their different heights. Their top edges, not their internal glyphs, define
the common band requested in the brief.

This is verified both from the production rectangles and from the dark top-border
pixels in rendered screenshots. Scaled shared tops are 355 / 444 / 533 / 710 at
100 / 125 / 150 / 200 percent respectively. Scaling and rounding were not changed.

## Middle-row anchors

| Anchor | LFO | EG1 | EG2 | VOICE |
|---|---:|---:|---:|---:|
| Panel origin Y | 202 | 202 | 202 | 202 |
| Header fill top / height | 206 / 14 | 206 / 14 | 206 / 14 | 206 / 14 |
| First knob centers Y | 247 | 247 | 247 | 247 |
| First knob label baseline | 277 | 277 | 277 | 277 |
| First readout top / height | 281 / 12 | 281 / 12 | 281 / 12 | 281 / 12 |
| Wave/Clock or Voice Mode top | 319 | — | — | 319 |
| Second knob centers Y | — | 321 | 321 | — |
| Second knob / Phase-Polarity / Voices-Glide label baseline | 351 | 351 | 351 | 351 |
| Second readout / selector outer top | 355 | 355 | 355 | 355 |
| Curve/Loop/Bend label baseline | — | 387 | 387 | 387 |
| Curve/Loop/Bend selector top / height | — | 391 / 18 | 391 / 18 | 391 / 18 |
| Logical panel bottom Y | 418 | 418 | 418 | 418 |

EG1 and EG2 corresponding knob, label and readout rectangles match exactly after
subtracting their module origins (240 and 480). ADSR center X offsets are
36, 92, 148, 204; lower-row center offsets are 36, 120, 204. Readouts are 48 x 12.
The LFO uses the same three-knob template. VOICE's two-knob centers are offsets
63 and 177, directly centered above its paired selector columns.

EG1 Curve selector is **(309,391,102,18)**: center X=309+102/2=**360**.
Its label box is **(309,377,102,13)**, also centered at 360. EG1's interior spans
252..468, whose center is 360. Its approved vertical placement and width remain.
EG2 Curve and Loop remain **(492,391,102,18)** and **(606,391,102,18)**.

Paired middle selectors use 12-pixel outer margins, 102-pixel widths and a
12-pixel central gap. LFO Wave/Clock and Phase/Polarity use identical columns.
VOICE Mode is **(732,319,216,18)**; Bend Range **(732,391,216,18)**.
Both have 12-pixel outer margins and center X=840, as do their labels.

## Top row, matrix and shell geometry

Top modules retain origins/widths: VCO1 0/170, XMOD 170/138, VCO2 308/170,
Mixer 478/104, VCF 582/252, AMP 834/126. Their logical Y range is 48..202.
Every top header fill starts at Y=52, height 14; its unchanged lower rule is Y=66.
Top selector label baseline is 157, outer top 162, height 18. The XMOD Hard Sync
label shares the primary knob label baseline 123. Its distinct button geometry
remains approved and unchanged.

Middle panels retain 240 x 216 dimensions. Matrix retains 960 x 150 at Y=418.
All panel shells inset one pixel, with a one-pixel bevel and a surface inset of
two pixels. Adjacent shells have two pixels between their outer bounds. Left/right
external shell margins are one pixel. Module header horizontal insets remain 12.
The 48-pixel global header, logo, small version, and patch buttons are unchanged.
Overall GUI remains **960 x 568**.

Matrix field layout is shared by both 480-pixel halves:

| Element | Left X / width | Right X / width | Row tops / height |
|---|---|---|---|
| Source | 30 / 96 | 510 / 96 | 450,490,530 / 18 |
| Destination | 150 / 202 | 630 / 202 | 450,490,530 / 18 |
| Amount | 366 / 102 | 846 / 102 | 450,490,530 / 22 |
| Route numbers | 10 / 14 | 490 / 14 | shared baseline 463,503,543 |
| Arrows | 132 / 14 | 612 / 14 | shared baseline 463,503,543 |

Row spacing is exactly 40. SOURCE/DESTINATION/AMOUNT headings retain Y=435,
height 12, baseline 445, each centered over its field. Matrix module title remains
centered at X=480. Number order remains 1/2/3 left, 4/5/6 right. Amount controls,
source/destination selectors, values and interaction are unchanged.

## Shared definitions and regression protection

Only enough geometry was centralized to prevent drift:
- Panel rectangles and shell/header insets moved into Layout.h for drawing and tests.
- Top knob Y and selector Y now have common anchors.
- VCO knob and selector columns derive from the same column template.
- Existing middle 4-knob, 3-knob, 2-knob, paired/full/centered selector templates remain.
- Label and field outer bounds are shared by rendering and geometry assertions.
- Matrix fields, headings, row numbers, arrows and hover outlines share half/row anchors.

Existing EditorTests.cpp now checks the questioned row, EG template equality,
centered Curve, paired/full margins, top row geometry, shell geometry, matrix
mirroring/spacing, native Tahoma metrics, and actual rendered shared top edges.
Normal Tahoma metrics measured here: height 12, ascent 10. Font creation and
DrawText behavior were not changed. Existing editor interaction, overlay,
resize/reopen, marker, amount and Hard Sync tests pass at all four scale factors.

## Visual evidence and preservation

- `artifacts/geometry/steel-100.png` — fresh full native STEEL preview.
- `artifacts/geometry/alignment-guides.png` — diagnostic only. Original render is
  unscaled in its left 960 x 568 region; annotations occupy an added right gutter.
- `artifacts/geometry/steel-125.png`, `steel-150.png`, `steel-200.png` — scale checks.
- `artifacts/geometry/before-steel.png` — before image.
- `artifacts/geometry/geometry.csv` — measured control field/label bounds and
  baselines from production definitions. Matrix per-route labels are absent (-1);
  their column headings are described in the table above.
- `artifacts/geometry/preview-check.log` — numerical assertions and editor tests.
- `artifacts/geometry/pixel-verification.log` — screenshot bounds/difference checks.
- `artifacts/geometry/scope-verification.log` — locked-source and binary hashes.

Pixel comparison found **11,935 changed pixels**, all confined to the moved top
controls and route numbers. The **entire middle row and header are pixel-identical**
to the before image. The production screenshot contains no diagnostic guides.

All 14 other port files match the before-pass hashes: DSP, parameters, state/preset
encoding, product identity, theme definitions, fonts/logo assets remain unchanged.
Only Layout.h, Editor.cpp and EditorTests.cpp were edited as implementation files.
No DSP test rerun or redesign, theme work, DSSI import, VST3 build, release package,
or installation occurred. Only the preview/editor-test targets were compiled.
Existing reference/expanded binaries and release ZIP retain their recorded hashes.

No remaining coordinate mismatch was found in equivalent control patterns.
The intentionally unequal control heights, unequal top-module widths, VCF Type/Slope
widths and sparse LFO/VOICE/AMP areas remain part of the approved geometry. Making
those visually identical would require changing that design and was not attempted.
