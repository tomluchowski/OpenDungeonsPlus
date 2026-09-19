<#
.SYNOPSIS
Build and run the isolated creature portrait exporter using the game's renderer.
#>
[CmdletBinding()]
param(
    [string]$DependencyPrefix,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$taskProjectRoot = (Resolve-Path -LiteralPath "$PSScriptRoot\..\..").Path
$taskCurrentPath = $env:Path
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $taskCurrentPath, 'Process')
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    . "$PSScriptRoot\Enter-OpenDungeonsPlus.ps1"
}
if (-not $DependencyPrefix) {
    $DependencyPrefix = $env:CEGUI_HOME
}
if (-not $DependencyPrefix) {
    throw 'Provide DependencyPrefix or load the project development environment first.'
}
$taskPrefix = (Resolve-Path -LiteralPath $DependencyPrefix).Path
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $taskProjectRoot 'build\portrait-export'
}
$taskOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
foreach ($taskRequired in @('bin\RenderSystem_GL3Plus.dll', 'bin\Codec_STBI.dll',
        'include\OGRE\Ogre.h', 'include\cegui-0\CEGUI\CEGUI.h', 'lib\OgreMain.lib')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskPrefix $taskRequired))) {
        throw "Missing portrait exporter prerequisite: $taskRequired"
    }
}
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskOriginalPath = $env:Path
Push-Location -LiteralPath $taskOutput
try {
    # Normalize the environment key for hidden child processes on Windows.
    [Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
    [Environment]::SetEnvironmentVariable('Path', "$taskPrefix\bin;$taskPrefix\lib;$taskOriginalPath", 'Process')
    & cl.exe /nologo /EHsc /MD /std:c++14 "/I$taskPrefix\include\OGRE" `
        "/I$taskPrefix\include\OGRE\RTShaderSystem" "/I$taskPrefix\include\cegui-0" `
        "/I$taskProjectRoot\source" "$taskProjectRoot\tools\portraits\portrait-export.cpp" `
        "$taskProjectRoot\source\render\CreaturePortrait.cpp" /Feportrait-export.exe `
        /link "/LIBPATH:$taskPrefix\lib" OgreMain.lib CEGUIBase-0.lib `
        CEGUIOgreRenderer-0.lib OgreRTShaderSystem.lib OgreBites.lib *> portrait-export-build.log
    if ($LASTEXITCODE -ne 0) {
        Get-Content portrait-export-build.log -Tail 25
        throw 'Portrait exporter compilation failed.'
    }
    $taskArguments = @($taskProjectRoot, $taskPrefix, $taskOutput) | ForEach-Object { '"' + $_ + '"' }
    $taskProcess = Start-Process -FilePath (Join-Path $taskOutput 'portrait-export.exe') `
        -ArgumentList $taskArguments -WorkingDirectory $taskOutput -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $taskOutput 'portrait-export-results.log') `
        -RedirectStandardError (Join-Path $taskOutput 'portrait-export-stderr.log')
    if ($taskProcess.ExitCode -ne 0) {
        Get-Content portrait-export-stderr.log -Tail 15
        throw "Portrait export failed: $($taskProcess.ExitCode)"
    }
    Select-String -LiteralPath 'portrait-export-results.log' -Pattern '^PORTRAITS='
    Write-Output "Portraits and logs: $taskOutput"
}
finally {
    [Environment]::SetEnvironmentVariable('Path', $taskOriginalPath, 'Process')
    Pop-Location
}
