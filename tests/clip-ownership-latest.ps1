param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
# These constexpr fixtures are compiler assertions, not an executable or a
# substitute for native lifecycle/GUI acceptance of a normally admitted build.
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -fsyntax-only -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 (Join-Path $PSScriptRoot 'clip-ownership-latest.cpp')
if($LASTEXITCODE){throw 'Clip ownership compile-time assertions failed'}
Write-Output '19 compile-time assertions passed; no runtime executable produced or run.'
