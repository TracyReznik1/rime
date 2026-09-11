#requires -Version 7.0
param([string]$InstallRoot=(Join-Path $env:LOCALAPPDATA 'RimeSkinManager'),[switch]$DesktopShortcut)
# Updates only the per-user helper and its shortcut. Does not register an IME.
$ErrorActionPreference='Stop'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=[IO.Path]::GetFullPath($InstallRoot)
$manifest=Join-Path $PSScriptRoot 'checksums.json'
$files=Get-Content -LiteralPath $manifest -Raw -Encoding utf8 | ConvertFrom-Json
$selected=@($files | Where-Object { $_.path -eq 'WeaselSkinManager.exe' -or $_.path -match '^(manager-runtime|notices)[\\/]' })
if (!($selected | Where-Object path -EQ 'WeaselSkinManager.exe')) { throw 'Manager executable is not listed in checksums.' }
$key=(Get-FileHash -LiteralPath $manifest).Hash.Substring(0,12)
$destination=Join-Path $root $key
$exe=Join-Path $destination 'WeaselSkinManager.exe'
# WScript.Shell can use the system ANSI code page when saving a shortcut name.
# Keep the filename portable; the application itself uses Chinese labels.
$shortcuts=@((Join-Path ([Environment]::GetFolderPath('Programs')) 'Rime Skin Manager.lnk'))
if($DesktopShortcut) { $shortcuts += Join-Path ([Environment]::GetFolderPath('Desktop')) 'Rime Skin Manager.lnk' }
$knownRoots=@($root,(Join-Path $env:LOCALAPPDATA 'RimeSkinManager'))
$shell=New-Object -ComObject WScript.Shell
foreach($shortcut in $shortcuts) {
 if (!(Test-Path -LiteralPath $shortcut)) { continue }
 $existing=$shell.CreateShortcut($shortcut).TargetPath
 if (!($knownRoots | Where-Object { $existing.StartsWith($_+'\',[StringComparison]::OrdinalIgnoreCase) })) { throw 'An unrelated shortcut already has this name; preserve it and rename it before continuing.' }
}
foreach($file in $selected) {
 $source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $file.path))
 $target=[IO.Path]::GetFullPath((Join-Path $destination $file.path))
 if (!$source.StartsWith($PSScriptRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or !$target.StartsWith($destination+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe package path.' }
 if ((Get-FileHash -LiteralPath $source).Hash -ne $file.sha256) { throw "Package file changed: $($file.path)" }
 New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
 if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target).Hash -ne $file.sha256) {
  Copy-Item -LiteralPath $source -Destination $target -Force
 }
 if ((Get-FileHash -LiteralPath $target).Hash -ne $file.sha256) { throw 'Installed helper checksum mismatch.' }
}
foreach($shortcut in $shortcuts) {
 $link=$shell.CreateShortcut($shortcut)
 $link.TargetPath=$exe
 $link.WorkingDirectory=$destination
 $link.Description='Rime static skin manager (requires the patched Weasel renderer)'
 $link.Save()
 if($shell.CreateShortcut($shortcut).TargetPath -ne $exe) { throw 'Shortcut target was not preserved; choose an ASCII installation directory.' }
}
$record=@{Executable=$exe;Shortcuts=$shortcuts;PackageSha256=(Get-FileHash -LiteralPath $manifest).Hash}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'current.json') -Encoding utf8
Write-Output "Manager installed: $exe"
Write-Output ("Shortcuts: "+($shortcuts -join ', '))
Write-Output 'This helper requires the patched Weasel renderer. The official renderer cannot display imported backgrounds.'
