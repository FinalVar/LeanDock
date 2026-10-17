# Read-only, single snapshot. Does not synthesize input or activate a window.
$ErrorActionPreference='Stop'
if(-not ('LeanDockCardInputRead' -as [type])){
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class LeanDockCardInputRead {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr hwnd,string name);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
}
'@
}
$taskbar=[LeanDockCardInputRead]::FindWindow('Shell_TrayWnd',$null)
if($taskbar -eq [IntPtr]::Zero){throw 'Taskbar not present'}
[uint32]$ownerId=0
$threadId=[LeanDockCardInputRead]::GetWindowThreadProcessId($taskbar,[ref]$ownerId)
$result=[ordered]@{At=(Get-Date -Format o);Window=$taskbar.ToInt64();Process=$ownerId;Thread=$threadId}
foreach($name in @('Ready','Visible','Fits','View','Switches','Errors','PowerActive','VisiblePages')){
 $raw=[LeanDockCardInputRead]::GetProp($taskbar,"LeanDock.Card.$name").ToInt64()
 $result[$name]=if($raw){$raw-1}else{$null}
}
foreach($name in @('Ready','Native','Xaml','TouchpadStarts','Fallbacks','Axis','DMReady','DMContacts','DMActive','DMUpdates','DMState','DMError','DMSwitches','DMFrames')){
 $result["Wheel$name"]=[LeanDockCardInputRead]::GetProp($taskbar,"LeanDock.Card.Wheel.$name").ToInt64()
}
[pscustomobject]$result|ConvertTo-Json
