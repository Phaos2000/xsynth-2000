# Historical factory library

50 selectable historical records in three banks: 42 sound records and eight
informational-name patch records retained verbatim and in upstream order.
Duplicates are intentional. The original binary patches are ordered by filename,
matching Xsynth's `ls *.Xpatch` file-list mechanism; text banks retain file order.

| Display bank | Source | Records | Attribution / redistribution basis |
| --- | --- | ---: | --- |
| XSYNTH 1.0.2 | Eleven separate `.Xpatch` files in the 1999 Xsynth 1.0.2 archive | 11 | Steve J. Brookes; original distribution's GPL v2 or later notices and COPYING retained |
| XSYNTH-DSSI FACTORY | `extra/factory_patches.Xsynth`, Xsynth-DSSI 0.9.4 | 14 | Steve J. Brookes' original patches, adapted/distributed by Sean Bolton's Xsynth-DSSI; upstream GPL v2 or later distribution, README/AUTHORS/COPYING/installation list retained |
| XSYNTH-DSSI 0.1 | `extra/version_0.1_patches.Xsynth`, Xsynth-DSSI 0.9.4 | 25 | Official legacy DSSI 0.1 set distributed by Sean Bolton; upstream GPL v2 or later distribution, notices retained; individual patch authors not separately identified |

The original archive genuinely contains `bell`, `default`, `heart_beat`,
`plucked`, `reso_sweep`, `squelchy_bass`, `strings`, `sync_bass`, `sync_sweep`,
`theremin`, and `voice`. It does **not** contain a separate packed factory bank.
Source initialization defaults are not invented as additional named presets.

Despite its filename, the official DSSI `factory_patches.Xsynth` describes itself
in its first four record names as the original Xsynth 1.0.2 patches converted to
DSSI. It contains ten sound records plus those four informational records. It is
not described here as a newly authored later sound set. The 0.1 bank has 21 sound
records and four trailing informational records. Both are installed bank files
in the upstream `extra/Makefile.am`.

## Sources and checksums

- Original archive: https://www.ibiblio.org/pub/linux/apps/sound/misc/xsynth-1.0.2.tar.gz
  SHA-256 `FF6B22C37B040ADE81EB7D9B8FA3631FD3C1E200A194D9D99EE109AD6B3F062D`.
- Official DSSI archive: https://deb.debian.org/debian/pool/main/x/xsynth-dssi/xsynth-dssi_0.9.4.orig.tar.gz
  SHA-256 `D3D24941A509C6193EAC2B2EE3C9A6057C832565A0B66E58C3BD2E89AFD8DC02`.
- Author's project page: https://smbolton.com/xsynth-dssi.html
- Official repository (redirected author account): https://github.com/theabolton/xsynth-dssi

`manifest.json` records every copied source file's SHA-256, formats, counts,
authors, URLs and decisions. `source/` preserves original bytes and notices.
Original patch data use binary physical-value Xpatch records; both bundled DSSI
banks use text patch format 0. No source values/names are manually retuned.

## Conversion and runtime

`tools/GenerateFactory.cpp` converts the original files with the existing native
Xpatch importer and the DSSI banks with the existing validated converter.
`port/FactoryData.h` stores complete immutable native states shared across
instances. There is no runtime text-bank parsing for factory selection and no
factory allocation or parsing in the audio callback. DSSI pitch ratios map
directly into 0.25x–4x oscillator range, Master Tune stays neutral, extension
defaults and existing approximation warnings are preserved. Original Xpatch
files use the CLASSIC renderer; DSSI conversion retains its documented CLEAN
oscillator and filter approximation behavior. Selection is state loading, not
a new parameter or MIDI program-change system.

Factory records cannot be overwritten by Save Patch. The header menu starts
with a separate generated INIT patch, followed by these three historical banks.
Navigation wraps across INIT and all 50 historical records. INIT is a VCO1-only
CLASSIC saw with open LP filter, sustained envelopes and no active modulation;
it is not historical material. Compatibility startup defaults remain unchanged.

Sound edits add ` *` to the current name. Successful explicit user Save/Load
uses the filename stem even if the sound matches a factory record. DSSI import
uses the source patch name until saved under a user filename. Cancelled/failed
saves preserve the current name. Names are editor metadata, not a new field in
native state; pure host-state recall may infer a built-in sound or show CUSTOM.

## Additional candidates / external-only material

`extra/friendly_patches.Xsynth` in upstream 0.9.4 contains 57 records and combines
startup patches by Sean Bolton, Steve J. Brookes and Silvan (Michael McIntyre).
The related `src/gui_friendly_patches.c` explicitly questions contributor patch
licensing, including Silvan's. Its bytes are excluded from the factory library
and candidate resources; archive presence alone does not resolve that notice.

The four compatibility banks `XsynthBank1`, `XsynthBank3`, `XsynthBank2018`, and
`MoogLikeBank` from https://linuxsynths.com/XsynthBanksDemos/xsynth.html remain
external-only. Source page, bank headers, site material and a narrow licensing
search did not establish explicit redistribution permission. No community bank
is bundled. All 239 records remain supported by Import Xsynth-DSSI Bank and
local regression fixtures. The page also links Carla `.carxs` conversions;
their format/redistribution rights were not verified and they are excluded.
Users can obtain external banks themselves from the source page.

Only these approved factory sources and notices are included in the plugin.
Community fixtures and their derived audio remain local development inputs and
are excluded from the public binary and corresponding-source packages.

Source research was recorded 2026-10-17. The original geometric title's separate
provenance is recorded in assets/FONT-SOURCE.md in the corresponding source.
