# Windows startup failures and verification

As of September 5, 2026, the user confirmed that the Release executable starts
directly without errors. This result is supported by the runtime evidence below,
in addition to successful builds and static DLL checks.
That confirmation preceded enabling dynamic shadows; the resulting startup
failure and its resource-group correction are documented below.
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

## Enabling dynamic shadows prevented subsequent startup

At 14:24:35 on September 5, 2026, the settings handler saved `Dynamic Shadows=Yes`
and explicitly called `exit(0)`. The preceding run reached the main menu at
3440 x 1440 fullscreen with shadows disabled. The subsequent attempts at 14:24:38,
14:24:41 and 14:27:21 failed before CEGUI initialization with shadows enabled.
The abrupt exit on saving is existing behavior in
[SettingsWindow.cpp](../../source/modes/SettingsWindow.cpp); the failure on the
next startup is a separate resource-loading bug.

[RenderManager.cpp](../../source/render/RenderManager.cpp) enables texture shadows
during construction. OGRE 13.6.5 then looks up its four shadow-extrusion programs
through `OgreInternal`. The installed library uses
`OGRE_RESOURCEMANAGER_STRICT=2`, but the generated configuration registered
`Media/Main` only in the isolated `Graphics` group. The files were parsed there,
yet the internal lookup could not find them. The DLL and shader files exist;
this failure does not require reinstalling dependencies.

The [resource template](../../cmake/config/resources.cfg.in) now also registers
the same `Media/Main` directory under `OgreInternal`. Its existing `Graphics`
registration is deliberately retained: game shaders include `OgreUnifiedShader.h`
using strict group-local file lookup. Moving the directory out of `Graphics`
would reintroduce missing-header errors. The two resource pools remain separate;
no dependency files, user settings or game C++ sources are changed.
The maintained Windows configuration script regenerates the template and resolves
both entries to the existing local OGRE installation.

The preserved evidence under `build/windows` is:

- `startup-shadow-setting-change-game.log`: successful startup with shadows off,
  followed by the explicit exit after saving the shadow setting.
- `startup-before-shadow-fix-Ogre.log` and `startup-before-shadow-fix-game.log`:
  the failed startup with shadows enabled.
- `shadow-resources-before.cfg`: the original resource-group assignment.
- `shadow-resource-probe.cpp`: an isolated check using the installed OGRE DLL and
  its own script parser, without creating a renderer or game window.
- `shadow-probe-before.log`: all four internal shadow lookups fail, exit code 1;
  the game shader header remains accessible.
- `shadow-probe-after.log`: all four internal shadow lookups and their source-file
  loads succeed, and the game shader header remains accessible, exit code 0.
- `shadow-resource-validation.json`: configuration, probe and unchanged executable
  evidence from before the user's subsequent startup attempt.

The probe checks resource registration, program lookup and source-file loading;
without a renderer, OGRE uses null shader programs, so it does not verify shader
compilation or rendering. The maintained Windows configuration script succeeded
and prepared the corrected configuration beside the existing Release executable.
The user configuration still enables dynamic shadows at 3440 x 1440 fullscreen.
The executable is unchanged, so no C++ rebuild is required for this template-only
fix. The game version remains 0.7.1; the root README already links to this startup
record, and no separate release changelog entry is needed for this unreleased fix.
The user's subsequent 14:52:23 run had shadows enabled and reached CEGUI and the
main-menu scene at 14:52:24, confirming that the blocking shadow-program lookup is
resolved during actual startup. A later failure while entering a level is tracked
separately below; shadow appearance and broader gameplay remain unverified.

## Legacy test-map exception after successful startup

The same user-launched run selected
`levels/skirmish/TestLegacyNoScripts.level` at 14:52:53. Client/server setup
completed, the client was accepted at 14:52:55 and the server started its first
turn at 14:52:58. The frame listener then began destruction without reaching the
normal post-render-loop `Disconnecting client...` message in `ODApplication.cpp`.
CEGUI and OGRE subsequently shut down. This identifies exception unwinding after
entering the level, but the existing logs do not contain the exception text.

The logs contain rejected legacy skills, missing mesh material assignments and
server-side attempts to rotate objects without rendering nodes. These messages
precede further execution and do not by themselves identify the fatal exception;
none has been changed speculatively.

The diagnostic gap is in the exception path: `startGame` owns the file logger,
which was destroyed before `main.cpp` displayed caught exceptions in a Windows
dialog. [ODApplication.cpp](../../source/ODApplication.cpp) now logs
`std::exception::what()` while that logger remains alive, then rethrows the same
exception to preserve the existing dialog and exit behavior. This covers OGRE,
CEGUI and standard C++ exceptions; it is diagnostic instrumentation, not a claim
that the gameplay failure is fixed.

