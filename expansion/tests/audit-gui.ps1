$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$preview=Join-Path $root 'build/Release/xsynth_preview.exe'
$outputPath=Join-Path $root 'artifacts/audit-render.png'
$rows=@()
foreach($percent in 100,200,400,600){
 $samples=@()
 foreach($pass in 1..3){
  $timer=[Diagnostics.Stopwatch]::StartNew()
  $process=Start-Process -FilePath $preview -ArgumentList @('--snapshot',('"'+$outputPath+'"'),$percent) -WindowStyle Hidden -PassThru
  $process.WaitForExit()
  $timer.Stop()
  if($process.ExitCode -ne 0){throw "Preview failed at $percent percent"}
  $samples+=@{wallMs=$timer.Elapsed.TotalMilliseconds;cpuMs=$process.TotalProcessorTime.TotalMilliseconds}
 }
 $cpu=@($samples.cpuMs | Sort-Object)
 $wall=@($samples.wallMs | Sort-Object)
 $width=500*$percent/100;$height=573*$percent/100
 $rows+=@{percent=$percent;width=$width;height=$height;medianWallMs=$wall[1];medianCpuMs=$cpu[1];frameBuffersMiB=2*$width*$height*4/1MB;samples=$samples}
}
@{method='Three fresh preview processes per scale; includes process/GDI+ startup, cold full rendering, PNG encoding and file writing. Not warm repaint timings.';rows=$rows} | ConvertTo-Json -Depth 5
