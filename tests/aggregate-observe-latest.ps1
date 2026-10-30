param([ValidateRange(1,300)][int]$Seconds=30,[Parameter(Mandatory)][string]$OutputPath)
$ErrorActionPreference='Stop'
# Passive, bounded observation only. No screenshots, input, activation or hooks.
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
public static class LeanDockPassiveObserver {
 [StructLayout(LayoutKind.Sequential)] struct Point {public int x,y;}
 [StructLayout(LayoutKind.Sequential)] struct Cursor {public int size,flags;public IntPtr handle;public Point point;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string c,string t);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetProp(IntPtr w,string p);
 [DllImport("user32.dll")] static extern bool IsWindow(IntPtr w);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")] static extern bool GetCursorInfo(ref Cursor c);
 [DllImport("user32.dll")] static extern IntPtr LoadCursor(IntPtr i,IntPtr id);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 public sealed class Attempt {
  public string At;public long Cookie;public long Pid;public bool Visible,Ended;
  public int CursorSamples,BusyCursorSamples;public long PanelError=-1;
 }
 public sealed class Result {
  public string State;public int Samples,CursorReadFailures;public List<Attempt> Attempts=new List<Attempt>();
  public bool InputSent=false,DesktopCaptured=false,CorrelationOnly=true;
 }
 public static Result Run(int seconds){
  var result=new Result();var busy=LoadCursor(IntPtr.Zero,new IntPtr(32650));
  var wait=LoadCursor(IntPtr.Zero,new IntPtr(32514));long lastCookie=0;Attempt current=null;
  var clock=System.Diagnostics.Stopwatch.StartNew();
  while(clock.ElapsedMilliseconds<seconds*1000L){
   var bar=FindWindow("Shell_TrayWnd",null);result.Samples++;
   var cookie=bar==IntPtr.Zero?0:GetProp(bar,"LeanDock.Aggregate.Cookie").ToInt64();
   if(cookie!=lastCookie){
    if(current!=null)current.Ended=true;
    current=null;lastCookie=cookie;
    if(cookie!=0){current=new Attempt{At=DateTimeOffset.Now.ToString("o"),Cookie=cookie};result.Attempts.Add(current);}
   }
   if(current!=null){
    current.Pid=GetProp(bar,"LeanDock.Aggregate.PanelPid").ToInt64();
    current.PanelError=GetProp(bar,"LeanDock.Card.PanelError").ToInt64()-1;
    var window=GetProp(bar,"LeanDock.Aggregate.PanelWindow");uint owner;
    if(window!=IntPtr.Zero&&IsWindow(window)){GetWindowThreadProcessId(window,out owner);
     if(owner==current.Pid&&IsWindowVisible(window))current.Visible=true;}
    var cursor=new Cursor{size=Marshal.SizeOf<Cursor>()};
    if(GetCursorInfo(ref cursor)){current.CursorSamples++;
     if((cursor.flags&1)!=0&&((busy!=IntPtr.Zero&&cursor.handle==busy)||(wait!=IntPtr.Zero&&cursor.handle==wait)))current.BusyCursorSamples++;
    }else result.CursorReadFailures++;
   }
   Thread.Sleep(50);
  }
  result.State=result.Attempts.Count==0?"NotObserved":"Observed";return result;
 }
}
'@
$result=[LeanDockPassiveObserver]::Run($Seconds)
$result|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
$result|ConvertTo-Json -Depth 5
