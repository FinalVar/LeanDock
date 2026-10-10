param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $PSScriptRoot 'cards-preferences-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -municode -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'cards-preferences-latest.cpp') -o $exe -liphlpapi
if($LASTEXITCODE -ne 0){throw 'Preferences/network compilation failed'}
$data=Join-Path $PSScriptRoot ('data-cards-'+[guid]::NewGuid().ToString('N'))
& $exe $data | Tee-Object -FilePath (Join-Path $PSScriptRoot 'cards-preferences-latest.txt')
if($LASTEXITCODE -ne 0){throw 'Preferences/network regression failed'}
