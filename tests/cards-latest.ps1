param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
if(-not $CompilerRoot){$CompilerRoot=Join-Path $env:ProgramFiles 'Windhawk\Compiler'}
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'cards-latest.cpp') -o (Join-Path $PSScriptRoot 'cards-latest.exe')
if($LASTEXITCODE -ne 0){throw 'Card core compilation failed'}
& (Join-Path $PSScriptRoot 'cards-latest.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'cards-latest.txt')
if($LASTEXITCODE -ne 0){throw 'Card core regression failed'}
