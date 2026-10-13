$ErrorActionPreference = 'Stop'
$taskRoot = 'C:\Users\mario\od-deps'
$taskCmake = "$taskRoot\tools\cmake-3.31.8-windows-x86_64\bin\cmake.exe"
$taskPatch = Join-Path $PSScriptRoot 'patches/ogre-multiwindow-settings.patch'
$ErrorActionPreference = 'Continue'
& git -C "$taskRoot\src\ogre" apply --recount --reverse --check $taskPatch *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Output 'Applying OGRE multi-window settings compatibility fixes'
    & git -C "$taskRoot\src\ogre" apply --recount $taskPatch
    if ($LASTEXITCODE -ne 0) { throw 'OGRE compatibility patch failed' }
}
$ErrorActionPreference = 'Stop'
$taskOptions = @('-S', "$taskRoot\src\ogre", '-B', "$taskRoot\build\ogre",
    '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_INSTALL_PREFIX=$taskRoot\install",
    "-DCMAKE_PREFIX_PATH=$taskRoot\install", '-DOGRE_BUILD_DEPENDENCIES=OFF',
    '-DOGRE_STATIC=OFF', '-DOGRE_CONFIG_THREAD_PROVIDER=std', '-DOGRE_CONFIG_THREADS=3',
    '-DOGRE_BUILD_RENDERSYSTEM_GL3PLUS=ON', '-DOGRE_BUILD_RENDERSYSTEM_GL=OFF',
    '-DOGRE_BUILD_RENDERSYSTEM_D3D9=OFF', '-DOGRE_BUILD_RENDERSYSTEM_D3D11=OFF',
    '-DOGRE_BUILD_RENDERSYSTEM_GLES2=OFF', '-DOGRE_BUILD_PLUGIN_FREEIMAGE=OFF',
    '-DOGRE_BUILD_PLUGIN_EXRCODEC=OFF', '-DOGRE_BUILD_PLUGIN_BSP=OFF',
    '-DOGRE_BUILD_PLUGIN_PCZ=OFF', '-DOGRE_BUILD_PLUGIN_DOT_SCENE=OFF',
    '-DOGRE_BUILD_COMPONENT_JAVA=OFF', '-DOGRE_BUILD_COMPONENT_PYTHON=OFF',
    '-DOGRE_BUILD_COMPONENT_CSHARP=OFF', '-DOGRE_BUILD_COMPONENT_VOLUME=OFF',
    '-DOGRE_BUILD_COMPONENT_PAGING=OFF', '-DOGRE_BUILD_COMPONENT_TERRAIN=OFF',
    '-DOGRE_BUILD_COMPONENT_PROPERTY=OFF', '-DOGRE_BUILD_COMPONENT_MESHLODGENERATOR=OFF',
    '-DOGRE_BUILD_COMPONENT_HLMS=OFF', '-DOGRE_BUILD_COMPONENT_OVERLAY_IMGUI=OFF',
    '-DOGRE_BUILD_SAMPLES=OFF', '-DOGRE_BUILD_TOOLS=OFF', '-DOGRE_BUILD_TESTS=OFF',
    '-DOGRE_ENABLE_PRECOMPILED_HEADERS=ON', '-DOGRE_BUILD_MSVC_MP=OFF', '-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /W3 /GR /EHsc /MP4')
Write-Output 'Configuring OGRE'
$ErrorActionPreference = 'Continue'
& $taskCmake @taskOptions *> "$taskRoot\logs\ogre-configure.log"
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath "$taskRoot\logs\ogre-configure.log" -Tail 55; throw 'OGRE configuration failed' }
foreach ($taskConfiguration in @('Release', 'Debug')) {
    Write-Output "Building and installing OGRE ($taskConfiguration)"
    $ErrorActionPreference = 'Continue'
    & $taskCmake --build "$taskRoot\build\ogre" --config $taskConfiguration --target INSTALL --parallel 4 *> "$taskRoot\logs\ogre-$taskConfiguration.log"
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath "$taskRoot\logs\ogre-$taskConfiguration.log" -Tail 55; throw "OGRE $taskConfiguration build failed" }
}
Write-Output 'OGRE installed'
