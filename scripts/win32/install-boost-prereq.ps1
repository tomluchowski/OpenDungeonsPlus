$ErrorActionPreference = 'Stop'
$taskRoot = 'C:\Users\mario\od-deps'
$taskVsPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
Import-Module "$taskVsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $taskVsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
Set-Location -LiteralPath "$taskRoot\src\boost_1_82_0"
Write-Output 'Preparing Boost build tool'
$ErrorActionPreference = 'Continue'
& .\bootstrap.bat *> "$taskRoot\logs\boost-bootstrap.log"
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath "$taskRoot\logs\boost-bootstrap.log" -Tail 40; throw 'Boost bootstrap failed' }
$taskCompiler = (Get-Command cl.exe).Source.Replace('\', '/')
$taskCompilerSetup = "$taskVsPath\VC\Auxiliary\Build\vcvarsall.bat".Replace('\', '/')
'using msvc : 14.3 : "{0}" : <setup>"{1}" ;' -f $taskCompiler, $taskCompilerSetup | Set-Content -LiteralPath "$taskRoot\boost-user-config.jam" -Encoding ASCII
Write-Output 'Building and installing Boost libraries for Release and Debug'
$ErrorActionPreference = 'Continue'
& .\b2.exe -j8 "--user-config=$taskRoot\boost-user-config.jam" --reconfigure toolset=msvc-14.3 architecture=x86 address-model=64 variant=release,debug link=static,shared runtime-link=shared threading=multi --layout=versioned --with-filesystem --with-locale --with-program_options --with-thread --with-system --with-chrono --with-date_time --with-atomic "--prefix=$taskRoot\install" install *> "$taskRoot\logs\boost-build.log"
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath "$taskRoot\logs\boost-build.log" -Tail 50; throw 'Boost build failed' }
Write-Output 'Boost installed'
