param([ValidateSet('Status','Start','Stop','Install','Uninstall','PauseCorners','ResumeCorners','ReloadCorners')][string]$Action='Status')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$exe=Join-Path $root 'latest.exe'
$taskName='NativeTaskbarDock'
$session=(Get-Process -Id $PID).SessionId
if(-not ('NativeDockManagementPathV1' -as [type])){
Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices;
public static class NativeDockManagementPathV1 {
 [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint access,bool inherit,uint id);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern bool QueryFullProcessImageName(IntPtr process,uint flags,StringBuilder path,ref uint size);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 public static string Read(uint id){IntPtr p=OpenProcess(0x1000,false,id);if(p==IntPtr.Zero)return null;try{uint n=32768;var path=new StringBuilder((int)n);return QueryFullProcessImageName(p,0,path,ref n)?path.ToString():null;}finally{CloseHandle(p);}}
}
'@
}
function Invoke-Control([string]$Argument){
    $p=Start-Process -FilePath $exe -ArgumentList $Argument -WindowStyle Hidden -PassThru
    if(-not $p.WaitForExit(20000)){throw "Controller command timed out: $Argument"}
    return $p.ExitCode
}
function Get-ExactProcesses {
    @(Get-CimInstance Win32_Process -Filter "Name='latest.exe'" | Where-Object {$_.SessionId -eq $session} | ForEach-Object {
        # Win32_Process can omit elevated process metadata for normal clients.
        $resolvedPath=$_.ExecutablePath
        if(-not $resolvedPath){$resolvedPath=[NativeDockManagementPathV1]::Read($_.ProcessId)}
        if($resolvedPath -ieq $exe -and $_.CommandLine -notmatch '\s--'){
            [pscustomobject]@{ProcessId=$_.ProcessId;SessionId=$_.SessionId;ExecutablePath=$resolvedPath;CommandLine=$_.CommandLine}
        }
    })
}
function Get-ShellProcess {
    $p=@(Get-CimInstance Win32_Process -Filter "Name='explorer.exe'" | Where-Object {$_.SessionId -eq $session -and $_.ExecutablePath -ieq (Join-Path $env:windir 'explorer.exe')})
    if($p.Count -ne 1){throw "Expected one current-session Explorer, got $($p.Count)"}
    $p[0]
}
function Get-OwnTask {
    $t=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if($t -and (@($t.Actions).Count -ne 1 -or $t.Actions[0].Execute -ine $exe)){
        throw 'A different task already uses NativeTaskbarDock; refusing to modify it.'
    }
    $t
}
function Get-LiveStatus {
    $code=Invoke-Control '--status'
    $processes=Get-ExactProcesses
    $backend=$null
    $shell=Get-ShellProcess
    if(Test-Path -LiteralPath (Join-Path $root 'latest-backend.json')){
        $backend=Get-Content -LiteralPath (Join-Path $root 'latest-backend.json') -Raw | ConvertFrom-Json
    }
    $modules=@((Get-Process -Id $shell.ProcessId).Modules | Where-Object {$_.FileName -ieq (Join-Path $root 'latest.dll') -or $_.FileName -like '*\Windhawk\*'} | Select-Object ModuleName,FileName)
    $runtime=$null;$corners=$null
    if(Test-Path -LiteralPath (Join-Path $root 'latest-status.json')){$runtime=Get-Content -LiteralPath (Join-Path $root 'latest-status.json') -Raw | ConvertFrom-Json}
    if(Test-Path -LiteralPath (Join-Path $root 'hotcorners\latest-status.json')){$corners=Get-Content -LiteralPath (Join-Path $root 'hotcorners\latest-status.json') -Raw | ConvertFrom-Json}
    $running=$processes.Count -eq 1 -and $runtime -and $runtime.controllerPid -eq $processes[0].ProcessId
    $cornerReady=$running -and $runtime.hotCornersReady -and $corners -and $corners.pid -eq $processes[0].ProcessId -and $corners.running -and $corners.rawInputRegistered
    $dockReady=$code -eq 0 -and $processes.Count -eq 1 -and $backend -and $backend.explorerPid -eq $shell.ProcessId -and $backend.ownerPid -eq $processes[0].ProcessId -and $backend.state -eq 2 -and -not $backend.failed -and $backend.styledElements -gt 0
    $live=$dockReady -and $cornerReady
    $t=Get-OwnTask
    [pscustomobject]@{Live=$live;Running=$running;DockReady=$dockReady;HotCornersReady=$cornerReady;DockSupported=$runtime.dockSupported;Elevated=$runtime.elevated;HotCorners=$corners;Controller=@($processes | Select-Object ProcessId,ExecutablePath);ExplorerPid=$shell.ProcessId;Backend=$backend;LoadedComponents=$modules;Task=if($t){[pscustomobject]@{Name=$t.TaskName;State=[string]$t.State;RunLevel=[string]$t.Principal.RunLevel;LogonType=[string]$t.Principal.LogonType}}else{$null};WindhawkService=[string](Get-Service Windhawk -ErrorAction SilentlyContinue).Status}
}
function Stop-Native {
    $code=Invoke-Control '--stop'
    if($code -ne 0){throw "Normal restoration did not confirm (exit $code)."}
    if((Get-ExactProcesses).Count){throw 'Controller is still running after Stop.'}
}
function Install-NativeTask {
    $old=Get-OwnTask
    if($old){return}
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent().Name
    $principal=New-ScheduledTaskPrincipal -UserId $identity -LogonType Interactive -RunLevel Highest
    $trigger=New-ScheduledTaskTrigger -AtLogOn -User $identity
    $trigger.Delay='PT5S'
    $settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([timespan]::Zero) -MultipleInstances IgnoreNew -StartWhenAvailable
    $action=New-ScheduledTaskAction -Execute $exe -WorkingDirectory $root
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Native Dock and hot corners. One hidden process; elevated to support existing administrator foreground windows.' | Out-Null
}
function Start-Native {
    # A Windows taskbar version mismatch disables only the Dock at startup.
    $null=Invoke-Control '--verify'
    if((Invoke-Control '--verify-corners') -ne 0){throw 'Hot-corner configuration verification failed.'}
    if((Get-Service Windhawk -ErrorAction SilentlyContinue).Status -eq 'Running'){throw 'Windhawk is still active. Complete a separately backed-up migration first.'}
    $t=Get-OwnTask
    if(-not $t){throw 'Install the current-user logon task first.'}
    if(-not $t.Settings.Enabled){Enable-ScheduledTask -TaskName $taskName | Out-Null}
    Start-ScheduledTask -TaskName $taskName
    for($i=0;$i -lt 40;$i++){
        Start-Sleep -Milliseconds 1000
        $status=Get-LiveStatus
        if($status.Live){return $status}
        if($status.Running -and -not $status.DockSupported -and $status.HotCornersReady){return $status}
        if($status.Backend -and $status.Backend.explorerPid -eq $status.ExplorerPid -and $status.Backend.failed){throw 'Native backend reported failure.'}
    }
    throw 'The taskbar did not confirm readiness.'
}
switch($Action){
 'Status' {Get-LiveStatus | ConvertTo-Json -Depth 6}
 'Start' {Start-Native | ConvertTo-Json -Depth 6}
 'Stop' {Stop-Native; 'Stopped; logon task retained. The DLL remains in Explorer until it exits.'}
 'Install' {Install-NativeTask; 'Current-user logon task registered. Start is a separate action.'}
 'Uninstall' {Stop-Native; if(Get-OwnTask){Unregister-ScheduledTask -TaskName $taskName -Confirm:$false}; 'Task removed; files retained.'}
 'PauseCorners' {if((Invoke-Control '--corners-pause') -ne 0){throw 'Cannot pause corners.'};Get-LiveStatus | ConvertTo-Json -Depth 6}
 'ResumeCorners' {if((Invoke-Control '--corners-resume') -ne 0){throw 'Cannot resume corners.'};Get-LiveStatus | ConvertTo-Json -Depth 6}
 'ReloadCorners' {if((Invoke-Control '--corners-reload') -ne 0){throw 'Reload rejected; previous settings retained.'};Get-LiveStatus | ConvertTo-Json -Depth 6}
}
