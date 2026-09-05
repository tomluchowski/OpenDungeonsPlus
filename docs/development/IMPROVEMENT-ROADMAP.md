# Product improvement audit and roadmap

## Purpose and snapshot

This document turns the current visual and usability concerns into separate,
reviewable work items. It covers visual presentation, controls, interaction
feedback and onboarding, and coordinates them with work already open in the
original repository.

The snapshot was taken on September 5, 2026. The original repository's default
branch was still `shaders-improvement`; after fetching it locally, its head was
`be44649f`. GitHub reported 12 open issues and 13 open pull requests. These
counts and merge states can change and must be refreshed before starting each
implementation branch.

This audit is stored on `docs/improvement-roadmap`, created from the verified
fork state that already contains the Windows, live-settings and progressive
edge-scrolling work. The roadmap itself is fork documentation. Future upstream
contributions must still be assembled from the then-current upstream branch with
only their required dependencies.

## Evidence and limits

The findings use four evidence types:

- The user's current Windows play test: the game starts and runs, but the visual
  presentation and interaction are still perceived as substantially below the
  intended quality level.
- The current source, CEGUI layouts, fonts and configuration in this branch.
- The six repository screenshots from 2021 through 2024, all at 1920x1200.
- The original repository's open issues, pull requests and remote branches as
  fetched or queried on September 5, 2026.

The repository screenshots establish recurring composition and readability
problems, but they are historical and do not prove every pixel of the current
renderer output. No new screenshots or first-time-player study were captured for
this audit. Manual visual acceptance and gameplay testing remain with the user.

## Main conclusion

Changing the renderer alone will not solve the reported quality gap. The current
problems are distributed across four layers:

1. **Readability:** most HUD geometry and fonts are fixed in pixels and remain
   very small at modern resolutions.
2. **Interaction:** important actions are represented by icons and hover-only
   explanations, while action state and invalid-target feedback are weak.
3. **Guidance:** the game has a long help page but no tutorial level or staged
   first-run flow.
4. **Presentation:** the world lacks a consistent hierarchy of materials,
   lighting, ownership, silhouettes and effects; some improvements are already
   waiting in upstream pull requests.

The safest order is therefore readability, interaction feedback, onboarding,
then coordinated visual and asset work. This makes later art improvements visible
and usable instead of placing them behind the current interface friction.

## Current-state inventory

