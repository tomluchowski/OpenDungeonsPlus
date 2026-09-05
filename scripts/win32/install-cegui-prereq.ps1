$ErrorActionPreference = 'Stop'
$taskRoot = 'C:\Users\mario\od-deps'
$taskCmake = "$taskRoot\tools\cmake-3.31.8-windows-x86_64\bin\cmake.exe"
$taskPatch = Join-Path $PSScriptRoot 'patches/cegui-msvc-snprintf.patch'
$ErrorActionPreference = 'Continue'
& git -C "$taskRoot\src\cegui" apply --reverse --check $taskPatch *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Output 'Applying CEGUI snprintf compatibility fix for modern MSVC'
    & git -C "$taskRoot\src\cegui" apply $taskPatch
    if ($LASTEXITCODE -ne 0) { throw 'CEGUI compatibility patch failed' }
}
$ErrorActionPreference = 'Stop'
$taskOptions = @('-S', "$taskRoot\src\cegui", '-B', "$taskRoot\build\cegui",
    '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_INSTALL_PREFIX=$taskRoot\install",
    "-DCMAKE_PREFIX_PATH=$taskRoot\install", '-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /W3 /GR /EHsc /MP4',
    '-DCEGUI_BUILD_RENDERER_OGRE=ON', '-DCEGUI_BUILD_RENDERER_OPENGL=OFF',
    '-DCEGUI_BUILD_RENDERER_OPENGL3=OFF', '-DCEGUI_BUILD_RENDERER_OPENGLES=OFF',
    '-DCEGUI_BUILD_RENDERER_DIRECT3D9=OFF', '-DCEGUI_BUILD_RENDERER_DIRECT3D10=OFF',
    '-DCEGUI_BUILD_RENDERER_DIRECT3D11=OFF', '-DCEGUI_BUILD_XMLPARSER_EXPAT=ON',
    '-DCEGUI_BUILD_XMLPARSER_LIBXML2=OFF', '-DCEGUI_BUILD_IMAGECODEC_FREEIMAGE=OFF',
    '-DCEGUI_SAMPLES_ENABLED=OFF', '-DCEGUI_BUILD_APPLICATION_TEMPLATES=OFF',
    '-DCEGUI_BUILD_TESTS=OFF', '-DCEGUI_BUILD_PERFORMANCE_TESTS=OFF', '-DCEGUI_BUILD_DATAFILES_TEST=OFF',
    '-DCEGUI_BUILD_LUA_MODULE=OFF', '-DCEGUI_BUILD_LUA_GENERATOR=OFF', '-DCEGUI_BUILD_PYTHON_MODULES=OFF',
    '-DCEGUI_LIB_INSTALL_DIR:PATH=lib', '-DCEGUI_INCLUDE_INSTALL_DIR:PATH=include/cegui-0',
    "-DFREETYPE_LIB_DBG=$taskRoot/install/lib/freetyped.lib",
    "-DEXPAT_LIB_DBG=$taskRoot/install/lib/libexpatd.lib",
    "-DPCRE_LIB_DBG=$taskRoot/install/lib/pcred.lib",
    "-DBOOST_ROOT=$taskRoot/install", "-DBOOST_INCLUDEDIR=$taskRoot/install/include/boost-1_82",
    '-DPYTHON_EXECUTABLE=C:/Users/mario/AppData/Local/Programs/Python/Python310/python.exe')
Write-Output 'Configuring CEGUI'
$ErrorActionPreference = 'Continue'
& $taskCmake @taskOptions *> "$taskRoot\logs\cegui-configure.log"
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath "$taskRoot\logs\cegui-configure.log" -Tail 60; throw 'CEGUI configuration failed' }
foreach ($taskConfiguration in @('Release', 'Debug')) {
    Write-Output "Building and installing CEGUI ($taskConfiguration)"
    $ErrorActionPreference = 'Continue'
    & $taskCmake --build "$taskRoot\build\cegui" --config $taskConfiguration --target INSTALL --parallel 4 *> "$taskRoot\logs\cegui-$taskConfiguration.log"
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath "$taskRoot\logs\cegui-$taskConfiguration.log" -Tail 60; throw "CEGUI $taskConfiguration build failed" }
}
Write-Output 'CEGUI installed'
