# Xsynth 2000 — final identity and credits checkpoint

Product: **Xsynth 2000**. Vendor: **Phaos**. Version: **0.1.0**.
No release build, ZIP, installation, DSP change or theme change in this checkpoint.

## Changes

- Removed DEVELOPMENT from the main header, leaving its space empty. Nothing
  replaced it and no other header element moved.
- Removed the temporary development suffix from About and the preview window title.
- About now uses the requested two-layer attribution: original Xsynth 1.0.2 and
  1999 code by Steve J. Brookes; Xsynth 2000 synthesis development, Windows VST3
  implementation and GUI by Phaos, copyright 2026.
- Included the requested version, URL, year and GPL source statement. The original
  clickable URL and its hit rectangle are unchanged; no browser was launched.
- Updated current README, CREDITS, implementation/identity descriptions, preset
  error wording and the future package's START-HERE text to the derivative-instrument
  framing. No packaged artifact was rewritten.

Normal text retains the existing Tahoma font, size, color mapping and centering.
The existing About bitmap title remains in its central position. The extra credit
lines were reflowed only within the existing informational text area (Y=334..454).
Panel bounds/bevel/screws, close button, logo, URL, and THEME selector did not move.
The selector remains at (436,456), size 144 x 18. The fixed faceplate geometry,
all controls, label baselines, header/logo/version/patch buttons remain unchanged.

## Canonical metadata

`port/Version.h` now defines:
- XSYNTH_PRODUCT_NAME = "Xsynth 2000"
- XSYNTH_VENDOR_NAME = "Phaos"
- XSYNTH_VERSION = "0.1.0"
- XSYNTH_VERSION_LABEL derives "v0.1.0" from that version
- Windows resource numeric tuple remains 0,1,0,0 in the same header

The factory, resource strings, small main-header version, About version, preview
window title and product message-box captions consume these definitions.
CMake reads the name/version from this header for bundle/project metadata.
Normal prose and historical release records retain their literal version text.

Processor CID remains **7923268C1C9D43DEAA65DAAC78C5232D**.
Controller CID remains **0C0B572A7CD34DC595F21C4122650E26**.
No IDs were generated or borrowed from reference Xsynth. Host parameter IDs,
XSED/XSEU state schemas, native .XseDev presets and existing import behavior remain.

## Files changed

Implementation/build metadata:
- port/Version.h — canonical name/vendor/version and narrow/wide display macros.
- port/Editor.cpp — requested credits, removed temporary label, canonical captions.
- port/Preview.cpp — canonical title without development wording.
- port/Plugin.cpp — factory strings now use the same definitions; no behavior edits.
- port/Resources.rc — name/vendor resource strings use the same definitions.
- CMakeLists.txt — project version/bundle name read from Version.h.

Current documentation/future packaging text:
- README
- CREDITS.md
- PRODUCT-IDENTITY.md
- IMPLEMENTATION.md
- package.ps1 (wording only; not executed)
- IDENTITY_CREDITS_UPDATE.md (this report)

Evidence and temporary source snapshots are in artifacts/identity/. Existing
editor/theme tests were run without changing their implementation.

## Verification

- Preview/editor/theme-test targets compiled successfully. No product target built.
- Existing editor tests pass at 100/125/150/200 percent, including exact geometry,
  About input isolation, close/reopen, modulation hover, amount and Hard Sync input.
- Geometry CSV is byte-identical to the completed pixel-geometry audit.
- Main screenshot differs in exactly **180 pixels**, all within the former
  DEVELOPMENT text rectangle. Logo, version, patch buttons and entire faceplate
  are otherwise pixel-identical to the approved geometry preview.
- About screenshot differs in **4,359 pixels**, confined to that removed header
  label and the permitted credit-text regions. About logo, frame, controls and
  theme selector pixels are identical to before.
- Six-theme selection/rendering tests pass: STEEL, 98, INDUSTRIAL, MULTIMEDIA,
  2000, SAND. Exact label/readout masks, deterministic renders, unchanged patch
  bytes/gestures and identical hitboxes verified. STEEL remains default.
- Theme persistence passes selection, editor reopen and new-instance recall;
  invalid stored values fall back to STEEL. This test used and deleted its own
  temporary registry key; the user's saved preference was not changed.
- Compiled preview Windows resources report Product=Xsynth 2000, Vendor=Phaos,
  string version=0.1.0 and numeric version=0.1.0.0. CMake version is 0.1.0.
- Plugin.cpp matches the pre-task file exactly after substituting the new metadata
  macros back to their identical literals. This verifies the IDs and complete
  processing/state implementation were preserved without building a new VST3.
- **36 locked files** (DSP/identity/layout/theme sources, DSP reference renders/
  presets/metrics and license files) are byte-identical. DSP tests were not rerun
  and no audio baseline was regenerated.
- All **65 preserved reference source files** match their manifest. Reference and
  existing derivative binaries and the previous release ZIP retain their hashes.

Logs: artifacts/identity/preview-check.log, theme-persistence.log, verification.log.
Verification scripts and the coordinate export are in the same directory.

## Screenshots

- Main: artifacts/identity/main-steel.png
- About: artifacts/identity/about-steel.png

Both are fresh native 960 x 568 STEEL renders. No new theme comparison set was made.

## Wording intentionally retained

Historical checkpoints (EXPANSION_CHECKPOINT.md, DIGITAL_IDENTITY_CHECKPOINT.md,
NAMING_UPDATE.md and prior audit reports), archived artifacts/packages, original
README history, upstream notices and tests retain the wording appropriate to
those records. The checkpoint documents already mark earlier names as historical.

Internal XsynthExpansionDev window/registry names, .XseDev extension, directory
names and expansion-related code/test symbols remain stable. They preserve
compatibility and are not the public product identity. Words such as "synthesis
development" in the requested credits and "expanded envelopes" describing actual
features remain appropriate. The future ZIP filename still includes "checkpoint"
because no public-release packaging decision was requested and DAW listening
validation remains outstanding.

No Xsynth-DSSI code, parser, UI or additional author attribution was added.
GPL/COPYING/AUTHOR and upstream historical attribution remain intact. Existing
bitmap provenance and SDK licensing notes are retained.
