# Configuring and compiling on Windows

For the completed `feature/live-settings` work, see [LIVE-SETTINGS.md](LIVE-SETTINGS.md)
for its build and runtime evidence. The baseline startup verification below
predates those settings changes.

As of September 5, 2026. The [prerequisites](WINDOWS-DEV-SETUP.md) are installed
and CMake and the Windows x64 game builds in Release and Debug have
completed successfully; the user's startup attempts exposed a resource-path error,
which has been corrected and rebuilt; a subsequent run reached the main-menu scene
and shut down normally, and the user confirmed that Release starts without errors.
The four resolved build errors and their evidence are recorded in
[WINDOWS-BUILD-FIXES.md](WINDOWS-BUILD-FIXES.md).
The startup evidence and subsequent correction are recorded in
[WINDOWS-STARTUP-FIXES.md](WINDOWS-STARTUP-FIXES.md).

## 1. Prepare the PowerShell session

In a new PowerShell console:

```powershell
Set-Location -LiteralPath 'C:\Users\mario\GitHub\OpenDungeonsPlus'
. .\scripts\win32\Enter-OpenDungeonsPlus.ps1
```

The dot at the start loads the compiler and search paths into the same session;
then configure and build in this console. CMake 3.31.8,
Python 3.10.11 and the x64 compiler from Visual Studio 2022 Build Tools are expected.

Some automated shells provide both `PATH` and `Path`. MSBuild then fails before
starting `CL.exe` with `System.ArgumentException: An item with the same key has
already been added`. Normalize the process environment before loading the helper:

```powershell
$taskCurrentPath = $env:Path
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[System.Environment]::SetEnvironmentVariable('Path', $taskCurrentPath, 'Process')
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. .\scripts\win32\Enter-OpenDungeonsPlus.ps1
```

This changes only the current build process. A normal PowerShell console with one
path variable does not need this step.
If needed, check without building:

```powershell
Get-Command cl.exe, cmake.exe, python.exe | Select-Object Name, Source
cmake --version
python --version
```

## 2. Configure CMake

```powershell
& .\scripts\win32\configure-windows-prereqs.ps1
```

The script also loads the environment helper itself and uses:

- Source: project root, derived from the script path.
- Build directory: `build\windows`.
- Generator: `Visual Studio 17 2022`, architecture `x64`.
- `OD_BUILD_TESTING=OFF` and `BUILD_TESTING=OFF`.
- Installation target: `build\windows\install`.
- Python under `C:\Users\mario\AppData\Local\Programs\Python\Python310`:
  `python.exe`, `include`, `libs\python310.lib`, `libs\python310_d.lib`.

The configuration log is replaced on every invocation:
`C:\Users\mario\od-deps\logs\opendungeons-configure.log`.
On errors, the script prints the last lines and aborts.
Reconfigure after changes to CMake files or source lists;
for ordinary changes to existing C++ files, use the existing build.

## 3. Build the game

Release from the same prepared console:

```powershell
cmake --build .\build\windows --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Release game build failed' }
& .\scripts\win32\prepare-windows-runtime.ps1
```

After changing a class layout in a header, or after an interrupted rebuild, create
the next test executable with a clean build so that no object file can retain the
previous layout:

```powershell
cmake --build .\build\windows --config Release --target opendungeons-plus --clean-first --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Clean Release game build failed' }
& .\scripts\win32\prepare-windows-runtime.ps1
```

