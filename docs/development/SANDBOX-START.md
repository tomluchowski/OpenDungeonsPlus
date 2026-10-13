# Sandbox start state

The sandbox level (`levels/skirmish/Sandbox.level`, mode code in `source/gamemap/SandboxMode.cpp`)
does not start with everything researched.

- Available at the start: Treasury, Dormitory (the lair), Hatchery and the worker summon spell
  (`roomTreasury`, `roomDormitory`, `roomHatchery`, `spellSummonWorker` in the `[SkillDone]` list of the seat).
- Rooms: the other base rooms become available one after the other. Order: Library, Training Hall,
  Wooden Bridge, Guard Room, Workshop, Prison, Torture Chamber, Temple, Crypt, Casino, Arena, Stone Bridge.
  The first message of the game tells the player about the starting rooms; each unlock goes through
  `Seat::addSkill`, so the player gets the usual "... is now available." notice.
- Delay: `SandboxRoomUnlockIntervalSeconds` in `config/rooms.cfg` (default 120 seconds between two rooms,
  counted in game time). The first room comes after one interval. The reference text gives the order of the
  announcements but no timing, so the interval is a plain default that can be changed in the file.
- Spells and traps are researched in the library as usual (nothing of them is researched at the start).
- A room the level already lists in `[SkillDone]` (or that was researched in the meantime) is skipped, so a
  level with all rooms researched has nothing to unlock. `levels/skirmish/SandboxEverything.level` is such a
  variant: every base room and the original spell and trap set are available from the start, there is no timer.
- The unlock progress is not saved: a loaded game starts again from the first room that is not yet available.

Check: `python source/tests/check_sandbox_start.py`.
