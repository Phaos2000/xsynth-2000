# Building the current development source

Use Windows x64, CMake 3.25 or later, Visual Studio 2026 with the x64 C++
toolchain and Windows SDK. The validated toolchain is MSVC 19.51 / Visual Studio
18, Windows SDK 10.0.26100.0, Release x64 with static CRT.

From this repository root:

```powershell
cmake -S expansion -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel 1
```

The plugin is `build/VST3/Release/Xsynth 2000.vst3`. Building does not install it
globally. To build only the plugin use `--target Xsynth2000`; the SDK validator
runs as part of that target. A full build is needed before running all tests.
The existing SDK subset is used without modification; an alternative can be
selected with `-DVST3_SDK_ROOT="absolute SDK path"`. No SDK checkout/submodule
update is part of these commands.

## Regression fixtures and tests

Own historical audio/native-patch references, synthetic DSSI cases and required
source snapshots are included. Optional third-party DSSI banks and corresponding
audio references are not published; see expansion/tests/EXTERNAL-FIXTURES.md.
Restore the unchanged external corpus locally before running all integration
tests. These ignored local fixtures are not disposable build output, and a
mismatching or missing baseline must not be regenerated just to make tests pass.

Two tests also need the independently preserved faithful Xsynth binary. Supply
its DLL at configure time to register preserved_plugin_dsp and coexist:

```powershell
cmake -S expansion -B build -DREFERENCE_PLUGIN="C:/path/Xsynth.vst3/Contents/x86_64-win/Xsynth.vst3"
ctest --test-dir build -C Release --output-on-failure
```

Current main adds the source-aware routing and Wave-sharing regression suites.
Without the faithful binary, the two binary-comparison tests are not registered.
DSSI integration tests require
the external corpus in addition to source. For a fresh public checkout without
that corpus, run the self-contained suite with:

```powershell
ctest --test-dir build -C Release -E "^(dssi_import|interactions)$" --output-on-failure
```

This exclusion is explicit dependency handling, not a claim that missing-fixture
tests passed. The original C renderer comparison is built from retained C
sources independently. Performance/sampling tools are offline audit executables;
write their outputs under ignored build/artifact directories.

## Assets and historical scripts

Production inputs port/FactoryData.h, port/FactoryFamilies.h, assets/PreparedFactory.inc, factory/rom/ and port/TitleBitmap.h, plus
assets/xsynth-title-native.png are intentionally checked in, with generators and
original inputs. Python is not needed to build the plugin. Python 3 with Pillow
can check the independently constructed wordmark:

```powershell
python expansion/tests/WordmarkTests.py
```

No external bitmap charset is required. expansion/release/ and packaging scripts
are retained historical release-workflow source, not new release artifacts, and
are not invoked by the build above. Dated notes may mention original local audit
paths; generated exports are not source dependencies. Current validation is
summarized in expansion/docs/VALIDATION-CHECKPOINT.md. SDK revisions and notices
remain in SDK-COMPONENTS.txt, VST3-SDK-LICENSE.txt and expansion/SDK_REVISION.txt.

Factory ROM regeneration is optional and uses Python3 with NumPy. The normal C++ build embeds the checked-in prepared data and needs no Python. The original procedural formulas and recipes are in expansion/tools/rom/ and expansion/factory/rom/recipes/. No commercial reference WAVs or archives are included.

Verify the current complete bank with: python expansion/tools/rom/rebuild_current.py --output build/rom-rebuild
