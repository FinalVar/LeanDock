param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
if(-not $CompilerRoot){$CompilerRoot=Join-Path $env:ProgramFiles 'Windhawk\Compiler'}
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'start'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$flags=@('-target','x86_64-w64-mingw32','-std=c++23','-O2','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00','-static','-static-libstdc++','-static-libgcc','-Wl,--gc-sections','-Wl,--strip-all')
& (Join-Path $CompilerRoot 'bin\clang++.exe') @flags '-shared' (Join-Path $root 'src\start-layout-latest.cpp') '-o' (Join-Path $out 'latest.dll') '-lole32' '-loleaut32' '-lruntimeobject' '-lcomctl32'
if($LASTEXITCODE){throw 'Start layout compilation failed'}
Get-Item -LiteralPath (Join-Path $out 'latest.dll') | Select-Object Name,Length
