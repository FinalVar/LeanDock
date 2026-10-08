param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$clang=Join-Path $CompilerRoot 'bin\clang++.exe'
$objects=@('buffer','hook','trampoline','hde-hde64') | ForEach-Object {Join-Path $root ('build\objects\'+$_+'.o')}
$args=@('-target','x86_64-w64-mingw32','-std=c++23','-O0','-static','-DUNICODE','-D_UNICODE',
    '-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-D__USE_MINGW_ANSI_STDIO=0','-I',(Join-Path $root 'include'),
    (Join-Path $PSScriptRoot 'latest.cpp'))+$objects+@('-o',(Join-Path $PSScriptRoot 'latest.exe'),'-lbcrypt')
& $clang @args 2>&1 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'latest-build.log')
if($LASTEXITCODE -ne 0){throw 'Runtime regression compilation failed'}
$text=& (Join-Path $PSScriptRoot 'latest.exe')
$code=$LASTEXITCODE
$text | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'latest.txt')
$text
if($code -ne 0){throw 'Runtime regressions failed'}
