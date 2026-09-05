# GUI scaling

## Policy

The interface combines automatic resolution scaling with a user-selected scale.

- Window geometry, skin graphics and hit targets use a 1024 x 768 design area.
- The automatic factor uses the shorter display axis so the interface keeps its
  proportions and remains inside the window on wide or tall aspect ratios.
- Fonts use the existing 800 x 600 font reference. This preserves the current
  text-to-control ratio while making the 10-pixel base fonts approximately 18
  pixels high at 1920 x 1080 and 36 pixels high at 3840 x 2160.
- The Video settings page provides 80%, 90%, 100%, 110% and 120% presets.
- Selecting a preset previews it immediately. Apply saves it; Cancel restores
  the saved value.
- The saved value is stored as `UI Scale` in the `[Game]` section of the user
  configuration. The default is 100%.

The scale is recalculated whenever CEGUI receives a display-size change, so
resolution and fullscreen changes do not require a restart.

## Implementation

`Gui` records the original area, minimum size, maximum size and tab height of
each non-generated CEGUI window. It reapplies those original values with the
current automatic and user factors after a display-size or UI-scale change.
Scaling from the original values prevents rounding errors from accumulating.

Common OpenDungeons fonts and skin images use CEGUI's shortest-axis automatic
scaling. Mouse coordinates remain in display pixels; scaling the actual CEGUI
window rectangles keeps visual controls and hit targets aligned without a
separate pointer transform.

Explicit image sizes embedded in formatted labels scale with their containing
windows, so tab and submenu icons do not remain fixed at 28 or 32 pixels on
high-resolution displays.

The top status bar keeps its existing left and right anchors while its pixel
metrics scale. The bottom room and spell actions use two compact rows; the wave
portal joins the room grid so the final action remains visible at the smallest
supported window size and 120% UI scale. The main-menu root uses the complete
display area so its final button remains inside the clipping area at the
high-resolution 120% limit.

Layouts loaded during `Gui` construction are registered automatically. Code
that creates windows later must register their containing tree after assigning
the final areas:

```cpp
getModeManager().getGui().registerWindowHierarchy(parentWindow);
```

Destroyed windows are removed from the scale registry through the CEGUI window
manager event.

## Automated verification

The Windows Release target compiled successfully after the scaling changes were
applied to the user's latest complete fork state. That baseline already includes
the verified Windows build, runtime, live-settings and edge-scrolling changes.

Before the branch ancestry was corrected, an incremental Windows binary crashed
before the main menu because stale
MSVC object files disagreed about the `ModeManager` class layout: the constructor
stored `mGui` at offset `0x250`, while an old inline accessor read offset `0x260`.
That upstream-only test binary is not validation for this corrected branch. A
clean Release rebuild of the corrected fork-based branch regenerated every
object file and completed successfully. After switching branches that change C++
class layouts, rebuild this target with `--clean-first` before runtime testing.

