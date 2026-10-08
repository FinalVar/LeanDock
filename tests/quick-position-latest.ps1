param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
if(-not $CompilerRoot){$CompilerRoot=Join-Path $env:ProgramFiles 'Windhawk\Compiler'}
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'quick-position-latest.cpp') -o (Join-Path $PSScriptRoot 'quick-position-latest.exe')
if($LASTEXITCODE){throw 'Quick position test compilation failed'}
& (Join-Path $PSScriptRoot 'quick-position-latest.exe') | Tee-Object -FilePath (Join-Path $PSScriptRoot 'quick-position-latest.txt')
if($LASTEXITCODE){throw 'Quick position test failed'}
