param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -municode -DUNICODE -D_UNICODE -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'quick-lifecycle-latest.cpp') -o (Join-Path $PSScriptRoot 'quick-lifecycle-latest.exe')
if($LASTEXITCODE){throw 'Quick lifecycle test compilation failed'}
& (Join-Path $PSScriptRoot 'quick-lifecycle-latest.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'quick-lifecycle-latest.txt')
if($LASTEXITCODE){throw 'Quick lifecycle test failed'}
