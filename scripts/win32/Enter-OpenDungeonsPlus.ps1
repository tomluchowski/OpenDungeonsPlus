$taskDependencyRoot = 'C:\Users\mario\od-deps'
$taskVsPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
Import-Module "$taskVsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $taskVsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:Path = "$taskDependencyRoot\tools\cmake-3.31.8-windows-x86_64\bin;$taskDependencyRoot\install\bin;$taskDependencyRoot\install\lib;C:\Users\mario\AppData\Local\Programs\Python\Python310;" + $env:Path
$env:CMAKE_PREFIX_PATH = "$taskDependencyRoot\install"
$env:CEGUI_HOME = "$taskDependencyRoot\install"
$env:OIS_HOME = "$taskDependencyRoot\install"
$env:BOOST_ROOT = "$taskDependencyRoot\install"
$env:BOOST_INCLUDEDIR = "$taskDependencyRoot\install\include\boost-1_82"
$env:BOOST_LIBRARYDIR = "$taskDependencyRoot\install\lib"
$env:LIB = "$taskDependencyRoot\install\lib;" + $env:LIB
Write-Output 'OpenDungeonsPlus development environment loaded (Windows x64).'