The corrected branch's clean build regenerated `resources.cfg` with nonexistent
OGRE media paths. The user's 23:21:36 startup on September 5, 2026 then failed
while initializing CEGUI because `OgreUnifiedShader.h` could not be opened in
`OgreInternal`. Running the existing Windows runtime preparation script restored
the installed media paths; the executable was not changed. All configured
resource directories exist, and the existing isolated OGRE resource probe passes.
The user's subsequent run reached the main-menu scene at 23:27:27 and shut down
normally at 23:28:32; the user confirmed startup and reported the settings defects
described below. See the
[failure record](WINDOWS-STARTUP-FIXES.md#resource-path-regression-after-the-gui-scaling-clean-build)
and the updated [Release build commands](BUILDING.md#3-build-the-game).

All 39 CEGUI layout files parse as XML. Static bounds checks cover the main
menu, top HUD, bottom action tabs, Settings and Skill Tree at 800 x 660,
1920 x 1080 and 3840 x 2160 with 80%, 100% and 120% UI scale; all nine
size-and-scale combinations fit their outer available areas. These checks did not
verify nested font metrics, input handling or renderer clipping and therefore did
not detect the user's subsequent settings failures.

## Settings geometry and clipping correction

On September 5, 2026, the user reported shifted settings controls, mouse interaction
that did not follow the displayed interface and a covered confirmation button.
The source trace and an isolated CEGUI probe identified these causes:

- `Gui::applyScale` resized controls before updating the fonts and skin images.
  The combobox skin derives its edit field and arrow dimensions from the font.
  At 3440 x 1440, the field stayed 58 pixels high when changing to 120% instead
  of growing to 70 pixels; switching to 80% then left it 70 pixels high instead
  of 47. Updating font and image metrics before applying window geometry fixes
  this one-change lag, including the generated child controls and their targets.
- Fixed settings labels were shorter than their scaled fonts, the title and tab
  strip overlapped, the dynamic-shadow checkbox extended past the page, and
  dynamically generated renderer option fields overlapped the following row.
  The settings layout now provides sufficient text height and row spacing,
  including the nickname field's existing padding, and keeps the title, tabs,
  page content and footer separate. No settings values or behavior were removed.
- The installed CEGUI Ogre renderer explicitly disabled scissor clipping for
  every batch. CEGUI still generated vertices outside the scrollable page while
  hit testing correctly clipped those controls. At 100%, one off-page option
  had geometry from y=1076 to y=1134 over the Apply button at y=1086 to y=1142,
  despite having an empty clip rectangle. This explains the visible control
  covering a different clickable target. The maintained
  [CEGUI clipping patch](../../scripts/win32/patches/cegui-ogre-clipping.patch)
  restores each batch's clipping flag; the existing prerequisite installer
  applies it before building the library.

### Verification and limits

The local probe uses the installed CEGUI library and its NullRenderer, without a
game window or GPU renderer. Its generator takes the actual scaling methods,
settings layout and dynamic option geometry from the selected repository state;
it substitutes only the game/renderer initialization and a representative set of
ten two-choice renderer options. The comparison against commit `1822217e` fails;
the corrected state passes with no failed assertions.

It covers 3440 x 1440, 800 x 660, 1920 x 1080 and 3840 x 2160, each with the
sequence 100% -> 120% -> 80% -> 100%. Checks cover all four settings pages,
font/field heights, renderer option row separation, title/tab and page/footer
bounds, absence of unnecessary horizontal scrolling, scrollbar and slider
tracking, dragging the dialog, and Apply/UI-scale hit targets after dragging.
The separate geometry capture proves that scissor clipping is required; this
NullRenderer test does not verify the patched OpenGL output on the GPU.

Evidence under `build/windows`:

- `generate-settings-scale-probe.py` and `build-settings-scale-probe.ps1`: the
  reproducible isolated probe; pass `--baseline` to the build script for the
  comparison with commit `1822217e`.
- `settings-scale-geometry-before.log`: generated vertices beyond page clipping.
- `settings-scale-regression-before.log` and `settings-scale-regression-after.log`:
  failing and passing assertions, respectively.
- `game-Release-settings-scaling-fix.log`: successful Release rebuild; the EXE
  was updated at 23:39:49 on September 5, 2026.

The patched CEGUI library built and installed successfully in Release and Debug.
Runtime preparation then staged the Release DLL beside the executable; its
SHA-256 matches the installed source. Library logs are in `od-deps/logs` as
`cegui-Release.log` and `cegui-Debug.log`. The Debug game executable was not
rebuilt for these GUI corrections.

The user subsequently confirmed that the reported settings problems are fixed
and requested a commit. The updated game's 23:47:28 run loaded the main-menu
scene, entered a game map at 23:47:47 and followed the normal shutdown path at
23:48:31. That log is preserved as
`build/windows/settings-scaling-user-confirmed-game.log`. This records acceptance
of the reported settings corrections; the complete menu/HUD/dialog matrix below
has not been confirmed individually.

The game version remains 0.7.1 because this is an unreleased correction. The root
README already links the maintained development, build and startup guides; no
additional README change or separate release changelog entry is needed.

## Manual verification

Run the following checks at 80%, 100% and 120% UI scale for each representative
window size:

| Size | Purpose |
| --- | --- |
| 800 x 660 | Small supported window |
| 1920 x 1080 | Full HD |
| 3840 x 2160 | High-resolution display |

At every size, verify:

1. Open every main-menu page.
2. Open every game HUD tab and click the first and last action in each tab.
3. Check every Settings page, including dropdown lists and tooltips.
4. Open Help, Objectives, the Skill Tree and every exit dialog.
5. Change the resolution while the Settings window is open, then repeat one
   click near each window edge.
6. Confirm that text is readable and that no control is clipped, overlaps
   another control, misses clicks or has an offset pointer target.
