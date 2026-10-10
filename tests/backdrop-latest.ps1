param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'backdrop-latest.cpp') -o (Join-Path $PSScriptRoot 'backdrop-latest.exe')
if($LASTEXITCODE){throw 'Backdrop test compilation failed'}
& (Join-Path $PSScriptRoot 'backdrop-latest.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'backdrop-latest.txt')
if($LASTEXITCODE){throw 'Backdrop transition test failed'}
