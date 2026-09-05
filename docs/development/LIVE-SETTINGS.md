# Live settings and fullscreen navigation

Completed September 5, 2026, on `feature/live-settings`, stacked after the
Windows-support, dynamic-shadow and settings-option branches. Documentation
describes implementation and evidence separately.

## Requested behavior

All settings exposed by the settings window must take effect after Apply without
restarting the game. This includes resolution and fullscreen during a running
game. The in-game main navigation must remain visible at 3440 x 1440 fullscreen.
Preserve the running game and existing settings functionality.

## Findings and implementation

- The game creates its render window explicitly. The settings code used
  `getAutoCreatedWindow()`, which does not identify that window. It now obtains
  the window from the frame listener and notifies the camera, input and CEGUI
  after changing its size or fullscreen state.
- VSync was saved from the fullscreen checkbox. It now uses its own checkbox
  and applies VSync and its interval to the current window.
- Dynamic shadows can change the scene's shadow technique and the existing
  materials' `shadowingEnabled` parameter without exiting. This retains the
  integrated shadow technique required by the earlier shader fix.
- Mouse and keyboard capture changes recreate OIS devices before the next input
  capture, outside the Apply callback. Minimap selection recreates the minimap
  at the next frame; minimap click handling refreshes its screen position.
- Volume, ambient lighting and camera speed already preview their changes.
  Autoscroll and keeper voice read current configuration during operation.
- Opening settings reloads the current configuration into the controls.
- The keeper-hand projection cache now also tracks the camera aspect ratio so
  that resizing does not leave its position calculated for the old window shape.
- Resolution and fullscreen changes now resize or restyle the current render
  window directly. FSAA, gamma, colour depth, display frequency and other pixel
  format options create a hidden replacement render window after the current
  input capture has finished. The active camera viewport, CEGUI render target
  and OIS devices move to it before the old window is hidden. The original
  primary OpenGL window remains as a hidden context anchor; later replacement
  windows are explicitly destroyed.
- Replacement is transactional. The old window stays usable until the transfer
  succeeds. A creation or transfer failure restores the previous renderer values
  and persisted video configuration. Fullscreen windows are changed to windowed
  state before destruction so that their cleanup cannot reset the display mode
  selected for the replacement window.
- Live nickname changes use a separate request and server acknowledgement.
  The server derives the player from the connection and updates the same player
  object; it does not create a second player or restart the handshake. An
  optional capability byte in the existing nickname handshake prevents sending
  new messages to older peers. Existing message identifiers are preserved by
  appending the new enum entries. Replay playback does not send rename requests.
  Servers without this capability retain their existing session nickname; the
  new configured name remains available for a subsequent connection.

## Setting coverage

| Settings control | Apply behavior in this branch |
| --- | --- |
| Music Volume | Changes the active listener while the slider moves and persists on Apply. |
| Dynamic Shadows | Changes the active scene shadow technique and material shadow state on Apply. |
| Renderer | The maintained Windows build exposes only the active OpenGL 3+ renderer, so there is no alternative value to apply. |
| Full Screen | Restyles the current render window and updates camera, input and CEGUI dimensions on Apply. |
| Video Mode | Resizes the current render window and updates camera, input and CEGUI dimensions on Apply. |
| VSync and VSync Interval | Update the current render window on Apply. |
| Reversed Z-Buffer | Updates GL clip control and the active depth convention on Apply. |
| Debug Layer | Enables or disables GL debug output on Apply. |
| Separate Shader Objects | Reloads the loaded GPU programs for the selected shader mode on Apply. |
| FSAA | Replaces the active render window with one using the selected pixel format after Apply. |
| sRGB Gamma Conversion | Replaces the active render window with one using the selected pixel format after Apply. |
| Colour Depth | Replaces the active render window when another unique depth is available and selected; this system exposes only 32. |
| Display Frequency | Replaces the active render window when a fullscreen frequency is available and selected. |
| Keyboard Grab and Mouse Grab | Recreate the OIS devices before the next input capture after Apply. |
| Autoscroll | The camera input path reads the persisted value every frame after Apply. |
| Pan Speed | Updates the active camera while the slider moves and persists on Apply. |
| Keeper Voice | Subsequent keeper sound requests read the persisted selection after Apply. |
| Minimap Type | Recreates the active game or editor minimap on the next frame after Apply. |
| Nickname | Updates the connected session through the negotiated live-rename message and persists for later connections. |
| Ambient Light | Updates the active scene while the slider moves and persists on Apply. |

