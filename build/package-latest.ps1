param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'artifacts\package\latest'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'maintenance/files-latest.ps1')
$build=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'latest.json') -Raw|ConvertFrom-Json
foreach($f in $build.RuntimeFiles){if((Get-FileHash -LiteralPath (Join-Path $root $f.Name)).Hash -ne $f.SHA256){throw 'Build receipt does not match runtime binaries'}}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if($out -eq $root -or $root.StartsWith($out.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Output must not contain source checkout'}
if(Test-Path -LiteralPath $out){throw 'Package output already exists. Use a new versioned parent directory with a latest child.'}
$names=[Collections.Generic.List[string]]::new()
foreach($f in @('latest.exe','latest.dll','start/latest.dll','panels/latest.dll','latest.ps1','latest.ini','README.md','NOTICE.md','LICENSE.txt','.gitattributes')){$names.Add($f)}
foreach($dir in @('include','src','baseline','build','tests','hotcorners','start','panels','maintenance','docs','upstream','vendor')){
 foreach($f in Get-ChildItem -LiteralPath (Join-Path $root $dir) -File -Recurse){
  $relative=[IO.Path]::GetRelativePath($root,$f.FullName).Replace('\','/')
  if($relative -match '/(objects|backups|[^/]*-fixture)/' -or $relative -match '/previous-' -or $relative -match 'latest-status\.json$' -or $relative -match '^tests/.*\.(json|txt|bin|ini)$' -or $relative -match '^maintenance/.*\.json$'){continue}
  if($f.Extension -in @('.cpp','.c','.h','.hpp','.inc','.py','.ps1','.md','.ini','.json','.def','.txt') -and $relative -notin $names){$names.Add($relative)}
 }
}
$files=@(foreach($name in $names){$path=Resolve-LeanDockChild $root $name;[pscustomobject]@{Name=$name;SHA256=(Get-FileHash -LiteralPath $path).Hash;Bytes=(Get-Item $path).Length}})
New-Item -ItemType Directory -Path $out -Force|Out-Null
foreach($f in $files){Copy-LeanDockFile $root $out $f.Name $f.SHA256}
[pscustomobject]@{Schema=1;Product='LeanDock';Version=$build.Version;BuiltAt=$build.BuiltAt;CreatedAt=(Get-Date -Format o);RuntimeFiles=$build.RuntimeFiles;Files=$files;ConfigurationPolicy='Preserve existing latest.ini and hotcorners/latest.ini';SourcePolicy='Exact local source snapshot; may contain separately maintained uncommitted changes'}|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $out 'package-latest.json')
$validated=Read-LeanDockPackage $out
[pscustomobject]@{Package=$out;Version=$validated.Version;Files=$validated.Files.Count}|ConvertTo-Json
