$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$python=if($env:LEANDOCK_PYTHON){$env:LEANDOCK_PYTHON}else{'python'}
& $python (Join-Path $PSScriptRoot 'tray-styles-latest.py') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'tray-styles-latest.txt')
if($LASTEXITCODE -ne 0){throw 'Tray style checks failed'}
