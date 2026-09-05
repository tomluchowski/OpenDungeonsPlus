# Windows startup failures and verification

As of September 5, 2026, the user confirmed that the Release executable starts
directly without errors. This result is supported by the runtime evidence below,
in addition to successful builds and static DLL checks.
See [BUILDING.md](BUILDING.md) for the executable and runtime setup.

## Runtime files and generated resource configuration

The initial build directory contained no dependency DLLs, while `plugins.cfg`
loads OGRE plugins from that directory. The maintained
[runtime preparation script](../../scripts/win32/prepare-windows-runtime.ps1)
now stages the Release DLLs, including the dynamically loaded OGRE and CEGUI
modules, and configures the existing Python installation's module paths.

The generated `resources.cfg` initially pointed at
`build/windows/install/share/OGRE/Media`, although the installed Windows OGRE
media is under `C:\Users\mario\od-deps\install\Media`. The script corrects those
paths and excludes the shader subdirectories absent from this OGRE installation.
Both standalone preparation and preparation through the configuration script
completed successfully; the copied DLL hashes match their installed sources.

The static report `build/windows/runtime-validation.json` checked the Release
executable and 22 DLLs, four OGRE plugins and 14 resource directories. It found
no missing imported DLLs or architecture mismatches, but did not execute the
resource-loading code and therefore did not detect the path error below.

## Absolute Windows resource paths were prefixed with the game directory

The user's 14:00 startup reached OGRE and its OpenGL 3+ renderer, but reported
that `OgreUnifiedShader.h` could not be found in the Graphics resource group.
Cloud2, DirtInstanced, Fog and ReflMetal shader/material errors followed.

The actual paths in both the game and OGRE logs were malformed, for example:

```text
./C:/Users/mario/od-deps/install/Media/Main
```

In [ResourceManager.cpp](../../source/utils/ResourceManager.cpp),
`setupOgreResources` treated a path as absolute only if it began with `/`.
It therefore prefixed valid drive-qualified Windows paths with `./`, even though
the configured directories and `OgreUnifiedShader.h` existed on disk.
The condition now uses the existing Boost filesystem path API's `is_absolute()`
check, preserving absolute paths and continuing to prefix relative game paths.

The original runtime evidence is retained locally in:

- `build/windows/startup-before-path-fix-Ogre.log`.
- `build/windows/startup-before-path-fix-game.log`.

Both binaries were rebuilt successfully after this correction:

- `build/windows/game-Release-startup-path-fix.log`, exit code 0.
- `build/windows/game-Debug-startup-path-fix.log`, exit code 0.

The Release executable was replaced with the corrected build. A subsequent run
from 14:07:36 to 14:07:52 registered the absolute OGRE media paths correctly,
initialized CEGUI and its skin, loaded the main-menu scene, rendered its materials
and followed the normal shutdown path. The earlier missing-resource and material
errors are absent from the new OGRE, game and CEGUI logs.
The game reached the normal post-render-loop disconnect and shutdown statements
in `ODApplication.cpp`; these are not reached by an exception unwinding that loop.

The preserved post-fix evidence is:

- `build/windows/startup-after-path-fix-Ogre.log`.
- `build/windows/startup-after-path-fix-game.log`.
- `build/windows/startup-after-path-fix-CEGUI.log`.
- `build/windows/startup-path-fix-validation.json`, including the checked binary's SHA-256.

These logs verify the resource-path fix during an actual startup. Existing shader
extension, shader-version and deprecated material-syntax warnings remain; no
warnings were disabled. The user subsequently confirmed error-free startup on
September 5, 2026, completing the verification of the direct Release startup.

## Build process note

The sandboxed process environment supplied both `Path` and `PATH`, causing the
Visual Studio environment helper and MSBuild to reject the duplicate dictionary
key before compilation. Running the same build in the approved normal process
environment succeeded; no system environment setting was changed.

## Verified outcome and remaining scope

The direct Release startup objective is complete: the user confirmed error-free
startup and the runtime logs verify that the earlier loading failures are gone.
Broader gameplay and visual QA remain with the user; Debug runtime staging and
packaging remain separate, unverified work. The game version remains 0.7.1;
no release has been published.
