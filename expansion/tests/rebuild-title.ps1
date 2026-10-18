# Original fixed wordmark generator; no external font/image input.
param([string]$Python = 'python')
$ErrorActionPreference='Stop'
& $Python (Join-Path (Split-Path $PSScriptRoot -Parent) 'tools/GenerateTitle.py')
if ($LASTEXITCODE) { throw 'Title generation failed' }
