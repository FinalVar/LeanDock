param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'build/toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$target=Join-Path $PSScriptRoot 'gesture-thumbnail-latest.exe'
& (Join-Path $CompilerRoot 'bin/clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O1 -static -ffunction-sections -fdata-sections '-Wl,--gc-sections' -DUNICODE -D_UNICODE -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'gesture-thumbnail-latest.cpp') -o $target -luser32
if($LASTEXITCODE){throw 'Gesture adapter fixture compilation failed'}
& $target | Tee-Object -FilePath (Join-Path $PSScriptRoot 'gesture-thumbnail-latest.txt')
if($LASTEXITCODE){throw 'Gesture adapter fixture failed'}
