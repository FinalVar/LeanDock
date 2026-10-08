function Resolve-LeanDockCompiler {
    param([string]$CompilerRoot)
    if(-not $CompilerRoot){$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT}
    if(-not $CompilerRoot){$CompilerRoot=[Environment]::GetEnvironmentVariable('LEANDOCK_COMPILER_ROOT','User')}
    if(-not $CompilerRoot){throw 'Specify -CompilerRoot or LEANDOCK_COMPILER_ROOT for a standalone Clang/MinGW/WinRT toolchain. Windhawk is not required.'}
    $resolved=[IO.Path]::GetFullPath($CompilerRoot)
    foreach($relative in @('bin/clang++.exe','bin/clang-20.exe','bin/ld.lld.exe','include/winrt/Windows.UI.Xaml.h')){
        if(-not (Test-Path -LiteralPath (Join-Path $resolved $relative) -PathType Leaf)){throw "Incomplete LeanDock toolchain: missing $relative"}
    }
    $resolved
}
