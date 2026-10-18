# Optional regression fixtures

The release source builds the plugin without external test data. It includes
the full production code, test code and the redistributable historical factory
inputs. These development-only fixtures are not in the public source archive:

- `dssi/banks/{XsynthBank1,XsynthBank3,XsynthBank2018,MoogLikeBank}`: obtain from
  https://linuxsynths.com/XsynthBanksDemos/xsynth.html. Their redistribution terms
  were not verified; do not redistribute them as part of this project.
- DSSI audio regression outputs under `dssi/`: likewise excluded. Local tests
  retain them; release validation ran against those existing baselines.
- Native development reference WAVs under `reference-audio/` and the separately
  preserved reference Xsynth binary: omitted from this build-source package.

Tests using these files will require the corresponding local fixtures. Do not
regenerate a failing baseline to make a test pass. Pure source, geometry, editor,
factory-bank and wordmark tests do not require the community-bank downloads.
