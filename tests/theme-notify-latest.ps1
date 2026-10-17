param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$out=Join-Path $PSScriptRoot 'theme-notify-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE (Join-Path $PSScriptRoot 'theme-notify-latest.cpp') -o $out
if($LASTEXITCODE){throw 'Theme notification fixture compilation failed'}
& $out | Tee-Object -FilePath (Join-Path $PSScriptRoot 'theme-notify-latest.txt')
if($LASTEXITCODE){throw 'Theme notification regression failed'}
