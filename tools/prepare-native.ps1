#requires -Version 7.0
$ErrorActionPreference='Stop'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
New-Item -ItemType Directory -Path deps,work,lib64 -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.ATL -property installationPath
if (!$vs) { throw 'Install Visual Studio 2022 Build Tools with MSVC x64 and ATL.' }
Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$seven=(Get-Command 7z.exe -ErrorAction SilentlyContinue).Source
if (!$seven) { $seven=Join-Path $env:ProgramFiles '7-Zip\7z.exe' }
if (!(Test-Path $seven)) { throw 'Install 7-Zip before preparing native dependencies.' }
function Fetch([string]$url,[string]$path,[string]$hash) {
 if (!(Test-Path $path)) { Invoke-WebRequest $url -OutFile $path }
 if ((Get-FileHash $path).Hash -ne $hash) { throw "Dependency checksum mismatch: $path" }
}
Fetch 'https://archives.boost.io/release/1.84.0/source/boost_1_84_0.zip' 'deps/boost_1_84_0.zip' 'CC77EB8ED25DA4D596B25E77E4DBB6C5AFAAC9CDDD00DC9CA947B6B268CC76A4'
if (!(Test-Path deps/boost_1_84_0/boost/version.hpp)) {
 & $seven x deps/boost_1_84_0.zip -odeps -y | Out-Null
 if ($LASTEXITCODE) { throw 'Boost extraction failed.' }
}
if (!(Test-Path deps/boost_1_84_0/stage/lib/libboost_regex-vc143-mt-s-x64-1_84.lib)) {
 Push-Location deps/boost_1_84_0
 try {
  & .\bootstrap.bat
  if ($LASTEXITCODE) { throw 'Boost bootstrap failed.' }
  # Use the already initialized compiler environment, including current VS minor versions.
  $compiler=($env:VCToolsInstallDir+'bin/Hostx64/x64/cl.exe').Replace('\','/')
  $setup=(Join-Path $root 'work/boost-env.bat').Replace('\','/')
  '@echo off' | Set-Content -LiteralPath $setup -Encoding ascii
  ('using msvc : 14.3 : "'+$compiler+'" : <setup>"'+$setup+'" ;') | Set-Content project-config.jam -Encoding utf8
  & .\b2.exe toolset=msvc-14.3 architecture=x86 address-model=64 variant=release link=static runtime-link=static threading=multi --with-filesystem --with-locale --with-regex --with-system --with-thread --with-date_time --with-chrono --with-serialization stage -j2
  if ($LASTEXITCODE) { throw 'Boost build failed.' }
 } finally { Pop-Location }
}
$archives=@{
 'rime-1c23358-Windows-msvc-x64.7z'='05FCF8CC2D058A0186DD9F04D6E021AD41687DB50DC81E85CF655DFABFDF0009'
 'rime-deps-1c23358-Windows-msvc-x64.7z'='3EDE059E6C1F4CDD5843CED3205F76666B706E5F55CCF8E56E2D04791A376FF6'
}
foreach ($name in $archives.Keys) {
 Fetch "https://github.com/rime/librime/releases/download/1.13.1/$name" "deps/$name" $archives[$name]
 & $seven x "deps/$name" -odeps/rime-x64 -y | Out-Null
 if ($LASTEXITCODE) { throw 'librime extraction failed.' }
}
Copy-Item deps/rime-x64/dist/include/rime*.h -Destination include -Force
Copy-Item deps/rime-x64/include/glog -Destination include -Recurse -Force
Copy-Item deps/rime-x64/dist/lib/rime.lib -Destination lib64 -Force
Copy-Item deps/rime-x64/dist/lib/rime.dll -Destination output -Force
$metadata=Get-Content extension.json -Raw | ConvertFrom-Json
$version=[version]$metadata.upstream_tag
$values=@{BOOST_ROOT=[Security.SecurityElement]::Escape((Join-Path $root 'deps/boost_1_84_0'));PLATFORM_TOOLSET='v143';VERSION_MAJOR=$version.Major;VERSION_MINOR=$version.Minor;VERSION_PATCH=$version.Build;PRODUCT_VERSION="$version.1";FILE_VERSION="$($version.Major),$($version.Minor),$($version.Build),1"}
$props=Get-Content weasel.props.template -Raw
foreach ($key in $values.Keys) { $props=$props.Replace('$'+$key,[string]$values[$key]) }
[IO.File]::WriteAllText((Join-Path $root 'weasel.props'),$props,[Text.UTF8Encoding]::new($false))
Write-Output 'Native dependencies ready.'
