param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$out=Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$exe=Join-Path $out 'latest.exe'
$resource=Join-Path $out 'latest-resource.res'
$sdk='C:\Program Files (x86)\Windows Kits\10'
$sdkVersion='10.0.26100.0'
$sourceItems=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|Where-Object Extension -in @('.cpp','.h','.rc','.manifest')|
 Sort-Object Name|ForEach-Object {[pscustomobject]@{Name=$_.Name;SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
foreach($name in @('../include/dock-geometry-latest.h','../include/icon-optics-latest.h','../include/aggregate-bridge-latest.h')){
 $sourceItems+= [pscustomobject]@{Name=$name;SHA256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $name)).Hash}
}
Push-Location $PSScriptRoot
try {
 & (Join-Path $sdk "bin\$sdkVersion\x64\rc.exe") /nologo /I (Join-Path $sdk "Include\$sdkVersion\um") /I (Join-Path $sdk "Include\$sdkVersion\shared") /fo $resource 'latest.rc'
 if($LASTEXITCODE){throw 'Aggregate manifest compilation failed'}
} finally { Pop-Location }
$compileLog=Join-Path $out 'compile-latest.log'
if(Test-Path -LiteralPath $compileLog){Copy-Item -LiteralPath $compileLog -Destination ($compileLog+'.before-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
Set-Content -LiteralPath $compileLog -Value '' -Encoding utf8NoBOM
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static -mwindows -municode -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 -include windowsx.h (Join-Path $PSScriptRoot 'latest.cpp') $resource -o $exe -lole32 -loleaut32 -lruntimeobject -ldwmapi -lshcore -lshell32 -lshlwapi -ladvapi32 -lwtsapi32 -lgdi32 -luuid -ld3d11 -ld2d1 -ldwrite -lwindowscodecs -ldxgi -liphlpapi -lpowrprof -lbcrypt -lwbemuuid -lwlanapi -limm32 -lversion 2>&1 | Tee-Object -FilePath (Join-Path $out 'compile-latest.log')
if($LASTEXITCODE){throw 'Aggregate compilation failed'}
foreach($sourceItem in $sourceItems){
 if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $sourceItem.Name)).Hash -ne $sourceItem.SHA256){throw 'Source changed during build'}
}
$result=[pscustomobject]@{Stage='candidate';Version='0.8.112';Executable=$exe;SHA256=(Get-FileHash -LiteralPath $exe).Hash;BuiltAt=(Get-Date -Format o);Inputs=$sourceItems}
$result|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'latest.json') -Encoding utf8NoBOM
Copy-Item -LiteralPath $exe -Destination (Join-Path $PSScriptRoot 'latest.exe') -Force
$pin='#pragma once'+[Environment]::NewLine+'inline constexpr wchar_t AggregatePanelHash[]=L"'+$result.SHA256+'";'+[Environment]::NewLine
Set-Content -LiteralPath (Join-Path $PSScriptRoot '../include/aggregate-panel-pin-generated.h') -Value $pin -Encoding utf8NoBOM
$result|Select-Object Version,SHA256,BuiltAt|ConvertTo-Json

