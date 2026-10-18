# GUI geometry frozen — 2026-10-17

Current product name: **Xsynth 2000**, vendor **Phaos**, development **0.1.0**.
See NAMING_UPDATE.md for the naming and logo update; earlier names below describe historical checkpoints.

The final optical pass is complete. Preserve this geometry unless a later
functional requirement absolutely requires a change, or the user explicitly
requests another revision. Native dimensions remain 960 × 568.

Subsequent explicit exception: the theme brief centres EG1's existing 102 × 18
Curve field at local X=69, preserving its Y and size. That correction was made
before theme implementation, and the layout was frozen again. See THEMES.md.
No theme changes any coordinates. The expansion VST3 has since been rebuilt
with all six theme choices; the earlier preview-only status below is historical.

Final adjustments (including the explicitly requested logo/pixel follow-up):

- The later strict-grid request supersedes the three-pixel local optical offsets.
  LFO Wave/Clock and Voice Mode now share the same first-row Y coordinate (271,
  below the header); the next rows are at 307 and 343, exactly 36 pixels apart.
  Phase/Polarity retain their shared baseline with lower EG readouts and VOICE's
  split fields.
- EG1 Curve now uses exactly the same 102 × 18 field, 12-pixel local left inset,
  label baseline and Y position as EG2 Curve. EG2 Loop occupies the second field
  of that same template. Both envelopes retain identical local knob coordinates.
- Voice Mode and Bend Range retain identical full-width margins. Selector values
  now use symmetric 11-pixel text insets, correcting the former four-pixel left
  bias caused by reserving an arrow gutter only on the right. Values and labels
  share the field centre, including Voices and Filter Depth's adjoining readout
  groups. The Tahoma font and shared vertical text placement are retained.
- The header logo has a crisp one-native-pixel down/right shadow, using an
  existing dark teal. It is clipped to the original logo footprint and drawn
  beneath the original opaque bitmap pixels. No glyph, scale, position, padding,
  font or source asset was altered. At 100%, 384 background pixels inside the
  original footprint change; the rest of the header remains byte-identical.
- Hard Sync was reviewed and retains its compact rectangular beveled toggle.
  Its centred ON/OFF state and cyan treatment already suit the discrete controls;
  its distinct button shape communicates immediate toggling without a menu arrow.
- The matrix's field, amount, number and arrow baselines were reviewed and retained.

Shared templates are in port/Layout.h. All middle-module knob readouts remain
48 × 12, centred on the original-size knobs; selector pairs are 102 × 18 with a
12-pixel gap and 12-pixel module insets. Full-width selectors are 216 × 18.
Only layout and editor rendering changed; no DSP or parameter code changed.

Validation: preview build and native editor tests pass, including 100/125/150/200%
rendering and input routing. Render verification passes at those four scales,
including integer title pixels, deterministic surface dithering and unchanged
foreground treatment. The original opaque title pixels are verified exactly
at each scale, including with the new shadow. Current logs are
artifacts/logo-pixel-build.log and artifacts/logo-pixel-verification.log.

Frozen visual references: artifacts/logo-pixel-100.png and
artifacts/logo-pixel-200.png. Earlier final-optical screenshots remain for comparison.
This pass rebuilt the preview and editor tests;
the existing packaged VST3 predates the final alignment/optical passes.
