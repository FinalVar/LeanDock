param(
 [ValidateRange(5,60)][int]$DurationSeconds=15,
 [string]$OutputPath=(Join-Path $PSScriptRoot 'network-observe-latest.json')
)
$ErrorActionPreference='Stop'
# Reuse the numeric, read-only window-property contract from the resource probe.
# This does not activate windows, send input, change adapters or generate traffic.
if(-not ('LeanDockNetworkProbe' -as [type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class LeanDockNetworkProbe {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string t);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr GetProp(IntPtr w,string n);
 public static long Value(IntPtr w,string n){var p=GetProp(w,"LeanDock.Card."+n).ToInt64();return p==0?-1:p-1;}
}
'@}
$watch=[Diagnostics.Stopwatch]::StartNew()
$samples=[Collections.Generic.List[object]]::new()
$previous=$null
do {
 $begin=$watch.Elapsed.TotalMilliseconds
 $adapters=@(Get-NetAdapter -IncludeHidden | Where-Object {
  $_.InterfaceOperationalStatus -eq 1 -and -not $_.IMFilter -and -not $_.EndPointInterface -and
  -not $_.OperationalStatusDownMediaDisconnected -and -not $_.OperationalStatusDownInterfacePaused -and
  $_.InterfaceType -in @(6,71) -and
  ($_.HardwareInterface -or ($_.InterfaceType -eq 71 -and $_.NdisPhysicalMedium -in @(1,9)))
 })
 $stats=@(Get-NetAdapterStatistics -IncludeHidden)
 $at=($begin+$watch.Elapsed.TotalMilliseconds)/2
 $links=@(foreach($adapter in $adapters){
  $row=@($stats | Where-Object Name -EQ $adapter.Name)
  if($row.Count -ne 1){throw 'Adapter statistics did not match uniquely'}
  [pscustomobject]@{Id=[string]$adapter.NetLuid;Index=[int]$adapter.InterfaceIndex;
   WirelessPort=([int]$adapter.InterfaceType -eq 71 -and -not $adapter.HardwareInterface);
   Received=[uint64]$row[0].ReceivedBytes;Sent=[uint64]$row[0].SentBytes}
 })
 $received=$null;$sent=$null
 if($previous -and $at -gt $previous.At){
  $valid=$true;$rx=0.0;$tx=0.0
  foreach($link in $links){
   $before=@($previous.Links | Where-Object Id -EQ $link.Id)
   if($before.Count -ne 1 -or $link.Received -lt $before[0].Received -or $link.Sent -lt $before[0].Sent){$valid=$false;break}
   $rx+=($link.Received-$before[0].Received);$tx+=($link.Sent-$before[0].Sent)
  }
  if($valid){$received=$rx*1000/($at-$previous.At);$sent=$tx*1000/($at-$previous.At)}
 }
 $bar=[LeanDockNetworkProbe]::FindWindow('Shell_TrayWnd',$null)
 $sample=[pscustomobject]@{
  AtMs=$at;QueryDurationMs=$watch.Elapsed.TotalMilliseconds-$begin;
  Interfaces=$links;ReferenceReceivedBytesPerSecond=$received;ReferenceSentBytesPerSecond=$sent;
  DockVisible=[LeanDockNetworkProbe]::Value($bar,'Visible');DockActive=[LeanDockNetworkProbe]::Value($bar,'NetworkActive');
  DockInterfaces=[LeanDockNetworkProbe]::Value($bar,'NetworkInterfaces');DockSamples=[LeanDockNetworkProbe]::Value($bar,'NetworkSamples');
  DockReceivedBytesPerSecond=[LeanDockNetworkProbe]::Value($bar,'NetworkReceive');DockSentBytesPerSecond=[LeanDockNetworkProbe]::Value($bar,'NetworkSend')
 }
 $samples.Add($sample);$previous=[pscustomobject]@{At=$at;Links=$links}
 if($watch.Elapsed.TotalSeconds -lt $DurationSeconds){Start-Sleep -Milliseconds 800}
}while($watch.Elapsed.TotalSeconds -lt $DurationSeconds)
$active=@($samples | Where-Object { $_.DockVisible -eq 1 -and $_.DockActive -eq 1 })
$hotspot=@($samples | Where-Object { @($_.Interfaces | Where-Object WirelessPort).Count -gt 0 })
$result=[pscustomobject]@{
 At=(Get-Date -Format o);DurationSeconds=$watch.Elapsed.TotalSeconds;
 Reference='Read-only Get-NetAdapter/Statistics counters; sample windows differ from Dock';
 HotspotObserved=($hotspot.Count -gt 0);DockNetworkObserved=($active.Count -gt 0);
 InterfaceCountMismatches=@($active | Where-Object { $_.DockInterfaces -ne $_.Interfaces.Count }).Count;
 NativeUiAcceptance=$false;Samples=$samples.ToArray()
}
$result | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $OutputPath
$result | Select-Object At,DurationSeconds,HotspotObserved,DockNetworkObserved,InterfaceCountMismatches,NativeUiAcceptance | ConvertTo-Json
