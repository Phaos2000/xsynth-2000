# Xsynth 2000 product identity

Bundle: Xsynth 2000.vst3
Factory product: Xsynth 2000
Vendor: Phaos
Version: 0.1.0
Processor CID: 7923268C1C9D43DEAA65DAAC78C5232D
Controller CID: 0C0B572A7CD34DC595F21C4122650E26
Host parameter namespace: 0x1000..0x104B (logical index + 0x1000).
Processor state: XSED, version 1. Controller UI state: XSEU, version 1.
Native preset extension: .XseDev.
Reference XSV3 version-1 state and original .Xpatch import are supported.
Reference Xsynth IDs and binary are retained independently at the workspace root.

Build from this directory with .\build.ps1. Default SDK dependency is the
sibling ../external/vst3sdk checkout, overridable with -DVST3_SDK_ROOT=<path>.
Output: build/VST3/Release/Xsynth 2000.vst3.
The reference .\build.ps1 remains in the workspace root and produces Xsynth.vst3.

Xsynth 2000 is a new digital synthesizer developed from Steve J. Brookes'
Xsynth 1.0.2 (1999). The faithful/reference product remains Xsynth.

The established expansion CIDs, parameter/state namespaces, preset extension,
native window class and UI-preference registry key remain stable so existing
development projects, patches and theme preferences continue to work. Their
internal XsynthExpansionDev spelling is historical, not the public product name.

Canonical product name, vendor and version: port/Version.h. Factory, Windows
resources, main/header version, About version, preview title and CMake metadata
consume these definitions. The numeric Windows resource tuple remains 0,1,0,0.
