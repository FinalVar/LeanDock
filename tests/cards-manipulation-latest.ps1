param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$compiler=Resolve-LeanDockCompiler $CompilerRoot
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $compiler 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'cards-manipulation-latest.cpp') -o (Join-Path $PSScriptRoot 'cards-manipulation-latest.exe') -luser32 -lole32 -loleaut32 -lruntimeobject -luuid
if($LASTEXITCODE -ne 0){throw 'Card manipulation compilation failed'}
& (Join-Path $PSScriptRoot 'cards-manipulation-latest.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'cards-manipulation-latest.txt')
if($LASTEXITCODE -ne 0){throw 'Card manipulation regression failed'}
