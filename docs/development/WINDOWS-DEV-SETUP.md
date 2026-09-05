# Windows development environment

As of September 5, 2026, Mario's local computer, Windows x64.
The prerequisites are installed; their library builds and the game's
CMake configuration succeeded.
The Release and Debug game builds succeeded after four targeted fixes.
The Release runtime files are now staged for testing the executable directly
from File Explorer. The user's startup attempts exposed incorrect handling of
absolute Windows resource paths; both builds now include the correction.
A subsequent Release run loaded the main-menu scene and shut down normally
without the earlier errors; the user confirmed that direct Release startup works
without errors. Broader gameplay tests and packaging remain unverified.

## Entry point and file responsibilities

This documentation and the scripts under [scripts/win32](../../scripts/win32)
are the maintained connection between the project and the external installation.
Use [BUILDING.md](BUILDING.md) for daily work;
sources and rebuilding are covered in [WINDOWS-PREREQUISITES.md](WINDOWS-PREREQUISITES.md).
[AGENTS.md](../../AGENTS.md) directs new agent sessions to these files.

The large downloads, library sources and compiled files are stored outside
the Git repository so they can be reused independently of the work branch.
The scripts deliberately contain the specific paths of this local installation;
on another computer, these paths must be checked and adjusted.
VS Code is the editor; the compiler, SDK and libraries are also required.

## Locations

