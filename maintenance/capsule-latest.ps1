param(
 [ValidateSet('Status','Start','Stop','Update','Rollback','Watch')][string]$Action='Status',
 [string]$PackagePath='', [string]$Destination=(Split-Path $PSScriptRoot -Parent),
 [string]$BackupPath='', [switch]$ValidateOnly, [int]$ParentProcessId=0, [long]$ParentStarted=0
)
# The architecture migration uses the existing exact-path, checksum, trust and
# rollback primitives. It never changes Windows security, pins or other tasks.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'files-latest.ps1')
$Destination=[IO.Path]::GetFullPath($Destination).TrimEnd('\','/')
$exe=Resolve-LeanDockChild $Destination 'latest.exe'
$capsuleExe=Resolve-LeanDockChild $Destination 'capsule/latest.exe'
$session=(Get-Process -Id $PID).SessionId
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$admin=[Security.Principal.WindowsPrincipal]::new($identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if(-not ('CapsuleMaintenanceWindows' -as [type])){
 Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices;
public static class CapsuleMaintenanceWindows {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent,IntPtr after,string cls,string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint a,bool i,uint p);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern bool QueryFullProcessImageName(IntPtr p,uint f,StringBuilder b,ref uint n);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr p);
 public static string Read(uint id){var p=OpenProcess(0x1000,false,id);if(p==IntPtr.Zero)return null;try{uint n=32768;var b=new StringBuilder((int)n);return QueryFullProcessImageName(p,0,b,ref n)?b.ToString():null;}finally{CloseHandle(p);}}
}
'@
}
function Real-ShellId {
 $w=[IntPtr]::Zero
 while(($w=[CapsuleMaintenanceWindows]::FindWindowEx([IntPtr]::Zero,$w,'Shell_TrayWnd',$null)) -ne [IntPtr]::Zero){
  [uint32]$id=0;$null=[CapsuleMaintenanceWindows]::GetWindowThreadProcessId($w,[ref]$id)
  $p=Get-Process -Id $id -ErrorAction SilentlyContinue
  if($p -and $p.SessionId -eq $session -and [CapsuleMaintenanceWindows]::Read($id) -ieq (Join-Path $env:windir 'explorer.exe')){return $id}
 }
 return 0
}
function Invoke-Capsule([string]$Mode){
 $p=Start-Process -FilePath $capsuleExe -ArgumentList $Mode -WindowStyle Hidden -PassThru
 if(-not $p.WaitForExit(20000)){$p.Kill();$p.WaitForExit();throw "Exact helper timed out: $Mode"}
 $p.ExitCode
}
function Read-CapsuleStatus {
 $code=Invoke-Capsule '--status'
 $state=Get-Content -LiteralPath (Join-Path $Destination 'capsule\latest-status.json') -Raw|ConvertFrom-Json
 if($state.architecture -ne 'capsule'){throw 'This deployment is not the capsule architecture'}
 $native=& (Join-Path $Destination 'native-control-latest.ps1') -Action Status|ConvertFrom-Json
 $native|Add-Member -Force NoteProperty Capsule $state
 $native|Add-Member -Force NoteProperty architecture 'capsule'
 $controller=Get-Content -LiteralPath (Join-Path $Destination 'latest-status.json') -Raw|ConvertFrom-Json
 if($controller.PSObject.Properties['searchPositionSupported']){
  $native|Add-Member -Force NoteProperty SearchPosition ([pscustomobject]@{
   Supported=$controller.searchPositionSupported;Active=$controller.searchPositionActive
   Blocked=$controller.searchPositionBlocked;Failures=$controller.searchPositionFailures
   Error=$controller.searchPositionError;Window=$controller.searchPositionWindow
   Adjustments=$controller.searchPositionAdjustments;InitStage=$controller.searchInitStage
   InitError=$controller.searchInitError
  })
 }
 $native.DockReady=$native.DockReady -and $state.ready -and $state.trayReady -and $state.normalUser
 $native.Live=$native.Live -and $native.DockReady
 $native.ComponentHealth=Get-LeanDockComponentHealth $native
 $native
}
function Read-OwnedTask {
 $task=Get-ScheduledTask -TaskName NativeTaskbarDock -ErrorAction Stop
 if(@($task.Actions).Count -ne 1 -or $task.Actions[0].Execute -ine $exe -or
    -not [string]::IsNullOrWhiteSpace($task.Actions[0].Arguments) -or
    $task.Principal.UserId -notin @($identity.Name,$identity.User.Value,$env:USERNAME)){
  throw 'NativeTaskbarDock task identity does not match this deployment'
 }
 $task
}
function Stop-Capsule {
 $before=Read-CapsuleStatus
 & (Join-Path $Destination 'native-control-latest.ps1') -Action Stop|Out-Null
 foreach($id in @($before.Capsule.uiPid,$before.Capsule.guardPid)|Where-Object {$_ -gt 0}){
  $p=Get-Process -Id $id -ErrorAction SilentlyContinue
  if($p){
   if($p.Path -ine $capsuleExe -or $p.SessionId -ne $session){throw 'Capsule process identity changed'}
   if(-not $p.WaitForExit(12000)){throw 'Capsule did not exit and restore the native taskbar'}
  }
 }
}
function Start-Capsule {
 $task=Read-OwnedTask
 if([string]$task.Principal.RunLevel -ne 'Highest'){throw 'Existing companion task privilege differs'}
 $existing=Read-CapsuleStatus
 $required=@(Get-LeanDockReadinessRequirements $existing)
 $receiptPath=Join-Path $Destination 'latest.json'
 if(Test-Path -LiteralPath $receiptPath){
  $receipt=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
  if($receipt.Architecture -eq 'capsule' -and $receipt.Status){
   $required=@($required+@(Get-LeanDockReadinessRequirements $receipt.Status)|Sort-Object -Unique)
  }
 }
 if($existing.Live -and -not @(Get-LeanDockReadinessFailures $existing $required).Count){return $existing}
 # XAML diagnostics and detour images deliberately remain pinned after Stop.
 # Re-attaching to this same Explorer failed with E_UNEXPECTED in the real
 # lifecycle test. Use the established exact-host reload, never reinitialize
 # the pinned XAML consumer in place or report UI readiness as full recovery.
 $reload=$existing.Backend -and $existing.Backend.explorerPid -eq $existing.ExplorerPid
 if($existing.Running){
  Stop-Capsule
 }
 if($reload){
  Restart-OwnedExplorer $existing.ExplorerPid
 }
 Start-ScheduledTask -TaskName NativeTaskbarDock
 $deadline=[datetime]::UtcNow.AddSeconds(45)
 do {
  Start-Sleep -Milliseconds 300
  $state=Read-CapsuleStatus
  $missing=@(Get-LeanDockReadinessFailures $state $required)
  if($state.Live -and $state.Capsule.guardPid -and $state.Capsule.uiPid -and -not $missing.Count){return $state}
 }while([datetime]::UtcNow -lt $deadline)
 throw ('Capsule did not recover all required components: '+($missing -join ', '))
}
function Save-Plan($plan) {
 $plan|ConvertTo-Json -Depth 15|Set-Content -LiteralPath (Join-Path $BackupPath 'transaction-latest.json') -Encoding utf8NoBOM
}
function Copy-CapsuleFile([string]$SourceRoot,[string]$TargetRoot,[string]$Relative,[string]$Hash){
 $src=Resolve-LeanDockChild $SourceRoot $Relative;$dst=Resolve-LeanDockChild $TargetRoot $Relative
 if((Get-FileHash -LiteralPath $src).Hash -ine $Hash){throw 'Source changed after validation'}
 if((Test-Path -LiteralPath $dst) -and (Get-FileHash -LiteralPath $dst).Hash -ieq $Hash){return}
 Copy-LeanDockFile $SourceRoot $TargetRoot $Relative $Hash
}
function Restart-OwnedExplorer([int]$ExplorerId){
 $shell=Get-Process -Id $ExplorerId -ErrorAction SilentlyContinue
 if($shell){
  if($shell.Path -ine (Join-Path $env:windir 'explorer.exe') -or $shell.SessionId -ne $session){throw 'Explorer identity mismatch'}
  $shell.Kill();if(-not $shell.WaitForExit(10000)){throw 'Explorer did not exit'}
 }
 $deadline=[datetime]::UtcNow.AddSeconds(25)
 do{
  # UI/relay have already stopped, so only the real Explorer taskbar may match.
  if(Real-ShellId){return}
  Start-Sleep -Milliseconds 250
 }while([datetime]::UtcNow -lt $deadline)
 throw 'Explorer did not recreate taskbar'
}
# Same pinned-host release rule as maintenance/latest.ps1. A disabled subclass
# is not an unloaded DLL: replace changed native companions only after their
# exact owning process has retired. The Search adapter shares the Start DLL.
function Get-CapsulePinnedHosts([string[]]$Changed){
 @(foreach($entry in @(
  @('start/latest.dll','StartMenuExperienceHost.exe','SystemApps\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\StartMenuExperienceHost.exe'),
  @('start/latest.dll','SearchHost.exe','SystemApps\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\SearchHost.exe'),
  @('panels/latest.dll','ShellHost.exe','System32\ShellHost.exe'))){
  if($entry[0] -notin $Changed){continue}
  $expected=Join-Path $env:windir $entry[2]
  foreach($record in Get-CimInstance Win32_Process -Filter ("Name='"+$entry[1]+"'")|Where-Object SessionId -EQ $session){
   $p=Get-Process -Id $record.ProcessId -ErrorAction SilentlyContinue
   if(-not $p){continue}
   if($p.Path -ine $expected -or $p.SessionId -ne $session){throw 'Pinned companion identity mismatch'}
   $modulePath=Resolve-LeanDockChild $Destination $entry[0]
   if(@($p.Modules|Where-Object FileName -IEQ $modulePath).Count){
    [pscustomobject]@{Id=$p.Id;Path=$p.Path;Start=$p.StartTime.ToUniversalTime().Ticks;Module=$entry[0]}
   }
  }
 })
}
function Stop-CapsulePinnedHosts([object[]]$Hosts){
 foreach($owned in $Hosts){
  $p=Get-Process -Id $owned.Id -ErrorAction SilentlyContinue
  if(-not $p){continue}
  if($p.Path -ine $owned.Path -or $p.SessionId -ne $session -or
     $p.StartTime.ToUniversalTime().Ticks -ne $owned.Start){throw 'Companion changed before unload'}
  $p.Kill();if(-not $p.WaitForExit(10000)){throw 'Pinned companion did not exit'}
 }
}
function Grant-CapsuleSearchRead($plan){
 # Grant one package read/execute on one owned component, not its containing
 # directory or user documents. Do not modify AppContainer/system policies.
 if(-not ('LeanDockSearchPackageSid' -as [type])){
  Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;using System.Security.Principal;
public static class LeanDockSearchPackageSid {
 [DllImport("userenv.dll",CharSet=CharSet.Unicode)] static extern int DeriveAppContainerSidFromAppContainerName(string name,out IntPtr sid);
 [DllImport("advapi32.dll")] static extern IntPtr FreeSid(IntPtr sid);
 public static string Get(){IntPtr sid;int hr=DeriveAppContainerSidFromAppContainerName("MicrosoftWindows.Client.CBS_cw5n1h2txyewy",out sid);if(hr<0)Marshal.ThrowExceptionForHR(hr);try{return new SecurityIdentifier(sid).Value;}finally{FreeSid(sid);}}
}
'@
 }
 $path=Resolve-LeanDockChild $Destination 'start/latest.dll'
 $acl=Get-Acl -LiteralPath $path
 $sid=[Security.Principal.SecurityIdentifier]::new([LeanDockSearchPackageSid]::Get())
 $rule=[Security.AccessControl.FileSystemAccessRule]::new($sid,[Security.AccessControl.FileSystemRights]::ReadAndExecute,[Security.AccessControl.AccessControlType]::Allow)
 $acl.AddAccessRule($rule)
 $plan.StartAccessApplied=$acl.GetSecurityDescriptorSddlForm([Security.AccessControl.AccessControlSections]::Access)
 Save-Plan $plan
 Set-Acl -LiteralPath $path -AclObject $acl
 if((Get-Acl -LiteralPath $path).GetSecurityDescriptorSddlForm([Security.AccessControl.AccessControlSections]::Access) -ne $plan.StartAccessApplied){throw 'Search component read permission did not match the recorded transaction'}
}
function Restore-CapsuleStartAccess($plan){
 if(-not $plan.PSObject.Properties['StartAccessApplied'] -or -not $plan.StartAccessApplied){return}
 $path=Resolve-LeanDockChild $Destination 'start/latest.dll';$acl=Get-Acl -LiteralPath $path
 $current=$acl.GetSecurityDescriptorSddlForm([Security.AccessControl.AccessControlSections]::Access)
 if($current -eq $plan.StartAccessBefore){return}
 if($current -ne $plan.StartAccessApplied){throw 'Start component access changed externally; refusing to replace it'}
 $acl.SetSecurityDescriptorSddlForm($plan.StartAccessBefore,[Security.AccessControl.AccessControlSections]::Access)
 Set-Acl -LiteralPath $path -AclObject $acl
}
function Read-Package {
 $p=Resolve-LeanDockChild $PackagePath 'package-latest.json'
 $manifest=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json
 if($manifest.Schema -ne 1 -or $manifest.Product -ne 'LeanDock' -or $manifest.Architecture -ne 'capsule' -or $manifest.LegacyComponentPolicy -ne 'Preserve' -or
    @($manifest.Files).Count -gt 150){throw 'Invalid capsule package'}
 $seen=@{}
 foreach($file in $manifest.Files){
  if($file.Name -match '(^|/)(latest\.ini|latest\.json|latest-status\.json)$|^(backups|verification)/'){throw 'Configuration/runtime state in package'}
  if($seen.ContainsKey($file.Name)){throw 'Duplicate package member'}
  $seen[$file.Name]=$true
  $path=Resolve-LeanDockChild $PackagePath $file.Name
  if($file.SHA256 -notmatch '^[A-Fa-f0-9]{64}$' -or (Get-FileHash -LiteralPath $path).Hash -ine $file.SHA256){throw 'Capsule package hash mismatch'}
 }
 foreach($name in @('latest.exe','latest.dll','capsule/latest.exe','start/latest.dll','panels/latest.dll','native-control-latest.ps1','latest.ps1','maintenance/capsule-latest.ps1','maintenance/files-latest.ps1','README.md','NOTICE.md','LICENSE.txt')){
  if(-not $seen.ContainsKey($name)){throw "Missing package member: $name"}
 }
 $manifest
}
function Confirm-Trust($manifest) {
 $policy=Get-LeanDockApplicationControlState
 Assert-LeanDockRuntimeTrust (Get-LeanDockRuntimeTrust $Destination) $policy
 Assert-LeanDockRuntimeTrust (Get-LeanDockRuntimeTrust $PackagePath) $policy
 $new=Join-Path $PackagePath 'capsule/latest.exe'
 if($policy -eq 'On' -and (Get-AuthenticodeSignature -LiteralPath $new).Status -ne 'Valid'){
  throw 'Smart App Control is enabled; no runtime was stopped'
 }
 if($manifest.VerificationExecutableSHA256 -ine (Get-FileHash -LiteralPath $new).Hash -or
    -not $manifest.TechnicalChecksPassed){throw 'Package lacks matching technical verification'}
 if($manifest.CompanionExecutableSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $PackagePath 'latest.exe')).Hash -or
    $manifest.CompanionBackendSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $PackagePath 'latest.dll')).Hash){throw 'Companion hash mismatch'}
}
function Restore-CapsuleTransaction($plan) {
 Test-LeanDockRestore $plan $BackupPath
 if((Get-FileHash -LiteralPath (Join-Path $BackupPath 'task-before.xml')).Hash -ne $plan.TaskSHA256){throw 'Original task XML checksum mismatch'}
 Assert-LeanDockRuntimeTrust (Get-LeanDockRuntimeTrust $Destination $plan $BackupPath) (Get-LeanDockApplicationControlState)
 $task=Read-OwnedTask
 if($plan.State -ne 'Prepared'){
  $changed=@($plan.Files|Where-Object {$_.PreviousHash -ne $_.NewHash}|ForEach-Object Name)
  $pinned=@(Get-CapsulePinnedHosts $changed)
  # This works even if copying stopped halfway through the package.
  $children=@(Get-CimInstance Win32_Process -Filter "Name='latest.exe'"|Where-Object {$_.SessionId -eq $session -and $_.ExecutablePath -ieq $capsuleExe})
  $p=Start-Process -FilePath $exe -ArgumentList '--stop' -WindowStyle Hidden -PassThru
  if(-not $p.WaitForExit(35000)){$p.Kill();$p.WaitForExit();throw 'Controller stop timed out during recovery'}
  if($p.ExitCode -ne 0){throw 'Controller could not confirm restoration during recovery'}
  foreach($entry in $children){
   $child=Get-Process -Id $entry.ProcessId -ErrorAction SilentlyContinue
   if($child -and $child.Path -ieq $capsuleExe -and -not $child.WaitForExit(12000)){throw 'Capsule recovery cleanup is still pending'}
  }
  Stop-CapsulePinnedHosts $pinned
  Restart-OwnedExplorer (Real-ShellId)
 }
 Test-LeanDockRestore $plan $BackupPath
 foreach($f in $plan.Files){
  if($f.Existed){Copy-CapsuleFile (Join-Path $BackupPath 'original') $Destination $f.Name $f.PreviousHash}
  else{$target=Resolve-LeanDockChild $Destination $f.Name;if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target -Force}}
 }
 Restore-CapsuleStartAccess $plan
 if($plan.WasRunning){
  $state=& (Join-Path $Destination 'latest.ps1') -Action Start|ConvertFrom-Json
  $deadline=[datetime]::UtcNow.AddSeconds(20)
  do {
   $missing=@(Get-LeanDockReadinessFailures $state @($plan.RequiredComponents))
   if(-not $missing.Count){break}
   if([datetime]::UtcNow -ge $deadline){throw ('Rollback components missing: '+($missing -join ', '))}
   Start-Sleep -Milliseconds 500
   $state=& (Join-Path $Destination 'latest.ps1') -Action Status|ConvertFrom-Json
  }while($true)
 }
 $plan.State='RolledBack';$plan.CompletedAt=Get-Date -Format o;Save-Plan $plan
 New-Item -ItemType File -Path (Join-Path $BackupPath 'restored') -Force|Out-Null
}
if($Action -eq 'Status'){Read-CapsuleStatus|ConvertTo-Json -Depth 5;return}
if($Action -eq 'Stop'){Stop-Capsule;'Stopped; native taskbar restored.';return}
if($Action -eq 'Start'){Start-Capsule|ConvertTo-Json -Depth 5;return}
if($Action -eq 'Watch'){
 $deadline=[datetime]::UtcNow.AddMinutes(5)
 do {
  if((Test-Path -LiteralPath (Join-Path $BackupPath 'committed')) -or (Test-Path -LiteralPath (Join-Path $BackupPath 'restored'))){return}
  $parent=Get-Process -Id $ParentProcessId -ErrorAction SilentlyContinue
  if(-not $parent -or $parent.StartTime.ToUniversalTime().Ticks -ne $ParentStarted){$Action='Rollback';break}
  Start-Sleep -Seconds 1
 }while([datetime]::UtcNow -lt $deadline)
 if($Action -eq 'Watch'){throw 'Parent still alive; concurrent rollback was not attempted'}
}
if(-not $ValidateOnly -and -not $admin){throw 'The verified native companion reload requires an elevated maintenance process; no runtime was stopped.'}
$mutex=[Threading.Mutex]::new($false,('Local\LeanDock.Maintenance.'+$identity.User.Value))
$locked=$false
try {
 try{$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true}
 if(-not $locked){throw 'Another LeanDock maintenance transaction is running'}
 $task=Read-OwnedTask
 if($Action -eq 'Rollback'){
  if(-not $BackupPath){$BackupPath=(Get-Content -LiteralPath (Join-Path $Destination 'latest.json') -Raw|ConvertFrom-Json).Backup}
  $BackupPath=[IO.Path]::GetFullPath($BackupPath)
  if(-not $BackupPath.StartsWith((Join-Path $Destination 'backups')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Rollback outside deployment backups'}
  $plan=Get-Content -LiteralPath (Resolve-LeanDockChild $BackupPath 'transaction-latest.json') -Raw|ConvertFrom-Json
  if($plan.Architecture -ne 'capsule' -or $plan.Destination -ine $Destination -or $plan.Session -ne $session){throw 'Rollback belongs to another deployment/session'}
  Test-LeanDockRestore $plan $BackupPath
  if($ValidateOnly){'Rollback material verified';return}
  Restore-CapsuleTransaction $plan;return
 }
 $PackagePath=[IO.Path]::GetFullPath($PackagePath).TrimEnd('\','/')
 if($PackagePath -eq $Destination -or $PackagePath.StartsWith($Destination+'\',[StringComparison]::OrdinalIgnoreCase) -or
    $Destination.StartsWith($PackagePath+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Package and deployment must be separate'}
 $manifest=Read-Package;Confirm-Trust $manifest
 $before=& (Join-Path $Destination 'latest.ps1') -Action Status|ConvertFrom-Json
  if($before.architecture -and $before.architecture -ne 'capsule'){throw 'Unrecognized source architecture'}
  if(-not $before.Live -or $before.ComponentHealth.State -ne 'Ready'){throw 'Current Dock and retained components must be healthy before updating'}
 # Staged validators parse config, but those copies never enter the payload.
 foreach($name in @('latest.ini','hotcorners/latest.ini')){
  $hash=(Get-FileHash -LiteralPath (Resolve-LeanDockChild $Destination $name)).Hash
  Copy-CapsuleFile $Destination $PackagePath $name $hash
 }
 foreach($mode in @('--verify','--verify-corners')){
  $verify=Start-Process -FilePath (Join-Path $PackagePath 'latest.exe') -ArgumentList $mode -WindowStyle Hidden -PassThru
  if(-not $verify.WaitForExit(15000)){$verify.Kill();$verify.WaitForExit();throw "Native verification timed out: $mode"}
  if($verify.ExitCode -ne 0){throw "Native verification failed: $mode ($($verify.ExitCode))"}
 }
 if($ValidateOnly){
  [pscustomobject]@{State='Prepared';Architecture='capsule';Version=$manifest.Version;Files=$manifest.Files.Count;
   ElevationRequired=(-not $admin);PreservesWindowsPins=$true;TaskRunLevelAfter='Highest unchanged; capsule child Medium';
   LegacyComponents=@(Get-LeanDockReadinessRequirements $before);LegacyComponentPolicy=$manifest.LegacyComponentPolicy;
   DisabledLegacySurface=@('native-dock-styles','icon-optics','tray-layout','old-cards','old-hide-reveal')}|ConvertTo-Json -Depth 6
  return
 }
 if($manifest.LegacyComponentPolicy -ne 'Preserve'){
  throw 'User requires legacy companion retention; refusing this package'
 }
 # Test the actual elevated-parent -> Medium child route before stopping anything.
 $probe=Start-Process -FilePath (Join-Path $PackagePath 'latest.exe') -ArgumentList '--capsule-token-probe' -WindowStyle Hidden -PassThru
 if(-not $probe.WaitForExit(15000)){$probe.Kill();$probe.WaitForExit();throw 'Ordinary-user token preflight timed out'}
 if($probe.ExitCode -ne 0){throw "Ordinary-user token preflight failed: $($probe.ExitCode)"}
 $tokenProof=Get-Content -LiteralPath (Join-Path $PackagePath 'capsule\token-probe-latest.json') -Raw|ConvertFrom-Json
 if(-not $tokenProof.normalMediumUser){throw 'Child token did not verify Medium normal-user context'}
 $BackupPath=Join-Path $Destination ('backups\capsule-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
 New-Item -ItemType Directory -Path $BackupPath|Out-Null
 $taskXml=Export-ScheduledTask -TaskName NativeTaskbarDock
 $taskXml|Set-Content -LiteralPath (Join-Path $BackupPath 'task-before.xml') -Encoding unicode
 $files=@(foreach($entry in $manifest.Files){
  $target=Resolve-LeanDockChild $Destination $entry.Name;$exists=Test-Path -LiteralPath $target
  $hash=if($exists){(Get-FileHash -LiteralPath $target).Hash}else{$null}
  if($exists){Copy-LeanDockFile $Destination (Join-Path $BackupPath 'original') $entry.Name $hash}
  [pscustomobject]@{Name=$entry.Name;Existed=$exists;PreviousHash=$hash;NewHash=$entry.SHA256}
 })
 # Preserve the complete original runnable set, including unchanged DLLs.
 # An in-place leftover is not a recovery copy.
 foreach($name in @('latest.dll','start/latest.dll','panels/latest.dll')){
  if(@($files|Where-Object Name -EQ $name).Count){continue}
  $hash=(Get-FileHash -LiteralPath (Resolve-LeanDockChild $Destination $name)).Hash
  Copy-LeanDockFile $Destination (Join-Path $BackupPath 'original') $name $hash
  $files+=[pscustomobject]@{Name=$name;Existed=$true;PreviousHash=$hash;NewHash=$hash}
 }
 $receipt=Join-Path $Destination 'latest.json'
 $receiptHash=(Get-FileHash -LiteralPath $receipt).Hash
 Copy-LeanDockFile $Destination (Join-Path $BackupPath 'original') 'latest.json' $receiptHash
 $files+= [pscustomobject]@{Name='latest.json';Existed=$true;PreviousHash=$receiptHash;NewHash=$null}
 $config=@(foreach($name in @('latest.ini','hotcorners/latest.ini')){
  [pscustomobject]@{Name=$name;Hash=(Get-FileHash -LiteralPath (Resolve-LeanDockChild $Destination $name)).Hash}
 })
 $plan=[pscustomobject]@{Schema=1;Architecture='capsule';State='Prepared';Destination=$Destination;Session=$session;
  TaskSHA256=(Get-FileHash -LiteralPath (Join-Path $BackupPath 'task-before.xml')).Hash;
  PackagePath=$PackagePath;NewExecutableHash=(Get-FileHash -LiteralPath (Join-Path $PackagePath 'latest.exe')).Hash;
  WasRunning=[bool]$before.Running;RequiredComponents=@(Get-LeanDockReadinessRequirements $before);
  Files=$files;Protected=$config;
  StartAccessBefore=(Get-Acl -LiteralPath (Resolve-LeanDockChild $Destination 'start/latest.dll')).GetSecurityDescriptorSddlForm([Security.AccessControl.AccessControlSections]::Access);
  StartAccessApplied=$null;At=(Get-Date -Format o);CompletedAt=$null;Error=$null}
 Save-Plan $plan
 Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $BackupPath 'maintenance.ps1')
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'files-latest.ps1') -Destination (Join-Path $BackupPath 'files-latest.ps1')
 $watchArgs="-NoProfile -File `"$BackupPath\maintenance.ps1`" -Action Watch -Destination `"$Destination`" -BackupPath `"$BackupPath`" -ParentProcessId $PID -ParentStarted $((Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks)"
 Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList $watchArgs -WindowStyle Hidden|Out-Null
 try {
  # The existing stop protocol detaches all old components. Only Explorer is
  # restarted, after exact identity verification, to release its old Dock DLL.
  $shell=Get-Process -Id $before.ExplorerPid -ErrorAction Stop
  if($shell.Path -ine (Join-Path $env:windir 'explorer.exe') -or $shell.SessionId -ne $session){throw 'Explorer identity mismatch'}
  $shellStart=$shell.StartTime.ToUniversalTime().Ticks
  $changed=@($plan.Files|Where-Object {$_.PreviousHash -ne $_.NewHash}|ForEach-Object Name)
  $pinned=@(Get-CapsulePinnedHosts $changed)
  $plan.State='Stopping';Save-Plan $plan
  & (Join-Path $Destination 'latest.ps1') -Action Stop|Out-Null
  Stop-CapsulePinnedHosts $pinned
  $shell=Get-Process -Id $before.ExplorerPid -ErrorAction SilentlyContinue
  if($shell -and $shell.StartTime.ToUniversalTime().Ticks -ne $shellStart){throw 'Explorer changed before unload'}
  Restart-OwnedExplorer $before.ExplorerPid
  foreach($entry in $manifest.Files){Copy-CapsuleFile $PackagePath $Destination $entry.Name $entry.SHA256}
  Grant-CapsuleSearchRead $plan
  $plan.State='Starting';Save-Plan $plan
  $after=Start-Capsule
  $deadline=[datetime]::UtcNow.AddSeconds(20)
  do{
   $missing=@(Get-LeanDockReadinessFailures $after @($plan.RequiredComponents))
   if(-not $missing.Count){break}
   if([datetime]::UtcNow -ge $deadline){throw ('Retained components missing: '+($missing -join ', '))}
   Start-Sleep -Milliseconds 500;$after=Read-CapsuleStatus
  }while($true)
  foreach($p in $config){if((Get-FileHash -LiteralPath (Resolve-LeanDockChild $Destination $p.Name)).Hash -ne $p.Hash){throw 'Preserved configuration changed'}}
  [pscustomobject]@{Version=$manifest.Version;Architecture='capsule';UpdatedAt=(Get-Date -Format o);
   Backup=$BackupPath;RuntimeFiles=@('latest.exe','latest.dll','capsule/latest.exe','start/latest.dll','panels/latest.dll');Status=$after;SourcePublication='local-package';
   VisualInteractionAcceptance='pending-user';LegacyComponents='preserved; native Dock surface disabled';TaskChanged=$false}|
   ConvertTo-Json -Depth 7|Set-Content -LiteralPath $receipt -Encoding utf8NoBOM
  $plan.State='Applied';$plan.CompletedAt=Get-Date -Format o;Save-Plan $plan
  New-Item -ItemType File -Path (Join-Path $BackupPath 'committed')|Out-Null
  $after|ConvertTo-Json -Depth 5
 }catch {
  $plan.Error=$_.Exception.Message;Save-Plan $plan
  try{Restore-CapsuleTransaction $plan}catch{
   $_|Out-String|Set-Content -LiteralPath (Join-Path $BackupPath 'manual-recovery-required.txt')
   throw
  }
  throw
 }
}finally{if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()}
