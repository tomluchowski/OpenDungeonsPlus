# Windows build: errors and fixes

As of September 5, 2026. The errors were investigated during the actual build with
Visual Studio 2022, MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 and x64.
The libraries used and the local environment are documented in
[WINDOWS-DEV-SETUP.md](WINDOWS-DEV-SETUP.md).

## Window icon: unsuitable 32-bit interface

The first Release build failed in [ODApplication.cpp](../../source/ODApplication.cpp)
when setting the window icon:

```text
error C2065: GCL_HICON: undeclared identifier
warning C4311 / C4302: pointer truncation from HICON to LONG
```

The affected Windows code used `SetClassLong`, `GCL_HICON` and a
cast of the icon handle to `LONG`. The installed Windows SDK header
`um/WinUser.h` explicitly removes `GCL_HICON` when `_WIN64` is defined;
in addition, `LONG` is not wide enough for a 64-bit pointer.

The fix uses `SetClassLongPtr`, `GCLP_HICON`
and `LONG_PTR` at the same location. The existing icon and the timing of its assignment
are preserved; the change remains exclusively in the Windows code path.

Evidence of the original error: `build/windows/game-Release-initial.log`.
The original build output is retained locally for traceability.

## CEGUI macro changes Boost headers

The same build reported `C2039: _snprintf is not a member of std`, including
when compiling `ODClient.cpp`, `SettingsWindow.cpp` and `ModeManager.cpp`.
The error message points to `boost/assert/source_location.hpp`;
there, Boost correctly uses `std::snprintf` on current MSVC versions.

The cause is in `CEGUI/PropertyHelper.h` from the CEGUI tag in use, which was included
earlier: a macro defined for all MSVC versions replaces `snprintf` with
`_snprintf`, also changing the subsequent Boost code to `std::_snprintf`.
The project itself does not define this macro.

The [CEGUI patch](../../scripts/win32/patches/cegui-msvc-snprintf.patch) limits
this replacement to `_MSC_VER < 1900`; for newer compilers, the existing
standard function remains usable. The
[installation script](../../scripts/win32/install-cegui-prereq.ps1) applies the
patch before the CEGUI build and recognizes a patch that has already been applied.
CEGUI was subsequently rebuilt and installed successfully in Release and Debug;
the installed header has been compared with the patched source using SHA-256.
Detection of the already-applied patch was successfully verified with
`git apply --reverse --check`.
Logs: `build/windows/cegui-rebuild-driver.log` in the project and
`cegui-Release.log` / `cegui-Debug.log` in the external library log directory.

## Boost linker path points to the header directory

After the two compiler fixes, Release reached the linker and failed
with `LNK1104` for `libboost_filesystem-vc143-mt-x64-1_82.lib`.
The matching file was installed and had already been found correctly in the CMake cache.
However, the generated Visual Studio project contained the additional search path
`include/boost-1_82/stage/lib`, where this library is not located.

The cause in [CMakeLists.txt](../../CMakeLists.txt) was deriving the
MSVC linker path from `Boost_INCLUDE_DIRS`. It now uses
`Boost_LIBRARY_DIRS`, determined by the package search, instead; this means the
fix does not require a computer-specific path and preserves the existing automatic
Boost linking under MSVC.
Evidence of the linker error: `build/windows/game-Release-pass1.log`.

## Python debug library and header selection conflict

The full Debug compiler run succeeded, but the linker aborted
with `LNK1104` for `python310.lib`. CMake had already added the existing
`python310_d.lib` as an explicit Debug dependency.

When including Python under MSVC, `pybind11/detail/common.h` temporarily
undefines the `_DEBUG` macro unless `Py_DEBUG` is defined.
As a result, `Python/include/pyconfig.h` generates an additional automatic
link request for `python310.lib`, even though CMake selects the debug library.

If a Python debug library is configured and pybind11 has not already
activated debug mode, the game target under MSVC therefore defines
`Py_DEBUG` exclusively for the Debug configuration.
The empty macro value matches the definition in the Windows Python header;
Release and configurations without a detected debug library remain unchanged.
This selects headers and the library variant consistently instead of allowing
both variants to be used simultaneously through an additional search path.
Evidence: `build/windows/game-Debug-pass2.log`.

## Verification

After reconfiguring, the generated Visual Studio project contains the
actual Boost library directory for Release and Debug.

| Check | Result / evidence |
| --- | --- |
| CEGUI with patch, Release and Debug | Successfully built and installed; driver log `build/windows/cegui-rebuild-driver.log` |
| Release game build after all four fixes | Exit code 0, `build/windows/game-Release-pass3.log`, output `build/windows/opendungeons-plus.exe` |
| Debug game build after all four fixes | Exit code 0, `build/windows/game-Debug-pass3.log`, output `build/windows/opendungeons-plus_d.exe` |
| Binaries | PE signature and AMD64 machine identifier checked for both generated programs |
| Python variants | Static import check: Release uses only `python310.dll`, Debug only `python310_d.dll` as the Python runtime; `build/windows/game-Release-dependencies.log` and `game-Debug-dependencies.log` |

Both final build logs contain no compiler or linker errors.
The earlier failed attempts are deliberately retained as separate logs.
The Python DLL check was performed with `dumpbin /dependents` on both programs;
no game instance was started.

The game builds are run using `cmake --build build/windows --config Release --parallel 4`
or the same command with `--config Debug`, after configuring
with the [Windows configuration script](../../scripts/win32/configure-windows-prereqs.ps1).
Game startup, manual QA and the visible appearance of the icon are
separate checks performed by the user.

The builds are not warning-free: among other things,
warnings about numeric conversions, the deprecated `/Gm` option and,
in the Release linking step, `LNK4075` for combining Edit-and-Continue with
linker optimization remain, as does the warning in the Debug linking step about
combining `/INCREMENTAL` and `/FORCE`. This work fixes the confirmed build failures;
it does not disable warnings.

## Version and scope

The game version remains 0.7.1; this work fixes build errors and does not
publish a new game version. The technical change is committed together with this
error log; local status and getting-started notes are updated in a
separate documentation commit.
