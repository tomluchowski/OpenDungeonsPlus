# Tasks and division of work

As of September 5, 2026, recorded from the conversation so far.

## Goal of the collaboration

Codex should handle implementation as independently as possible; the user wants
to focus mainly on testing and visual assessment.

The following assessments reflect the conversation so far, not a
guarantee of fully autonomous implementation. The detailed technical findings,
priorities, branch boundaries and upstream overlap are recorded in the
[product improvement audit and roadmap](IMPROVEMENT-ROADMAP.md).

## Tasks discussed

| No. | Task | Assessment so far | Participation or outstanding prerequisite |
| --- | --- | --- | --- |
| 1 | Make the Windows build reliable | Very feasible, but full autonomy is not guaranteed. | Local Windows or dependency problems may require user intervention. |
| 2 | Stabilize the Ogre 14 port | Assessed as feasible, but a large architecture and compatibility area without a reliable guarantee of completeness. | The scope and obstacles must first be examined in the code. |
| 3 | Make the GUI scalable | Assessed as likely to be fully implementable by Codex. | The user tests the visual presentation in particular; a game version that builds and starts locally is needed to verify the result. |
| 4 | Resolve mouse/cursor handling cleanly | Assessed as likely to be fully implementable by Codex. | The user checks the behavior in the game; the specific scope of changes still needs to be defined. |
| 5 | Make the UI easier to understand | Assessed as largely implementable by Codex. | Open decisions about interaction and wording must be clarified or explicitly delegated. |
| 6 | Tutorial / onboarding | Assessed as implementable by Codex, provided it may define the content and flow itself. | The necessary decision-making authority has so far only been mentioned as a prerequisite and has not yet been granted. |

## Suggested starting point

In the conversation, tasks 3 and 4 were suggested as a starting point to minimize
the user's own effort; this is not yet an instruction to implement these tasks.

For GUI scaling, first establish an environment in which the game
compiles and starts locally; the prerequisites discussed are in the
[Windows documentation](WINDOWS-DEV-SETUP.md).

## Open decisions and verification

Missing functional, design or content decisions are not made
silently; they must be unambiguously determined from the project, answered by the
user or explicitly delegated to Codex.

The user handles manual game tests, QA and visual acceptance; results
and any errors found are then evaluated together.
