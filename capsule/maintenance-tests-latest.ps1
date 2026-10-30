$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'maintenance\files-latest.ps1')
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repo 'maintenance\capsule-latest.ps1'),[ref]$tokens,[ref]$errors)
if($errors){throw ($errors|Out-String)}
foreach($name in @('Read-Package','Copy-CapsuleFile','Start-Capsule','Get-CapsulePinnedHosts','Stop-CapsulePinnedHosts','Grant-CapsuleSearchRead','Restore-CapsuleStartAccess')){
 $fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$false)
 if(-not $fn){throw "Missing transaction function: $name"}
 . ([scriptblock]::Create($fn.Extent.Text))
}
$work=Join-Path $PSScriptRoot ('build\maintenance-fixture\'+[guid]::NewGuid().ToString('N'))
$PackagePath=Join-Path $work 'source';$target=Join-Path $work 'target'
New-Item -ItemType Directory -Path $PackagePath,$target -Force|Out-Null
$checks=0
function Check([bool]$ok,[string]$what){$script:checks++;if(-not $ok){throw $what}}
function Reject([scriptblock]$action,[string]$what){$bad=$false;try{& $action|Out-Null}catch{$bad=$true};Check $bad $what}
$names=@('latest.exe','latest.dll','capsule/latest.exe','start/latest.dll','panels/latest.dll','native-control-latest.ps1','latest.ps1','maintenance/capsule-latest.ps1','maintenance/files-latest.ps1','README.md','NOTICE.md','LICENSE.txt')
$files=@(foreach($name in $names){
 $p=Resolve-LeanDockChild $PackagePath $name;New-Item -ItemType Directory -Path (Split-Path $p -Parent) -Force|Out-Null
 [IO.File]::WriteAllText($p,'fixture: '+$name)
 [pscustomobject]@{Name=$name;SHA256=(Get-FileHash -LiteralPath $p).Hash}
})
$manifest=[pscustomobject]@{Schema=1;Product='LeanDock';Architecture='capsule';LegacyComponentPolicy='Preserve';Files=$files}
function Save-Manifest {$manifest|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $PackagePath 'package-latest.json')}
Save-Manifest
Check ((Read-Package).Files.Count -eq $names.Count) 'Complete split-runtime package'
$manifest.LegacyComponentPolicy='Retire';Save-Manifest
Reject {Read-Package} 'A package retiring old features must fail before loading'
$manifest.LegacyComponentPolicy='Preserve';$manifest.Files=$files[0..10];Save-Manifest
Reject {Read-Package} 'Missing payload must fail'
$manifest.Files=$files+@($files[0]);Save-Manifest
Reject {Read-Package} 'Duplicate payload must fail'
$manifest.Files=$files+@([pscustomobject]@{Name='latest.ini';SHA256=('A'*64)});Save-Manifest
Reject {Read-Package} 'User configuration must not be overwritten by package'
$manifest.Files=$files+@([pscustomobject]@{Name='../escape';SHA256=('A'*64)});Save-Manifest
Reject {Read-Package} 'Path escape must fail'
$manifest.Files=$files;Save-Manifest
$hash=$files[0].SHA256
Copy-CapsuleFile $PackagePath $target 'latest.exe' $hash
Check ((Get-FileHash -LiteralPath (Join-Path $target 'latest.exe')).Hash -eq $hash) 'Copy verifies destination'
$locked=[IO.File]::Open((Join-Path $target 'latest.exe'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try{
 Copy-CapsuleFile $PackagePath $target 'latest.exe' $hash
 Check ($locked.Length -gt 0) 'Identical mapped runtime is not opened for writing'
}finally{$locked.Dispose()}
Reject {Copy-CapsuleFile $PackagePath $target 'latest.exe' ('F'*64)} 'Wrong source hash is rejected before no-op path'
[IO.File]::WriteAllText((Join-Path $PackagePath 'latest.exe'),'tampered')
Reject {Read-Package} 'Changed binary fails verification'
$Destination=$target
$script:fixtureCalls=[Collections.Generic.List[string]]::new()
$script:healthy=[pscustomobject]@{
 Live=$true;Running=$true;ExplorerPid=22;Backend=[pscustomobject]@{explorerPid=22}
 StartLayoutSupported=$true;StartLayoutActive=$true;StartLayoutReady=$true;StartLayoutBlocked=$false
 SearchPosition=[pscustomobject]@{Supported=$true;Active=$true;Blocked=$false;Failures=0}
 QuickSettingsPosition=[pscustomobject]@{Supported=$true;Active=$true;Blocked=$false}
 QuickTheme=[pscustomobject]@{active=$true};ClientEdge=[pscustomobject]@{Supported=$true;Ready=$true}
 Capsule=[pscustomobject]@{guardPid=11;uiPid=12}
}
[pscustomobject]@{Architecture='capsule';Status=$healthy}|ConvertTo-Json -Depth 5|
 Set-Content -LiteralPath (Join-Path $Destination 'latest.json')
function Read-OwnedTask { [pscustomobject]@{Principal=[pscustomobject]@{RunLevel='Highest'}} }
function Read-CapsuleStatus {$script:fixtureStatus}
function Stop-Capsule {$script:fixtureCalls.Add('Stop')}
function Restart-OwnedExplorer([int]$ExplorerId) {$script:fixtureCalls.Add("Reload:$ExplorerId")}
function Start-ScheduledTask {param($TaskName) $script:fixtureCalls.Add("Start:$TaskName");$script:fixtureStatus=$script:healthy}
function Start-Sleep {param($Milliseconds)}
$script:fixtureStatus=$healthy
$null=Start-Capsule
Check ($fixtureCalls.Count -eq 0) 'A fully healthy Start is idempotent'
$script:fixtureStatus=($healthy|ConvertTo-Json -Depth 5|ConvertFrom-Json)
$script:fixtureStatus.Live=$false
$null=Start-Capsule
Check (($fixtureCalls -join ',') -eq 'Stop,Reload:22,Start:NativeTaskbarDock') 'Failed pinned host stops before exact shell reload'
$fixtureCalls.Clear()
$script:fixtureStatus=($healthy|ConvertTo-Json -Depth 5|ConvertFrom-Json)
$script:fixtureStatus.Live=$false;$script:fixtureStatus.Running=$false
$null=Start-Capsule
Check (($fixtureCalls -join ',') -eq 'Reload:22,Start:NativeTaskbarDock') 'Stopped pinned host reloads before attach'
$fixtureCalls.Clear()
$script:fixtureStatus=($healthy|ConvertTo-Json -Depth 5|ConvertFrom-Json)
$script:fixtureStatus.StartLayoutReady=$false
$null=Start-Capsule
Check ($fixtureCalls.Contains('Reload:22')) 'UI readiness cannot hide a missing retained Start component'
$fixtureCalls.Clear()
$script:fixtureStatus=($healthy|ConvertTo-Json -Depth 5|ConvertFrom-Json)
$script:fixtureStatus.SearchPosition.Active=$false
$null=Start-Capsule
Check ($fixtureCalls.Contains('Reload:22')) 'UI readiness cannot hide an inactive retained Search positioner'
$fixtureCalls.Clear()
$badSearch=($healthy|ConvertTo-Json -Depth 5|ConvertFrom-Json)
$badSearch.SearchPosition.Failures=1
Check (@(Get-LeanDockReadinessFailures $badSearch @('SearchPositionActive')).Count -eq 1) 'Active marker cannot hide Search attach failure'
Check ('SearchPosition' -in (Get-LeanDockComponentHealth $badSearch).Unavailable) 'Search attach failures remain visible in health'
$badSearch.SearchPosition.Failures=0;$badSearch.SearchPosition.Active=$false
Check ('SearchPosition' -in (Get-LeanDockComponentHealth $badSearch).Pending) 'Absent Search host is pending rather than accepted'
$script:fixtureStatus=($healthy|ConvertTo-Json -Depth 5|ConvertFrom-Json)
$script:fixtureStatus.Live=$false;$script:fixtureStatus.Running=$false;$script:fixtureStatus.Backend=$null
$null=Start-Capsule
Check (($fixtureCalls -join ',') -eq 'Start:NativeTaskbarDock') 'Cold start does not restart a clean Explorer'
& {
 # Only fixture objects: exercise path/session/start-time and mapped-module
 # ownership before releasing a changed Start/Search companion.
 $session=4
 $expected=Join-Path $env:windir 'SystemApps\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\SearchHost.exe'
 $fake=[pscustomobject]@{Id=421;Path=$expected;SessionId=$session;StartTime=[datetime]'2026-10-21';Killed=$false;Modules=@([pscustomobject]@{FileName=(Join-Path $Destination 'start/latest.dll')})}
 $fake|Add-Member ScriptMethod Kill {$this.Killed=$true}
 $fake|Add-Member ScriptMethod WaitForExit {param($ms) return $true}
 function Get-CimInstance {param($ClassName,$Filter) if($Filter -eq "Name='SearchHost.exe'"){[pscustomobject]@{ProcessId=421;SessionId=$session}}}
 function Get-Process {param($Id,$ErrorAction) if($Id -eq 421){$fake}}
 Check (@(Get-CapsulePinnedHosts @('latest.exe')).Count -eq 0) 'Unchanged companion never schedules a host restart'
 $owned=@(Get-CapsulePinnedHosts @('start/latest.dll'))
 Check ($owned.Count -eq 1 -and $owned[0].Id -eq 421) 'Only exact changed DLL owner is captured'
 $fake.Modules=@()
 Check (@(Get-CapsulePinnedHosts @('start/latest.dll')).Count -eq 0) 'A host without our module remains untouched'
 $fake.Modules=@([pscustomobject]@{FileName=(Join-Path $Destination 'start/latest.dll')})
 $fake.Path=Join-Path $target 'SearchHost.exe'
 Reject {Get-CapsulePinnedHosts @('start/latest.dll')} 'Same basename at another path is refused'
 $fake.Path=$expected;$fake.StartTime=$fake.StartTime.AddSeconds(1)
 Reject {Stop-CapsulePinnedHosts $owned} 'PID reuse is refused before termination'
 Check (-not $fake.Killed) 'Reused process is not killed'
 $fake.StartTime=$fake.StartTime.AddSeconds(-1);$fake.SessionId=5
 Reject {Stop-CapsulePinnedHosts $owned} 'Session drift is refused before termination'
 $fake.SessionId=4
 Stop-CapsulePinnedHosts $owned
 Check $fake.Killed 'Exact stopped component host can release its pinned image'
}
& {
 $fixture=Join-Path $Destination 'start/latest.dll';New-Item -ItemType Directory -Path (Split-Path $fixture) -Force|Out-Null
 [IO.File]::WriteAllText($fixture,'ACL fixture only; not an executable')
 $beforeAcl=(Get-Acl -LiteralPath $fixture).GetSecurityDescriptorSddlForm([Security.AccessControl.AccessControlSections]::Access)
 $parent=Split-Path $fixture
 $beforeParent=(Get-Acl -LiteralPath $parent).Sddl
 $plan=[pscustomobject]@{StartAccessBefore=$beforeAcl;StartAccessApplied=$null}
 function Save-Plan($p){Check ([bool]$p.StartAccessApplied) 'Intended ACL is recorded before mutation'}
 Grant-CapsuleSearchRead $plan
 $sid=[LeanDockSearchPackageSid]::Get()
 $added=@((Get-Acl -LiteralPath $fixture).GetAccessRules($true,$false,[Security.Principal.SecurityIdentifier])|Where-Object {$_.IdentityReference.Value -eq $sid})
 Check ($added.Count -eq 1 -and $added[0].AccessControlType -eq 'Allow' -and -not ($added[0].FileSystemRights -band [Security.AccessControl.FileSystemRights]::Write)) 'Single package receives no file write permission'
 Check ((Get-Acl -LiteralPath $parent).Sddl -eq $beforeParent) 'Containing directory permissions remain unchanged'
 $first=$plan.StartAccessApplied;Grant-CapsuleSearchRead $plan
 Check ($plan.StartAccessApplied -eq $first) 'Repeated scoped grant is idempotent'
 Restore-CapsuleStartAccess $plan
 Check ((Get-Acl -LiteralPath $fixture).GetSecurityDescriptorSddlForm([Security.AccessControl.AccessControlSections]::Access) -eq $beforeAcl) 'Rollback restores original file permissions'
}
[pscustomobject]@{Checks=$checks;Failures=0;WindowsMutations=$false;Fixture=$work}|
 ConvertTo-Json|Tee-Object -FilePath (Join-Path $PSScriptRoot 'build\maintenance-tests-latest.json')
