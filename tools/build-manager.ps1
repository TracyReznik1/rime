#requires -Version 7.0
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
$python=Join-Path $root 'deps\skin-manager-venv\Scripts\python.exe'
if (!(Test-Path $python)) {
 python -m venv deps/skin-manager-venv
 if ($LASTEXITCODE) { throw 'venv failed' }
}
& $python -m pip install -r tools/requirements-manager.txt
if ($LASTEXITCODE) { throw 'dependency installation failed' }
& $python -m PyInstaller --noconfirm --clean --windowed --onedir --name WeaselSkinManager --contents-directory manager-runtime --distpath work/manager-dist --workpath work/manager-build --specpath work tools/skin_manager.py
if ($LASTEXITCODE) { throw 'manager build failed' }
