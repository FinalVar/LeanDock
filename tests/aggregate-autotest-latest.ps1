param(
 [string]$Destination='C:\Users\x\Documents\Codex\Tools\NativeTaskbarDock',
 [string]$OutputDirectory,
 [ValidateRange(1,5)][int]$ProbeCycles=3,
 [ValidateRange(0,300)][int]$ObserveSeconds=30
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $OutputDirectory){$OutputDirectory=Join-Path (Split-Path $root -Parent) 'outputs/native-aggregate-autotest'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
$destinationPath=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
if($output -eq $destinationPath -or $output.StartsWith($destinationPath+'\',[StringComparison]::OrdinalIgnoreCase)){
 throw 'Test outputs must stay outside formal deployment'
}
New-Item -ItemType Directory -Path $output -Force|Out-Null
$run=Join-Path $output ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $run|Out-Null
$mutex=[Threading.Mutex]::new($false,'Local\LeanDock.Aggregate.Autotest')
$locked=$false;$stages=[Collections.Generic.List[object]]::new();$failure=$null
$pwsh='C:\Program Files\PowerShell\7\pwsh.exe'
function Invoke-Stage([string]$Name,[string]$Script,[string[]]$Arguments=@(),[int]$TimeoutSeconds=180){
 $info=[Diagnostics.ProcessStartInfo]::new($pwsh)
 $info.UseShellExecute=$false;$info.CreateNoWindow=$true
 $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 foreach($arg in @('-NoProfile','-File',$Script)+$Arguments){[void]$info.ArgumentList.Add($arg)}
 $process=[Diagnostics.Process]::new();$process.StartInfo=$info
 $watch=[Diagnostics.Stopwatch]::StartNew();$timedOut=$false
 try{
  [void]$process.Start()
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  if(-not $process.WaitForExit($TimeoutSeconds*1000)){
   $timedOut=$true
   # This exact Process object belongs to this test; never kill by image name.
   $process.Kill($true);[void]$process.WaitForExit(10000)
  }
  $log=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
  $log|Set-Content -LiteralPath (Join-Path $run ($Name+'.txt')) -Encoding utf8NoBOM
  $entry=[pscustomobject]@{Name=$Name;ExitCode=$process.ExitCode;TimedOut=$timedOut;Milliseconds=$watch.ElapsedMilliseconds}
  $stages.Add($entry)
  if($timedOut -or $entry.ExitCode -ne 0){throw "Stage $Name failed; inspect its saved log"}
 }finally{$process.Dispose()}
}
function Snapshot {
 $files=@(foreach($name in @('latest.exe','latest.dll','aggregate/latest.exe','start/latest.dll','panels/latest.dll','latest.ini','cards/latest.ini','hotcorners/latest.ini')){
  [pscustomobject]@{Name=$name;SHA256=(Get-FileHash -LiteralPath (Join-Path $destinationPath $name)).Hash}
 })
 $status=& (Join-Path $destinationPath 'latest.ps1') -Action Status|ConvertFrom-Json
 [pscustomobject]@{Files=$files;Live=$status.Live;Backend=$status.Backend.version;ExplorerPid=$status.ExplorerPid;Controller=$status.Controller;Health=$status.ComponentHealth}
}
try{
 try{$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true}
 if(-not $locked){throw 'Another aggregate test run is already active'}
 $before=Snapshot
 $before|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $run 'before.json')
 Invoke-Stage 'native-geometry' (Join-Path $PSScriptRoot 'cards-aggregate-latest.ps1')
 Invoke-Stage 'native-host-lifetime' (Join-Path $PSScriptRoot 'cards-aggregate-host-latest.ps1')
 Invoke-Stage 'panel-lifecycle' (Join-Path $PSScriptRoot 'aggregate-lifecycle-latest.ps1') @('-OutputDirectory',(Join-Path $run 'lifecycle'))
 for($cycle=1;$cycle -le $ProbeCycles;$cycle++){
  $dir=Join-Path $run ('probe-'+$cycle);New-Item -ItemType Directory -Path $dir|Out-Null
  $exe=Join-Path $dir 'latest.exe'
  Copy-Item -LiteralPath (Join-Path $destinationPath 'aggregate/latest.exe') -Destination $exe
  $expected=@($before.Files|Where-Object Name -EQ 'aggregate/latest.exe')[0].SHA256
  if((Get-FileHash -LiteralPath $exe).Hash -ne $expected){throw 'Isolated probe does not match installed panel'}
  Invoke-Stage ('probe-'+$cycle) (Join-Path $PSScriptRoot 'aggregate-probe-latest.ps1') @('-Executable',$exe) 90
  $control=Get-Content -LiteralPath (Join-Path $dir 'controls-probe-latest.json') -Raw|ConvertFrom-Json
  $metrics=Get-Content -LiteralPath (Join-Path $dir 'metrics-probe-latest.json') -Raw|ConvertFrom-Json
  if(-not $control.normalUser -or -not $control.readOnly -or $control.writes -ne 0 -or
    $control.visibleWindows -or -not $control.hiddenReadsUnchanged -or -not $metrics.hiddenSamplingUnchanged){
   throw "Probe cycle $cycle did not preserve hidden read-only boundaries"
  }
  foreach($dpi in @(100,125,150,200)){
   if(-not(Test-Path -LiteralPath (Join-Path $dir "control-panel-probe/dpi-$dpi.png"))){throw "Missing $dpi DPI render"}
  }
 }
 if($ObserveSeconds){
  Invoke-Stage 'passive-observation' (Join-Path $PSScriptRoot 'aggregate-observe-latest.ps1') @('-Seconds',"$ObserveSeconds",'-OutputPath',(Join-Path $run 'observation.json')) ($ObserveSeconds+30)
 }
}catch{$failure=$_.Exception.Message}
finally{
 try{
  $after=Snapshot
  $after|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $run 'after.json')
  $unchanged=$null -ne $before -and (($before.Files|ConvertTo-Json -Compress) -ceq ($after.Files|ConvertTo-Json -Compress))
  if($null -ne $before -and -not $unchanged){$failure='Formal runtime/config hash changed during the test window; investigate concurrent activity'}
  if(-not $after.Live -or $after.Health.State -ne 'Ready'){$failure='Formal components are not ready after testing'}
  $ownedLeftovers=@(Get-CimInstance Win32_Process -Filter "Name='latest.exe'"|Where-Object {
   $_.ExecutablePath -and $_.ExecutablePath.StartsWith($run+'\',[StringComparison]::OrdinalIgnoreCase)
  })
  if($ownedLeftovers.Count){$failure='Isolated probe process still running; inspect exact path/PID before cleanup'}
 }catch{if(-not $failure){$failure=$_.Exception.Message}}
 $observer=if(Test-Path -LiteralPath (Join-Path $run 'observation.json')){Get-Content -LiteralPath (Join-Path $run 'observation.json') -Raw|ConvertFrom-Json}else{$null}
 $result=[ordered]@{
  State=$(if($failure){'NeedsAttention'}else{'AutomatedChecksPassed'});At=(Get-Date -Format o)
  RunDirectory=$run;Failure=$failure;Stages=$stages.ToArray();ProbeCycles=$ProbeCycles
  InstalledFilesUnchanged=$unchanged;ExplorerUnchanged=($null -ne $before -and $before.ExplorerPid -eq $after.ExplorerPid)
  ProbeLeftovers=@($ownedLeftovers|Select-Object ProcessId,ExecutablePath);Observation=$observer
  Boundaries=@('No desktop input, screenshots or focus changes','No formal Update/Stop/Start or device writes','Offscreen renders are fixtures, not desktop screenshots','Missing natural hover observations are NotObserved, never a pass','Busy cursor observation is correlation only; other applications may contribute','No performance or whole-machine memory claim from these checks')
 }
 $json=$result|ConvertTo-Json -Depth 9
 $json|Set-Content -LiteralPath (Join-Path $run 'latest.json') -Encoding utf8NoBOM
 if($locked){
  $latest=Join-Path $output 'latest.json'
  if(Test-Path -LiteralPath $latest){Copy-Item -LiteralPath $latest -Destination (Join-Path $run 'previous-result.json')}
  $json|Set-Content -LiteralPath $latest -Encoding utf8NoBOM
  $mutex.ReleaseMutex()
 }
 $mutex.Dispose();$json
}
if($failure){throw $failure}
