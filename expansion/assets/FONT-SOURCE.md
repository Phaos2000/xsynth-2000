# Original fixed Xsynth 2000 wordmark

Original fixed bitmap wordmark artwork created for Xsynth 2000 from independently
constructed geometric primitives. Created for the Phaos project on 2026-10-17.
This is deterministic programmatic construction, not an image-model generation.
No external charset pixels, contours or glyph masks are used as inputs.

Source: tools/GenerateTitle.py. Literal characters only: X S Y N T H 2 0.
Output: port/TitleBitmap.h and assets/xsynth-title-native.png. No standalone font.
Code and newly constructed artwork are provided under GPL-2.0-or-later; COPYING.
The project record does not claim authorship of CRYPGRAU or exclusive copyright
in AI-assisted code/output.

Construction: native-grid binary polygon coverage, common 5-pixel vertical and
horizontal strokes; diagonal band intercepts give approximately the same normal
thickness (6/sqrt(1+(23/38)^2) = 5.13 pixels, 45-degree joins 7/sqrt(2) = 4.95).
An inset octagonal counter holds the zero's corner stroke to that same system.
One master zero is reused identically for all three digits. Spacing is external
to glyph geometry. One-pixel light/dark bevels are inside the original mask.
Existing theme color mapping, integer replication and hard shadow are unchanged.

Canvas 336x38. Occupied-art box [0,0,336,38), cap top 0, baseline exclusive 38.
Header and About share the same resource and unchanged placement/padding.
Regenerate: Python 3 with Pillow, python tools/GenerateTitle.py.

Retired CRYPGRAU/DNS files and earlier attempts exist solely in internal audit
history under artifacts/, excluded from public source and binary packages.
Their unknown provenance is not asserted to prove infringement. No reuse or
percentage-difference argument is used to establish the new artwork's provenance.
