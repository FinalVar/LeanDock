param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $PSScriptRoot 'aggregate-dismiss-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -mconsole -municode `
    -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 -include windowsx.h `
    (Join-Path $PSScriptRoot 'aggregate-dismiss-latest.cpp') -o $exe `
    -lole32 -loleaut32 -lruntimeobject -ldwmapi -lshcore -lshell32 -lshlwapi -ladvapi32 -lwtsapi32 -lgdi32 -luuid `
    -ld3d11 -ld2d1 -ldwrite -lwindowscodecs -ldxgi -lpdh -liphlpapi -lpowrprof -lbcrypt -lwbemuuid -lwlanapi -limm32 -lversion 2>&1 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'aggregate-dismiss-build.log')
if($LASTEXITCODE){throw 'Aggregate dismiss test compilation failed'}
& $exe | Tee-Object -FilePath (Join-Path $PSScriptRoot 'aggregate-dismiss-latest.txt')
if($LASTEXITCODE){throw 'Aggregate dismiss tests failed'}
