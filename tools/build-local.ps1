#requires -Version 7.0
param([ValidateSet('ui','native-test','first-key-test','service-test','server','tsf')][string]$Target = 'ui')
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$env:PYTHONIOENCODING = 'utf-8'
$root = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.ATL -property installationPath
if (!$vs) { throw 'MSVC and ATL are required.' }
Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
function Check-Exit { if ($LASTEXITCODE) { throw "Native command exited with $LASTEXITCODE" } }
$project = if ($Target -in @('server','service-test')) { 'WeaselServer\WeaselServer.vcxproj' } else { 'WeaselUI\WeaselUI.vcxproj' }
if ($Target -eq 'tsf') { $project = 'WeaselTSF\WeaselTSF.vcxproj' }
& msbuild $project /t:Build /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=$root\" /p:WindowsTargetPlatformVersion=10.0 /v:minimal /nologo /fl "/flp:logfile=work\build-$Target.log;encoding=UTF-8"
Check-Exit
if ($Target -eq 'native-test') {
  & cl /nologo /EHsc /std:c++17 /utf-8 /MT /DUNICODE /D_UNICODE /DNDEBUG /Iinclude /IWeaselUI /Ideps\boost_1_84_0 tests\skin_native.cpp /Fework\skin_native.exe /Fowork\skin_native.obj /link msbuild\Release\x64\WeaselUI.lib user32.lib gdi32.lib advapi32.lib ole32.lib shell32.lib comctl32.lib /LTCG
  Check-Exit
}
if ($Target -eq 'service-test') {
  & cl /nologo /EHsc /std:c++17 /utf-8 /MT /DUNICODE /D_UNICODE /DNDEBUG /Iinclude /IWeaselUI /Ideps\boost_1_84_0 tests\skin_service.cpp /Fework\skin_service.exe /Fowork\skin_service.obj /link msbuild\Release\x64\WeaselIPC.lib /LIBPATH:deps\boost_1_84_0\stage\lib user32.lib gdi32.lib advapi32.lib ole32.lib shell32.lib comctl32.lib /LTCG
  Check-Exit
}
if ($Target -eq 'first-key-test') {
  & cl /nologo /EHsc /std:c++17 /utf-8 /MT /DUNICODE /D_UNICODE /DNDEBUG /Iinclude /IWeaselUI /Ideps\boost_1_84_0 tests\skin_first_key.cpp /Fework\skin_first_key.exe /Fowork\skin_first_key.obj /link msbuild\Release\x64\WeaselUI.lib user32.lib gdi32.lib advapi32.lib ole32.lib shell32.lib comctl32.lib /LTCG
  Check-Exit
}
