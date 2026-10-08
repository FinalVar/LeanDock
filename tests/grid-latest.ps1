param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$clang=Join-Path $CompilerRoot 'bin\clang++.exe'
$out=Join-Path $PSScriptRoot 'grid-latest.exe'
& $clang -target x86_64-w64-mingw32 -std=c++23 -O2 -static (Join-Path $PSScriptRoot 'grid-latest.cpp') -o $out
if($LASTEXITCODE -ne 0){throw 'Grid test compilation failed.'}
& $out
if($LASTEXITCODE -ne 0){throw 'Grid tests failed.'}
