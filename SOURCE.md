# Corresponding source

Distribute `Xsynth-2000-0.1.0-source.zip` alongside the matching Windows archive
at the same download location. This release supplies source directly rather
than relying on a written source offer. COPYING contains GPL version 2; the
original source grants version 2 or any later version.

The source archive contains `expansion/` (the exact product sources and build
configuration) and `external/vst3sdk/` (the SDK core, build support, validator and
module-info utility source needed for this Windows build). SDK license notices
are retained. Unused SDK GUI/examples, research archives, previous candidates,
old font atlases and unverified community preset bytes are excluded.

## Build

Install CMake 3.25 or later and Visual Studio 2026 with the x64 C++ toolchain and
Windows SDK. The validated toolchain is MSVC 19.51 / Visual Studio 18, Windows
SDK 10.0.26100.0, Release x64 with static CRT. From the source archive root:

```powershell
cmake -S expansion -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target Xsynth2000 --parallel 1
```

The result is `build/VST3/Release/Xsynth 2000.vst3`. The SDK validator runs as
part of that target. Building does not install the plugin into a system folder.

The SDK root revision is recorded in expansion/SDK_REVISION.txt; component
revisions are in SDK-COMPONENTS.txt. The included subset is sufficient for this
product; the full upstream SDK is available at https://github.com/steinbergmedia/vst3sdk.

The generated title data is already included, so Python is not required to
build the plugin. To regenerate the original wordmark, install Python 3 and
Pillow and run `python expansion/tools/GenerateTitle.py`. Its complete geometric
source and provenance are included; no external bitmap font is required.

Test source is included. Local external-bank corpora and test audio are omitted
where unnecessary for building or lacking verified redistribution permission.
Tests that need those fixtures require obtaining them separately; see
expansion/tests/EXTERNAL-FIXTURES.md. Their omission does not affect the plugin
build or the included historical factory resources.
