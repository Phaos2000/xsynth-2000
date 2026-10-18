param([string]$Configuration='Release',[switch]$PreviewOnly)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
# Normalize inherited duplicate environment-variable casing before MSBuild.
function Invoke-CleanProcess([string]$exe,[string[]]$arguments) {
 $info=[System.Diagnostics.ProcessStartInfo]::new()
 $info.FileName=$exe
 $info.WorkingDirectory=$PSScriptRoot
 $info.UseShellExecute=$false
 $info.RedirectStandardOutput=$true
 $info.RedirectStandardError=$true
 $envCopy=@{}
 foreach($pair in [Environment]::GetEnvironmentVariables().GetEnumerator()){$envCopy[$pair.Key]=$pair.Value}
 $info.Environment.Clear()
 foreach($key in $envCopy.Keys){$info.Environment[$key]=$envCopy[$key]}
 foreach($arg in $arguments){$info.ArgumentList.Add($arg)}
 $p=[System.Diagnostics.Process]::Start($info)
 $stdout=$p.StandardOutput.ReadToEndAsync()
 $stderr=$p.StandardError.ReadToEndAsync()
 $p.WaitForExit()
 Write-Output $stdout.GetAwaiter().GetResult()
 Write-Output $stderr.GetAwaiter().GetResult()
 if($p.ExitCode -ne 0){throw "$exe exited with $($p.ExitCode)"}
}
if(-not $PreviewOnly){
 & (Join-Path $PSScriptRoot 'validate-dsp.ps1')
 Write-Output 'DSP gate passed. Proceeding to the VST3 build.'
}
Invoke-CleanProcess 'C:\Program Files\CMake\bin\cmake.exe' @('-S','.', '-B','build', '-G','Visual Studio 18 2026','-A','x64')
if($PreviewOnly){
 Invoke-CleanProcess 'C:\Program Files\CMake\bin\cmake.exe' @('--build','build','--config',$Configuration,'--target','xsynth_preview','xsynth_editor_tests','xsynth_theme_tests','--parallel','1')
 Invoke-CleanProcess 'C:\Program Files\CMake\bin\ctest.exe' @('--test-dir','build','-C',$Configuration,'-R','^(editor|themes)$','--output-on-failure')
 exit
}
Invoke-CleanProcess 'C:\Program Files\CMake\bin\cmake.exe' @('--build','build','--config',$Configuration,'--parallel','1')
Invoke-CleanProcess 'C:\Program Files\CMake\bin\ctest.exe' @('--test-dir','build','-C',$Configuration,'--output-on-failure')
