# Creature portrait export

This maintained tool exports the game's existing creature portraits to PNG files
for asset work and visual comparison. It reuses
[`CreaturePortrait.cpp`](../../source/render/CreaturePortrait.cpp), including its
framing, clipping correction, cache and cleanup, instead of copying that renderer.
It discovers unique mesh names from `config/creatures.cfg`.

These are model-rendered reference images, not newly illustrated artwork.
The tool does not change the game's portrait assets or runtime behavior.

## Windows prerequisites

Use the Windows build prerequisites: MSVC x64 and the project's installed OGRE/CEGUI libraries, render plugins, codecs
and media. No extra packages or game installation are required. The wrapper
loads the repository environment helper when MSVC is not already available.
On another workstation, load your own x64 compiler environment first and pass
your dependency installation prefix explicitly.

## Build and export

From the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/win32/export-creature-portraits.ps1
```

The default output is `build/portrait-export/`. To select the installed libraries
and an output folder:

```powershell
./scripts/win32/export-creature-portraits.ps1 -DependencyPrefix 'C:/dev/od-deps/install' -OutputDirectory 'build/portrait previews'
```

The C++ entry point and build wrapper are tracked source; generated executables,
object files, logs and PNGs are build outputs. Repeating the command regenerates
files with the same names in the selected output directory. Existing local
preview scripts are not needed. The export uses a hidden rendering window and
does not launch the game or access saved games or user settings.

Each configured mesh produces `portrait-<mesh-name>.png`; the existing Kobold and
Orc GUI composition checks additionally produce `portrait-gui-<mesh-name>.png`.
Compilation output, renderer messages and results use the `portrait-export-`
prefix. Successful runs report `PORTRAITS=<count>` and return zero.

## Verification

The exporter checks nonempty rendered pixels, cache reuse, preservation of world
shadow parameters, and cleanup of temporary scenes, materials, viewports and
textures. A missing configuration, empty mesh set or rendering failure returns
an error. The wrapper reports compiler/export failures and preserves their logs.

The Windows build and hidden export passed for all 33 configured meshes on
September 7, 2026, including an output path containing spaces. These checks
validate the existing export path; artistic acceptance of future illustrated
portraits remains separate. Linux and other render plugins are not validated
by this Windows wrapper.
