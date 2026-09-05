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

All 39 CEGUI layout files parse as XML. Static bounds checks cover the main
menu, top HUD, bottom action tabs, Settings and Skill Tree at 800 x 660,
1920 x 1080 and 3840 x 2160 with 80%, 100% and 120% UI scale; all nine
size-and-scale combinations fit their available areas.

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
