param([ValidateRange(1,180)][int]$Seconds=1,[ValidateRange(25,1000)][int]$IntervalMs=50,[string]$OutputPath=(Join-Path $PSScriptRoot 'samples-latest.jsonl'))
$ErrorActionPreference='Stop'
if(-not ('LeanDockMaterialProbe' -as [type])){Add-Type @'
using System;using System.Text;using System.Runtime.InteropServices;
public static class LeanDockMaterialProbe {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr h,string n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string n);
 [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr h,uint m,UIntPtr w,IntPtr l,uint f,uint t,out UIntPtr r);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll",EntryPoint="GetWindowLongW")] public static extern int GetWindowLong(IntPtr h,int i);
 [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(IntPtr h,out uint color,out byte alpha,out uint flags);
 [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h,int a,out int value,int size);
 [DllImport("kernel32.dll")] public static extern ulong GetTickCount64();
}
'@}
function Prop([IntPtr]$h,[string]$key){$v=[LeanDockMaterialProbe]::GetProp($h,('LeanDock.'+$key)).ToInt64();if($v){$v-1}else{$null}}
$taskbar=[LeanDockMaterialProbe]::FindWindow('Shell_TrayWnd',$null)
if(-not $taskbar){throw 'Taskbar unavailable'}
$owner=0u;[void][LeanDockMaterialProbe]::GetWindowThreadProcessId($taskbar,[ref]$owner)
if((Get-Process -Id $owner).ProcessName -ne 'explorer'){throw 'Unexpected taskbar owner'}
$message=[LeanDockMaterialProbe]::RegisterWindowMessage('LeanDock.Material.Probe.1')
$writer=[IO.StreamWriter]::new($OutputPath,$false,[Text.UTF8Encoding]::new($false))
$watch=[Diagnostics.Stopwatch]::StartNew();$count=0;$valid=0
try {
 Write-Output "Read-only material sampling active for $Seconds seconds"
 while($watch.Elapsed.TotalSeconds -lt $Seconds){
  $tick=[LeanDockMaterialProbe]::GetTickCount64();$ret=[UIntPtr]::Zero
  $ok=[LeanDockMaterialProbe]::SendMessageTimeout($taskbar,$message,[UIntPtr]1,[IntPtr][long]$tick,2,200,[ref]$ret)
  $matched=$ok -ne [IntPtr]::Zero -and (Prop $taskbar 'Probe.At') -eq $tick
  if($matched){$valid++}
  $fg=[LeanDockMaterialProbe]::GetForegroundWindow();$type=[Text.StringBuilder]::new(128)
  [void][LeanDockMaterialProbe]::GetClassName($fg,$type,128)
  $freeze=0;$cloaked=0
  $freezeHr=[LeanDockMaterialProbe]::DwmGetWindowAttribute($taskbar,15,[ref]$freeze,4)
  $cloakedHr=[LeanDockMaterialProbe]::DwmGetWindowAttribute($taskbar,14,[ref]$cloaked,4)
  $entry=[ordered]@{Tick=$tick;ElapsedMs=$watch.ElapsedMilliseconds;Matched=$matched;Foreground=$type.ToString();Frozen=$freeze;FreezeHr=$freezeHr;Cloaked=$cloaked;CloakedHr=$cloakedHr;ExStyle=[LeanDockMaterialProbe]::GetWindowLong($taskbar,-20)}
  $color=0u;$alpha=[byte]0;$flags=0u
  if([LeanDockMaterialProbe]::GetLayeredWindowAttributes($taskbar,[ref]$color,[ref]$alpha,[ref]$flags)){$entry.LayeredAlpha=$alpha;$entry.LayeredFlags=$flags}
  foreach($key in @('Ready','Error','Count','HostVisible','Theme','Argb','BrushOpacity',
   'BackgroundCopies','BackgroundCopyAt','BackgroundCopyResult','AppThumbnails','AppThumbnailAt','AppThumbnailResult',
   'BackgroundThumbnails','BackgroundThumbnailAt','BackgroundThumbnailResult')){$entry[$key]=Prop $taskbar ('Probe.'+$key)}
  $nodes=@();if($matched){for($i=0;$i -lt [Math]::Min(16,$entry['Count']);$i++){
   $n=[ordered]@{Index=$i};foreach($key in @('Opacity','VisualOpacity','Visible','Loaded','Width','Height','Type','Name')){$n[$key]=Prop $taskbar "Probe.Node$i.$key"};$nodes+=[pscustomobject]$n
  }}
  $entry.Nodes=$nodes
  $writer.WriteLine(($entry|ConvertTo-Json -Depth 5 -Compress));$writer.Flush();$count++
  Start-Sleep -Milliseconds $IntervalMs
 }
}finally{$writer.Dispose()}
[pscustomobject]@{Samples=$count;Valid=$valid;Seconds=$watch.Elapsed.TotalSeconds;Output=$OutputPath}|ConvertTo-Json
