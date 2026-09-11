#requires -Version 7.0
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
$metadata=Get-Content extension.json -Raw | ConvertFrom-Json
$package=Join-Path $root ("work\packages\rime-skins-"+$metadata.version)
if (Test-Path $package) { throw 'Package directory exists. Choose a new extension version or move the old package first.' }
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item output/WeaselServer.exe,output/weaselx64.dll,extension.json,LICENSE.txt -Destination $package
Copy-Item tools/install-extension.ps1,tools/install-manager.ps1 -Destination $package
Copy-Item work/manager-dist/WeaselSkinManager/* -Destination $package -Recurse
Copy-Item docs/USER_GUIDE.md -Destination "$package\README.md"
$revision=git rev-parse HEAD
if ($LASTEXITCODE) { throw 'Cannot record matching source revision.' }
$revision | Set-Content (Join-Path $package 'SOURCE_REVISION.txt') -Encoding utf8
if (!(Test-Path (Join-Path $package 'notices/Pillow/LICENSE'))) { throw 'Bundled dependency notices missing; rebuild manager.' }
$files=Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object { @{path=[IO.Path]::GetRelativePath($package,$_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} }
$files | ConvertTo-Json | Set-Content "$package\checksums.json" -Encoding utf8
Compress-Archive -LiteralPath $package -DestinationPath "$package.zip"
Write-Output "$package.zip"
