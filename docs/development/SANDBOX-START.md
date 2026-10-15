# Sandbox start state

The sandbox levels (`levels/skirmish/Sandbox.level` and the realms, mode code in `source/gamemap/SandboxMode.cpp`)
do not start with everything researched.

- Available at the start: the Dormitory (the lair), the Hatchery and the worker summon spell
  (`roomDormitory`, `roomHatchery`, `spellSummonWorker` in the `[SkillDone]` list of the seat). The free sandbox
  level `Sandbox.level` also starts with the Treasury.
- Rooms: the other base rooms become available one after the other after the start (game time, array
  `ROOM_UNLOCKS` in `SandboxMode.cpp`). Room number i (counted from 0) comes after (i + 1) times the interval
  `SandboxRoomUnlockIntervalSeconds` in `config/rooms.cfg` (default 120 seconds, so the first room after
  2 minutes and the last one after 26 minutes). Order: Treasury, Library, Training Hall, Workshop, Guard Room,
  Wooden Bridge, Prison, Torture Chamber, Crypt, Stone Bridge, Casino, Arena, Temple.
  The first message of the game tells the player about the starting rooms; each unlock goes through
  `Seat::addSkill`, so the player gets the usual "... is now available." notice. The next room and the time
  until it comes are shown on the HUD next to the score.
- Traps and doors: every trap and door the level allows becomes available as soon as the keeper owns a
  Workshop (one message). They do not have to be researched.
- Spells are researched in the library as usual (only the worker summon spell is there at the start).
- A room the level already lists in `[SkillDone]` (or that was researched in the meantime) is skipped, so a
  level with all rooms researched has nothing to unlock. `levels/skirmish/SandboxEverything.level` is such a
  variant: every base room and the full spell and trap set are available from the start, there is no timer.
- The elapsed time is saved with the game (`SandboxState` in the info block), so a loaded game continues the
  timetable where it stopped.

Check: `python source/tests/check_sandbox_start.py`.
