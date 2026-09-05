# Progressive mouse-edge scrolling

## Branch and scope

The work is isolated on `feature/progressive-edge-scrolling`, based on
`feature/live-settings`. It changes only in-game camera movement triggered by the
Autoscroll mouse-edge control.

## Previous behavior

With Autoscroll enabled, entering the outer 2% of any screen edge immediately
applied full camera acceleration. The camera therefore moved too quickly before
the pointer could be positioned precisely.

## Current behavior

The existing 2% activation area is retained. Within that area, scrolling now
starts slowly and increases linearly as the pointer approaches the edge. The
screen edge still provides the full configured pan speed. Horizontal and vertical
intensities are calculated independently, so the same behavior applies to the
top, bottom, left and right edges and combines naturally at corners.

Keyboard camera movement and the Pan Speed setting retain their existing behavior.
Disabling Autoscroll still disables mouse-edge movement.

Mouse-edge scrolling is also suppressed while the pointer is over the in-game
GUI. This prevents camera movement while using the bottom navigation, minimap or
top navigation. Moving the pointer back over the game world restores the normal
edge-scrolling behavior.

## Implementation

`GameMode::mouseMoved()` converts the pointer's distance from each edge into a
value from zero to one and passes it with the existing camera movement command.
`CameraManager` uses that value as the per-axis maximum pan speed while preserving
the existing acceleration and deceleration path. The existing CEGUI hit test sets
all four edge intensities to zero while the pointer is over an in-game GUI widget,
which also stops movement already initiated at an edge.

## Verification

On September 5, 2026, a clean Windows x64 Release build of the
`opendungeons-plus` target completed successfully and produced
`build/windows/opendungeons-plus.exe`. The existing compiler warnings remained;
the changed files produced no errors.

The first launch after the clean build failed because CMake regeneration had
overwritten the prepared Windows resource configuration. Running
`scripts/win32/prepare-windows-runtime.ps1` restored the installed OGRE media
paths; this was a generated runtime configuration problem rather than a camera
control failure.

The user then tested the Release executable in game and confirmed that the
progressive edge scrolling looks much better and that the reported control issue
is fixed. The runtime logs contain no unhandled exception for that test run and
end with normal engine shutdown.

The follow-up suppression of edge scrolling over the bottom navigation, minimap
and top navigation has been implemented and compiled. Manual in-game verification
of these three GUI areas is pending.

The project version remains 0.7.1 because this feature branch does not define a
release. The release notes are therefore unchanged, and the README has no
detailed mouse-edge control section that requires updating.
