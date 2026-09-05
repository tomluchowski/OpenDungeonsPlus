# Windows settings fixes

## Duplicate colour-depth choices

On September 5, 2026, the user reported that the colour-depth dropdown offered
`32`, `32`, `32` instead of distinct values.

In the installed OGRE 13.6.5 source,
`RenderSystems/GLSupport/src/OgreGLRenderSystemCommon.cpp`, `refreshConfig` first
deduplicates colour depths, then appends them again while enumerating fullscreen
refresh rates. This leaves repeated depths in the renderer's possible values.
[SettingsWindow.cpp](../../source/modes/SettingsWindow.cpp) previously inserted
every entry into its renderer-dependent dropdowns.

The settings code now sorts and deduplicates the local copy of each option's
possible values before sizing and filling its dropdown. If the renderer offers
only 32-bit colour, the list contains one `32`; any genuinely different supported
depths are retained. The dropdowns were already sorted, and selection and saving
still use the option text. No renderer options or user settings are modified by
building these lists.

Release and Debug rebuilt successfully with exit code 0 using the maintained
Windows environment helper. Logs are in
`build/windows/game-Release-colour-depth.log` and
`build/windows/game-Debug-colour-depth.log`; `git diff --check` also passed.
The updated dropdown has not yet been verified visually in the game.

The user performs the visual check: restart the updated Release executable,
open video settings, and confirm that the colour-depth list has no duplicates
and retains its current selection when the settings are reopened.
