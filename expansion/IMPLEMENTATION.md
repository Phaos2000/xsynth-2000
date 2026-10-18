# Xsynth 2000 implementation

Xsynth 2000, version 0.1.0, vendor Phaos, is a new digital synthesizer developed
from Steve J. Brookes' Xsynth 1.0.2 (1999).
It has independent VST3 classes, host parameter IDs, native processor/controller
state signatures, preset extension, editor class and output directories.
The completed reference edition is preserved at the workspace root.

Read EXPANSION_CHECKPOINT.md for the implemented feature set, exact parameter map,
compatibility behavior, layout, measurements and current test status.
Read PRODUCT-IDENTITY.md before changing plugin identities or packaging.
DSP_VALIDATION.md and PIXEL_GEOMETRY_AUDIT.md record the completed validation
and frozen faceplate. IDENTITY_CREDITS_UPDATE.md records the current wording.

Build: ./build.ps1 (Windows x64, VS 2026, CMake).
Default SDK: ../external/vst3sdk at SDK_REVISION.txt, with submodules.
For a separate dependency checkout use -DVST3_SDK_ROOT=<absolute SDK directory>.
The SDK is MIT; project code is GPL-2.0-or-later. Preserve original license and
attribution files. Source bitmap rights are separate; see assets/FONT-SOURCE.md.