Keep the game and its error dialogs closed while preparing the runtime. CMake
can regenerate `resources.cfg` during a build and restore paths that do not exist
in this local Windows installation. Run runtime preparation after the successful
Release build, including a clean build, before handing the executable to the user.
The September 5 GUI-scaling startup failure from this omitted step is recorded in
[startup fixes](WINDOWS-STARTUP-FIXES.md#resource-path-regression-after-the-gui-scaling-clean-build).

For Debug instead:

```powershell
cmake --build .\build\windows --config Debug --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Debug game build failed' }
```

The successfully generated output files are
`build\windows\opendungeons-plus.exe` and `build\windows\opendungeons-plus_d.exe`,
directly in the build directory. Both files were checked for their AMD64 PE signature and
the corresponding Python DLL without starting the game.
The build output appears in the console; if an error occurs, record the first specific
compiler/linker message and the configuration used.
The logged successful verification runs for this setup are located
under `build\windows\game-Release-pass3.log` and `game-Debug-pass3.log`.

## 4. Direct Release startup and manual verification

For the current local setup, double-click
`C:\Users\mario\GitHub\OpenDungeonsPlus\build\windows\opendungeons-plus.exe`
in File Explorer; no PowerShell session is needed to test the Release build.
Keep the executable in that directory with its DLLs, configuration and resource links.
The files have been prepared and checked, and the executable includes the fix for
absolute Windows resource paths. The 14:07 startup logs confirm main-menu scene
loading and normal shutdown without the earlier loading errors; the user then
confirmed an error-free direct startup on September 5, 2026.
That confirmation predates the dynamic-shadow startup failure. The resource
template now also registers OGRE's `Media/Main` in `OgreInternal`, while retaining
its `Graphics` entry for game shader includes; the user's subsequent run reached
the main menu with shadows enabled, as recorded in
[startup fixes](WINDOWS-STARTUP-FIXES.md).
The corrected configuration has been generated beside the executable and passed
the isolated OGRE resource test; no C++ rebuild is needed for this template change.
The user's later Legacy test-map reproduction identified a fragment shader removed
by OGRE's automatic illumination splitting. RenderManager now selects integrated
additive texture shadows, preserving the existing custom shader passes. Release
and Debug rebuilt successfully; the isolated OGRE pass test reproduces the missing
fragment programs with splitting and retains the original pass without it.
The Release executable is ready for the user to retest the same map with shadows
enabled; gameplay and shadow appearance have not yet been verified after this fix.
Build logs: `game-Release-integrated-shadows.log` and
`game-Debug-integrated-shadows.log` under `build/windows`.

The configuration script now calls
[prepare-windows-runtime.ps1](../../scripts/win32/prepare-windows-runtime.ps1).
It copies 20 installed Release library/plugin DLLs and the two Python runtime DLLs
next to the executable, and replaces the generated Unix-style OGRE media paths
with the existing Windows installation's `Media/RTShaderLib`,
`Media/RTShaderLib/GLSL` and `Media/Main` directories.
The missing HLSL, HLSL_Cg and materials subdirectories are not registered.
The other game resource entries and their existing junctions are preserved.

The generated `python310._pth` points to the existing Python installation, its
`Lib` and `DLLs` directories and the executable directory, with `import site` enabled;
Python documents this application-local module path mechanism in
[Finding modules on Windows](https://docs.python.org/3.10/using/windows.html#finding-modules).
This is a local development setup: OGRE media and the Python standard library
still reside outside the repository, and the installed Visual C++ runtime is used.
It is not a standalone distribution package.

If dependencies change or CMake regenerates the resource configuration outside
the configuration script, refresh the prepared files with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\win32\prepare-windows-runtime.ps1
```

The execution-policy option applies only to that process; it does not change the
system's policy. The setup has already been performed for the current Release executable.

As of September 5, 2026, static checks covered the executable and 22 DLLs,
all four configured OGRE plugins and all 14 resource directories, with no missing
DLL dependencies or incorrect CPU architectures; evidence is stored in
`build\windows\runtime-validation.json`. These checks do not start the game.

Debug still requires the prepared development environment; its direct-start
runtime files have not been staged. An optional console startup from the loaded
environment is:

```powershell
Push-Location -LiteralPath .\build\windows
try {
    & .\opendungeons-plus_d.exe
} finally {
    Pop-Location
}
```

The Debug startup command has not been verified in practice; its generated
`plugins_d.cfg` still names the Release variants of Codec_STBI and RenderSystem_GL3Plus,
so Debug plugin selection needs correction before its startup can be considered ready.
If a startup error occurs, record the actual message for diagnosis.
The user performs manual game tests and visual acceptance.

## Logs and resuming work

- Current installation and verification status: [WINDOWS-DEV-SETUP.md](WINDOWS-DEV-SETUP.md).
- Library logs: `C:\Users\mario\od-deps\logs\<name>-configure.log`,
  `<name>-Release.log`, `<name>-Debug.log`; Boost uses
  `boost-bootstrap.log` and `boost-build.log`.
- Rebuild dependencies only when actually needed, following
  [WINDOWS-PREREQUISITES.md](WINDOWS-PREREQUISITES.md).

`build` is excluded from Git and contains generated files;
`build\windows` also contains directory junctions to project resources.
Take these junctions into account when cleaning up and do not delete source directories through them.
After a new result, add the date, configuration used, error or success
and remaining checks to the Windows status document.
