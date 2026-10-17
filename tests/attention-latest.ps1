param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$out=Join-Path $PSScriptRoot 'attention-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE (Join-Path $PSScriptRoot 'attention-latest.cpp') -o $out
if($LASTEXITCODE){throw 'Attention test compilation failed'}
& $out
if($LASTEXITCODE){throw 'Attention hook regressions failed'}
