param([switch]$Record)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
function Run([string]$exe,[string[]]$arguments) {
 $info=[System.Diagnostics.ProcessStartInfo]::new(); $info.FileName=$exe; $info.WorkingDirectory=$PSScriptRoot; $info.UseShellExecute=$false
 $copy=@{}; foreach($p in [Environment]::GetEnvironmentVariables().GetEnumerator()){$copy[$p.Key]=$p.Value}
 $info.Environment.Clear(); foreach($key in $copy.Keys){$info.Environment[$key]=$copy[$key]}
 foreach($arg in $arguments){$info.ArgumentList.Add($arg)}
 $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true; $p=[System.Diagnostics.Process]::Start($info); $stdout=$p.StandardOutput.ReadToEndAsync(); $stderr=$p.StandardError.ReadToEndAsync(); $p.WaitForExit(); Write-Output $stdout.GetAwaiter().GetResult(); Write-Output $stderr.GetAwaiter().GetResult(); if($p.ExitCode){throw "$exe exited $($p.ExitCode)"}
}
Run 'C:\Program Files\CMake\bin\cmake.exe' @('-S','.', '-B','build-dsp-gate','-G','Visual Studio 18 2026','-A','x64','-DXSYNTH_PLUGIN=OFF','-DXSYNTH_DSP_HOST_TEST=ON')
Run 'C:\Program Files\CMake\bin\cmake.exe' @('--build','build-dsp-gate','--config','Release','--target','xsynth_dsp_validation','xsynth_reference_dsp_tests','xsynth_tests','xsynth_expansion_tests','xsynth_identity_tests','xsynth_legacy_tests','xsynth_dssi_tests','xsynth_pitch_tests','xsynth_factory_tests','--parallel','1')
Run 'C:\Program Files\CMake\bin\ctest.exe' @('--test-dir','build-dsp-gate','-C','Release','--output-on-failure','-E','sound_validation')
$argsList=@('tests/reference-audio'); if($Record){$argsList+='--record'}
Run "$PSScriptRoot/build-dsp-gate/Release/xsynth_dsp_validation.exe" $argsList
