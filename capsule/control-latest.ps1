param(
 [ValidateSet('Status','Start','Stop','Update','Rollback','PauseCorners','ResumeCorners','ReloadCorners')][string]$Action='Status',
 [string]$PackagePath='', [string]$Destination=$PSScriptRoot,
 [string]$BackupPath='', [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
if($Action -in @('PauseCorners','ResumeCorners','ReloadCorners')){
 & (Join-Path $PSScriptRoot 'native-control-latest.ps1') -Action $Action
 return
}
& (Join-Path $PSScriptRoot 'maintenance\capsule-latest.ps1') -Action $Action `
 -PackagePath $PackagePath -Destination $Destination -BackupPath $BackupPath -ValidateOnly:$ValidateOnly
