param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
if(-not $CompilerRoot){$CompilerRoot=Join-Path $env:ProgramFiles 'Windhawk\Compiler'}
$ErrorActionPreference='Stop'
$out=Join-Path $PSScriptRoot 'geometry-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 (Join-Path $PSScriptRoot 'geometry-latest.cpp') -o $out
if($LASTEXITCODE){throw 'Geometry test compilation failed'}
& $out | Tee-Object -FilePath (Join-Path $PSScriptRoot 'geometry-latest.txt')
if($LASTEXITCODE){throw 'Geometry tests failed'}
