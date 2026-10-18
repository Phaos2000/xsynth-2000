# Regression fixtures and optional external corpus

This development checkout includes intentional own regression fixtures:
reference-audio/ WAV/native patches, source snapshot headers under reference/,
pre-*/ and perf-*/, independently authored format0.dssi/format1.dssi, fixture
metadata/checksums, and approved source artwork. These are test inputs, not
ordinary benchmark outputs; do not delete or regenerate them during cleanup.

The existing public-distribution exclusion remains for:

- dssi/banks/{XsynthBank1,XsynthBank3,XsynthBank2018,MoogLikeBank}, obtained from
  https://linuxsynths.com/XsynthBanksDemos/xsynth.html. Their redistribution terms
  were not verified; they are not included in this public repository.
- The corresponding retained WAV corpus under dssi/. It remains a local-only
  test dependency, with hashes/mapping documentation retained where available.

Those files may remain on the developer's disk and are explicitly ignored.
Restore the original unchanged local corpus to its documented paths to run
DSSI import and full native import-interaction tests. Downloading banks alone
does not restore the immutable historical audio baselines. Do not bless new
baselines merely to satisfy a missing-fixture test.

The separately preserved faithful Xsynth binary is also external. Configure
REFERENCE_PLUGIN with its DLL path to run binary comparison/coexistence tests.
The older release-source archive omitted additional audio fixtures that are now
included here; that historical packaging policy did not classify them as junk.
See ../../SOURCE.md for the full and self-contained test commands.
