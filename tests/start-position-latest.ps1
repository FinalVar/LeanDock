param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$out=Join-Path $root 'capsule\build'
New-Item -ItemType Directory -Path $out -Force|Out-Null
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'start-position-latest.cpp') -o (Join-Path $out 'start-position-tests.exe')
if($LASTEXITCODE){throw 'Start/Search placement test build failed'}
& (Join-Path $out 'start-position-tests.exe') | Tee-Object -FilePath (Join-Path $out 'start-position-tests-latest.txt')
if($LASTEXITCODE){throw 'Start/Search placement checks failed'}
