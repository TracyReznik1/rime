#requires -Version 7.0
# Updates only the per-user helper and its shortcut. Does not register an IME.
$ErrorActionPreference='Stop'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=Join-Path $env:LOCALAPPDATA 'RimeSkinManager'
$manifest=Join-Path $PSScriptRoot 'checksums.json'
$files=Get-Content -LiteralPath $manifest -Raw -Encoding utf8 | ConvertFrom-Json
$selected=@($files | Where-Object { $_.path -eq 'WeaselSkinManager.exe' -or $_.path -match '^(manager-runtime|notices)[\\/]' })
if (!($selected | Where-Object path -EQ 'WeaselSkinManager.exe')) { throw 'Manager executable is not listed in checksums.' }
$key=(Get-FileHash -LiteralPath $manifest).Hash.Substring(0,12)
$destination=Join-Path $root $key
$exe=Join-Path $destination 'WeaselSkinManager.exe'
$shortcut=Join-Path ([Environment]::GetFolderPath('Programs')) '小狼毫皮肤管理.lnk'
$shell=New-Object -ComObject WScript.Shell
if (Test-Path -LiteralPath $shortcut) {
 $existing=$shell.CreateShortcut($shortcut).TargetPath
 if (!$existing.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'An unrelated shortcut already has this name; preserve it and rename it before continuing.' }
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
$link=$shell.CreateShortcut($shortcut)
$link.TargetPath=$exe
$link.WorkingDirectory=$destination
$link.Description='导入和切换搜狗静态横版皮肤，需要已安装改版小狼毫渲染模块。'
$link.Save()
$record=@{Executable=$exe;Shortcut=$shortcut;PackageSha256=(Get-FileHash -LiteralPath $manifest).Hash}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'current.json') -Encoding utf8
Write-Output "Manager installed: $exe"
Write-Output "Start menu: $shortcut"
Write-Output 'This helper requires the patched Weasel renderer. The official renderer cannot display imported backgrounds.'
