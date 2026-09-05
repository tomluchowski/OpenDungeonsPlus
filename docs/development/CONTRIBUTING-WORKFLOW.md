# Contributing to the original project

## Working independently in the fork

The project can be developed independently in your own fork; joining
the original project's team or having write permissions there is not necessary.
The actual personal write permissions to the original repository have not
been checked.

The original project is needed as upstream when you want to incorporate its new
changes or contribute your own changes back through a pull request;
both are optional if you only want to continue development in your own fork.

Use a separate work branch for each task even in the fork. For work solely
in the fork, create it from your own current development state so that
existing changes of your own are preserved. The following steps,
in contrast, describe a contribution to the original project and start its
work branch directly from the upstream state.

## Repositories and target branch

- Own fork: [Rokk001/OpenDungeonsPlus](https://github.com/Rokk001/OpenDungeonsPlus),
  configured locally as `origin`.
- Original project for our contributions:
  [tomluchowski/OpenDungeonsPlus](https://github.com/tomluchowski/OpenDungeonsPlus),
  configured locally as `upstream`.
- Target branch in the original project: `shaders-improvement` (as of September 5, 2026).
  Check the intended target branch on GitHub before a pull request.

## Currently configured: Windows support

As of September 5, 2026. Our work branch in the fork is
`feature/windows-support`; we continue the Windows work here.
Documentation and the preparation of a build environment also belong on a
work branch; a branch is not limited to new game features.

The branch was created from the existing fork state `a8ffa583` and includes
the setup scripts and development notes that had not yet been committed.
The default branch `shaders-improvement` remains at that state;
we do not rewrite its existing documentation commit.
When fetched, the confirmed default branch of the original project,
`upstream/shaders-improvement`, pointed to commit `be44649f`.
The fork's default branch was therefore one documentation commit ahead of
the original at the time of setup.

The setup is recorded in two commits: first the existing local
Windows setup scripts, then the documentation including this workflow.
There is no change to the game code or game version at this point;
README and the development documentation describe the actual state,
and an additional game changelog entry is not needed for this setup.

### Daily work

When resuming Windows support work, check `git status` and use `feature/windows-support`;
the environment and build commands are in [BUILDING.md](BUILDING.md).
Do not create a new branch for each session of this ongoing Windows task.
Commit completed, coherent changes separately and review them together
with their associated general build documentation; keep personal
computer notes in a separate documentation commit.

The default branch is not changed for this work. First fetch new changes from
the original with `git fetch upstream` and compare them before incorporating them;
a fetch alone changes neither working files nor local work branches.

`origin` is configured locally as the default target for future pushes and is
also explicitly set as the push remote for the Windows work branch.
The new branch currently exists only locally and has no remote-tracking branch yet;
the main project is configured as the source for fetching and as the future PR target.
A push is still only performed after explicit authorization.

### Separate task: live settings

The related work is kept as a local stack so each branch adds one reviewable task:
`feature/windows-support`, `fix/dynamic-shadows`,
`fix/settings-option-duplicates`, then `feature/live-settings`. The live-settings
changes affect shared game code, including Linux paths, so they remain separate
from installing and building on Windows. Their implementation and verification
status is in [LIVE-SETTINGS.md](LIVE-SETTINGS.md). Prepare upstream contributions
from these boundaries after checking their dependencies. Linux validation is
still pending.

### Path to the future Windows PR

The work branch contains our fork context, including personal paths
and notes; the directory name or a separate commit does not
automatically exclude them from a pull request.
The finished contribution will therefore later be assembled on a separate PR branch
directly from the then-current `upstream/shaders-improvement`.
This PR branch has not yet been created.

First get the Windows build actually working, make the setup scripts usable
on other computers and document the results of the user's game tests.
Include only the verified, generally reusable changes and their instructions
in the PR; local installation logs and agent instructions
from this fork stay outside the contribution.
Explicitly review the selection and all affected file differences before the PR
and rebuild the assembled state, since it must not depend on the private
fork context. Only after explicit push authorization, publish the PR branch
in your own fork and propose it against the original project's default branch.

## 1. Add the original project as a remote once

Show existing remotes:

```powershell
git remote -v
```

If `upstream` is still missing:

```powershell
git remote add upstream https://github.com/tomluchowski/OpenDungeonsPlus.git
```

## 2. Keep the base up to date

Before switching branches, use `git status` to check that there are no unsaved changes;
save ongoing work on its own branch first.

```powershell
git fetch upstream
git switch shaders-improvement
git merge --ff-only upstream/shaders-improvement
```

This updates the local base branch; do not develop features on this
branch. If Git rejects the fast-forward, inspect the divergent commits
before taking further steps.

To also update the base branch on GitHub in your own fork:

```powershell
git push origin shaders-improvement
```

## 3. Create a separate branch for each change

Example for work on GUI scaling:

```powershell
git switch -c feature/gui-scaling upstream/shaders-improvement
```

Adapt the name to the specific task. The branch starts directly from the
previously fetched original state so that existing fork-only changes do not
automatically become part of the contribution.

## 4. Develop, verify and commit

Implement the change and verify the affected functionality; include only files
belonging to this task in the commit. Before each commit, check whether the version,
README, changelog or other documentation needs to be updated.

Review the changes with `git diff`, stage the intended files explicitly with `git add`
and then check the full commit content with `git diff --cached`.

```powershell
git commit -m "Describe the change"
```

Replace the example message with a specific description of the change.

## 5. Push the branch to your own fork

For the example branch:

```powershell
git push -u origin feature/gui-scaling
```

When Codex performs the work, every push requires explicit authorization;
the commands in this guide do not themselves constitute authorization.

## 6. Create a pull request to the original project

On GitHub, open a pull request with these settings:

- Target repository (base repository): `tomluchowski/OpenDungeonsPlus`.
- Target branch (base): `shaders-improvement`.
- Source repository (head repository): `Rokk001/OpenDungeonsPlus`.
- Source branch (compare): your own work branch, `feature/gui-scaling` in the example.

Describe the problem, change and checks performed; before creating the PR,
check under "Files changed" that only the intended changes
are included.

## 7. Address review comments

Implement, verify and commit corrections on the same work branch; then
push that branch to your own fork again, which automatically updates the existing
pull request.

## Documentation in the fork and in the pull request

Personal development notes are stored under `docs/development/`. For a contribution to
the original project, explicitly include only the relevant documentation on the
work branch; the directory name alone does not exclude files from a
pull request.
