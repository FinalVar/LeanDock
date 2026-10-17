$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'maintenance/files-latest.ps1')
$work=Join-Path $PSScriptRoot ('maintenance-fixture/'+[guid]::NewGuid().ToString('N'))
$source=Join-Path $work 'source';$target=Join-Path $work 'target';$backup=Join-Path $work 'backup'
New-Item -ItemType Directory -Path $source,$target,$backup -Force|Out-Null
$script:checks=0
function Check([bool]$value,[string]$label){$script:checks++;if(-not $value){throw "Failed: $label"}}
function Reject([scriptblock]$action,[string]$label){$rejected=$false;try{& $action|Out-Null}catch{$rejected=$true};Check $rejected $label}
foreach($path in @('../escape','x/../../escape','C:\escape','\escape','x:stream','x/./a','x/../a','x./a','x /a','x/*','x//a','NUL','a/COM1.txt')){Reject {Resolve-LeanDockChild $target $path} ('path '+$path)}
$file=Resolve-LeanDockChild $target 'nested/latest.txt';Check ($file.StartsWith([IO.Path]::GetFullPath($target))) 'normal child'
$names=@('latest.exe','latest.dll','start/latest.dll','panels/latest.dll','latest.ps1','latest.ini','hotcorners/latest.ini','maintenance/latest.ps1','maintenance/files-latest.ps1','maintenance/resources-latest.ps1')
$files=@(foreach($name in $names){$p=Resolve-LeanDockChild $source $name;New-Item -ItemType Directory (Split-Path $p -Parent) -Force|Out-Null;('fixture-'+$name)|Set-Content $p;[pscustomobject]@{Name=$name;SHA256=(Get-FileHash $p).Hash}})
$manifest=[pscustomobject]@{Schema=1;Product='LeanDock';Version='test';Files=$files}
$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $source 'package-latest.json')
Check ((Read-LeanDockPackage $source).Files.Count -eq 10) 'complete package'
'changed'|Set-Content (Join-Path $source 'latest.exe');Reject {Read-LeanDockPackage $source} 'tampered payload'
'fixture-latest.exe'|Set-Content (Join-Path $source 'latest.exe')
$manifest.Files+= $files[0];$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $source 'package-latest.json');Reject {Read-LeanDockPackage $source} 'duplicate entry'
$manifest.Files=$files;$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $source 'package-latest.json')
'old'|Set-Content (Join-Path $target 'latest.dll');$oldHash=(Get-FileHash (Join-Path $target 'latest.dll')).Hash
Copy-LeanDockFile $target (Join-Path $backup 'original') 'latest.dll' $oldHash
$dll=$files|Where-Object Name -EQ 'latest.dll';$exe=$files|Where-Object Name -EQ 'latest.exe'
Copy-LeanDockFile $source $target 'latest.dll' $dll.SHA256
Copy-LeanDockFile $source $target 'latest.exe' $exe.SHA256
$plan=[pscustomobject]@{Destination=$target;Files=@([pscustomobject]@{Name='latest.dll';Existed=$true;PreviousHash=$oldHash;NewHash=$dll.SHA256},[pscustomobject]@{Name='latest.exe';Existed=$false;PreviousHash=$null;NewHash=$exe.SHA256})}
'user-config'|Set-Content (Join-Path $target 'latest.ini');$configHash=(Get-FileHash (Join-Path $target 'latest.ini')).Hash
'user changed new file'|Set-Content (Join-Path $target 'latest.exe')
Reject {Restore-LeanDockFiles $plan $backup} 'do not remove changed new file'
Check ((Get-FileHash (Join-Path $target 'latest.dll')).Hash -eq $dll.SHA256) 'preflight makes no partial restore'
Copy-LeanDockFile $source $target 'latest.exe' $exe.SHA256
'user source edit'|Set-Content (Join-Path $target 'latest.dll')
Reject {Restore-LeanDockFiles $plan $backup} 'do not overwrite edited existing file'
Copy-LeanDockFile $source $target 'latest.dll' $dll.SHA256
Restore-LeanDockFiles $plan $backup
Check ((Get-FileHash (Join-Path $target 'latest.dll')).Hash -eq $oldHash) 'restore original bytes'
Check (-not(Test-Path (Join-Path $target 'latest.exe'))) 'remove owned introduced file'
Check ((Get-FileHash (Join-Path $target 'latest.ini')).Hash -eq $configHash) 'preserve user configuration'
$outside=Join-Path $work 'outside';New-Item -ItemType Directory $outside|Out-Null
$junction=Join-Path $target 'junction';New-Item -ItemType Junction -Path $junction -Target $outside|Out-Null
Reject {Resolve-LeanDockChild $target 'junction/escape.txt'} 'junction traversal'
# Exercise the real rollback function with a partially copied first-install
# payload. Task/host calls are mocked; only fixture files can change.
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'maintenance/latest.ps1'),[ref]$tokens,[ref]$parseErrors)
Check ($parseErrors.Count -eq 0) 'maintenance parses'
$restore=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Restore-Transaction'},$false)
. ([scriptblock]::Create($restore.Extent.Text))
function Read-OwnedTask {return $null}
function Quiesce {throw 'A partial first install must not touch shell hosts'}
function Save-Plan {}
$Destination=Join-Path $work 'partial-install';$BackupPath=Join-Path $work 'partial-backup'
New-Item -ItemType Directory $Destination,$BackupPath|Out-Null
Copy-LeanDockFile $source $Destination 'latest.exe' $exe.SHA256
$partial=[pscustomobject]@{Destination=$Destination;TaskExisted=$false;WasRunning=$false;State='Applying';CompletedAt=$null;Files=@([pscustomobject]@{Name='latest.exe';Existed=$false;PreviousHash=$null;NewHash=$exe.SHA256})}
Restore-Transaction $partial
Check ($partial.State -eq 'RolledBack' -and -not(Test-Path (Join-Path $Destination 'latest.exe'))) 'partial first-install recovery without shell changes'
# A one-file Dock update restarts the shared controller. Admission checks must
# therefore include unchanged companions, before any runtime helper or Stop.
$unsigned=@(foreach($name in $script:LeanDockRuntimeFiles){[pscustomobject]@{Name=$name;Signature='NotSigned'}})
Reject {Assert-LeanDockRuntimeTrust $unsigned 'On'} 'active SAC blocks unsigned restart'
Assert-LeanDockRuntimeTrust $unsigned 'Off';Check $true 'off policy permits ordinary local build'
Assert-LeanDockRuntimeTrust $unsigned 'Evaluation';Check $true 'evaluation is distinguished from enforcement'
Reject {Assert-LeanDockRuntimeTrust $unsigned 'Unknown'} 'unknown policy does not silently allow restart'
Reject {Assert-LeanDockRuntimeTrust @() 'On'} 'empty trust inventory cannot pass'
$signed=@(foreach($name in $script:LeanDockRuntimeFiles){[pscustomobject]@{Name=$name;Signature='Valid'}})
Assert-LeanDockRuntimeTrust $signed 'On';Check $true 'signed inventory can proceed to real load/readiness checks'
$signed[2].Signature='NotSigned'
Reject {Assert-LeanDockRuntimeTrust $signed 'On'} 'unchanged unsigned Start companion prevents reload'
$signed[2].Signature='HashMismatch'
Reject {Assert-LeanDockRuntimeTrust $signed 'On'} 'invalid signature is not trusted'
# Same basename in different folders must never substitute a different runtime.
$trust=@(Get-LeanDockRuntimeTrust $source)
Check ($trust.Count -eq 4 -and ($trust|Where-Object Name -EQ 'start/latest.dll').SHA256 -eq ($files|Where-Object Name -EQ 'start/latest.dll').SHA256) 'exact four-file trust inventory'
$restoreTrust=@(Get-LeanDockRuntimeTrust $source $plan $backup)
Check (($restoreTrust|Where-Object Name -EQ 'latest.dll').SHA256 -eq $oldHash) 'rollback checks old bytes, not new payload'
$before=[pscustomobject]@{Running=$true;Live=$true;DockReady=$true;HotCornersReady=$true;StartLayoutSupported=$true;StartLayoutActive=$true;StartLayoutReady=$true;StartLayoutBlocked=$false;QuickSettingsPosition=[pscustomobject]@{Supported=$true;Active=$true;Blocked=$false};QuickTheme=[pscustomobject]@{active=$true}}
$required=@(Get-LeanDockReadinessRequirements $before)
Check ($required.Count -eq 7) 'snapshot all previously ready components'
$after=$before|ConvertTo-Json -Depth 5|ConvertFrom-Json
Check (@(Get-LeanDockReadinessFailures $after $required).Count -eq 0) 'same feature set passes'
$after.StartLayoutSupported=$false;$after.StartLayoutActive=$false;$after.StartLayoutReady=$false
Check (@(Get-LeanDockReadinessFailures $after $required).Count -eq 3) 'Live true cannot conceal lost Start layout'
Check ((Get-LeanDockComponentHealth $after).State -eq 'Degraded' -and 'StartLayout' -in (Get-LeanDockComponentHealth $after).Unavailable) 'report current Start regression explicitly'
$after=$before|ConvertTo-Json -Depth 5|ConvertFrom-Json
$after.QuickSettingsPosition.Active=$false;$after.QuickTheme=$null
Check (@(Get-LeanDockReadinessFailures $after $required).Count -eq 2) 'detect lost quick settings and theme'
Check ((Get-LeanDockComponentHealth $after).State -eq 'WaitingForHost') 'supported dormant host is pending, not failed'
$after=$before|ConvertTo-Json -Depth 5|ConvertFrom-Json
$after.StartLayoutBlocked=$true
Check (@(Get-LeanDockReadinessFailures $after $required).Count -eq 3) 'blocked host overrides stale active flags'
Check (@(Get-LeanDockReadinessRequirements ([pscustomobject]@{Running=$false})).Count -eq 0) 'stopped installation has no live baseline'
Reject {Get-LeanDockReadinessFailures $before @('MisspelledComponent')} 'unknown requirements cannot silently pass'
# Replay the actual bounded readiness function with observations, no real waits
# or desktop calls. First sample has a delayed companion, the next is healthy.
$confirm=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Confirm-Readiness'},$false)
. ([scriptblock]::Create($confirm.Extent.Text))
function Start-Sleep {}
$script:statusReads=0
function Native-Status {$script:statusReads++;$before}
$pending=$before|ConvertTo-Json -Depth 5|ConvertFrom-Json
$pending.StartLayoutReady=$false
$confirmed=Confirm-Readiness $pending $required
Check ($confirmed.StartLayoutReady -and $script:statusReads -eq 1) 'wait for companion after core readiness'
# Retain bounded fixtures for diagnosis. No recursive deletion in tests.
$result=[pscustomobject]@{Checks=$checks;Failures=0;WindowsMutations=$false;Fixture=$work}
$result|ConvertTo-Json|Tee-Object -FilePath (Join-Path $PSScriptRoot 'maintenance-latest.json')
