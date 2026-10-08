# Shared, side-effect-free validation plus exact-file copy/restore operations.
$ErrorActionPreference='Stop'
$script:LeanDockConfigFiles=@('latest.ini','hotcorners/latest.ini')
function Resolve-LeanDockChild([string]$Root,[string]$Relative){
 if([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.?([\\/]|$)|[:*?"<>|]' -or $Relative -match '(^|[\\/])[^\\/]*[. ]([\\/]|$)'){throw 'Unsafe relative package path'}
 if($Relative -match '[\\/]{2}|(^|[\\/])(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|[\\/]|$)'){throw 'Reserved or ambiguous package path'}
 $base=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
 $full=[IO.Path]::GetFullPath((Join-Path $base $Relative))
 if(-not $full.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Path escapes package root'}
 $cursor=$full
 while($cursor){
  if(Test-Path -LiteralPath $cursor){$item=Get-Item -LiteralPath $cursor -Force;if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse points are not accepted in maintenance paths'}}
  $next=Split-Path $cursor -Parent;if($next -eq $cursor){break};$cursor=$next
 }
 $full
}
function Read-LeanDockPackage([string]$Root){
 $manifestPath=Resolve-LeanDockChild $Root 'package-latest.json'
 $manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
 if($manifest.Schema -ne 1 -or $manifest.Product -ne 'LeanDock' -or -not $manifest.Files -or $manifest.Files.Count -gt 5000){throw 'Invalid LeanDock package manifest'}
 $seen=@{}
 foreach($file in $manifest.Files){
  $relative=$file.Name.Replace('\','/');$file.Name=$relative;$path=Resolve-LeanDockChild $Root $relative
  if($seen.ContainsKey($relative) -or $file.SHA256 -notmatch '^[A-Fa-f0-9]{64}$' -or -not(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Duplicate or missing package file'}
  if($relative -eq 'latest.json' -or $relative -match '^(backups|verification|artifacts)/' -or $relative -match '(^|/)latest-status\.json$'){throw 'Runtime state cannot be packaged'}
  $seen[$relative]=$true
  if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.SHA256){throw "Package checksum mismatch: $relative"}
 }
 foreach($required in @('latest.exe','latest.dll','start/latest.dll','panels/latest.dll','latest.ps1','latest.ini','hotcorners/latest.ini','maintenance/latest.ps1','maintenance/files-latest.ps1','maintenance/resources-latest.ps1')){if(-not $seen.ContainsKey($required)){throw "Missing required payload: $required"}}
 $manifest
}
function Copy-LeanDockFile([string]$SourceRoot,[string]$TargetRoot,[string]$Relative,[string]$ExpectedHash){
 $src=Resolve-LeanDockChild $SourceRoot $Relative;$dst=Resolve-LeanDockChild $TargetRoot $Relative
 if((Get-FileHash -LiteralPath $src).Hash -ine $ExpectedHash){throw 'Source changed after validation'}
 New-Item -ItemType Directory -Path (Split-Path $dst -Parent) -Force|Out-Null
 Copy-Item -LiteralPath $src -Destination $dst -Force
 if((Get-FileHash -LiteralPath $dst).Hash -ine $ExpectedHash){throw 'Copied file checksum mismatch'}
}
function Test-LeanDockRestore($Plan,[string]$BackupRoot){
 # Validate all rollback material and ownership before writing any old bytes.
 foreach($f in $Plan.Files){
  $target=Resolve-LeanDockChild $Plan.Destination $f.Name
  if($f.Existed){
   $source=Resolve-LeanDockChild $BackupRoot ('original/'+$f.Name);if((Get-FileHash -LiteralPath $source).Hash -ine $f.PreviousHash){throw 'Rollback checksum mismatch'}
   if($f.Name -notin @('latest.json','docs/verification-latest.json') -and (Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target).Hash -notin @($f.PreviousHash,$f.NewHash)){throw 'Owned file edited since update; refusing to overwrite it'}
  }
  elseif(Test-Path -LiteralPath $target){if((Get-FileHash -LiteralPath $target).Hash -ine $f.NewHash){throw 'New file modified since update; refusing to remove it'}}
 }
}
function Restore-LeanDockFiles($Plan,[string]$BackupRoot){
 Test-LeanDockRestore $Plan $BackupRoot
 foreach($f in $Plan.Files){
  if($f.Existed){Copy-LeanDockFile (Join-Path $BackupRoot 'original') $Plan.Destination $f.Name $f.PreviousHash}
  else{$target=Resolve-LeanDockChild $Plan.Destination $f.Name;if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target -Force}}
 }
}
