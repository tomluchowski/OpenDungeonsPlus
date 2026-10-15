# AI levels

The skirmish seat page offers three computer levels (easy, normal, hard). They are built in
`source/ai/AIFactory.cpp` as `KeeperAI` instances.

## Level values

| Parameter (`KeeperAI`)                         | easy | normal | hard |
|------------------------------------------------|------|--------|------|
| `minFightersToAttack` (attack with more than N creatures) | 14 | 11 | 8 |
| `minCreatureLevel` (minimum level for an attack) | 2  | 4      | 7    |
| `minHpPercentToFight` (retreat at health, %)   | 25   | 20     | 12   |
| `reactionPercent` (scale of all AI cooldowns)  | 150  | 100    | 60   |
| `maxTrapTiles`                                 | 2    | 5      | 8    |
| `maxDoors`                                     | 1    | 3      | 5    |

The ordering is monotonic: a harder level attacks with fewer creatures, waits for higher
creature levels, retreats later, reacts faster and builds more traps and doors.

The AI starts an attack when it has more non-worker creatures than the count, and it does not
attack while more creatures than the minimum level number are below that level.
`KeeperAI::handleAttack` needs more healthy fighters than `minFightersToAttack` and waits while
more than `minCreatureLevel` healthy fighters are below level `minCreatureLevel`. Weak creatures
are not sent home; they join the call to war.

Other values: the cooldowns for defense, saving wounded creatures and looking for rooms, and the
retreat of a whole attack at 50 percent of the fighters lost.

## Not built

Threat superiority needed over the enemy, share of the creatures used in the first fight, call to
war threshold and removal rules, a chance of using traps and doors instead of a count, delay before
placing a researched room, maximum workers, imprison percentage, openness, wait after an attack, the
"only attack attackers" and "never attack" flags, and the other economy fields (mining until,
exploring, dig policies). No further levels are offered.

The check `source/tests/check_ai_levels.py` verifies the table above against the factory.