## Runtime verification and remaining platform scope

- The user tested the rebuilt Release executable and confirmed that all reported
  failures are fixed. Resolution and fullscreen changes apply without restarting,
  mouse input remains aligned, the bottom-edge flicker is gone and the in-game
  main navigation remains visible at 3440 x 1440 fullscreen.
- Older-peer and replay packet boundaries are covered by automated tests, but a
  running mixed-version multiplayer session has not been tested.
- The maintained build uses Ogre-owned windows because `OD_USE_SFML_WINDOW` is
  disabled. Runtime window replacement has not been implemented or tested for
  the optional SFML-owned window path.
- The installed plugin set exposes only OpenGL 3+; there is no alternative
  renderer to switch to. In-process replacement of the active render backend is
  outside this implementation and no longer exits the game if encountered.

## Fullscreen and Windows display scaling

The existing Release executable's embedded manifest contained only its execution
level, with no DPI declaration. An isolated Windows API probe on this computer
reported a physical display mode of 3440 x 1440 but a DPI-unaware screen size of
2752 x 1152. Per-monitor awareness reported 3440 x 1440 at 120 DPI (125%).

The executable now includes [a DPI manifest](../../dist/opendungeons.manifest),
merged by MSVC and embedded through the icon resource for MinGW. This keeps
Windows window and input coordinates in physical pixels. Microsoft documents
the API virtualization and manifest settings in
[High DPI desktop application development](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)
and [Setting default DPI awareness](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process).

The test executable built without the manifest reports the size mismatch and
exits 1. The same test linked with the manifest extracted from the rebuilt game
reports matching 3440 x 1440 dimensions and exits 0. This verifies the coordinate
correction without opening the game. The missing navigation was consistent with
that mismatch, and the user's final fullscreen test confirmed the correction.
MinGW and moving the game between monitors have not been tested.

After the final clean Release build, its embedded manifest was extracted again
and contained `PerMonitorV2, PerMonitor`. The DPI probe still reported matching
3440 x 1440 screen and display-mode dimensions with per-monitor awareness, and
the real `ModeGame.layout` probe again placed the main navigation, minimap and
options button inside the 3440 x 1440 display; both probes exited with code 0.

## Verification

The first Release build of the live-settings changes failed on a leftover shadow
flag reference and a missing configuration include. Both were corrected. The
current window-replacement implementation builds successfully in Release and
Debug. The latest logs are `game-Release-live-settings-final.log` and
`game-Debug-live-settings-pass3.log`. Earlier builds described in BUILDING.md
predate this work and do not establish that all live settings work at runtime.

An incremental Release executable built while the input manager's class layout
was changing crashed in CEGUI before the main menu appeared. Targeted logging
showed that a GUI lookup was reading an impossible container size, while the same
GUI instance had just loaded all layouts successfully. This identified mixed
object files using different class layouts rather than a missing DLL or damaged
user configuration. A clean-first Release build recompiled every translation
unit and linked successfully on September 5, 2026. The user confirmed that this
clean-build executable started successfully and reached the settings window.
The temporary lookup logging used for this diagnosis was removed from the final
source.

The first clean-build runtime test then changed a windowed resolution from
1920 x 1440 to 2048 x 1152. The replacement window was created and the transfer
completed, but the game appeared frozen while GL3Plus rebuilt graphics programs;
the process had to be ended manually. Resolution and fullscreen changes do not
require a new OpenGL context, so they now resize or restyle the current render
window directly and notify the camera, input and CEGUI about its actual size.
Window replacement remains reserved for settings that determine the pixel format
when an OpenGL window is created. A clean Release build containing this correction
completed successfully on September 5, 2026. The user then confirmed that changing
the resolution applied immediately without a restart or freeze.

