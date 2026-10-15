# Machine-specific locations shared by the Windows scripts. Set OD_DEPS_ROOT,
# OD_PYTHON_ROOT or OD_VS_PATH to override the defaults below.
$taskDependencyRoot = if ($env:OD_DEPS_ROOT) { $env:OD_DEPS_ROOT } else { Join-Path $env:USERPROFILE 'od-deps' }
$taskPythonRoot = if ($env:OD_PYTHON_ROOT) { $env:OD_PYTHON_ROOT } else { Join-Path $env:LOCALAPPDATA 'Programs\Python\Python310' }
$taskVsPath = if ($env:OD_VS_PATH) { $env:OD_VS_PATH } else { 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' }
# CMake arguments use forward slashes.
$taskPythonCmakeRoot = $taskPythonRoot.Replace('\', '/')
