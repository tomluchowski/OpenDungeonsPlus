# Project context for future sessions

## Language

Communicate with the user in German.
Write and maintain all project documentation in English.
Use English for Git-related text, including commit messages, pull request titles,
descriptions and review comments.

## Project setup

Before working on this project, read:

1. [Windows environment and current status](docs/development/WINDOWS-DEV-SETUP.md).
2. [Configure and build commands](docs/development/BUILDING.md).
3. [Development documentation index](docs/development/README.md); follow the
   workflow and task notes when relevant to the request.

Windows development is organized on `feature/windows-support` in this fork.
The related work is split into a local branch stack: `feature/windows-support`,
`fix/dynamic-shadows`, `fix/settings-option-duplicates`, then
`feature/live-settings`. Continue each task on its matching branch; see
[live settings and verification](docs/development/LIVE-SETTINGS.md).
Read the current setup in [the contribution workflow](docs/development/CONTRIBUTING-WORKFLOW.md)
before Git operations: `origin` is the fork and `upstream` is the original project.
Preserve the existing default branch; continue each task on its own work branch.
The work branch includes local notes and is not the final upstream PR branch;
prepare that later from upstream using only the reviewed, reusable changes.
Do not push without explicit user authorization.

The Windows prerequisites are already installed in `C:\Users\mario\od-deps`;
their sources, binaries and logs deliberately live outside the repository.
The maintained instructions and scripts live in this repository under
`docs/development/` and `scripts/win32/`.
Do not rely on the old copies in `build/` or `od-deps/setup-scripts/`.

At the verified state on 2026-09-05, dependency builds, game CMake configuration
and both Windows x64 game builds (Release and Debug) succeeded.
User startup attempts exposed a Windows resource-path bug; both binaries now
include its correction. A subsequent Release run loaded the main-menu scene and
shut down normally without the earlier loading errors; the user subsequently
confirmed that the Release executable starts without errors.
That confirmation predates enabling dynamic shadows: a later startup failure was
traced to OGRE's internal shadow programs being registered only in Graphics.
The resource template now also exposes Media/Main through OgreInternal while
retaining Graphics access for shader includes; the headless OGRE resource test
fails before and passes after this correction, and the user's 14:52 run reached
the main menu with shadows enabled. That run later failed while entering
TestLegacyNoScripts.level. Added exception logging captured the cause during the
user's 15:05 reproduction: automatic additive illumination splitting removed
DirtInstanced's fragment shader, which GL3Plus requires. RenderManager now uses
integrated additive texture shadows to retain the custom shader passes. Release
and Debug rebuilt successfully; the isolated OGRE pass test fails with splitting
and passes without it. The user must still retest the map and shadow appearance
with the rebuilt executable; check the latest evidence in the startup notes.
Read [startup failures and verification](docs/development/WINDOWS-STARTUP-FIXES.md)
before investigating further startup issues; broader gameplay tests and packaging
remain unverified.
The Release executable now has its runtime DLLs staged beside it for direct
File Explorer startup; the configuration script maintains this through
`scripts/win32/prepare-windows-runtime.ps1`. Python's standard library and OGRE
media still use the external installation. Read BUILDING.md for the static
verification results and the remaining Debug runtime/plugin limitations.
Read the [Windows build fixes](docs/development/WINDOWS-BUILD-FIXES.md) for the four
diagnosed failures and verification logs; the CEGUI source now includes a
repository-managed compatibility patch applied by its installation script.
Read the current status document and check the actual files before making claims.
Do not reinstall dependencies or switch versions just because a new session starts.
Use the repository's environment helper and configuration script; the scripts
currently target Mario's Windows installation, with paths recorded in the docs.

Keep changes within the user's request and preserve existing work.
Do not read `.env` or other secret files.
Manual game tests, QA and visual acceptance are performed by the user.
When changing setup paths, versions, commands or verified build status, update the
linked documentation in the same task so the next session has the current state.
