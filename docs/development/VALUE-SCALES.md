# Own value scales

The prices, recharge times and reward odds of this project follow scales of its own. They are
rated by what an item does in play (strength, reach, duration, upkeep) and rounded to the step
of its scale, so a single value is never a copy of anything outside the project.

## Spell prices and recharge (`config/spells.cfg`)

- A price is a whole number of 50 mana steps. The step count is the rating of the spell: Possess 9
  steps (it also drains per second afterwards), the worker 28, a single creature effect 36 to 53,
  heal, eye, lightning and explosion 92 to 132, call to war 184, create gold 280, the hen spell 212,
  the defector 432, tremor 552, inferno 928 and the champion 2,160 steps (the free period stays
  price / drain per second = 48 seconds).
- The recharge is the number of turns before the spell can be cast again: 11 to 22 turns for
  creature spells, 76 to 92 turns for conversion, transformation and area spells, 0 for spells that
  are limited by their price alone.

## Room prices (`config/rooms.cfg`)

- `CostPerTile` is gold per tile, rated by usefulness and upkeep of the room. The treasury is the
  cheapest room and anchors the scale (72), living and storage rooms follow (90 to 110), working
  rooms sit around 165 to 240, the casino, bridges and torture room between 180 and 540, the crypt
  and the temple are the most expensive (620 and 930).

## Pit and company moods (`config/rooms.cfg`, `config/creatures.cfg`)

- The pit mood cap is 1,500 mood points. The victor bonus is 1,300 (87 percent of the cap), a
  spectator gains 28 and a creature alone in the pit loses 22 points per second. A hated
  neighbour costs 9 mood points per turn.

## Heart reward pool (`source/rooms/RoomDungeonTemple.cpp`)

- Each of the twelve specials has an integer weight, the weights add up to 100, so a weight is the
  chance in percent: gold 15, mana 14, map view 12, new workers 10, make happy 9, heal all 8, make
  safe 7, weaken walls 7, level up 6, stun workers 6, make unhappy 4, kill creatures 2. Supplies are
  common, effects that change the whole dungeon at once are rare.
