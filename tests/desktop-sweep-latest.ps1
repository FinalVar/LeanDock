param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$out=Join-Path $PSScriptRoot 'desktop-sweep-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 (Join-Path $PSScriptRoot 'desktop-sweep-latest.cpp') -o $out -lshell32 -ldwmapi -luser32
if($LASTEXITCODE){throw 'Desktop sweep test compilation failed'}
& $out | Tee-Object -FilePath (Join-Path $PSScriptRoot 'desktop-sweep-latest.txt')
if($LASTEXITCODE){throw 'Desktop sweep tests failed'}
