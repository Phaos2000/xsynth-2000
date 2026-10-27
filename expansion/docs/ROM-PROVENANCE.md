# Factory waveform provenance

The factory bank is original procedural material distributed with the project's GPL-2.0-or-later source. Publication of this development update was expressly authorized by the project owner on 2026-10-27.

Commercial wavetable archives were used only as a private aggregate research pool to assess variety and motion categories. No archive, commercial sample, extracted table, fitted coefficient set or direct waveform conversion is included here. The new constructions use independent mathematical formulas and original names.

`factory/rom/baseline36.xrom` preserves the original approved bank. `tools/rom/original_expansion.py` defines the 30 additions. Recipes, source-cycle normalization and table identities are recorded in `factory/rom/recipes/tables.json`. Listening acceptance is separate from permission to publish the development source.

The normal build embeds checked-in prepared playback data. To verify the complete encoded bank and prepared data from the original baseline and formulas, using Python 3 with NumPy:

```powershell
python expansion/tools/rom/rebuild_current.py --output build/rom-rebuild
```

The verifier writes only to the requested output directory and rejects any difference from the checked-in bank. Older ROM study scripts are historical research tooling; `rebuild_current.py` is the current complete-bank verification entry point.
