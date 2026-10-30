param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT,[switch]$DeferVisualAcceptance)
$ErrorActionPreference='Stop'
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){
 $log=Join-Path $PSScriptRoot 'build\tests-user-latest.log'
 if(Test-Path -LiteralPath $log){Copy-Item -LiteralPath $log -Destination ($log+'.before-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
 Set-Content -LiteralPath $log -Value '' -Encoding utf8NoBOM
 try{
  & (Join-Path $PSScriptRoot 'tests-latest.ps1') -CompilerRoot $CompilerRoot -IncludeTrayProbe -IncludeVisualProbe:(!$DeferVisualAcceptance) -DeferVisualAcceptance:$DeferVisualAcceptance 2>&1 |
   Tee-Object -FilePath (Join-Path $PSScriptRoot 'build\tests-user-latest.log')
  if(-not $?){throw 'Tests failed'}
  exit 0
 }catch{
  $_|Out-String|Add-Content -LiteralPath (Join-Path $PSScriptRoot 'build\tests-user-latest.log')
  exit 1
 }
}
# Test runner only. The installed controller uses its fixed capsule child path.
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class CapsuleUserTest {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
 struct SI {public int cb;public string reserved,desktop,title;public int x,y,cx,cy,cols,rows,fill,flags;public short show,reserved2;public IntPtr bytes,input,output,error;}
 [StructLayout(LayoutKind.Sequential)] struct SX {public SI start;public IntPtr attributes;}
 [StructLayout(LayoutKind.Sequential)] struct PI {public IntPtr process,thread;public uint pid,tid;}
 [DllImport("user32.dll")] static extern IntPtr GetShellWindow();
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr h,uint flags,StringBuilder b,ref uint size);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool InitializeProcThreadAttributeList(IntPtr list,int count,uint flags,ref IntPtr size);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool UpdateProcThreadAttribute(IntPtr list,uint flags,IntPtr attribute,IntPtr value,IntPtr size,IntPtr previous,IntPtr returned);
 [DllImport("kernel32.dll")] static extern void DeleteProcThreadAttributeList(IntPtr list);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcess(string app,StringBuilder cmd,IntPtr pa,IntPtr ta,bool inherit,uint flags,IntPtr env,string cwd,ref SX start,out PI info);
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr p,uint access,out IntPtr token);
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,out int value,int size,out int needed);
 [DllImport("kernel32.dll")] static extern uint ResumeThread(IntPtr h);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h,uint ms);
 [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr h,out uint code);
 [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr h,uint code);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 static void Check(bool ok){if(!ok)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}
 public static uint Run(string app,string command,string cwd){
  uint id;GetWindowThreadProcessId(GetShellWindow(),out id);
  var shell=OpenProcess(0x1080,false,id);Check(shell!=IntPtr.Zero);
  IntPtr list=IntPtr.Zero,value=IntPtr.Zero;bool initialized=false;PI pi=new PI();
  try{
   uint n=32768;var path=new StringBuilder((int)n);Check(QueryFullProcessImageName(shell,0,path,ref n));
   if(!String.Equals(path.ToString(),System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"explorer.exe"),StringComparison.OrdinalIgnoreCase))throw new Exception("Unexpected shell");
   IntPtr size=IntPtr.Zero;InitializeProcThreadAttributeList(IntPtr.Zero,1,0,ref size);
   list=Marshal.AllocHGlobal(size);Check(InitializeProcThreadAttributeList(list,1,0,ref size));initialized=true;
   value=Marshal.AllocHGlobal(IntPtr.Size);Marshal.WriteIntPtr(value,shell);
   Check(UpdateProcThreadAttribute(list,0,new IntPtr(0x20000),value,new IntPtr(IntPtr.Size),IntPtr.Zero,IntPtr.Zero));
   var sx=new SX();sx.start.cb=Marshal.SizeOf<SX>();sx.start.flags=1;sx.start.show=0;sx.attributes=list;
   Check(CreateProcess(app,new StringBuilder(command),IntPtr.Zero,IntPtr.Zero,false,0x08080004,IntPtr.Zero,cwd,ref sx,out pi));
   IntPtr token;Check(OpenProcessToken(pi.process,8,out token));
   try{int elevated,needed;Check(GetTokenInformation(token,20,out elevated,4,out needed));if(elevated!=0)throw new Exception("Test child remained elevated");}
   finally{CloseHandle(token);}
   Check(ResumeThread(pi.thread)!=UInt32.MaxValue);
   if(WaitForSingleObject(pi.process,180000)!=0)throw new TimeoutException("Test runner did not finish");
   uint code;Check(GetExitCodeProcess(pi.process,out code));return code;
  }catch{if(pi.process!=IntPtr.Zero)TerminateProcess(pi.process,1);throw;}
  finally{
   if(pi.thread!=IntPtr.Zero)CloseHandle(pi.thread);if(pi.process!=IntPtr.Zero)CloseHandle(pi.process);
   if(initialized)DeleteProcThreadAttributeList(list);if(list!=IntPtr.Zero)Marshal.FreeHGlobal(list);
   if(value!=IntPtr.Zero)Marshal.FreeHGlobal(value);CloseHandle(shell);
  }
 }
}
'@
$pwsh=Join-Path $PSHOME 'pwsh.exe'
$argsText='"'+$pwsh+'" -NoProfile -File "'+$PSCommandPath+'" -CompilerRoot "'+$CompilerRoot+'"'
if($DeferVisualAcceptance){$argsText+=' -DeferVisualAcceptance'}
$code=[CapsuleUserTest]::Run($pwsh,$argsText,(Split-Path $PSScriptRoot -Parent))
Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build\tests-user-latest.log')
if($code){throw "Ordinary-user tests failed: $code"}
