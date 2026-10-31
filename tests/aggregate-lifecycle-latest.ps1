param([Parameter(Mandatory)][string]$OutputDirectory,[string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'build/toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Use a fresh isolated lifecycle output directory'}
New-Item -ItemType Directory -Path $out|Out-Null
$target=Join-Path $out 'latest.exe'
$inputs=@((Get-ChildItem -LiteralPath (Join-Path $root 'aggregate') -File|Where-Object Extension -in @('.cpp','.h','.rc','.manifest')).FullName)
$inputs+=Join-Path $PSScriptRoot 'aggregate-lifecycle-latest.cpp'
foreach($name in @('aggregate-bridge-latest.h','dock-geometry-latest.h','icon-optics-latest.h')){$inputs+=Join-Path $root ('include/'+$name)}
$hashes=@($inputs|ForEach-Object{[pscustomobject]@{Path=$_;SHA256=(Get-FileHash -LiteralPath $_).Hash}})
$sdk='C:\Program Files (x86)\Windows Kits\10';$sdkVersion='10.0.26100.0';$resource=Join-Path $out 'fixture.res'
Push-Location (Join-Path $root 'aggregate')
try{
 & (Join-Path $sdk "bin/$sdkVersion/x64/rc.exe") /nologo /I (Join-Path $sdk "Include/$sdkVersion/um") /I (Join-Path $sdk "Include/$sdkVersion/shared") /fo $resource 'latest.rc'
 if($LASTEXITCODE){throw 'Lifecycle fixture manifest compilation failed'}
}finally{Pop-Location}
& (Join-Path $CompilerRoot 'bin/clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O1 -static -mwindows -municode -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 -include windowsx.h (Join-Path $PSScriptRoot 'aggregate-lifecycle-latest.cpp') $resource -o $target -lole32 -loleaut32 -lruntimeobject -ldwmapi -lshcore -lshell32 -lshlwapi -ladvapi32 -lwtsapi32 -lgdi32 -luuid -ld3d11 -ld2d1 -ldwrite -lwindowscodecs -ldxgi -lpdh -liphlpapi -lpowrprof -lbcrypt -lwbemuuid -lwlanapi -limm32 2>&1 | Tee-Object -FilePath (Join-Path $out 'compile-latest.log')
if($LASTEXITCODE){throw 'Lifecycle fixture compilation failed'}
foreach($inputFile in $hashes){if((Get-FileHash -LiteralPath $inputFile.Path).Hash -ne $inputFile.SHA256){throw 'Source changed during fixture build'}}
$hashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'source-inputs-latest.json') -Encoding utf8NoBOM
& (Join-Path $PSScriptRoot 'aggregate-probe-latest.ps1') -Executable $target -ProbeKind Lifecycle
if($LASTEXITCODE){throw 'Lifecycle fixture launch failed'}
$result=Get-Content -LiteralPath (Join-Path $out 'lifecycle-probe-latest.json') -Raw|ConvertFrom-Json
if(-not $result.passed -or -not $result.normalUser -or $result.ownedPanelWindows -ne 0){throw 'Lifecycle fixture did not pass'}
