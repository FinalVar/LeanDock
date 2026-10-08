param(
 [string]$DeploymentPath=(Split-Path $PSScriptRoot -Parent),
 [ValidateSet('Observe','Visible','Hidden','Media','Battery','Native')][string]$Scenario='Observe',
 [ValidateRange(5,300)][int]$DurationSeconds=30,
 [string]$OutputPath=(Join-Path $PSScriptRoot 'resources-latest.json'),
 [string]$BaselinePath=''
)
$ErrorActionPreference='Stop'
$DeploymentPath=[IO.Path]::GetFullPath($DeploymentPath)
$status=& (Join-Path $DeploymentPath 'latest.ps1') -Action Status|ConvertFrom-Json
if($Scenario -ne 'Native' -and -not $status.Live){throw 'Dock is not live; resource measurement refused.'}
if($Scenario -eq 'Native' -and $status.Running){throw 'Native baseline requires the controller to be stopped.'}
if(-not ('LeanDockResourceProbe' -as [type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class LeanDockResourceProbe {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string t);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr GetProp(IntPtr w,string n);
 [StructLayout(LayoutKind.Sequential)]public struct Power {public byte AC,Flags,Percent,Saver;public uint Remaining,Full;}
 [DllImport("kernel32.dll")]public static extern bool GetSystemPowerStatus(out Power power);
 public static long Value(IntPtr w,string n){var p=GetProp(w,n).ToInt64();return p==0?-1:p-1;}
}
'@}
$bar=[LeanDockResourceProbe]::FindWindow('Shell_TrayWnd',$null)
$targets=[Collections.Generic.List[object]]::new()
if($status.Running){$targets.Add([pscustomobject]@{Role='Controller';Id=[int]$status.Controller[0].ProcessId;Expected=(Join-Path $DeploymentPath 'latest.exe')})}
$targets.Add([pscustomobject]@{Role='ExplorerHost';Id=[int]$status.ExplorerPid;Expected=(Join-Path $env:windir 'explorer.exe')})
foreach($entry in @(@('StartHost',$status.StartLayout.pid,'SystemApps\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\StartMenuExperienceHost.exe'),@('QuickSettingsHost',$status.QuickTheme.pid,'System32\ShellHost.exe'))){
 if($entry[1]){
  $hostProcess=Get-Process -Id $entry[1] -ErrorAction SilentlyContinue
  if($hostProcess -and $hostProcess.Path -ieq (Join-Path $env:windir $entry[2]) -and $hostProcess.SessionId -eq (Get-Process -Id $PID).SessionId){$targets.Add([pscustomobject]@{Role=$entry[0];Id=[int]$entry[1];Expected=(Join-Path $env:windir $entry[2])})}
 }
}
foreach($target in $targets){
 $p=Get-Process -Id $target.Id
 if($p.Path -ine $target.Expected -or $p.SessionId -ne (Get-Process -Id $PID).SessionId){throw 'Resource target identity mismatch'}
 $target|Add-Member NoteProperty Started $p.StartTime.ToUniversalTime().Ticks
 $moduleRelative=switch($target.Role){'ExplorerHost'{'latest.dll'}'StartHost'{'start/latest.dll'}'QuickSettingsHost'{'panels/latest.dll'}default{$null}}
 $target|Add-Member NoteProperty ComponentLoaded $(if($moduleRelative){@($p.Modules|Where-Object FileName -IEQ (Join-Path $DeploymentPath $moduleRelative)).Count -gt 0}else{$null})
}
function Read-Threads {
 $map=@{};$counterTime=0UL;$frequency=0UL;try{
  $filter=($targets|ForEach-Object {'IDProcess='+$_.Id}) -join ' OR '
  foreach($t in Get-CimInstance Win32_PerfRawData_PerfProc_Thread -Filter $filter){
   $map[([string]$t.IDProcess+':'+$t.IDThread)]=[uint64]$t.ContextSwitchesPersec
   $counterTime=[uint64]$t.Timestamp_PerfTime;$frequency=[uint64]$t.Frequency_PerfTime
  }
 }catch{return @{Available=$false;Data=@{};Reason=$_.Exception.GetType().Name}}
 return @{Available=$true;Data=$map;Reason=$null;Time=$counterTime;Frequency=$frequency}
}
function Read-Sample {
 $processes=@(foreach($target in $targets){
  $p=Get-Process -Id $target.Id -ErrorAction SilentlyContinue
  if(-not $p -or $p.StartTime.ToUniversalTime().Ticks -ne $target.Started){throw 'Measured process exited or changed; discard this run.'}
  [pscustomobject]@{Role=$target.Role;Pid=$p.Id;CpuMs=$p.TotalProcessorTime.TotalMilliseconds;WorkingSetBytes=$p.WorkingSet64;PrivateBytes=$p.PrivateMemorySize64;Handles=$p.HandleCount;Threads=$p.Threads.Count}
 })
 $power=[LeanDockResourceProbe+Power]::new();$powerOk=[LeanDockResourceProbe]::GetSystemPowerStatus([ref]$power)
 [pscustomobject]@{At=(Get-Date -Format o);ElapsedMs=$watch.Elapsed.TotalMilliseconds;Processes=$processes;CardVisible=[LeanDockResourceProbe]::Value($bar,'LeanDock.Card.Visible');CardPage=[LeanDockResourceProbe]::Value($bar,'LeanDock.Card.View');CardSamples=[LeanDockResourceProbe]::Value($bar,'LeanDock.Card.Samples');MediaPlaying=[LeanDockResourceProbe]::Value($bar,'LeanDock.Card.MediaPlaying');KeepShown=[LeanDockResourceProbe]::Value($bar,'LeanDock.Hide.KeepShown');HideDecisions=[LeanDockResourceProbe]::GetProp($bar,'LeanDock.Hide.Decisions').ToInt64();AC=if($powerOk){[int]$power.AC}else{-1};BatteryPercent=if($powerOk -and $power.Percent -le 100){[int]$power.Percent}else{$null}}
}
$threadStart=Read-Threads
$watch=[Diagnostics.Stopwatch]::StartNew()
$samples=[Collections.Generic.List[object]]::new();$samples.Add((Read-Sample))
for($i=1;$i -le $DurationSeconds;$i++){
 $remaining=$i*1000-$watch.Elapsed.TotalMilliseconds
 if($remaining -gt 0){Start-Sleep -Milliseconds ([int]$remaining)}
 $samples.Add((Read-Sample))
}
$threadEnd=Read-Threads
$contextElapsed=if($threadStart.Available -and $threadEnd.Available -and $threadStart.Frequency -gt 0 -and $threadStart.Frequency -eq $threadEnd.Frequency -and $threadEnd.Time -gt $threadStart.Time){($threadEnd.Time-$threadStart.Time)/$threadStart.Frequency}else{0}
$first=$samples[0];$last=$samples[-1];$elapsed=($last.ElapsedMs-$first.ElapsedMs)/1000
$processors=[Environment]::ProcessorCount
$metrics=@(foreach($target in $targets){
 $a=$first.Processes|Where-Object Role -EQ $target.Role;$b=$last.Processes|Where-Object Role -EQ $target.Role
 $cpu=$b.CpuMs-$a.CpuMs;$all=@($samples|ForEach-Object {$_.Processes|Where-Object Role -EQ $target.Role})
 $switches=0L;$matched=0;$invalid=0
 foreach($key in $threadStart.Data.Keys|Where-Object {$_ -like ($target.Id.ToString()+':*')}){
  if($threadEnd.Data.ContainsKey($key) -and $threadEnd.Data[$key] -ge $threadStart.Data[$key]){$switches+=$threadEnd.Data[$key]-$threadStart.Data[$key];$matched++}else{$invalid++}
 }
 [pscustomobject]@{Role=$target.Role;Pid=$target.Id;ComponentLoaded=$target.ComponentLoaded;CpuMs=[math]::Round($cpu,3);CpuPercentMachine=[math]::Round($cpu/($elapsed*1000*$processors)*100,4);CpuPercentOneCore=[math]::Round($cpu/($elapsed*1000)*100,4);WorkingSetMeanMiB=[math]::Round(($all.WorkingSetBytes|Measure-Object -Average).Average/1MB,3);WorkingSetPeakMiB=[math]::Round(($all.WorkingSetBytes|Measure-Object -Maximum).Maximum/1MB,3);PrivateMeanMiB=[math]::Round(($all.PrivateBytes|Measure-Object -Average).Average/1MB,3);ContextSwitchesPerSecond=if($contextElapsed -gt 0 -and $matched){[math]::Round($switches/$contextElapsed,3)}else{$null};MatchedThreads=$matched;MissingOrResetThreads=$invalid;ContextSwitchSemantics='Lower bound over threads present in both snapshots; scheduling, not hardware wakeups';MaxHandles=($all.Handles|Measure-Object -Maximum).Maximum;MaxThreads=($all.Threads|Measure-Object -Maximum).Maximum}
})
$valid=switch($Scenario){
 'Visible' {@($samples|Where-Object CardVisible -NE 1).Count -eq 0}
 'Hidden' {@($samples|Where-Object CardVisible -NE 0).Count -eq 0}
 'Media' {@($samples|Where-Object {$_.CardVisible -ne 1 -or $_.CardPage -ne 1 -or $_.MediaPlaying -ne 1}).Count -eq 0}
 'Battery' {@($samples|Where-Object AC -NE 0).Count -eq 0}
 default {$true}
}
$receiptPath=Join-Path $DeploymentPath 'latest.json'
$receipt=if(Test-Path $receiptPath){Get-Content $receiptPath -Raw|ConvertFrom-Json}else{$null}
$result=[ordered]@{Schema=1;At=(Get-Date -Format o);Version=$receipt.Version;BackendVersion=$status.Backend.version;RuntimeHashes=@(foreach($file in @('latest.exe','latest.dll','start/latest.dll','panels/latest.dll')){[pscustomobject]@{Name=$file;SHA256=(Get-FileHash -LiteralPath (Join-Path $DeploymentPath $file)).Hash}});Scenario=$Scenario;ScenarioObserved=$valid;ElapsedSeconds=[math]::Round($elapsed,3);LogicalProcessors=$processors;OsBuild=[Environment]::OSVersion.Version.ToString();ACValues=@($samples.AC|Select-Object -Unique);CardPages=@($samples.CardPage|Select-Object -Unique);CardVisibleValues=@($samples.CardVisible|Select-Object -Unique);CardSampleDelta=$last.CardSamples-$first.CardSamples;HideDecisionDelta=$last.HideDecisions-$first.HideDecisions;Metrics=$metrics;Wakeups=[ordered]@{Available=$false;Reason='True wakeup/power attribution requires a separate ETW power trace. Context switches are reported separately and must not be renamed wakeups.'};Limits=@('Host process figures include Windows and other shell work; they are not incremental Dock cost.','No battery-life or energy saving claim.','The observer adds measurement overhead; compare only matching scenario, power, build and environment.');Comparison=$null;Samples=$samples.ToArray()}
if($BaselinePath){
 $base=Get-Content -LiteralPath $BaselinePath -Raw|ConvertFrom-Json
 $comparable=$valid -and $base.ScenarioObserved -and $base.Scenario -eq $Scenario -and $base.OsBuild -eq $result.OsBuild -and $base.LogicalProcessors -eq $processors -and (($base.ACValues -join ',') -eq ($result.ACValues -join ',')) -and (($base.CardPages -join ',') -eq ($result.CardPages -join ','))
 $result.Comparison=[ordered]@{Comparable=$comparable;BaselineVersion=$base.Version;Deltas=if($comparable){@(foreach($m in $metrics){$old=$base.Metrics|Where-Object Role -EQ $m.Role;if($old){[pscustomobject]@{Role=$m.Role;CpuPercentagePoints=[math]::Round($m.CpuPercentMachine-$old.CpuPercentMachine,4);PrivateMiB=[math]::Round($m.PrivateMeanMiB-$old.PrivateMeanMiB,3);WorkingSetMiB=[math]::Round($m.WorkingSetMeanMiB-$old.WorkingSetMeanMiB,3)}}})}else{@()}}
}
$output=[IO.Path]::GetFullPath($OutputPath);New-Item -ItemType Directory -Path (Split-Path $output -Parent) -Force|Out-Null
$result|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $output
[pscustomobject]@{Scenario=$Scenario;Observed=$valid;Seconds=$result.ElapsedSeconds;AC=$result.ACValues;Metrics=$metrics;Comparison=$result.Comparison}|ConvertTo-Json -Depth 7
