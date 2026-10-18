$ErrorActionPreference='Stop'
$bundle=Join-Path $PSScriptRoot 'build/VST3/Release/Xsynth 2000.vst3'
if(-not (Test-Path -LiteralPath $bundle)){throw 'Build Xsynth 2000 first.'}
$binary=Join-Path $bundle 'Contents/x86_64-win/Xsynth 2000.vst3'
$latestSource=Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'port') -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if((Get-Item -LiteralPath $binary).LastWriteTimeUtc -lt $latestSource.LastWriteTimeUtc){throw 'The VST3 predates current source. Run the gated build before packaging.'}
& (Join-Path $PSScriptRoot 'validate-dsp.ps1')
$destination=Join-Path $PSScriptRoot 'dist/Xsynth-2000-0.1.0-checkpoint'
New-Item -ItemType Directory -Force -Path $destination,(Join-Path $destination 'Patches'),(Join-Path $destination 'Source'),(Join-Path $destination 'Previews'),(Join-Path $destination 'Validation') | Out-Null
Copy-Item -LiteralPath $bundle -Destination $destination -Recurse -Force
Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.Xpatch' | Copy-Item -Destination (Join-Path $destination 'Patches') -Force
foreach($name in 'COPYING','AUTHOR','CREDITS.md','SDK_REVISION.txt','PRODUCT-IDENTITY.md','EXPANSION_CHECKPOINT.md','DIGITAL_IDENTITY_CHECKPOINT.md','GUI_GEOMETRY_FROZEN.md','THEMES.md','README','NAMING_UPDATE.md','DSP_VALIDATION.md'){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $destination -Force
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../external/vst3sdk/LICENSE.txt') -Destination (Join-Path $destination 'VST3-SDK-LICENSE.txt') -Force
foreach($name in 'digital-identity-build.log','digital-profile.csv','gui-verification.log'){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot "artifacts/$name") -Destination (Join-Path $destination 'Validation') -Force
}
foreach($name in 'expansion-layout-100.png','expansion-layout-200.png','with-dither-100.png','without-dither-100.png'){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot "artifacts/$name") -Destination (Join-Path $destination 'Previews') -Force
}
foreach($name in 'STEEL','98','INDUSTRIAL','MULTIMEDIA','2000','SAND','about-steel','five-theme-comparison'){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot "artifacts/themes/$name.png") -Destination (Join-Path $destination 'Previews') -Force
}
foreach($name in 'theme-release-build.log','theme-persistence-test.log','palette-verification.log','scope-verification.log','steel-scale-verification.log'){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot "artifacts/themes/$name") -Destination (Join-Path $destination 'Validation') -Force
}
foreach($name in 'naming-build.log','naming-gui-verification.log','naming-title-verification.log','naming-reference-verification.log'){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot "artifacts/$name") -Destination (Join-Path $destination 'Validation') -Force
}
$source=Join-Path $destination 'Source'
Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object {$_.Extension -in '.c','.h','.Xpatch','.md' -or $_.Name -in 'CMakeLists.txt','build.ps1','validate-dsp.ps1','package.ps1','verify-gui.ps1','COPYING','AUTHOR','README','SDK_REVISION.txt'} | Copy-Item -Destination $source -Force
foreach($directory in 'port','tests','assets'){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot $directory) -Destination $source -Recurse -Force
}
@'
Xsynth 2000 0.1.0 - vendor Phaos
Xsynth 2000 is a new digital synthesizer developed from Steve J. Brookes' Xsynth 1.0.2 (1999).

Copy the entire Xsynth 2000.vst3 bundle into your VST3 scan directory.
It can sit alongside the reference Xsynth.vst3; do not replace that bundle.
Both products have distinct VST3 classes, host parameter ranges and state formats.
No installer or system-folder mutation was performed by the build process.

Drag knobs vertically, Shift for fine adjustment, double-click for defaults.
Matrix amount fields drag horizontally and reset to signed zero on double-click.
Short selectors cycle; longer lists use menus. Section right-clicks provide editing
commands. Load/save remain in the header; the logo opens credits.
Xsynth 2000 native presets use .XseDev. Original .Xpatch imports receive safe defaults.
The default appearance is STEEL. Click THEME in About to cycle through STEEL, 98,
INDUSTRIAL, MULTIMEDIA, 2000 and SAND. This preference is per user, not per patch.
See THEMES.md for the current visual pass and eight-suite validation.

See DIGITAL_IDENTITY_CHECKPOINT.md for current behavior, measurements and tests.
EXPANSION_CHECKPOINT.md includes the complete parameter IDs and defaults.
Actual DAW listening/project recall and long-session testing are still needed.
This checkpoint has not completed final listening and real-time DAW validation.

Source and original notices are included. Get the official VST3 SDK at the pinned
SDK_REVISION.txt with its submodules, and set VST3_SDK_ROOT to its absolute path
when configuring Source with CMake. The SDK is not duplicated in this archive.
Bitmap font provenance and unresolved artwork license are recorded in CREDITS.md
and Source/assets/FONT-SOURCE.md; code licensing does not establish artwork rights.
'@ | Set-Content -LiteralPath (Join-Path $destination 'START-HERE.txt')
$archive=Join-Path $PSScriptRoot 'dist/Xsynth-2000-0.1.0-win64-checkpoint.zip'
Compress-Archive -LiteralPath $destination -DestinationPath $archive -Force
Get-FileHash -LiteralPath $archive | Format-List
