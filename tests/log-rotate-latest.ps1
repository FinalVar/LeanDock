param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'log-rotate-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -mconsole -municode `
    -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 `
    (Join-Path $PSScriptRoot 'log-rotate-latest.cpp') -o $exe 2>&1 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'log-rotate-build.log')
if($LASTEXITCODE){throw 'Log rotation test compilation failed'}
& $exe | Tee-Object -FilePath (Join-Path $PSScriptRoot 'log-rotate-latest.txt')
if($LASTEXITCODE){throw 'Log rotation tests failed'}
