# Development documentation

Here we collect guides and findings on contributing to OpenDungeonsPlus,
with one Markdown file per topic.

For a new session, first read [Windows development environment](WINDOWS-DEV-SETUP.md)
and [Configuring and compiling](BUILDING.md); the entry point for agents
is also recorded in [AGENTS.md](../../AGENTS.md) at the project root.

## Available guides

- [Contributing to the original project](CONTRIBUTING-WORKFLOW.md): fork, synchronization,
  the configured Windows work branch, separate commits and the path to a later
  pull request to the original project.
- [Tasks and division of work](TASKS.md): assessment so far of autonomous
  implementation, participation in testing and the suggested starting point.
- [Windows development environment](WINDOWS-DEV-SETUP.md): installed versions,
  exact locations, connections to the project and verified status.
- [Configuring and compiling](BUILDING.md): load the environment, run CMake,
  build Release/Debug and find logs.
- [Windows build errors and fixes](WINDOWS-BUILD-FIXES.md): confirmed
  compiler errors, their causes, targeted fixes and build evidence.
- [Windows startup errors and fixes](WINDOWS-STARTUP-FIXES.md): actual startup
  failures, runtime preparation, resource-path correction and outstanding verification.
- [Restoring prerequisites](WINDOWS-PREREQUISITES.md): sources,
  checksums, installation scripts, order and resolved installation problems.

## Adding further notes

Add new files here as needed and link them above, for example:

- `DEBUGGING.md`: traceable error analyses and solutions.
- `ARCHITECTURE-NOTES.md`: findings about the existing code and its relationships.
- `GUI-SCALING.md`: findings on GUI scaling once work on it begins.

For technical findings, record the affected code, verification steps and
open questions; label statements that have not yet been verified accordingly.

The collection is initially intended for our own fork; we decide which documentation
to include in the original project for each pull request.
