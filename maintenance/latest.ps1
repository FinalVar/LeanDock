param(
 [ValidateSet('Install','Update','Rollback','Watch')][string]$Action='Update',
 [string]$PackagePath='', [string]$Destination=(Split-Path $PSScriptRoot -Parent),
 [string]$BackupPath='', [switch]$ValidateOnly,
 [ValidateRange(0,60)][int]$NativeBaselineSeconds=0,
 [int]$ParentProcessId=0,[long]$ParentStarted=0
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'files-latest.ps1')
$Destination=[IO.Path]::GetFullPath($Destination).TrimEnd('\','/')
# Reject reparse-point parents even when the destination is not created yet.
$null=Resolve-LeanDockChild $Destination 'latest.exe'
$session=(Get-Process -Id $PID).SessionId
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
function Read-OwnedTask {
 $t=Get-ScheduledTask -TaskName NativeTaskbarDock -ErrorAction SilentlyContinue
 if($t -and (@($t.Actions).Count -ne 1 -or $t.Actions[0].Execute -ine (Join-Path $Destination 'latest.exe') -or -not [string]::IsNullOrWhiteSpace($t.Actions[0].Arguments) -or $t.Principal.UserId -notin @([Security.Principal.WindowsIdentity]::GetCurrent().Name,[Security.Principal.WindowsIdentity]::GetCurrent().User.Value,$env:USERNAME))){throw 'Scheduled task ownership mismatch'}
 $t
}
function Save-Plan($Plan){$Plan|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $BackupPath 'transaction-latest.json')}
function Native-Status {& (Join-Path $Destination 'latest.ps1') -Action Status|ConvertFrom-Json}
function Confirm-Readiness($State,[string[]]$Required){
 # Native hosts can attach after the core Dock. Preserve the old feature set,
 # with a bounded wait, rather than committing on Live (Dock + corners) alone.
 $deadline=[datetime]::UtcNow.AddSeconds(15)
 do {
  $missing=@(Get-LeanDockReadinessFailures $State $Required)
  if(-not $missing.Count){return $State}
  if([datetime]::UtcNow -ge $deadline){throw ('Components did not recover: '+($missing -join ', '))}
  Start-Sleep -Milliseconds 500
  $State=Native-Status
 }while($true)
}
function Get-PinnedHosts([string[]]$Changed){
 $state=Native-Status
 $candidates=[Collections.Generic.List[object]]::new()
 $candidates.Add(@('latest.dll',$state.ExplorerPid,(Join-Path $env:windir 'explorer.exe')))
 foreach($entry in @(@('start/latest.dll','StartMenuExperienceHost.exe','SystemApps\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\StartMenuExperienceHost.exe'),@('panels/latest.dll','ShellHost.exe','System32\ShellHost.exe'))){
  if($entry[0] -notin $Changed){continue}
  foreach($p in Get-CimInstance Win32_Process -Filter ("Name='"+$entry[1]+"'")|Where-Object SessionId -EQ $session){$candidates.Add(@($entry[0],$p.ProcessId,(Join-Path $env:windir $entry[2])))}
 }
 @(foreach($entry in $candidates){
  if($entry[0] -notin $Changed -or -not $entry[1]){continue}
  $p=Get-Process -Id $entry[1] -ErrorAction SilentlyContinue
  if(-not $p){continue}
  if($p.Path -ine $entry[2] -or $p.SessionId -ne $session){throw 'Pinned host identity mismatch'}
  $loaded=@($p.Modules|Where-Object FileName -IEQ (Join-Path $Destination $entry[0]))
  if($loaded.Count){[pscustomobject]@{Id=$p.Id;Path=$p.Path;Start=$p.StartTime.ToUniversalTime().Ticks;Module=$entry[0]}}
 })
}
function Quiesce([string[]]$Changed){
 # Stopping the controller detaches Explorer styling too. Use a clean taskbar
 # owner for every runtime update; its diagnostics hot reconnect is not proven.
 $hosts=Get-PinnedHosts @(@($Changed)+@('latest.dll')|Select-Object -Unique)
 & (Join-Path $Destination 'latest.ps1') -Action Stop|Out-Null
 foreach($hostInfo in $hosts){
  $p=Get-Process -Id $hostInfo.Id -ErrorAction SilentlyContinue
  if(-not $p){continue}
  if($p.Path -ine $hostInfo.Path -or $p.SessionId -ne $session -or $p.StartTime.ToUniversalTime().Ticks -ne $hostInfo.Start){throw 'Host changed before unload'}
  $p.Kill();if(-not $p.WaitForExit(10000)){throw 'Pinned host did not exit'}
 }
 if(@($hosts|Where-Object Module -EQ 'latest.dll').Count){
  for($i=0;$i -lt 80;$i++){if([NativeDockManagementPathV2]::FindWindow('Shell_TrayWnd',$null) -ne [IntPtr]::Zero){break};Start-Sleep -Milliseconds 250}
  if([NativeDockManagementPathV2]::FindWindow('Shell_TrayWnd',$null) -eq [IntPtr]::Zero){throw 'Explorer did not recreate its taskbar'}
 }
}
function Restore-Transaction($Plan){
 $null=Read-OwnedTask
 Test-LeanDockRestore $Plan $BackupPath
 # Stop the exact current controller and unload only components being restored.
 $runtime=@($Plan.Files.Name|Where-Object {$_ -in @('latest.exe','latest.dll','start/latest.dll','panels/latest.dll')})
 if($runtime.Count -and $Plan.WasRunning){
  Assert-LeanDockRuntimeTrust (Get-LeanDockRuntimeTrust $Destination $Plan $BackupPath) (Get-LeanDockApplicationControlState)
 }
 # A failed first install can have only a partial payload and no management
 # script yet. Until its task exists, no installed runtime needs stopping.
 if($runtime.Count -and ($Plan.TaskExisted -or (Read-OwnedTask)) -and (Test-Path -LiteralPath (Join-Path $Destination 'latest.exe'))){Quiesce $runtime}
 Restore-LeanDockFiles $Plan $BackupPath
 if(-not $Plan.TaskExisted){
  if(Read-OwnedTask){Unregister-ScheduledTask -TaskName NativeTaskbarDock -Confirm:$false}
 }elseif($Plan.WasRunning -and $runtime.Count){
  $live=& (Join-Path $Destination 'latest.ps1') -Action Start|ConvertFrom-Json
  $required=if($Plan.PSObject.Properties['RequiredComponents']){@($Plan.RequiredComponents)}else{@('Live')}
  $null=Confirm-Readiness $live $required
 }
 $Plan.State='RolledBack';$Plan.CompletedAt=(Get-Date -Format o);Save-Plan $Plan
 New-Item -ItemType File -Path (Join-Path $BackupPath 'restored') -Force|Out-Null
}
if($Action -eq 'Watch'){
 $deadline=[datetime]::UtcNow.AddMinutes(5)
 while([datetime]::UtcNow -lt $deadline){
  if((Test-Path (Join-Path $BackupPath 'committed')) -or (Test-Path (Join-Path $BackupPath 'restored')) -or (Test-Path (Join-Path $BackupPath 'manual-recovery-required'))){exit 0}
  $parent=Get-Process -Id $ParentProcessId -ErrorAction SilentlyContinue
  if(-not $parent -or $parent.StartTime.ToUniversalTime().Ticks -ne $ParentStarted){$Action='Rollback';break}
  Start-Sleep -Seconds 1
 }
 if($Action -eq 'Watch'){'Parent still running; no concurrent rollback attempted.'|Set-Content (Join-Path $BackupPath 'watchdog-deferred.txt');exit 1}
}
if(-not $ValidateOnly -and -not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Maintenance requires an elevated PowerShell session.'}
$mutex=[Threading.Mutex]::new($false,('Local\LeanDock.Maintenance.'+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value))
$locked=$false
try{
 try{$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true}
 if(-not $locked){throw 'Another LeanDock maintenance operation is running'}
 $task=Read-OwnedTask
 if($Action -eq 'Rollback'){
  if(-not $BackupPath){$receipt=Get-Content (Join-Path $Destination 'latest.json') -Raw|ConvertFrom-Json;$BackupPath=$receipt.Backup}
  $BackupPath=[IO.Path]::GetFullPath($BackupPath)
  if(-not $BackupPath.StartsWith((Join-Path $Destination 'backups')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Rollback directory is outside this deployment'}
  $null=Resolve-LeanDockChild $BackupPath 'transaction-latest.json'
  $plan=Get-Content -LiteralPath (Join-Path $BackupPath 'transaction-latest.json') -Raw|ConvertFrom-Json
  if($plan.Schema -ne 1 -or $plan.Destination -ine $Destination -or $plan.Session -ne $session){throw 'Rollback belongs to a different deployment/session'}
  if($ValidateOnly){Test-LeanDockRestore $plan $BackupPath;[pscustomobject]@{Action=$Action;Destination=$Destination;Backup=$BackupPath;Files=$plan.Files.Count}|ConvertTo-Json;return}
  Restore-Transaction $plan
  [pscustomobject]@{Action=$Action;State='RolledBack';Backup=$BackupPath}|ConvertTo-Json;return
 }
 if(-not $PackagePath){throw 'A complete local package directory is required'}
 $PackagePath=[IO.Path]::GetFullPath($PackagePath)
 if($PackagePath -eq $Destination -or $PackagePath.StartsWith($Destination+'\',[StringComparison]::OrdinalIgnoreCase) -or $Destination.StartsWith($PackagePath+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Package and deployment must be separate trees'}
 $manifest=Read-LeanDockPackage $PackagePath
 if($Action -eq 'Install'){
  if($task -or ((Test-Path $Destination) -and @(Get-ChildItem -LiteralPath $Destination -Force).Count)){throw 'Install requires an empty destination and no existing LeanDock task; use Update for an installed copy'}
 }else{if(-not $task -or -not(Test-Path (Join-Path $Destination 'latest.exe'))){throw 'Update requires the exact existing deployment'}}
 if((Get-Service Windhawk -ErrorAction SilentlyContinue).Status -eq 'Running'){throw 'Windhawk must be migrated separately before LeanDock maintenance'}
 $changes=@(foreach($f in $manifest.Files){
  $dst=Resolve-LeanDockChild $Destination $f.Name
  if($f.Name -in $script:LeanDockConfigFiles -and (Test-Path $dst)){continue}
  $existed=Test-Path -LiteralPath $dst -PathType Leaf
  $oldHash=if($existed){(Get-FileHash -LiteralPath $dst).Hash}else{$null}
  if($oldHash -ine $f.SHA256){[pscustomobject]@{Name=$f.Name;Existed=$existed;PreviousHash=$oldHash;NewHash=$f.SHA256}}
 })
 $runtimeChanged=@($changes.Name|Where-Object {$_ -in $script:LeanDockRuntimeFiles})
 if($runtimeChanged.Count){
  # All four files are reloaded by this controller lifecycle, even if only one
  # hash changed. Check the incoming set and existing recovery set before Stop
  # or executing the compatibility helper. Never probe a known unsigned denial.
  $policy=Get-LeanDockApplicationControlState
  Assert-LeanDockRuntimeTrust (Get-LeanDockRuntimeTrust $PackagePath) $policy
  if($Action -eq 'Update'){Assert-LeanDockRuntimeTrust (Get-LeanDockRuntimeTrust $Destination) $policy}
  $probe=Start-Process -FilePath (Join-Path $PackagePath 'latest.exe') -ArgumentList '--verify' -WindowStyle Hidden -PassThru
  try{
   if(-not $probe.WaitForExit(20000)){$probe.Kill();[void]$probe.WaitForExit(3000);throw 'Compatibility check timed out; exact probe terminated'}
   if($probe.ExitCode -ne 0){throw 'Package does not support this Windows component set'}
  }finally{$probe.Dispose()}
 }
 if($ValidateOnly){[pscustomobject]@{Action=$Action;Version=$manifest.Version;Destination=$Destination;ChangedFiles=$changes.Count;RuntimeChanged=$runtimeChanged;PreserveConfiguration=$true}|ConvertTo-Json;return}
 if(-not $changes.Count){[pscustomobject]@{State='AlreadyCurrent';Version=$manifest.Version}|ConvertTo-Json;return}
 $before=if($Action -eq 'Update'){Native-Status}else{$null}
 $oldReceipt=Join-Path $Destination 'latest.json'
 $receiptExisted=Test-Path -LiteralPath $oldReceipt
 $changes+= [pscustomobject]@{Name='latest.json';Existed=$receiptExisted;PreviousHash=if($receiptExisted){(Get-FileHash $oldReceipt).Hash}else{$null};NewHash=$null}
 $BackupPath=Join-Path $Destination ('backups\before-'+$manifest.Version+'-'+(Get-Date -Format 'yyyyMMdd-HHmmssfff'))
 New-Item -ItemType Directory -Path $BackupPath -Force|Out-Null
 foreach($f in $changes|Where-Object Existed){Copy-LeanDockFile $Destination (Join-Path $BackupPath 'original') $f.Name $f.PreviousHash}
 $protected=@(foreach($name in $script:LeanDockConfigFiles){$p=Join-Path $Destination $name;if(Test-Path $p){[pscustomobject]@{Name=$name;SHA256=(Get-FileHash $p).Hash}}})
 $required=if($Action -eq 'Install'){@('Live','StartLayoutSupported','QuickPositionSupported')}else{@(Get-LeanDockReadinessRequirements $before)}
 $plan=[pscustomobject]@{Schema=1;Action=$Action;State='Prepared';Destination=$Destination;PackagePath=$PackagePath;Version=$manifest.Version;Session=$session;TaskExisted=[bool]$task;WasRunning=[bool]$before.Running;RequiredComponents=$required;At=(Get-Date -Format o);CompletedAt=$null;Files=$changes;Protected=$protected;Error=$null}
 Save-Plan $plan
 $engine=Join-Path $BackupPath 'engine';New-Item -ItemType Directory -Path $engine|Out-Null
 foreach($name in @('latest.ps1','files-latest.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $engine $name)}
 $arguments=@('-NoProfile','-File',('"'+(Join-Path $engine 'latest.ps1')+'"'),'-Action','Watch','-Destination',('"'+$Destination+'"'),'-BackupPath',('"'+$BackupPath+'"'),'-ParentProcessId',$PID,'-ParentStarted',(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks)
 $guard=Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList $arguments -WindowStyle Hidden -PassThru
 $mutated=$false
 try{
  # Quiesce itself changes live state; recover even if it fails partway through.
  if($Action -eq 'Update' -and $runtimeChanged.Count){$mutated=$true;Quiesce $runtimeChanged}
  $plan.State='Applying';Save-Plan $plan
  foreach($f in $changes|Where-Object Name -NE 'latest.json'){$mutated=$true;Copy-LeanDockFile $PackagePath $Destination $f.Name $f.NewHash}
  foreach($f in $protected){if((Get-FileHash (Join-Path $Destination $f.Name)).Hash -ne $f.SHA256){throw 'User configuration changed during maintenance'}}
  if($Action -eq 'Install'){& (Join-Path $Destination 'latest.ps1') -Action Install|Out-Null}
  if($NativeBaselineSeconds -ge 5 -and $runtimeChanged -contains 'latest.dll'){
   Start-Sleep -Seconds 8
   try{& (Join-Path $Destination 'maintenance/resources-latest.ps1') -DeploymentPath $Destination -Scenario Native -DurationSeconds $NativeBaselineSeconds -OutputPath (Join-Path $BackupPath 'resources-native-latest.json')|Out-Null}
   catch{[pscustomobject]@{Measured=$false;Error=$_.Exception.Message}|ConvertTo-Json|Set-Content (Join-Path $BackupPath 'resources-native-unavailable.json')}
  }
  $state=if($Action -eq 'Install' -or ($plan.WasRunning -and $runtimeChanged.Count)){& (Join-Path $Destination 'latest.ps1') -Action Start|ConvertFrom-Json}else{Native-Status}
  $state=Confirm-Readiness $state $required
  $health=Get-LeanDockComponentHealth $state
  $receipt=if($receiptExisted){Get-Content -LiteralPath (Join-Path $BackupPath 'original/latest.json') -Raw|ConvertFrom-Json}else{[pscustomobject]@{}}
  $nativeBaseline=Join-Path $BackupPath 'resources-native-latest.json'
  foreach($kv in @{Version=$manifest.Version;UpdatedAt=(Get-Date -Format o);RuntimeFiles=$manifest.RuntimeFiles;Backup=$BackupPath;RollbackScript=(Join-Path $Destination 'maintenance/latest.ps1');Maintenance=[ordered]@{State='Applied';UnifiedEntry=$true;ConfigPreserved=$true;PackageFiles=$manifest.Files.Count;ChangedFiles=$changes.Count;RequiredComponents=$required;ComponentHealth=$health;NativeBaseline=if(Test-Path -LiteralPath $nativeBaseline){$nativeBaseline}else{$null}};SourcePublication=[ordered]@{State='local-package';Scope=$manifest.SourcePolicy}}.GetEnumerator()){$receipt|Add-Member -Force NoteProperty $kv.Key $kv.Value}
  $receipt|ConvertTo-Json -Depth 24|Set-Content -LiteralPath $oldReceipt
  ($plan.Files|Where-Object Name -EQ 'latest.json').NewHash=(Get-FileHash $oldReceipt).Hash
  $plan.State='Applied';$plan.CompletedAt=(Get-Date -Format o);Save-Plan $plan
  New-Item -ItemType File -Path (Join-Path $BackupPath 'committed')|Out-Null
  [pscustomobject]@{State='Applied';Version=$manifest.Version;Live=$state.Live;ComponentHealth=$health;Backup=$BackupPath;ChangedFiles=$changes.Count;ConfigPreserved=$true}|ConvertTo-Json -Depth 6
 }catch{
  $failure=$_;$plan.Error=$failure.Exception.Message;Save-Plan $plan
  try{if($mutated){Restore-Transaction $plan}else{New-Item -ItemType File -Path (Join-Path $BackupPath 'restored') -Force|Out-Null}}catch{
   $_.Exception.Message|Set-Content (Join-Path $BackupPath 'manual-recovery-required');throw "Update failed and recovery needs attention: $($_.Exception.Message)"
  }
  throw $failure
 }finally{[void]$guard.WaitForExit(3000);$guard.Dispose()}
}finally{if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()}
