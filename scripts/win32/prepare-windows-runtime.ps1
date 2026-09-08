$ErrorActionPreference = 'Stop'
$taskRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskBuild = Join-Path $taskRepo 'build\windows'
$taskPrefix = 'C:\Users\mario\od-deps\install'
$taskPythonRoot = 'C:\Users\mario\AppData\Local\Programs\Python\Python310'
$taskResourcesFile = Join-Path $taskBuild 'resources.cfg'

# These include the plugins loaded dynamically by OGRE and CEGUI, as well as
# the transitive dependencies of the installed Release libraries.
$taskRuntimeNames = @(
    'OgreBites.dll', 'OgreRTShaderSystem.dll', 'OgreOverlay.dll', 'OgreMain.dll'
    'OIS.dll', 'sfml-audio-2.dll', 'sfml-network-2.dll', 'sfml-system-2.dll'
    'CEGUIBase-0.dll', 'CEGUIOgreRenderer-0.dll'
    'CEGUICoreWindowRendererSet.dll', 'CEGUIExpatParser.dll'
    'Plugin_ParticleFX.dll', 'Plugin_OctreeSceneManager.dll'
    'Codec_STBI.dll', 'RenderSystem_GL3Plus.dll'
    'freetype.dll', 'libexpat.dll', 'pcre.dll', 'openal32.dll'
)
$taskRuntimeSources = @($taskRuntimeNames | ForEach-Object { Join-Path "$taskPrefix\bin" $_ })
$taskRuntimeSources += Join-Path $taskPythonRoot 'python310.dll'
$taskRuntimeSources += Join-Path $taskPythonRoot 'python3.dll'
foreach ($taskPath in ($taskRuntimeSources + @($taskResourcesFile, "$taskPythonRoot\Lib", "$taskPythonRoot\DLLs",
    "$taskPrefix\Media\RTShaderLib\GLSL", "$taskPrefix\Media\Main"))) {
    if (-not (Test-Path -LiteralPath $taskPath)) { throw "Required runtime input is missing: $taskPath" }
}

# The upstream template points to a Unix installation layout; the installed
# Windows OGRE media lives in install/Media and only includes GLSL shader sources.
$taskResourceLines = @(foreach ($taskLine in Get-Content -LiteralPath $taskResourcesFile) {
    if ($taskLine -match '^FileSystem=.*?/share/OGRE/Media/(.+)$') {
        $taskMediaPath = Join-Path "$taskPrefix\Media" $Matches[1]
        if (Test-Path -LiteralPath $taskMediaPath -PathType Container) {
            'FileSystem=' + ((Resolve-Path -LiteralPath $taskMediaPath).Path -replace '\\', '/')
        }
    } else {
        $taskLine
    }
})
foreach ($taskLine in $taskResourceLines) {
    if ($taskLine -match '^FileSystem=(.+)$') {
        $taskResourcePath = $Matches[1]
        if (-not [System.IO.Path]::IsPathRooted($taskResourcePath)) {
            $taskResourcePath = Join-Path $taskBuild $taskResourcePath
        }
        if (-not (Test-Path -LiteralPath $taskResourcePath -PathType Container)) {
            throw "Game resource directory is missing: $taskResourcePath"
        }
    }
}

foreach ($taskSource in $taskRuntimeSources) {
    Copy-Item -LiteralPath $taskSource -Destination $taskBuild -Force
}
[System.IO.File]::WriteAllLines($taskResourcesFile, [string[]]$taskResourceLines)

# Resolve the existing Python installation without a prepared shell or PATH.
# The paths apply only to the Python DLL beside this Release executable.
$taskPythonPaths = @('.', $taskPythonRoot, "$taskPythonRoot\Lib", "$taskPythonRoot\DLLs", 'import site')
[System.IO.File]::WriteAllLines((Join-Path $taskBuild 'python310._pth'), [string[]]$taskPythonPaths)
Write-Output "Release runtime prepared in $taskBuild; open opendungeons-plus.exe to test the game."
