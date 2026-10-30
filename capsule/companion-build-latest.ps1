param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT,
      [string]$PythonPath='C:\Users\x\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
& $PythonPath (Join-Path $PSScriptRoot 'companion-adapt-latest.py')
if($LASTEXITCODE){throw 'Companion adaptation refused source drift'}
$out=Join-Path $PSScriptRoot 'build\companion'
$source=Join-Path $out 'source'
$clang=Join-Path $CompilerRoot 'bin\clang++.exe'
$cc=Join-Path $CompilerRoot 'bin\clang-20.exe'
$flags=@('-target','x86_64-w64-mingw32','-O2','-DUNICODE','-D_UNICODE','-DWINVER=0x0A00',
 '-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00','-DNTDDI_VERSION=0x0A000008','-D__USE_MINGW_ANSI_STDIO=0','-DLEANDOCK_CAPSULE_SEARCH=1',
 '-ffunction-sections','-fdata-sections','-I',(Join-Path $source 'include'))
$link=@('-target','x86_64-w64-mingw32','-static','-static-libstdc++','-static-libgcc','-Wl,--gc-sections','-Wl,--strip-all')
$objects=@()
foreach($name in @('buffer','hook','trampoline','hde/hde64')){
 $obj=Join-Path $out (($name -replace '/','-')+'.o')
 & $cc @flags -std=c11 -c (Join-Path $source ('vendor/minhook/src/'+$name+'.c')) -o $obj
 if($LASTEXITCODE){throw "Companion C compilation: $name"}
 $objects+=$obj
}
foreach($name in @('runtime','styler','cards','companion-stubs')){
 Write-Output "Compiling companion $name"
 $obj=Join-Path $out ($name+'.o')
 & $clang @flags -std=c++23 -c (Join-Path $source ('src/'+$name+'-latest.cpp')) -o $obj 2>&1 |
  Tee-Object -FilePath (Join-Path $out ($name+'-compile.log'))
 if($LASTEXITCODE){throw "Companion C++ compilation: $name"}
 $objects+=$obj
}
& $clang @link -shared @objects -o (Join-Path $out 'latest.dll') -lole32 -loleaut32 -lruntimeobject -lversion -lshlwapi -lcomctl32 -lshell32 -lbcrypt -ldwmapi -lpsapi -luuid -lshcore -lgdi32 -liphlpapi
if($LASTEXITCODE){throw 'Companion backend link failed'}
& $clang @flags @link -std=c++23 -mwindows -municode (Join-Path $source 'src/controller-latest.cpp') (Join-Path $source 'src/hotcorners-latest.cpp') (Join-Path $source 'src/cards-settings-latest.cpp') -o (Join-Path $out 'latest.exe') -lshell32 -lbcrypt -ldwmapi -lwtsapi32 -ladvapi32 -lole32 -loleaut32 -luuid -lcomctl32 -lgdi32 -luserenv
if($LASTEXITCODE){throw 'Companion controller link failed'}
# Same Start runtime, now built from the adapted source snapshot so its pending
# placement reads capsule geometry before considering the native tray relay.
& $clang @flags @link -std=c++23 -shared (Join-Path $source 'src/start-layout-latest.cpp') -o (Join-Path $out 'start-latest.dll') -lole32 -loleaut32 -lruntimeobject -lcomctl32 -ldwmapi
if($LASTEXITCODE){throw 'Companion Start layout link failed'}
$inputs=Get-Content -LiteralPath (Join-Path $out 'inputs-latest.json') -Raw|ConvertFrom-Json -AsHashtable
foreach($name in $inputs.Keys){
 if((Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash -ne $inputs[$name]){throw "Companion input changed: $name"}
}
[pscustomobject]@{Architecture='capsule-companion';Version='0.9.0';At=(Get-Date -Format o);Inputs=$inputs;
 ExecutableSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'latest.exe')).Hash;
 BackendSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'latest.dll')).Hash;
 StartSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'start-latest.dll')).Hash;
 NativeSurfaceModulesLoaded=$false;Preserved=@('hot-corners','ctrl-alt-tab-styles-layout','start-compact','quick-settings','appbar-gap-compatibility')}|
 ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'latest.json') -Encoding utf8NoBOM
'Companion build passed; nothing was loaded'
