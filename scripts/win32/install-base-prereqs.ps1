param([string[]]$Only)

$ErrorActionPreference = 'Stop'
$taskRoot = 'C:\Users\mario\od-deps'
$taskCmake = "$taskRoot\tools\cmake-3.31.8-windows-x86_64\bin\cmake.exe"
$taskPrefix = "$taskRoot\install"

$taskPackages = @(
    @{ Name = 'expat'; Source = 'expat/expat'; Options = @('-DEXPAT_BUILD_TOOLS=OFF', '-DEXPAT_BUILD_EXAMPLES=OFF', '-DEXPAT_BUILD_TESTS=OFF', '-DEXPAT_BUILD_DOCS=OFF', '-DEXPAT_SHARED_LIBS=ON') },
    @{ Name = 'ois'; Source = 'ois'; Options = @('-DOIS_BUILD_DEMOS=OFF', '-DOIS_BUILD_SHARED_LIBS=ON') },
    @{ Name = 'sfml'; Source = 'sfml'; Options = @('-DSFML_BUILD_EXAMPLES=OFF', '-DSFML_BUILD_DOC=OFF') },
    @{ Name = 'freetype'; Source = 'freetype'; Options = @('-DFT_DISABLE_ZLIB=ON', '-DFT_DISABLE_BZIP2=ON', '-DFT_DISABLE_PNG=ON', '-DFT_DISABLE_HARFBUZZ=ON', '-DFT_DISABLE_BROTLI=ON') },
    @{ Name = 'pybind11'; Source = 'pybind11'; Options = @('-DPYBIND11_TEST=OFF', '-DPYBIND11_INSTALL=ON', '-DPYTHON_EXECUTABLE=C:/Users/mario/AppData/Local/Programs/Python/Python310/python.exe') },
    @{ Name = 'pcre'; Source = 'pcre-8.45'; Options = @('-DPCRE_BUILD_TESTS=OFF', '-DPCRE_BUILD_PCREGREP=OFF', '-DPCRE_BUILD_PCRECPP=OFF', '-DPCRE_SUPPORT_UTF=ON', '-DPCRE_SUPPORT_UNICODE_PROPERTIES=ON') }
)

foreach ($taskPackage in $taskPackages) {
    if ($Only -and $taskPackage.Name -notin $Only) { continue }
    $taskName = $taskPackage.Name
    $taskBuild = "$taskRoot\build\$taskName"
    $taskConfigureLog = "$taskRoot\logs\$taskName-configure.log"
    $taskOptions = @('-S', "$taskRoot\src\$($taskPackage.Source)", '-B', $taskBuild,
        '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_INSTALL_PREFIX=$taskPrefix",
        "-DCMAKE_PREFIX_PATH=$taskPrefix", '-DBUILD_SHARED_LIBS=ON', '-DCMAKE_DEBUG_POSTFIX=_d') + $taskPackage.Options
    Write-Output "Configuring $taskName"
    $ErrorActionPreference = 'Continue'
    & $taskCmake @taskOptions *> $taskConfigureLog
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $taskConfigureLog -Tail 45; throw "$taskName configuration failed" }
    foreach ($taskConfiguration in @('Release', 'Debug')) {
        $taskBuildLog = "$taskRoot\logs\$taskName-$taskConfiguration.log"
        Write-Output "Building and installing $taskName ($taskConfiguration)"
        $ErrorActionPreference = 'Continue'
        & $taskCmake --build $taskBuild --config $taskConfiguration --target INSTALL --parallel 8 *> $taskBuildLog
        $ErrorActionPreference = 'Stop'
        if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $taskBuildLog -Tail 45; throw "$taskName $taskConfiguration build failed" }
    }
    Write-Output "Installed $taskName"
}