That test changed the running game from 2048 x 1152 windowed to 3440 x 1440
fullscreen and exposed two related symptoms: mouse hits were below the displayed
controls and an approximately 1 cm strip flickered along the bottom edge. OGRE's
Win32 fullscreen path changed the window frame style and resized the window without
passing `SWP_FRAMECHANGED` to `SetWindowPos`. Microsoft documents that this flag is
required after changing frame styles with `SetWindowLong`, so the maintained OGRE
patch now supplies it. The first isolated fullscreen run then measured matching
3440 x 1440 window and client areas but retained the previous 148 x 72 test
viewport, because OGRE stored the requested size before refreshing the actual
window metrics. The patch now refreshes those metrics directly instead. The final
probe measured 3440 x 1440 for the screen, window, client area and viewport,
preserved the OpenGL context, rendered successfully and exited with code 0. The
patched renderer also built and installed successfully in Release and Debug, its
Release DLL was staged beside the game executable, and all three existing GL3Plus
regression variants passed. A final clean Release build then linked successfully;
the staged renderer matched the built and installed DLL by SHA-256, and the rebuilt
executable retained its per-monitor DPI manifest. The user's final running-game
test confirmed correct mouse alignment and removal of the bottom-edge flicker.

An isolated GL3Plus test then used the same windowed sequence as the application:
`setFullscreen(false)`, `resize` and `windowMovedOrResized`. The resize completed
in 5 ms, preserved the original OpenGL context and rendered the next frame
correctly. The same runs set VSync interval 1 and then disabled VSync; the WGL
driver query returned 1 and 0 respectively. The separable and monolithic shader
runs both exited with code 0. This verifies the engine-level path that avoids the
observed context rebuild; the user's running-game test confirmed CEGUI and
gameplay responsiveness after the resolution change.

The existing ODPacket unit tests and a new test for optional handshake fields
passed: two cases, 15 assertions. This verifies packet boundaries for original
and extended messages, not a running multiplayer session. The input-device
rollback now saves the previous capture configuration before attempting to
recreate the previous devices, so another device failure cannot leave the failed
selection persisted for the next launch.

An isolated CEGUI test loaded the real `ODSkin.scheme` and `ModeGame.layout`
using the NullRenderer, without opening the game or a graphics window. At
1280 x 1024, 3440 x 1440, 1920 x 1080 and 800 x 660, the main navigation tabs,
minimap and options button were visible in the layout and had nonempty clipping
rectangles inside the display. At 3440 x 1440, the navigation occupied the
bottom 133 pixels. This rules out an off-screen rectangle in that isolated
layout test, but does not verify GPU rendering, fullscreen or the user's failure.

Local, ignored evidence is in `build/windows/gui-layout-probe.cpp`,
`gui-layout-probe.log`, `gui-layout-probe-CEGUI.log` and
`game-Release-live-settings-pass1.log`. Subsequent evidence includes
`game-Release-live-settings-final.log`, `game-Debug-live-settings-pass3.log`,
`dpi-metrics-before.log`, `dpi-metrics-after.log`, `live-settings-after.manifest`
and `test-live-packets.log`. The manifest extracted from the current Release
executable as `live-settings-current.manifest` contains `PerMonitorV2, PerMonitor`.
The staged Release GL3Plus plugin has the same SHA-256 hash as the patched
installed plugin. Use the maintained build commands in
[BUILDING.md](BUILDING.md). The user completed the requested Windows game and
visual acceptance tests. Linux runtime behavior has not been verified.

## Renderer implementation

The installed renderer is OGRE 13.6.5 GL3Plus. Its configuration setters generally
update an option map, while `Win32Window::create` selects FSAA, gamma, colour depth
and display frequency. Its window `setFullscreen` method does not accept those
extra options. The application therefore recreates a secondary render window for
creation-time options and keeps the primary context alive.

[ogre-multiwindow-settings.patch](../../scripts/win32/patches/ogre-multiwindow-settings.patch)
adds the required OGRE 13.6.5 GL3Plus behavior. It keeps a separate program
pipeline for each OpenGL context and forgets that pipeline when its context is
destroyed, recognizes both WGL framebuffer-sRGB extension names, and applies
reversed depth, separate shader objects and debug output when their configuration
values change at runtime. It also refreshes the Win32 frame and actual render
dimensions when switching between windowed and fullscreen modes. The Windows
prerequisite installer applies the patch idempotently before building OGRE.

An isolated GL3Plus probe rendered the primary and two successive hidden windows
in green, verified FSAA 4 and hardware gamma on the first replacement and the
return to FSAA 0 without gamma on the second, destroyed the earlier replacement
while the later context was current, returned to the primary context and shut down
cleanly. Separate runs covered separable shaders, monolithic shaders and primary
context configuration changes. All three runs passed with exit code 0 after the
Release and Debug dependencies were rebuilt. This verifies the renderer mechanisms
independently of gameplay; the user subsequently confirmed the reported behavior
in the running game.
