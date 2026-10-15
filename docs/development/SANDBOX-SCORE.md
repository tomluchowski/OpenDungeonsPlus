# Sandbox score, bonus objectives and realms

A sandbox level can have a score, bonus objectives and a target. The server keeps them in `SandboxMode`
(`source/gamemap/SandboxMode.{h,cpp}`); the clients get the numbers with the server notification
`sandboxStatus` (once per second when something changed) and show them on the HUD (score next to the time limit,
next room and its timer) and in the sandbox window.

## Score

| Event | Points |
|---|---|
| A hero killed (by the keeper's creatures and traps) | 60 |
| A tile of land owned (also taken away again when the tile is lost) | 4 |
| A gold tile mined out | 2 |
| An item made in a workshop | 8 |
| A creature that comes through a portal | 15 |
| A creature converted in the torture chamber | 30 |

The land already owned at the start does not count. The score never goes below 0. The heroes of the toolbox
count like the ones of the waves.

Wave rewards: a wave beaten is announced with the points its heroes gave ("Wave 3 was beaten. Reward: 400 points.").
There is no other reward per wave.

## Bonus objectives

A realm has up to two bonus objectives that give a lump sum once. They are written as lines in the `[Info]` block of
the level file:

    SandboxBonus <kind> <count> <points> <arg or -> <text>

Kinds: `slaps` (count), `allSpells`, `happy` (count), `rooms` (count), `levelAtLeast` (count creatures of at least
the level in arg), `creatures` (count fighting creatures), `traps`, `doors`, `roomTiles` (count tiles of the room
type named in arg), `creatureClass` (count creatures of the class in arg), `creatureNamed` (the keeper owns the
creature of the level named in arg), `gold` (count gold held at once), `goldTiles` (count gold tiles mined out),
`roomTypes` (count different room types owned), `prisoners` (count prisoners held at once) and `trapsFired` (count
times a trap of the keeper fired). The server checks them once a second.

## Realms

More info lines make a sandbox level a realm:

    SandboxRealm <name>      # the name the progress file knows the realm by (the file name without extension)
    SandboxTarget <points>   # the realm is complete when the score stays at the target for 5 seconds
    SandboxNext <file>       # level file after the realm, relative to the levels folder

When the realm is complete a window asks whether to proceed to the next realm; "Stay here" keeps the game running
(the realm is not asked again). The realm is stored as complete in `sandbox-progress.txt` in the user data folder
(one realm name per line). A realm that another realm names as its next one is locked in the level list until that
other realm is complete; to open one without playing, write the name of the realm before it in that file.

The eight realms are `levels/skirmish/Quietcrag.level`, `Lanternreach`, `Splitstone`, `Barrackdeep`, `Embervault`,
`GrandDelve`, `Gloomkeep` and `Wyrmhollow`, in that order (realm index 1 to 8).

Targets and bonuses follow one formula. Target = 8000 + 6000 x realm index, rounded to 500. Each of the two bonus
objectives of a realm gives 20 % of the target, rounded to 500, so the bonuses together never reach the target.

| Realm | Target | Bonus objectives (points each) |
|---|---|---|
| Quietcrag | 14000 | four rooms, six fighting creatures (3000) |
| Lanternreach | 20000 | eight rooms, twelve rooms (4000) |
| Splitstone | 26000 | 12000 gold held, 20 gold tiles mined (5000) |
| Barrackdeep | 32000 | thirty fighting creatures, ten creatures of level 4 (6500) |
| Embervault | 38000 | 20000 gold held, sixteen rooms (7500) |
| GrandDelve | 44000 | a temple of 36 tiles, eight room types (9000) |
| Gloomkeep | 50000 | ten prisoners held, twenty traps fired (10000) |
| Wyrmhollow | 56000 | 30000 gold held, three creatures of level 8 (11000) |

Every realm starts with the Dormitory, the Hatchery and the worker summon spell and unlocks the other rooms on the
timetable of `SANDBOX-START.md`. The hero faction of a realm has a wave portal; the heroes of the toolbox come
from the sandbox window.

Check: `python source/tests/check_sandbox_score.py`.
