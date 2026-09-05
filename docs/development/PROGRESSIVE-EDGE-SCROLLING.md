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

## Implementation

`GameMode::mouseMoved()` converts the pointer's distance from each edge into a
value from zero to one and passes it with the existing camera movement command.
`CameraManager` uses that value as the per-axis maximum pan speed while preserving
the existing acceleration and deceleration path.

## Verification

On September 5, 2026, a clean Windows x64 Release build of the
`opendungeons-plus` target completed successfully and produced
`build/windows/opendungeons-plus.exe`. The existing compiler warnings remained;
the changed files produced no errors.

Manual gameplay verification is still required. Enable Autoscroll, enter a
playable map and check all four edges: movement should begin slowly on entering
the outer 2%, increase continuously toward the edge and reach the configured Pan
Speed at the edge. Also check a corner, keyboard movement and Autoscroll disabled.
