param([string]$CompilerRoot=$env:LEANDOCK_COMPILER_ROOT,[switch]$IncludeTrayProbe,[switch]$IncludeVisualProbe,[switch]$IncludeIdleProbe,[switch]$DeferVisualAcceptance)
$ErrorActionPreference='Stop'
if($IncludeVisualProbe -and $DeferVisualAcceptance){throw 'Choose an own-window visual probe or explicit pending user acceptance, not both'}
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\toolchain-latest.ps1')
$CompilerRoot=Resolve-LeanDockCompiler $CompilerRoot
$out=Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$test=Join-Path $out 'policy-tests.exe'
& (Join-Path $CompilerRoot 'bin\clang++.exe') -target x86_64-w64-mingw32 -std=c++23 -O2 -static (Join-Path $PSScriptRoot 'tests-latest.cpp') -o $test
if($LASTEXITCODE){throw 'Policy tests failed to compile'}
& $test
if($LASTEXITCODE){throw 'Policy tests failed'}
$p=Start-Process -FilePath (Join-Path $out 'latest.exe') -ArgumentList '--probe' -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(15000)){ $p.Kill(); $p.WaitForExit(); throw 'Hidden composition probe timed out' }
if($p.ExitCode){throw "Hidden composition probe failed: $($p.ExitCode)"}
'PASS hidden HostBackdropBrush + effect + geometry + animation initialization (not visual acceptance)'
foreach($mode in @('--protocol-tests','--apps-probe','--icons-probe','--time-key-probe','--controls-probe','--control-panel-probe','--data-probe','--guard-probe')){
 $p=Start-Process -FilePath (Join-Path $out 'latest.exe') -ArgumentList $mode -WindowStyle Hidden -PassThru
 if(-not $p.WaitForExit(15000)){ $p.Kill(); $p.WaitForExit(); throw "$mode timed out" }
 if($p.ExitCode){throw "$mode failed: $($p.ExitCode)"}
 "PASS $mode"
}
$extra=@()
$visualState=if($DeferVisualAcceptance){'PendingUserAcceptance'}else{'NotRequested'}
if($IncludeTrayProbe){$extra+='--native-tray-probe'}
if($IncludeVisualProbe){$extra+='--visual-probe'}
if($IncludeIdleProbe){$extra+='--idle-probe'}
foreach($mode in $extra){
 $p=Start-Process -FilePath (Join-Path $out 'latest.exe') -ArgumentList $mode -WindowStyle Hidden -PassThru
 if(-not $p.WaitForExit(45000)){$p.Kill();$p.WaitForExit();throw "$mode timed out"}
 if($mode -eq '--visual-probe' -and $p.ExitCode -eq 1459){
  $visualState='PendingInteractiveDesktop'
  'PENDING own-surface visual check: desktop is locked/inactive; captured pixels are not accepted'
  continue
 }
 if($p.ExitCode){throw "$mode failed: $($p.ExitCode)"}
 if($mode -eq '--visual-probe'){$visualState='CapturedPendingReview'}
 "PASS $mode"
}
$build=Get-Content -LiteralPath (Join-Path $out 'latest.json') -Raw|ConvertFrom-Json
foreach($sourceItem in $build.Inputs){
 if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $sourceItem.Name)).Hash -ne $sourceItem.SHA256){throw 'Source changed since build'}
}
$hash=(Get-FileHash -LiteralPath (Join-Path $out 'latest.exe')).Hash
if($hash -ne $build.SHA256){throw 'Executable changed since build'}
[pscustomobject]@{At=(Get-Date -Format o);SHA256=$hash;TechnicalChecksPassed=$true;
 ControlsReadOnlyProbe=(Get-Content -LiteralPath (Join-Path $out 'controls-probe-latest.json') -Raw|ConvertFrom-Json);
 ControlPanelOffscreenRendered=$true;ActualControlWritesAccepted=$false;
 NativeTrayProbe=[bool]$IncludeTrayProbe;OwnVisualProbe=($visualState -eq 'CapturedPendingReview');VisualState=$visualState;IdleProbe=[bool]$IncludeIdleProbe;
 VisualDeferredReason=$(if($DeferVisualAcceptance){'No desktop automation; the user will observe the authorized deployed update during normal use'}else{$null});
 UserInteractionAccepted=$false;FullLifecycleAccepted=$false}|
 ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'validation-latest.json') -Encoding utf8NoBOM
