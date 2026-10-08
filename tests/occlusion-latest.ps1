param([switch]$RunLive,[string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
if(-not $CompilerRoot){$CompilerRoot=Join-Path $env:ProgramFiles 'Windhawk\Compiler'}
$out=Join-Path $PSScriptRoot 'occlusion-fixture-latest.exe'
& (Join-Path $CompilerRoot 'bin/clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 (Join-Path $PSScriptRoot 'occlusion-fixture-latest.cpp') -o $out
if($LASTEXITCODE){throw 'Occlusion fixture compilation failed'}
if(-not $RunLive){'Built only; use RunLive for the bounded owned-window desktop test.';return}
$p=Start-Process -FilePath $out -WindowStyle Hidden -RedirectStandardOutput (Join-Path $PSScriptRoot 'occlusion-latest.json') -PassThru
if(-not $p.WaitForExit(30000)){throw "Owned fixture exceeded its bound; inspect PID $($p.Id)"}
if($p.ExitCode){throw 'Live occlusion test failed; inspect the recorded observations'}
Get-Content -LiteralPath (Join-Path $PSScriptRoot 'occlusion-latest.json') -Raw
