param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'icon-optics-latest.cpp') -o (Join-Path $PSScriptRoot 'icon-optics-latest.exe')
if($LASTEXITCODE -ne 0){throw 'Icon optics compilation failed'}
& (Join-Path $PSScriptRoot 'icon-optics-latest.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'icon-optics-latest.txt')
if($LASTEXITCODE -ne 0){throw 'Icon optics tests failed'}
