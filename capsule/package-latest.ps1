param([ValidateSet('Preserve')][string]$LegacyComponentPolicy='Preserve')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$legacyRoot='C:\Users\x\Documents\Codex\Tools\NativeTaskbarDock'
$companion=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build\companion\latest.json') -Raw|ConvertFrom-Json
foreach($p in $companion.Inputs.PSObject.Properties){
 if((Get-FileHash -LiteralPath (Join-Path $repo $p.Name)).Hash -ne $p.Value){throw "Companion source changed: $($p.Name)"}
}
$build=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build\latest.json') -Raw|ConvertFrom-Json
$proof=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build\validation-latest.json') -Raw|ConvertFrom-Json
$hash=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'build\latest.exe')).Hash
if($hash -ne $build.SHA256 -or $hash -ne $proof.SHA256 -or -not $proof.TechnicalChecksPassed -or
   -not $proof.NativeTrayProbe){throw 'Matching complete technical evidence is required'}
if(-not $proof.OwnVisualProbe -and $proof.VisualState -notin @('PendingInteractiveDesktop','PendingUserAcceptance')){
 throw 'Visual evidence must be captured or explicitly pending; never silently skip acceptance'
}
if($proof.VisualState -eq 'PendingUserAcceptance' -and -not $proof.VisualDeferredReason){
 throw 'Pending user acceptance requires the reason desktop automation was not performed'
}
foreach($sourceItem in $build.Inputs){
 if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $sourceItem.Name)).Hash -ne $sourceItem.SHA256){throw 'Source changed since tested build'}
}
$root=Join-Path $PSScriptRoot 'package'
$target=Join-Path $root 'latest'
New-Item -ItemType Directory -Path $root -Force|Out-Null
if(Test-Path -LiteralPath $target){
 if((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Package target cannot be a reparse point'}
 $resolved=[IO.Path]::GetFullPath($target)
 if(-not $resolved.StartsWith([IO.Path]::GetFullPath($root)+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Package path escaped its root'}
 Move-Item -LiteralPath $resolved -Destination (Join-Path $root ('previous-'+(Get-Date -Format 'yyyyMMdd-HHmmss')))
}
New-Item -ItemType Directory -Path $target|Out-Null
$map=@{
 'latest.exe'=(Join-Path $PSScriptRoot 'build\companion\latest.exe')
 'latest.dll'=(Join-Path $PSScriptRoot 'build\companion\latest.dll')
 'capsule/latest.exe'=(Join-Path $PSScriptRoot 'build\latest.exe')
 'start/latest.dll'=(Join-Path $PSScriptRoot 'build\companion\start-latest.dll')
 'panels/latest.dll'=(Join-Path $legacyRoot 'panels\latest.dll')
 'latest.ps1'=(Join-Path $PSScriptRoot 'control-latest.ps1')
 'README.md'=(Join-Path $PSScriptRoot 'README.md')
 'NOTICE.md'=(Join-Path $PSScriptRoot 'NOTICE.md')
 'LICENSE.txt'=(Join-Path $repo 'LICENSE.txt')
 'DECISIONS.md'=(Join-Path $repo 'DECISIONS.md')
 'maintenance/capsule-latest.ps1'=(Join-Path $repo 'maintenance\capsule-latest.ps1')
 'maintenance/files-latest.ps1'=(Join-Path $repo 'maintenance\files-latest.ps1')
 'licenses/ManagedShell.txt'=(Join-Path $PSScriptRoot 'licenses\ManagedShell.txt')
 'licenses/NativeCompanions-NOTICE.md'=(Join-Path $repo 'NOTICE.md')
 'include/dock-geometry-latest.h'=(Join-Path $repo 'include\dock-geometry-latest.h')
 'include/icon-optics-latest.h'=(Join-Path $repo 'include\icon-optics-latest.h')
 'include/start-menu-state-latest.h'=(Join-Path $repo 'include\start-menu-state-latest.h')
 'include/shell-actions-latest.h'=(Join-Path $repo 'include\shell-actions-latest.h')
 'include/start-position-latest.h'=(Join-Path $repo 'include\start-position-latest.h')
 'include/panel-position-latest.h'=(Join-Path $repo 'include\panel-position-latest.h')
 'include/search-position-latest.h'=(Join-Path $repo 'include\search-position-latest.h')
 'include/search-host-latest.h'=(Join-Path $repo 'include\search-host-latest.h')
 'src/search-position-latest.inc'=(Join-Path $repo 'src\search-position-latest.inc')
}
if((Get-FileHash -LiteralPath $map['latest.exe']).Hash -ne $companion.ExecutableSHA256 -or
   (Get-FileHash -LiteralPath $map['latest.dll']).Hash -ne $companion.BackendSHA256 -or
   (Get-FileHash -LiteralPath $map['start/latest.dll']).Hash -ne $companion.StartSHA256){throw 'Companion binary changed'}
$native=Get-Content -LiteralPath (Join-Path $repo 'latest.ps1') -Raw
$native=$native.Replace('NativeDockManagementPathV2','NativeDockManagementPathV3')
$native=$native.Replace('$p.WaitForExit(20000)','$p.WaitForExit(35000)')
$native=$native.Replace(' public static string Read(uint id){',
 ' [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent,IntPtr after,string cls,string title);'+
 "`n"+' public static string Read(uint id){')
$a=$native.IndexOf('function Get-ShellProcess {')
$b=$native.IndexOf('function Get-OwnTask {',$a)
if($a -lt 0 -or $b -le $a){throw 'Native control shell anchor drifted'}
$native=$native.Substring(0,$a)+@'
function Get-ShellProcess {
 $window=[IntPtr]::Zero
 do {
  $window=[NativeDockManagementPathV3]::FindWindowEx([IntPtr]::Zero,$window,'Shell_TrayWnd',$null)
  if($window -eq [IntPtr]::Zero){break}
  [uint32]$ownerId=0;$null=[NativeDockManagementPathV3]::GetWindowThreadProcessId($window,[ref]$ownerId)
  if([NativeDockManagementPathV3]::Read($ownerId) -ieq (Join-Path $env:windir 'explorer.exe')){
   $p=Get-CimInstance Win32_Process -Filter "ProcessId=$ownerId"
   if($p.SessionId -eq $session){return $p}
  }
 }while($true)
 throw 'The real Explorer taskbar is not ready'
}

'@+$native.Substring($b)
if(-not $native.Contains(' -and $backend.styledElements -gt 0')){throw 'Native readiness anchor drifted'}
$native=$native.Replace(' -and $backend.styledElements -gt 0','')
$nativePath=Join-Path $PSScriptRoot 'build\native-control-latest.ps1'
$native|Set-Content -LiteralPath $nativePath -Encoding utf8NoBOM
$map['native-control-latest.ps1']=$nativePath
# A normal-user double-click entry also works after the UI was explicitly
# closed while retained shell companions were left running. Keep it inside
# the existing installation directory; never add a Desktop or Start shortcut.
$shortcutPath=Join-Path $PSScriptRoot 'build\LeanDock.lnk'
$shortcutShell=New-Object -ComObject WScript.Shell
try {
 $shortcut=$shortcutShell.CreateShortcut($shortcutPath)
 $shortcut.TargetPath=Join-Path $PSHOME 'pwsh.exe'
 $shortcut.Arguments="-NoProfile -WindowStyle Hidden -File `"$legacyRoot\latest.ps1`" -Action Start"
 $shortcut.WorkingDirectory=$legacyRoot
 $shortcut.IconLocation=(Join-Path $legacyRoot 'capsule\latest.exe')+',0'
 $shortcut.Description='启动 LeanDock，保留原生配套功能'
 $shortcut.Save()
}finally{
 if($shortcut){[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut)}
 [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcutShell)
}
$map['LeanDock.lnk']=$shortcutPath
foreach($file in Get-ChildItem -LiteralPath $PSScriptRoot -File|Where-Object Extension -in @('.cpp','.h','.rc','.manifest','.ps1','.py')){
 $map['capsule/'+$file.Name]=$file.FullName
}
$files=@(foreach($name in $map.Keys|Sort-Object){
 $destination=Join-Path $target $name
 New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force|Out-Null
 Copy-Item -LiteralPath $map[$name] -Destination $destination
 [pscustomobject]@{Name=$name;SHA256=(Get-FileHash -LiteralPath $destination).Hash}
})
[pscustomobject]@{Schema=1;Product='LeanDock';Architecture='capsule';Version=$build.Version;
 LegacyComponentPolicy=$LegacyComponentPolicy;TechnicalChecksPassed=$true;VerificationExecutableSHA256=$hash;
 VisualVerificationState=$proof.VisualState;UserInteractionAccepted=$false;
 CompanionExecutableSHA256=$companion.ExecutableSHA256;CompanionBackendSHA256=$companion.BackendSHA256;
 At=(Get-Date -Format o);Files=$files}|
 ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $target 'package-latest.json') -Encoding utf8NoBOM
[pscustomobject]@{Package=$target;Files=$files.Count;SHA256=$hash}|ConvertTo-Json
