param([switch]$SkipGenerate,[switch]$ControllerOnly,[switch]$BackendOnly,
      [string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT,[string]$PythonPath=$env:LEANDOCK_PYTHON)
if(-not $CompilerRoot){$CompilerRoot=Join-Path $env:ProgramFiles 'Windhawk\Compiler'}
if(-not $PythonPath){$PythonPath=(Get-Command python -CommandType Application -ErrorAction Stop).Source}
$ErrorActionPreference='Stop'
if($ControllerOnly -and $BackendOnly){throw 'ControllerOnly and BackendOnly are mutually exclusive.'}
$root=Split-Path $PSScriptRoot -Parent
$compiler=$CompilerRoot
$clang=Join-Path $compiler 'bin\clang++.exe'
$clangC=Join-Path $compiler 'bin\clang-20.exe'
$python=$PythonPath
if(-not $SkipGenerate) {
    if(-not $ControllerOnly){
        & $python (Join-Path $PSScriptRoot 'generate-latest.py')
        if($LASTEXITCODE -ne 0){throw 'Source generation failed.'}
    }
    if(-not $BackendOnly){
        & $python (Join-Path $PSScriptRoot 'adapt-hotcorners-latest.py')
        if($LASTEXITCODE -ne 0){throw 'Hot-corner adaptation failed.'}
    }
}
$build=Join-Path $root 'build\objects'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$common=@('-target','x86_64-w64-mingw32','-O2','-DUNICODE','-D_UNICODE',
    '-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00',
    '-DNTDDI_VERSION=0x0A000008','-D__USE_MINGW_ANSI_STDIO=0',
    '-ffunction-sections','-fdata-sections','-I',(Join-Path $root 'include'))
$objects=[Collections.Generic.List[string]]::new()
if(-not $ControllerOnly){
$headerTime=(@(Get-ChildItem -LiteralPath (Join-Path $root 'include') -File)+@(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Filter '*.inc' -File) | Measure-Object LastWriteTimeUtc -Maximum).Maximum
foreach($name in @('buffer','hook','trampoline','hde\hde64')) {
    $file=Join-Path $root ('vendor\minhook\src\'+$name+'.c')
    $object=Join-Path $build (($name -replace '\\','-')+'.o')
    & $clangC @common '-std=c11' '-c' $file '-o' $object 2>&1 | Tee-Object -FilePath (Join-Path $build 'last-compile.log')
    if($LASTEXITCODE -ne 0){throw "C compilation failed: $name"}
    $objects.Add($object)
}
foreach($name in @('runtime','size','tray','hide','show','styler','cards')) {
    $file=Join-Path $root ('src\'+$name+'-latest.cpp')
    $object=Join-Path $build ($name+'.o')
    if((Test-Path -LiteralPath $object) -and
       (Get-Item -LiteralPath $object).LastWriteTimeUtc -gt (Get-Item -LiteralPath $file).LastWriteTimeUtc -and
       (Get-Item -LiteralPath $object).LastWriteTimeUtc -gt $headerTime) {
        $objects.Add($object)
        continue
    }
    Write-Output "Compiling $name"
    & $clang @common '-std=c++23' '-c' $file '-o' $object 2>&1 | Tee-Object -FilePath (Join-Path $build ($name+'-compile.log'))
    if($LASTEXITCODE -ne 0){throw "C++ compilation failed: $name"}
    $objects.Add($object)
}
}
$link=@('-target','x86_64-w64-mingw32','-static','-static-libstdc++','-static-libgcc','-Wl,--gc-sections','-Wl,--strip-all')
$dll=Join-Path $root 'latest.dll'
if(-not $ControllerOnly){
if(Test-Path -LiteralPath $dll){Copy-Item -LiteralPath $dll -Destination (Join-Path $PSScriptRoot ('previous-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.dll'))}
& $clang @link '-shared' @objects '-o' $dll '-lole32' '-loleaut32' '-lruntimeobject' '-lversion' '-lshlwapi' '-lcomctl32' '-lshell32' '-lbcrypt' '-ldwmapi' '-lpsapi' '-luuid' '-lshcore' '-lgdi32'
if($LASTEXITCODE -ne 0){throw 'Standalone backend link failed.'}
}
if(-not $BackendOnly){
    & $clang @common @link '-std=c++23' '-mwindows' '-municode' (Join-Path $root 'src\controller-latest.cpp') (Join-Path $root 'src\hotcorners-latest.cpp') '-o' (Join-Path $root 'latest.exe') '-lshell32' '-lbcrypt' '-ldwmapi' '-lwtsapi32' '-ladvapi32'
    if($LASTEXITCODE -ne 0){throw 'Controller compilation failed.'}
}
if(-not $ControllerOnly -and -not $BackendOnly){& (Join-Path $PSScriptRoot 'start-latest.ps1') -CompilerRoot $compiler}
[pscustomobject]@{
    Version='0.5.1';ControllerVersion='0.3.1';BackendVersion='0.4.1';StartLayoutVersion='0.1.2';BuiltAt=(Get-Date -Format o);Standalone=$true;HotCornersIntegrated=$true;CardsIntegrated=$true
    RuntimeFiles=@(Get-Item -LiteralPath (Join-Path $root 'latest.exe'),$dll,(Join-Path $root 'start\latest.dll') | ForEach-Object {
        [pscustomobject]@{Name=[IO.Path]::GetRelativePath($root,$_.FullName);Bytes=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
} | ConvertTo-Json -Depth 4 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'latest.json')