Both diagnostic builds, Release and Debug, completed successfully with exit code 0:
`build/windows/game-Release-exception-diagnostics.log` and
`build/windows/game-Debug-exception-diagnostics.log`.
The pre-change logs are preserved as `build/windows/legacy-crash-before-Ogre.log`,
`legacy-crash-before-game.log` and `legacy-crash-before-CEGUI.log`.

### Captured cause and correction

The user's diagnostic run reached the main menu at 15:05:03 and selected the same
map at 15:05:11. At 15:05:19, the added logging captured an
`Ogre::InvalidStateException` before the dialog:

```text
RenderSystem does not support FixedFunction, but technique of 'DirtInstanced'
has no Fragment Shader. Use the RTSS or write custom shaders.
```

The exception originates in OGRE 13.6.5's `SceneManager::_setPass`,
`OgreMain/src/OgreSceneManager.cpp:935`, in the installed dependency source tree.
The captured logs are preserved under `build/windows` as
`legacy-crash-diagnostic-Ogre.log`, `legacy-crash-diagnostic-game.log` (line 897)
and `legacy-crash-diagnostic-CEGUI.log`.

The original [DirtInstanced material](../../materials/scripts/DirtInstanced.material)
already defines both a vertex and a fragment program. The missing program is on
a derived pass: the game enabled `SHADOWTYPE_TEXTURE_ADDITIVE`, causing OGRE to
split opaque material passes into ambient, per-light and decal stages. In
`OgreTechnique.cpp`, this automatic splitting clears fragment programs on derived
ambient and per-light passes. GL3Plus cannot draw these fixed-function passes.
The material is used by the instanced unrevealed ground tiles created when the
level starts, explaining why startup could succeed before entering the map.

[RenderManager.cpp](../../source/render/RenderManager.cpp) now uses
`SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED`. This keeps texture-shadow generation
enabled while retaining the existing custom lighting/shadow shader passes instead
of generating incompatible illumination passes. The game's shaders already
calculate lighting and sample shadow maps; this matches OGRE's documented
[integrated texture-shadow mode](https://ogrecave.github.io/ogre/api/13/_shadows.html#Integrated-Texture-Shadows).
The shadow toggle, material definitions and user configuration are unchanged.

### Verification

- The user's reproduction verified that exception logging records the actual
  fatal error before the existing Windows dialog.
- `build/windows/illumination-pass-probe.cpp` exercises the installed OGRE DLL's
  real material-pass splitting and render queue, without a renderer or game
  window. Its representative programmable pass has the same default lighting,
  ambient and diffuse state as DirtInstanced; it uses null shader programs and
  no textures. The queue flags follow `SceneManager::updateRenderQueueSplitOptions`.
- `illumination-pass-additive.log`: two queued passes, both missing their fragment
  programs, exit code 1.
- `illumination-pass-integrated.log`: one unchanged original pass with its fragment
  program, exit code 0. `illumination-pass-off.log` also passes, exit code 0.
- Release and Debug rebuilt successfully, exit code 0, with logs in
  `game-Release-integrated-shadows.log` and `game-Debug-integrated-shadows.log`.
  The Release executable was rebuilt at 15:17:22 and Debug at 15:17:29.
  Binary hashes and test scope are recorded in `integrated-shadow-validation.json`.

These checks verify the identified pass-generation failure and successful builds;
they do not compile the actual GLSL on the GPU or verify the rendered scene.
The user must launch the rebuilt Release executable and enter
`TestLegacyNoScripts.level` again with dynamic shadows enabled, then check both
stability and lighting/shadow appearance. This correction is ready for that test;
the actual gameplay result and shadow coverage remain unverified.

## Build process note

The sandboxed process environment supplied both `Path` and `PATH`, causing the
Visual Studio environment helper and MSBuild to reject the duplicate dictionary
key before compilation. Running the same build in the approved normal process
environment succeeded; no system environment setting was changed.

## Verified outcome and remaining scope

The earlier resource-path fix was verified by the user's error-free startup.
The later dynamic-shadow resource-group correction passed the isolated resource
check and the user's subsequent main-menu startup with shadows enabled.
The separate Legacy test-map exception was captured and traced to automatic
illumination-pass splitting; the integrated shadow-mode correction passed the
isolated OGRE pass test and both builds. Verification in the user's game session
is still pending.
Broader gameplay and visual QA remain with the user; Debug runtime staging and
packaging remain separate, unverified work. The game version remains 0.7.1;
no release has been published.
