# Shared validation, exact-file copy/restore and maintenance readiness guards.
$ErrorActionPreference='Stop'
$script:LeanDockConfigFiles=@('latest.ini','hotcorners/latest.ini')
$script:LeanDockRuntimeFiles=@('latest.exe','latest.dll','start/latest.dll','panels/latest.dll')
function Get-LeanDockApplicationControlState {
 # Read only. Absence/read failure is unknown, not permission to restart code.
 try {
  $value=Get-ItemPropertyValue -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -Name VerifiedAndReputablePolicyState -ErrorAction Stop
  switch($value){0{'Off'} 1{'On'} 2{'Evaluation'} default{'Unknown'}}
 } catch {'Unknown'}
}
function Get-LeanDockRuntimeTrust([string]$Root,$RestorePlan=$null,[string]$BackupRoot=''){
 @(foreach($name in $script:LeanDockRuntimeFiles){
  $file=if($RestorePlan){@($RestorePlan.Files|Where-Object Name -EQ $name)}else{@()}
  $path=if($file.Count -and $file[0].Existed){Resolve-LeanDockChild $BackupRoot ('original/'+$name)}else{Resolve-LeanDockChild $Root $name}
  if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing runtime dependency: $name"}
  $signature=Get-AuthenticodeSignature -LiteralPath $path
  [pscustomobject]@{Name=$name;SHA256=(Get-FileHash -LiteralPath $path).Hash;Signature=[string]$signature.Status}
 })
}
function Assert-LeanDockRuntimeTrust($Artifacts,[string]$Policy){
 if($Policy -notin @('Off','Evaluation','On')){throw 'Smart App Control state is unknown; runtime restart was not attempted.'}
 foreach($name in $script:LeanDockRuntimeFiles){if(@($Artifacts|Where-Object Name -EQ $name).Count -ne 1){throw "Runtime trust inventory is incomplete or duplicated: $name"}}
 if($Policy -ne 'On'){return}
 # Conservative maintenance gate, not an imitation of Windows cloud reputation.
 # Previously loaded unsigned code is not evidence it will be admitted again.
 $unsafe=@($Artifacts|Where-Object Signature -NE 'Valid')
 if($unsafe.Count){throw ('Smart App Control is enabled and runtime signature readiness is unverified: '+($unsafe.Name -join ', ')+'. Restart refused before this stage changes live components. Use a trusted signed package; this tool never changes application-control policy.')}
}
function Get-LeanDockReadinessRequirements($Before){
 if(-not $Before.Running){return}
 'Live'
 if($Before.StartLayoutSupported){'StartLayoutSupported'}
 if($Before.StartLayoutActive -or $Before.StartLayoutReady){'StartLayoutActive'}
 if($Before.StartLayoutReady){'StartLayoutReady'}
 if($Before.QuickSettingsPosition.Supported){'QuickPositionSupported'}
 if($Before.QuickSettingsPosition.Active){'QuickPositionActive'}
 if($Before.QuickTheme.active){'QuickThemeReady'}
}
function Get-LeanDockReadinessFailures($State,[string[]]$Required){
 foreach($name in $Required){
  $ok=switch($name){
   'Live' {[bool]$State.Live}
   'StartLayoutSupported' {$State.StartLayoutSupported -and -not $State.StartLayoutBlocked}
   'StartLayoutActive' {($State.StartLayoutActive -or $State.StartLayoutReady) -and -not $State.StartLayoutBlocked}
   'StartLayoutReady' {$State.StartLayoutReady -and -not $State.StartLayoutBlocked}
   'QuickPositionSupported' {$State.QuickSettingsPosition.Supported -and -not $State.QuickSettingsPosition.Blocked}
   'QuickPositionActive' {$State.QuickSettingsPosition.Active -and -not $State.QuickSettingsPosition.Blocked}
   'QuickThemeReady' {[bool]$State.QuickTheme.active}
   default {throw "Unknown readiness requirement: $name"}
  }
  if(-not $ok){$name}
 }
}
function Get-LeanDockComponentHealth($State){
 if(-not $State.Running){return [pscustomobject]@{State='Stopped';Unavailable=@();Pending=@()}}
 $unavailable=@(
  if(-not $State.DockReady){'Dock'}
  if(-not $State.HotCornersReady){'HotCorners'}
  if(-not $State.StartLayoutSupported -or $State.StartLayoutBlocked){'StartLayout'}
  if(-not $State.QuickSettingsPosition.Supported -or $State.QuickSettingsPosition.Blocked){'QuickSettings'}
 )
 $pending=@(
  if('StartLayout' -notin $unavailable -and -not $State.StartLayoutReady){'StartLayout'}
  if('QuickSettings' -notin $unavailable -and (-not $State.QuickSettingsPosition.Active -or -not $State.QuickTheme.active)){'QuickSettings'}
 )
 [pscustomobject]@{State=if($unavailable.Count){'Degraded'}elseif($pending.Count){'WaitingForHost'}else{'Ready'};Unavailable=$unavailable;Pending=$pending}
}
function Resolve-LeanDockChild([string]$Root,[string]$Relative){
 if([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.?([\\/]|$)|[:*?"<>|]' -or $Relative -match '(^|[\\/])[^\\/]*[. ]([\\/]|$)'){throw 'Unsafe relative package path'}
 if($Relative -match '[\\/]{2}|(^|[\\/])(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|[\\/]|$)'){throw 'Reserved or ambiguous package path'}
 $base=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
 $full=[IO.Path]::GetFullPath((Join-Path $base $Relative))
 if(-not $full.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Path escapes package root'}
 $cursor=$full
 while($cursor){
  if(Test-Path -LiteralPath $cursor){$item=Get-Item -LiteralPath $cursor -Force;if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse points are not accepted in maintenance paths'}}
  $next=Split-Path $cursor -Parent;if($next -eq $cursor){break};$cursor=$next
 }
 $full
}
function Read-LeanDockPackage([string]$Root){
 $manifestPath=Resolve-LeanDockChild $Root 'package-latest.json'
 $manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
 if($manifest.Schema -ne 1 -or $manifest.Product -ne 'LeanDock' -or -not $manifest.Files -or $manifest.Files.Count -gt 5000){throw 'Invalid LeanDock package manifest'}
 $seen=@{}
 foreach($file in $manifest.Files){
  $relative=$file.Name.Replace('\','/');$file.Name=$relative;$path=Resolve-LeanDockChild $Root $relative
  if($seen.ContainsKey($relative) -or $file.SHA256 -notmatch '^[A-Fa-f0-9]{64}$' -or -not(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Duplicate or missing package file'}
  if($relative -eq 'latest.json' -or $relative -match '^(backups|verification|artifacts)/' -or $relative -match '(^|/)latest-status\.json$'){throw 'Runtime state cannot be packaged'}
  $seen[$relative]=$true
  if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.SHA256){throw "Package checksum mismatch: $relative"}
 }
 foreach($required in @('latest.exe','latest.dll','start/latest.dll','panels/latest.dll','latest.ps1','latest.ini','hotcorners/latest.ini','maintenance/latest.ps1','maintenance/files-latest.ps1','maintenance/resources-latest.ps1')){if(-not $seen.ContainsKey($required)){throw "Missing required payload: $required"}}
 $manifest
}
function Copy-LeanDockFile([string]$SourceRoot,[string]$TargetRoot,[string]$Relative,[string]$ExpectedHash){
 $src=Resolve-LeanDockChild $SourceRoot $Relative;$dst=Resolve-LeanDockChild $TargetRoot $Relative
 if((Get-FileHash -LiteralPath $src).Hash -ine $ExpectedHash){throw 'Source changed after validation'}
 New-Item -ItemType Directory -Path (Split-Path $dst -Parent) -Force|Out-Null
 Copy-Item -LiteralPath $src -Destination $dst -Force
 if((Get-FileHash -LiteralPath $dst).Hash -ine $ExpectedHash){throw 'Copied file checksum mismatch'}
}
function Test-LeanDockRestore($Plan,[string]$BackupRoot){
 # Validate all rollback material and ownership before writing any old bytes.
 foreach($f in $Plan.Files){
  $target=Resolve-LeanDockChild $Plan.Destination $f.Name
  if($f.Existed){
   $source=Resolve-LeanDockChild $BackupRoot ('original/'+$f.Name);if((Get-FileHash -LiteralPath $source).Hash -ine $f.PreviousHash){throw 'Rollback checksum mismatch'}
   if($f.Name -notin @('latest.json','docs/verification-latest.json') -and (Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target).Hash -notin @($f.PreviousHash,$f.NewHash)){throw 'Owned file edited since update; refusing to overwrite it'}
  }
  elseif(Test-Path -LiteralPath $target){if((Get-FileHash -LiteralPath $target).Hash -ine $f.NewHash){throw 'New file modified since update; refusing to remove it'}}
 }
}
function Restore-LeanDockFiles($Plan,[string]$BackupRoot){
 Test-LeanDockRestore $Plan $BackupRoot
 foreach($f in $Plan.Files){
  if($f.Existed){Copy-LeanDockFile (Join-Path $BackupRoot 'original') $Plan.Destination $f.Name $f.PreviousHash}
  else{$target=Resolve-LeanDockChild $Plan.Destination $f.Name;if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target -Force}}
 }
}
