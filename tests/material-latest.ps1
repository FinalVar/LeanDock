param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT)
$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$target=Join-Path $PSScriptRoot 'material-latest.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O1 -static -DUNICODE -D_UNICODE (Join-Path $PSScriptRoot 'material-latest.cpp') -o $target -lole32 -loleaut32 -lruntimeobject -luser32
if($LASTEXITCODE){throw 'Material fixture compilation failed'}
if(-not ('LeanDockMaterialManifest' -as [type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class LeanDockMaterialManifest {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr BeginUpdateResource(string p,bool d);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool UpdateResource(IntPtr h,IntPtr t,IntPtr n,ushort l,byte[] d,uint s);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool EndUpdateResource(IntPtr h,bool d);
}
'@}
$manifest=@'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
 <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3"><security><requestedPrivileges><requestedExecutionLevel level="asInvoker" uiAccess="false"/></requestedPrivileges></security></trustInfo>
 <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1"><application><supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/><maxversiontested Id="10.0.26100.0"/></application></compatibility>
</assembly>
'@
$bytes=[Text.UTF8Encoding]::new($false).GetBytes($manifest)
$handle=[LeanDockMaterialManifest]::BeginUpdateResource($target,$false)
if($handle -eq [IntPtr]::Zero){throw 'Could not embed fixture manifest'}
try{if(-not [LeanDockMaterialManifest]::UpdateResource($handle,[IntPtr]24,[IntPtr]1,0,$bytes,$bytes.Length)){throw 'Fixture manifest update failed'}}finally{if(-not [LeanDockMaterialManifest]::EndUpdateResource($handle,$false)){throw 'Fixture manifest save failed'}}
& $target 2>&1 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'material-latest.txt')
if($LASTEXITCODE){throw "Material fixture returned $LASTEXITCODE"}
