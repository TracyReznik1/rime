#requires -Version 7.0
param([ValidateSet('Install','Restore','Status')][string]$Action='Install')
$ErrorActionPreference='Stop'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
if (![Environment]::Is64BitProcess) { throw 'Run with x64 PowerShell 7.' }
$installInfo=Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Rime\Weasel'
$installedRoot=[IO.Path]::GetFullPath($installInfo.WeaselRoot)
$official=Join-Path $installedRoot 'WeaselServer.exe'
if (!(Test-Path -LiteralPath $official)) { throw 'Install official Weasel before installing this extension.' }
$extensionRoot=Join-Path $installedRoot 'skin-extension'
$stateFile=Join-Path $extensionRoot 'installation.json'
$reg='HKLM:\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32'
function Write-State($value) {
 $temp="$stateFile.tmp"
 $value | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $temp -Encoding utf8
 Move-Item -LiteralPath $temp -Destination $stateFile -Force
}
function Start-Server([string]$path) {
 Start-Process -FilePath $path -WorkingDirectory (Split-Path $path) -WindowStyle Hidden
 Start-Sleep -Seconds 3
 if (!(Get-Process WeaselServer -ErrorAction SilentlyContinue | Where-Object Path -EQ $path)) { throw "Server failed: $path" }
}
if ($Action -eq 'Status') {
 if (Test-Path $stateFile) { Get-Content $stateFile }
 Write-Output "Registered TSF: $((Get-Item $reg).GetValue(''))"
 exit
}
$admin=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (!$admin.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run installation or restoration from an administrator PowerShell 7 window.' }
if ($Action -eq 'Restore') {
 $state=Get-Content $stateFile -Raw -Encoding utf8 | ConvertFrom-Json
 if ($state.Restored) { Write-Output 'Already restored.'; exit }
 if ((Get-Item $reg).GetValue('') -ne $state.CurrentDll) { throw 'Another installer changed TSF. Refusing to overwrite its registration.' }
 if (!(Test-Path -LiteralPath $state.PreviousDll)) { throw 'Previous module is unavailable; preserve installation.json and repair official Weasel.' }
 Set-Item -LiteralPath $reg -Value $state.PreviousDll
 Start-Server $official
 $state.Restored=$true
 Write-State $state
 Write-Output 'Official server and previous TSF registration restored. Restart applications. User skins and YAML are preserved.'
 exit
}
$metadata=Get-Content (Join-Path $PSScriptRoot 'extension.json') -Raw -Encoding utf8 | ConvertFrom-Json
$version=(Get-Item -LiteralPath $official).VersionInfo
$base="$($version.FileMajorPart).$($version.FileMinorPart).$($version.FileBuildPart)"
if ($base -ne $metadata.upstream_tag -or $metadata.architecture -ne 'x64') { throw "Package requires x64 Weasel $($metadata.upstream_tag); found $base." }
$checksums=Get-Content (Join-Path $PSScriptRoot 'checksums.json') -Raw -Encoding utf8 | ConvertFrom-Json
foreach ($file in $checksums) {
 $path=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $file.path))
 if (!$path.StartsWith($PSScriptRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe package path' }
 if ((Get-FileHash -LiteralPath $path).Hash -ne $file.sha256) { throw "Package file changed: $($file.path)" }
}
$hash=(Get-FileHash (Join-Path $PSScriptRoot 'checksums.json')).Hash.Substring(0,12)
$destination=Join-Path $extensionRoot $hash
if ($destination -match '[^\x00-\x7f]') { throw 'This engine requires an ASCII installation path. Reinstall official Weasel into an ASCII path first.' }
$previous=(Get-Item $reg).GetValue('')
$state=$null
if (Test-Path $stateFile) {
 $state=Get-Content $stateFile -Raw -Encoding utf8 | ConvertFrom-Json
 if (!$state.Restored -and $previous -ne $state.CurrentDll) { throw 'Registration changed externally; restore or repair before upgrading.' }
 if (!$state.Restored) { $previous=$state.PreviousDll }
}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$server=Join-Path $destination 'WeaselServer.exe'
$dll=Join-Path $destination 'weaselx64.dll'
if (!(Test-Path -LiteralPath (Join-Path $destination 'complete.json'))) {
 # Reuse the user's installed runtime. Exclude our entire subtree before /E.
 & robocopy $installedRoot $destination /E /R:1 /W:1 /NFL /NDL /NJH /NJS /XD $extensionRoot (Join-Path $installedRoot 'skin-trial') /XF uninstall.exe | Out-Null
 if ($LASTEXITCODE -gt 7) { throw 'Installed runtime copy failed.' }
 Copy-Item (Join-Path $PSScriptRoot 'WeaselServer.exe'),(Join-Path $PSScriptRoot 'weaselx64.dll'),(Join-Path $PSScriptRoot 'WeaselSkinManager.exe') -Destination $destination -Force
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'manager-runtime') -Destination $destination -Recurse -Force
 foreach ($name in @('WeaselServer.exe','weaselx64.dll','WeaselSkinManager.exe')) {
  if ((Get-FileHash (Join-Path $destination $name)).Hash -ne (Get-FileHash (Join-Path $PSScriptRoot $name)).Hash) { throw 'Installed binary verification failed.' }
 }
 $metadata | ConvertTo-Json | Set-Content (Join-Path $destination 'complete.json') -Encoding utf8
}
$newState=[pscustomobject]@{Version=$metadata.version;Base=$base;PreviousDll=$previous;CurrentDll=$dll;Server=$server;Restored=$false}
Write-State $newState
try {
 Set-Item -LiteralPath $reg -Value $dll
 Start-Server $server
} catch {
 Set-Item -LiteralPath $reg -Value $previous
 Start-Server $official
 $newState.Restored=$true
 Write-State $newState
 throw
}
Write-Output "Installed. Skin manager: $(Join-Path $destination 'WeaselSkinManager.exe')"
Write-Output 'Restart applications to load the new module. Start this extension server again after signing in for its tray menu; the standalone manager always works.'