| Purpose | Actual path |
| --- | --- |
| Project | `C:\Users\mario\GitHub\OpenDungeonsPlus` |
| External development directory | `C:\Users\mario\od-deps` |
| Library installation prefix | `C:\Users\mario\od-deps\install` |
| Headers / link libraries / DLLs | Under `install\include`, `install\lib`, `install\bin`; some DLLs are also under `install\lib` |
| Library sources | `C:\Users\mario\od-deps\src` |
| Library builds | `C:\Users\mario\od-deps\build` |
| Archives and checksums | `C:\Users\mario\od-deps\downloads` |
| Configuration and library logs | `C:\Users\mario\od-deps\logs` |
| CMake | `C:\Users\mario\od-deps\tools\cmake-3.31.8-windows-x86_64\bin\cmake.exe` |
| Visual Studio Build Tools | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` |
| Python | `C:\Users\mario\AppData\Local\Programs\Python\Python310` |
| 7-Zip for source archives | `C:\Users\mario\scoop\shims\7z.exe` |
| Generated game build | `C:\Users\mario\GitHub\OpenDungeonsPlus\build\windows` |
| Intended game installation directory | `C:\Users\mario\GitHub\OpenDungeonsPlus\build\windows\install` (not yet installed) |

Copies from the installation still exist under `od-deps\setup-scripts`,
`od-deps\Enter-OpenDungeonsPlus.ps1`, `od-deps\INSTALLATION.md` and the ignored
project directory `build`; use the maintained project files for further work,
as the old copies are not updated automatically.
A new clone contains the instructions and scripts, but not the external installation.

## Installed state

| Component | Version / configuration |
| --- | --- |
| Visual Studio 2022 Build Tools | 17.14.39, MSVC toolset directory 14.44.35207, compiler detected by CMake 19.44.35228.0, target and host x64 |
| Windows SDK | 10.0.26100.0 |
| CMake | 3.31.8, portable installation |
| Python | 3.10.11 with headers, Release and Debug import libraries and debug binaries |
| OGRE | 13.6.5, GL3Plus; OgreMain, Bites, Overlay, RTShaderSystem, Octree, ParticleFX, STBI |
| CEGUI | 0.9999.0 from `tomluchowski/cegui`, tag `scissors_test_disabled`, with OgreRenderer, Expat and the saved MSVC compatibility patch |
| OIS | 1.5.1 |
| SFML | 2.5.1 |
| Boost | 1.82.0; filesystem, locale, program_options, thread, system, chrono, date_time, atomic |
| pybind11 | 2.10.4 |
| FreeType | 2.12.1 |
| Expat | 2.8.4 |
| PCRE | 8.45 with UTF and Unicode properties |

The compiled libraries are available for x64 in Release and Debug;
Boost is also available in static and shared variants, each using the dynamic C++ runtime.
pybind11 provides headers and CMake configuration.
Git, VS Code, 7-Zip and a suitable Visual C++ runtime were already present.

OGRE, CEGUI, OIS, SFML and Python 3.10 follow the existing
[Snap build recipe](../../snap/snapcraft.yaml); the specific Windows options are
in the scripts. This documents OGRE 13.6.5 and does not establish an Ogre 14 port.
The older general information in README and AppVeyor does not supersede this local state.

## How the project and external installation connect

[Enter-OpenDungeonsPlus.ps1](../../scripts/win32/Enter-OpenDungeonsPlus.ps1)
loads the Visual Studio development environment for x64 and sets the following
in the current PowerShell session:

- `PATH` for CMake, library DLLs and Python.
- `CMAKE_PREFIX_PATH`, `CEGUI_HOME` and `OIS_HOME` to `od-deps\install`.
- `BOOST_ROOT`, `BOOST_INCLUDEDIR`, `BOOST_LIBRARYDIR` to the installed Boost files.
- `LIB` additionally to `od-deps\install\lib` for MSVC's automatic linking.

CMake was also added to the user PATH during installation;
still load the environment helper to ensure an unambiguous build environment.
Compiler initialization uses
`Common7\Tools\Microsoft.VisualStudio.DevShell.dll` under the Build Tools;
Boost additionally uses `VC\Auxiliary\Build\vcvarsall.bat`.
The compiler is located there under
`VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe`.

[configure-windows-prereqs.ps1](../../scripts/win32/configure-windows-prereqs.ps1)
determines the project root relative to its own location, loads the
adjacent environment helper and configures `build\windows` with Visual Studio 2022/x64.
It explicitly passes the Python executable, headers and both import libraries
and disables the test targets. During the original setup, the
game sources and `CMakeLists.txt` were not changed; the subsequent fixes
from the actual compilation are recorded in the [build error log](WINDOWS-BUILD-FIXES.md).

## Verified and still pending

| Step | Status / evidence |
| --- | --- |
| Load compiler, CMake and Python | Successfully verified using the environment helper |
| Build and install libraries | Release and Debug successful; CEGUI subsequently rebuilt and installed with the saved MSVC patch; logs under `od-deps\logs` |
| Configure the game | Successfully reconfigured with the build fixes; `opendungeons-configure.log` ends with successful configuration and generation |
| Configuration using the scripts added to the project | Successfully rerun, also from `docs\development`; the project path and adjacent environment helper are resolved correctly |
| Compile the game | Release and Debug successful with exit code 0; commands in [BUILDING.md](BUILDING.md), evidence in the [build error log](WINDOWS-BUILD-FIXES.md) |
| Check generated programs | Both files have the AMD64 PE signature; Release imports only `python310.dll`, Debug only `python310_d.dll` as the Python runtime |
| Prepare direct Release startup | 22 DLLs staged beside the executable, Python module paths recorded locally and OGRE media paths corrected; static checks of 23 PE files, four plugins and 14 resource directories passed; see [BUILDING.md](BUILDING.md) and `build\windows\runtime-validation.json` |
| Start the Release game directly | User confirmed error-free startup after the path fix; the 14:07 logs show main-menu scene loading and normal shutdown without the earlier loading errors; see [startup fixes](WINDOWS-STARTUP-FIXES.md) |
| Test gameplay, GUI and cursor | Broader manual gameplay and visual QA remain with the user |
| Create an installation package | Not yet verified; older packaging logic cannot find some expected Boost DLL names |

CMake found Python, pybind11, OIS, OGRE, CEGUI including OgreRenderer, SFML and the
required Boost link libraries. The message
`DEPS_BOOST_FILESYSTEM_BIN_REL-NOTFOUND` concerns the separate DLL search for
packaging; do not infer either a failed link test or a working
installation package from it.
The reconfiguration also reports `Boost toolset is unknown` for
MSVC 19.44.35228.0; generation and both linking steps still succeed
after correcting the Boost search path described in the
[build error log](WINDOWS-BUILD-FIXES.md).

The build errors are fixed; existing compiler/linker warnings are recorded in the
build error log. The configuration script now also runs
[prepare-windows-runtime.ps1](../../scripts/win32/prepare-windows-runtime.ps1)
to stage the local Release runtime without requiring a prepared shell for startup.
Debug runtime staging is still pending; its generated plugin list also mixes
Debug plugins with Release variants of Codec_STBI and RenderSystem_GL3Plus.
Direct Release startup is verified by the user's confirmation and runtime logs;
broader gameplay tests remain with the user.
