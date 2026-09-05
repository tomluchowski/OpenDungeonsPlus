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
Read the current setup in [the contribution workflow](docs/development/CONTRIBUTING-WORKFLOW.md)
before Git operations: `origin` is the fork and `upstream` is the original project.
Preserve the existing default branch; continue this Windows task on its work branch.
The work branch includes local notes and is not the final upstream PR branch;
prepare that later from upstream using only the reviewed, reusable changes.
Do not push without explicit user authorization.

The Windows prerequisites are already installed in `C:\Users\mario\od-deps`;
their sources, binaries and logs deliberately live outside the repository.
The maintained instructions and scripts live in this repository under
`docs/development/` and `scripts/win32/`.
Do not rely on the old copies in `build/` or `od-deps/setup-scripts/`.

At the verified state on 2026-09-05, dependency builds, game CMake configuration
and both Windows x64 game builds (Release and Debug) succeeded; game startup,
manual gameplay tests and packaging remain unverified.
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
