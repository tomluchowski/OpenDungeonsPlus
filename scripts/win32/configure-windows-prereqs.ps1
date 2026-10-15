$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Enter-OpenDungeonsPlus.ps1')
$taskRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskRoot = $taskDependencyRoot
$taskOptions = @('-S', $taskRepo, '-B', "$taskRepo\build\windows",
    '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DOD_BUILD_TESTING=OFF', '-DBUILD_TESTING=OFF',
    "-DCMAKE_INSTALL_PREFIX=$taskRepo/build/windows/install",
    "-DPYTHON_EXECUTABLE=$taskPythonCmakeRoot/python.exe", "-DPYTHON_LIBRARY=$taskPythonCmakeRoot/libs/python310.lib",
    "-DPYTHON_DEBUG_LIBRARY=$taskPythonCmakeRoot/libs/python310_d.lib", "-DPYTHON_INCLUDE_DIR=$taskPythonCmakeRoot/include")
Write-Output 'Checking the original project configuration with the installed prerequisites'
$ErrorActionPreference = 'Continue'
& cmake @taskOptions *> "$taskRoot\logs\opendungeons-configure.log"
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath "$taskRoot\logs\opendungeons-configure.log" -Tail 60; throw 'Project configuration failed' }
Write-Output 'Original project configuration succeeded'
& (Join-Path $PSScriptRoot 'prepare-windows-runtime.ps1')
