param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -fsyntax-only -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 (Join-Path $PSScriptRoot 'start-lifecycle-latest.cpp')
if($LASTEXITCODE){throw 'Start lifecycle compile-time assertions failed'}
Write-Output '14 Start lifecycle assertions passed (host exits, finite initialization retries, no timer budget renewal, transport failure stop, active adoption); no executable produced or run.'
