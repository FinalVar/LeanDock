param([string]$PackagePath=(Join-Path $PSScriptRoot 'package\latest'),
      [string]$Destination='C:\Users\x\Documents\Codex\Tools\NativeTaskbarDock')
$ErrorActionPreference='Stop'
$out=Join-Path $PSScriptRoot 'build'
$result=Join-Path $out 'install-latest.json'
$log=Join-Path $out 'install-latest.log'
foreach($file in @($result,$log)){
 if(Test-Path -LiteralPath $file){
  Copy-Item -LiteralPath $file -Destination ($file+'.before-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
 }
}
$receipt=[ordered]@{StartedAt=(Get-Date -Format o);State='Started';ProcessId=$PID;Success=$false;Error=$null;FinishedAt=$null}
function Save-Receipt {$receipt|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $result -Encoding utf8NoBOM}
Save-Receipt
$transcript=$false
try{
 Start-Transcript -LiteralPath $log -Force|Out-Null;$transcript=$true
 & (Join-Path $PackagePath 'maintenance\capsule-latest.ps1') -Action Update -PackagePath $PackagePath -Destination $Destination
 $receipt.Success=$true;$receipt.State='Applied'
}catch{
 $receipt.State='Failed';$receipt.Error=$_.Exception.Message
 $_|Format-List * -Force|Out-String|Write-Output
}finally{
 $receipt.FinishedAt=Get-Date -Format o;Save-Receipt
 if($transcript){Stop-Transcript|Out-Null}
}
if(-not $receipt.Success){exit 1}
