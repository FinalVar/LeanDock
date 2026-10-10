param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT,[switch]$IncludeHosted)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$clang=Join-Path $CompilerRoot 'bin\clang++.exe'
$items=@(@{Name='hotcorners-core';Source=(Join-Path $root 'hotcorners\tests\latest.cpp')},@{Name='hotcorners-config';Source=(Join-Path $PSScriptRoot 'hotcorners-config-latest.cpp')},@{Name='hotcorners-host';Source=(Join-Path $PSScriptRoot 'hotcorners-host-latest.cpp')})
if(-not $IncludeHosted){$items=@($items | Where-Object Name -ne 'hotcorners-host')}
if($IncludeHosted){
    $existing=$null
    if([Threading.Mutex]::TryOpenExisting('Local\NativeHotCorners.1.Instance',[ref]$existing)){
        $existing.Dispose();throw 'Hosted tests require the production hot-corner singleton to be stopped.'
    }
}
foreach($item in $items){
    $out=Join-Path $PSScriptRoot ($item.Name+'-latest.exe')
    & $clang -target x86_64-w64-mingw32 -std=c++23 -O2 -static -DUNICODE -D_UNICODE -I (Join-Path $root 'include') $item.Source -o $out -lshell32 -ldwmapi -lwtsapi32
    if($LASTEXITCODE -ne 0){throw ($item.Name+' build failed')}
    & $out | Tee-Object -FilePath (Join-Path $PSScriptRoot ($item.Name+'-latest.txt'))
    if($LASTEXITCODE -ne 0){throw ($item.Name+' checks failed')}
}