| Area | Confirmed evidence | Player effect | Priority | Upstream relationship |
| --- | --- | --- | --- | --- |
| HUD scale and text | The common fonts are fixed at 10 or 12 pixels with automatic scaling disabled; the top status bar is 23 pixels high, its icons are 30 pixels, and several top buttons are 25 pixels. | Text and targets remain difficult to read and click at high resolutions. | P0 | Directly matches [issue #6](https://github.com/tomluchowski/OpenDungeonsPlus/issues/6). [PR #29](https://github.com/tomluchowski/OpenDungeonsPlus/pull/29) changes one affected spell layout. |
| Bottom action bar | The bar is fixed at 133 pixels high; room buttons are mostly 40 pixels, while other action buttons are 60 pixels. The screenshots show a large unused dark area beside the icons. | Space is consumed without improving discoverability, and the meaning of the icon grid is unclear without hovering. | P0 | [Issues #4](https://github.com/tomluchowski/OpenDungeonsPlus/issues/4) and [#6](https://github.com/tomluchowski/OpenDungeonsPlus/issues/6); PR #29 partially changes the spell arrangement only. |
| Settings and modal layouts | The settings window uses a fixed 545x500 pixel rectangle at a fixed offset, with many fixed child coordinates. Other dialogs mix fixed and proportional geometry. | Dialogs do not form a coherent scale system and can feel cramped or misplaced after resolution changes. | P0 | No dedicated open PR; depends on the live display work already completed in the fork. |
| Action discoverability | Core build, destroy and spell actions are image buttons whose detailed meaning is carried mainly by tooltips. The help text explains controls only after opening the help window. | A new player must experiment, hover or read a long reference page before understanding basic actions. | P0 | Central subject of issues #4 and #6. |
| Active action and invalid targets | Upstream issue #4 reports that a click can appear to do nothing and that modes such as summoning are mistaken for immediate commands. The current action system selects a mode, then waits for a world click. | The player cannot reliably tell what mode is active, where it can be used or why an attempted action failed. | P0 | No focused open PR; should be fixed before creating a tutorial. |
| Camera and pointer | The fork now applies resolution changes live, keeps pointer coordinates aligned, makes edge speed progressive and stops edge movement over the GUI. Other camera requests remain open. | The verified fork removes several immediate Windows problems, but centered zoom, alternative drag panning and the remaining requests in issue #11 are not covered. | P0 | [Issue #11](https://github.com/tomluchowski/OpenDungeonsPlus/issues/11); the aggregate PR #16 contains additional camera work and must be checked before implementation. |
| Event messages | Events are rendered in a fixed rectangle using 16-pixel category icons and coloured inline text. Objective success/failure and defeat are delivered as short event messages plus sound. | Important information competes with routine notices and may be missed or misunderstood. | P1 | Issues #4, [#5](https://github.com/tomluchowski/OpenDungeonsPlus/issues/5) and [#7](https://github.com/tomluchowski/OpenDungeonsPlus/issues/7). |
| Defeat state | Losing sets the player state and sends messages such as `You lost the game`; no dedicated defeat layout with explicit next actions exists. | The end state lacks visual weight and a clear next step. | P1 | Directly matches issue #5. |
| Help and onboarding | The help window contains one long, scrollable text assembled in code. The README points to that help and an old external video. No level file contains a tutorial reference. | Basic concepts are presented as reference material instead of being taught through actions and feedback. | P0 | Directly matches issue #4; the 27 levels in PR #30 are additional content, not a tutorial. |
| World hierarchy | Repository screenshots show strong repeated tile patterns, dark areas, similarly weighted room surfaces and many small props and creatures competing for attention. | Rooms, ownership, creatures, hazards and objectives are not readable at a glance. | P1 | [PR #45](https://github.com/tomluchowski/OpenDungeonsPlus/pull/45) adds coloured room emblems and supersedes PR #37. |
| Fog of war | The screenshots show visible per-tile fog structure; upstream issue #13 describes the same grid-like result. | Exploration boundaries look assembled from separate pieces rather than one coherent fog mass. | P1 | [Issue #13](https://github.com/tomluchowski/OpenDungeonsPlus/issues/13) is directly addressed by [PR #41](https://github.com/tomluchowski/OpenDungeonsPlus/pull/41). Do not duplicate it. |
| Materials and environment variety | Floors and walls repeat strongly across rooms and corridors. The editor currently selects the existing visual types rather than a broad material palette per dungeon. | Dungeons lack visual identity and repeated tiles expose the grid. | P2 | [Issues #12](https://github.com/tomluchowski/OpenDungeonsPlus/issues/12) and [#14](https://github.com/tomluchowski/OpenDungeonsPlus/issues/14); no open implementation PR. |
| Creature readability and animation | The screenshots show small creature silhouettes relative to rooms; upstream reports static bodies during movement and abrupt transitions. | Creatures lack presence and are difficult to read during normal play. | P2 | Issues #11 and [#8](https://github.com/tomluchowski/OpenDungeonsPlus/issues/8). |
| Voice, captions and music | Voice and sound systems exist, but upstream reports unclear lines, missing captions and repetitive or stuttering ambient music. | Important feedback may be unintelligible, and the soundscape can feel unfinished. | P1 for captions, P2 for replacement audio | [Issues #7](https://github.com/tomluchowski/OpenDungeonsPlus/issues/7) and [#9](https://github.com/tomluchowski/OpenDungeonsPlus/issues/9). |
| Source assets | Runtime assets are versioned, but upstream asks for editable project sources in a defined location. | Larger art work is harder to review, reproduce and improve consistently without source files. | P2 prerequisite | [Issue #10](https://github.com/tomluchowski/OpenDungeonsPlus/issues/10). |

## Work already completed in this fork

The following verified work should be treated as the current fork baseline and
not rediscovered as new roadmap tasks:

- Reliable Windows Release and Debug compilation and direct Release startup.
- Runtime dependency and resource preparation for the local Windows build.
- Dynamic-shadow startup and shader-pass corrections.
- Deduplicated renderer setting choices.
- Live resolution and fullscreen changes with updated render, GUI, camera and
  input dimensions.
- Correct pointer alignment after live resolution changes.
- Progressive edge scrolling and suppression of edge scrolling over the HUD,
  minimap and top navigation.

These changes are still fork branches rather than merged upstream work. Any pull
request to the original repository must be assembled and validated against fresh
upstream state.

## Ordered implementation roadmap

### Gate 0: refresh and reconcile upstream work

Before every feature branch:

1. Fetch `upstream` and re-query open issues and pull requests.
2. Check whether PR #29, #41, #45, #21, #16 or #15 has merged, changed scope or
   been replaced.
3. Compare the exact affected files before choosing a base.
4. Build the branch from current upstream and add only dependencies it actually
   needs.

This gate prevents duplicate fixes and large avoidable merge conflicts.

### 1. `feature/gui-scaling`

**Goal:** make the HUD, dialogs, fonts, tooltips and hit targets readable and
clickable across supported window sizes and after live resolution changes.

**Scope:** establish one scale policy, scale the common fonts and skin metrics,
reflow the top bar and bottom action area, and adapt fixed dialogs without changing
their game behavior.

**Required decision before implementation:** automatic scaling, a user-selected
scale, or both; supported scale limits must also be chosen. The repository does
not determine that product behavior unambiguously.

**Dependency and overlap:** resolve PR #29 first because it changes
`WindowTabSpells.layout`. Use the fork's live display update path where runtime
resize notification is required, but keep unrelated Windows setup out of the
eventual upstream contribution.

**Verification:** test every main menu, game HUD tab, settings page, help,
objectives, skill tree and exit dialog at representative small, full-HD and
high-resolution window sizes; verify no clipping, overlap, missed click target or
pointer offset.

### 2. `feature/action-state-feedback`

**Goal:** make every two-step action explain its current mode, valid target and
failure reason.

**Scope:** add a persistent active-action label near the action bar, strengthen
the selected state, distinguish valid and invalid world targets, and show one
specific reason when an attempted build, spell or placement cannot proceed.

**Dependency:** GUI scaling should land first so the feedback has a stable visual
location and text size.

**Verification:** a player must be able to select, cancel and complete digging,
room building, trap placement, summoning and a targeted spell without consulting
the help page, and an invalid click must explain why it failed.

### 3. Camera follow-up branches

The verified progressive edge-scrolling branch covers only edge speed and GUI
suppression. The remaining issue #11 requests should stay separate:

- `fix/camera-zoom-anchor`: keep zoom focused on the intended world position.
- `feature/camera-drag-pan`: provide an accessible pointer-based pan alternative.
- `fix/camera-speed-consistency`: reconcile keyboard, edge and configured pan
  speed after reproducing the remaining discrepancy.

First compare the camera and minimap commits still present in aggregate PR #16.
Do not claim issue #11 is closed merely because edge scrolling is improved.

### 4. `feature/tutorial-level`

**Goal:** teach the minimum playable loop through actions in a real level.

**Dependency:** action-state feedback must be available first; a tutorial cannot
teach reliably while valid targets and failed actions remain unclear.

**Required decision before implementation:** approve the lesson sequence and the
level's completion point. A proposed sequence is camera movement, selecting tiles
to dig, summoning a worker, mining and storing gold, building basic creature rooms,
opening the skill system and placing one defensive item.

**Scope:** prefer the existing level, objective and display-text systems; add a
new framework only if those systems cannot express a verified lesson step.

**Verification:** a first-time player can finish the approved sequence without
the README, external video or full help page.

### 5. `feature/defeat-screen`

**Goal:** make the end state unmistakable and give the player explicit next
actions.

**Required decision before implementation:** choose which actions are offered,
such as retry, return to menu, continue observing or exit.

**Scope:** one modal end-state presentation, correct input blocking and the
approved actions; gameplay balance remains outside this branch.

**Verification:** defeat appears once, suppresses obsolete combat feedback and
each offered action leads to the stated destination.

### 6. `feature/voice-captions`

**Goal:** pair every important spoken keeper message with readable text and avoid
repeating unchanged warnings excessively.

**Scope:** map voice events to captions, route them through the existing event
system, and suppress exact repeats until the underlying state changes.

**Dependency:** GUI scaling should land first; replacement voice recordings are a
separate asset task.

**Verification:** each important spoken event has matching text, and repeated
bed, hunger, combat and objective warnings follow the approved repeat policy.

### 7. Integrate pending upstream visual work before new art changes

Review and test PR #45 for room ownership and PR #41 for fog continuity. The
newly fetched upstream branch `room-seat-emblems-paul424-improvement` already adds
another coloured building element on top of the room-emblem work, but it was not
an open pull request in the captured snapshot. Clarify its intended destination
before building on it.

PR #37 should not be combined with PR #45 because #45 explicitly supersedes it.
Renderer-heavy work should also check the status and intended direction of PR #15
before committing to APIs that may change during the Ogre 14 transition.

### 8. `docs/visual-direction`

**Goal:** define a small, reviewable visual target before replacing individual
assets.

**Scope:** establish rules for room identity, owner colour, material value range,
light and shadow hierarchy, creature silhouette size, effect intensity and UI/world
separation using current game screenshots.

**Verification:** the document includes approved before/target examples and gives
an artist or implementer enough constraints to make consistent changes.

### 9. Visual implementation branches

After the visual direction and pending upstream PRs are resolved, split work by
asset family:

- `feature/environment-material-variety`: issues #12 and #14, beginning with one
  complete corridor or room family and its editor support.
- `feature/creature-readability`: silhouette, scale, selection and status
  readability without replacing animation sets.
- `feature/character-animation-pass`: issue #8, one creature family per reviewable
  contribution.
- `feature/ambient-audio-pass`: issue #9, with licensed sources and loop testing.
- `docs/asset-source-workflow`: issue #10, defining source-file locations,
  licences and export steps before accepting larger asset replacements.

Animation, modelling, texture painting, voice acting and music require artistic
quality decisions and licensed source material; they cannot be promised as fully
automatic implementation tasks.

## Open issue coordination

| Issue | Disposition in this roadmap |
| --- | --- |
| [#4 Gameplay is not discoverable](https://github.com/tomluchowski/OpenDungeonsPlus/issues/4) | Primary source for action-state feedback and the tutorial; split its individual failures into focused branches rather than one large fix. |
| [#5 Improve failure sequence](https://github.com/tomluchowski/OpenDungeonsPlus/issues/5) | Covered by the separate defeat-screen branch after the offered actions are approved. |
| [#6 Interface and mouse navigation](https://github.com/tomluchowski/OpenDungeonsPlus/issues/6) | GUI scaling is the first remaining branch; current fork cursor and live-resize fixes address only part of the report. |
| [#7 Voice acting and captions](https://github.com/tomluchowski/OpenDungeonsPlus/issues/7) | Captions and repeat control come before replacement recordings. |
| [#8 Character animations](https://github.com/tomluchowski/OpenDungeonsPlus/issues/8) | Later asset branch after creature readability and source-asset workflow. |
| [#9 Ambient music](https://github.com/tomluchowski/OpenDungeonsPlus/issues/9) | Later audio branch requiring licensed sources and manual listening acceptance. |
| [#10 Project asset sources](https://github.com/tomluchowski/OpenDungeonsPlus/issues/10) | Define the source and export workflow before broad art replacement. |
| [#11 Camera navigation](https://github.com/tomluchowski/OpenDungeonsPlus/issues/11) | Progressive edge scrolling is only a partial fix; keep zoom, drag pan and remaining speed behavior separate and check PR #16 first. |
| [#12 Hallway styles](https://github.com/tomluchowski/OpenDungeonsPlus/issues/12) | Part of the later environment-material work. |
| [#13 Seamless fog](https://github.com/tomluchowski/OpenDungeonsPlus/issues/13) | Already addressed by open PR #41; review rather than duplicate. |
| [#14 More map materials](https://github.com/tomluchowski/OpenDungeonsPlus/issues/14) | Later environment and editor branch after the visual direction is approved. |
| [#42 Explicit variable types](https://github.com/tomluchowski/OpenDungeonsPlus/issues/42) | Code-style task outside this product roadmap; never mix it into UI or visual branches. |

## Open pull-request coordination

The GitHub query returned no formal review decision or status-check result for the
13 open pull requests. Mergeability reported by GitHub was available for some but
not all and is only a momentary signal. Re-query the individual pull request before
depending on it.

| PR | Relationship to this roadmap | Action |
| --- | --- | --- |
| [#15 Ogre v14.6 v3](https://github.com/tomluchowski/OpenDungeonsPlus/pull/15) | May change rendering APIs and the long-term graphics baseline. | Treat as a decision gate for renderer-heavy work; it does not block layout, feedback or onboarding work. |
| [#16 Combined crash, UI, portability and level work](https://github.com/tomluchowski/OpenDungeonsPlus/pull/16) | Large aggregate containing cursor, camera and UI changes alongside unrelated work; several parts have been split or merged separately. | Do not use as a clean feature base; inspect its individual commits before camera or pointer work. |
| [#20 User data folder](https://github.com/tomluchowski/OpenDungeonsPlus/pull/20) | Changes where levels and configuration are read. | Recheck before adding a tutorial level so installed and user data remain correct. |
| [#21 Cross-platform fixes](https://github.com/tomluchowski/OpenDungeonsPlus/pull/21) | Overlaps parts of the fork's Windows portability work. | Reconcile exact files before preparing the Windows upstream contribution. |
| [#28 Bridge rerouting](https://github.com/tomluchowski/OpenDungeonsPlus/pull/28) | Gameplay bug fix with no direct roadmap overlap. | Keep independent. |
| [#29 Two-row spell buttons](https://github.com/tomluchowski/OpenDungeonsPlus/pull/29) | Directly changes a layout required by GUI scaling. | Resolve or use as an explicit dependency before editing that layout. |
| [#30 New levels](https://github.com/tomluchowski/OpenDungeonsPlus/pull/30) | Adds content but no staged tutorial. | Do not treat it as onboarding; check level-list ordering if both land. |
| [#31 Integration-test launcher and macOS fixes](https://github.com/tomluchowski/OpenDungeonsPlus/pull/31) | Improves automated validation. | Useful infrastructure, but independent of product behavior. |
| [#32 Destructible traps](https://github.com/tomluchowski/OpenDungeonsPlus/pull/32) | Gameplay mechanics outside this audit. | Keep independent. |
| [#33 Configurable room hit points](https://github.com/tomluchowski/OpenDungeonsPlus/pull/33) | Gameplay mechanics outside this audit and currently reported conflicting. | Keep independent and do not mix with visual room work. |
| [#37 Room ownership tint](https://github.com/tomluchowski/OpenDungeonsPlus/pull/37) | Earlier whole-floor ownership treatment. | Do not integrate; PR #45 explicitly supersedes it. |
| [#41 Overlapping fog tiles](https://github.com/tomluchowski/OpenDungeonsPlus/pull/41) | Direct visual improvement for issue #13. | Review and test it instead of implementing another fog solution. |
| [#45 Room ownership emblems](https://github.com/tomluchowski/OpenDungeonsPlus/pull/45) | Directly improves room ownership readability without tinting the whole floor. | Preferred pending direction; test it before further room-material changes. |

## Recommended next action

The first new implementation branch should be `feature/gui-scaling`, after the
scale policy is chosen and PR #29 is rechecked. It addresses the most pervasive
confirmed defect, enables clearer feedback and onboarding, and does not require a
renderer or asset-pipeline decision.
