param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'shell-actions-latest.cpp') -o (Join-Path $PSScriptRoot 'shell-actions-latest.exe')
if($LASTEXITCODE -ne 0){throw 'Shell actions test compilation failed'}
& (Join-Path $PSScriptRoot 'shell-actions-latest.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'shell-actions-latest.txt')
if($LASTEXITCODE -ne 0){throw 'Shell actions test failed'}
