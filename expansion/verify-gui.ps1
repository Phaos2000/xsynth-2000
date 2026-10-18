$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$preview=Join-Path $PSScriptRoot 'build/Release/xsynth_preview.exe'
$artifacts=Join-Path $PSScriptRoot 'artifacts'
New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
function Snapshot([string]$path,[int]$scale,[bool]$dither,[bool]$demo=$false) {
 $arguments=@('--snapshot',('"'+$path+'"'),$scale)
 if(-not $dither){$arguments+='--no-dither'}
 if($demo){$arguments+='--demo'}
 $process=Start-Process -FilePath $preview -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
 if($process.ExitCode -ne 0){throw "Snapshot failed: $path"}
}
foreach($scale in 100,125,150,200){
 foreach($mode in 'with-dither','without-dither'){
  Snapshot (Join-Path $artifacts "$mode-$scale.png") $scale ($mode -eq 'with-dither')
 }
 Snapshot (Join-Path $artifacts "expansion-layout-$scale.png") $scale $true $true
}
Snapshot (Join-Path $artifacts 'determinism-check.png') 100 $true
if((Get-FileHash (Join-Path $artifacts 'determinism-check.png')).Hash -ne (Get-FileHash (Join-Path $artifacts 'with-dither-100.png')).Hash){throw 'Non-deterministic dither'}
$foreground=@([Drawing.Color]::FromArgb(226,226,212).ToArgb(),[Drawing.Color]::FromArgb(127,215,216).ToArgb(),[Drawing.Color]::FromArgb(239,244,224).ToArgb())
$native=[Drawing.Bitmap]::new((Join-Path $PSScriptRoot 'assets/xsynth-title-native.png'))
try {
 foreach($percent in 100,125,150,200){
  $a=[Drawing.Bitmap]::new((Join-Path $artifacts "with-dither-$percent.png"))
  $b=[Drawing.Bitmap]::new((Join-Path $artifacts "without-dither-$percent.png"))
  try {
   $s=$percent/100.;$different=0
   if($a.Width -ne 960*$s -or $a.Height -ne 568*$s){throw 'Unexpected native dimensions'}
   for($y=0;$y -lt $a.Height;$y++){for($x=0;$x -lt $a.Width;$x++){
    $ca=$a.GetPixel($x,$y).ToArgb();$cb=$b.GetPixel($x,$y).ToArgb()
    if($ca -ne $cb){$different++;if($ca -in $foreground -or $cb -in $foreground){throw 'Dither changed foreground text or pointers'}}
   }}
   if($different -eq 0){throw 'Dither did not alter any surface pixels'}
   $step=[math]::Max(1,[math]::Floor($s));$inset=[int][math]::Floor(([math]::Round(48*$s)-38*$step)/2)
   for($y=0;$y -lt $native.Height;$y++){for($x=0;$x -lt $native.Width;$x++){
    $color=$native.GetPixel($x,$y)
    if($color.A -eq 0){continue}
    for($dy=0;$dy -lt $step;$dy++){for($dx=0;$dx -lt $step;$dx++){
     if($a.GetPixel($inset+$x*$step+$dx,$inset+$y*$step+$dy).ToArgb() -ne $color.ToArgb()){throw 'Title pixel geometry changed'}
    }}
   }}
   "PASS: $percent% dimensions, integer title pixels, preserved foreground; $different dithered material pixels."
  } finally {$a.Dispose();$b.Dispose()}
 }
} finally {$native.Dispose()}
'PASS: repeat render is byte-identical; both matrix columns and six routes remain visible.'
