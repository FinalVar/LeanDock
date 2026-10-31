param([Parameter(Mandatory)][string]$OutputDirectory,[string]$CasesFile,[string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
# Builds the real-desktop material check and runs each case. It shows the
# panel material on screen for about a second per case; see the .cpp header.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'build/toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $out -Force|Out-Null
$target=Join-Path $out 'latest.exe'
$sdk='C:\Program Files (x86)\Windows Kits\10';$sdkVersion='10.0.26100.0';$resource=Join-Path $out 'fixture.res'
Push-Location (Join-Path $root 'aggregate')
try{
 & (Join-Path $sdk "bin/$sdkVersion/x64/rc.exe") /nologo /I (Join-Path $sdk "Include/$sdkVersion/um") /I (Join-Path $sdk "Include/$sdkVersion/shared") /fo $resource 'latest.rc'
 if($LASTEXITCODE){throw 'Glass check manifest compilation failed'}
}finally{Pop-Location}
& (Join-Path $CompilerRoot 'bin/clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O1 -static -mwindows -municode -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 -include windowsx.h (Join-Path $PSScriptRoot 'aggregate-glass-latest.cpp') $resource -o $target -lole32 -loleaut32 -lruntimeobject -ldwmapi -lshcore -lshell32 -lshlwapi -ladvapi32 -lwtsapi32 -lgdi32 -luuid -ld3d11 -ld2d1 -ldwrite -lwindowscodecs -ldxgi -liphlpapi -lpowrprof -lbcrypt -lwbemuuid -lwlanapi -limm32 -lversion
if($LASTEXITCODE){throw 'Glass check compilation failed'}
# One case per line of extra arguments, e.g. '--backdrop --raw'; blank = real desktop.
$cases=if($CasesFile){@(Get-Content -LiteralPath $CasesFile)}else{@('--backdrop','--backdrop --raw','')}
foreach($case in $cases){
 $p=Start-Process -FilePath $target -ArgumentList ("--glass-check `"$out`" "+$case) -PassThru -Wait
 "case '$case' exit=$($p.ExitCode)"
}
Get-ChildItem -LiteralPath $out -Filter 'glass*.png'|ForEach-Object{"$($_.Name) $($_.Length)"}
