# Temple sacrifice

Creatures dropped into the pool of a temple (a temple tile with the room on all eight sides)
are sacrificed during the next upkeep. The sacrifice itself gives no mana and no research
points. The only possible outcomes are a new creature or a special (a gift box). Praying
creatures still give mana; that part is separate (see `TemplePrayerMana*` in `config/rooms.cfg`).

## Rules

- The pool of a temple remembers the last three sacrificed creatures (class name and level).
  Older entries are dropped. The queue is not saved, after loading a game it is empty.
- After every sacrifice the recipes are checked in the order of `TempleRecipe1`, `TempleRecipe2`,
  and so on. The first recipe whose inputs, in the order they were sacrificed, are the newest
  entries of the queue wins. A recipe with more inputs than the queue holds never matches.
- A matching recipe empties the queue and gives its result. A sacrifice that matches no recipe
  gives nothing and stays in the queue.
- Because the first match wins, an earlier recipe can hide a later one that starts with the
  same inputs.
- The level of a new creature is the average level of the sacrificed inputs, rounded down
  (no +1). It is created on the pool tile and not created if the keeper is at the creature limit.
- The result `ManaBoost` puts a mana gift box on the pool tile. It has to be carried to the
  dungeon temple like the other gift boxes; the amount is the default amount of the mana box.
- Workers cannot be sacrificed. Prisoners cannot be sacrificed yet (only creatures of the
  keeper's own seat are accepted).

## Config

`TempleRecipeCount` and `TempleRecipeN` in `config/rooms.cfg`. A recipe line is
`Input+Input[+Input]=Result` with creature definition names from `config/creatures.cfg`.
The result is a creature definition name, `ManaBoost` or the fork specific `Workers`
(`TempleWorkersGiven` workers of the keeper, default 10).

## Recipes

All recipes are fork recipes (23 rows).

| Inputs | Result |
|---|---|
| Elf + Elf | Orc |
| Gnome + Gnome + Gnome | Mana Boost |
| Lich + Lich | Pit Demon |
| Troll + Troll | Cultist |
| Lava Spawn + Lava Spawn | Lizard Man |
| Cultist + Cultist | Goblin |
| Rat + Rat | Lava Spawn |
| Skeleton + Skeleton | Dark Elf |
| Orc + Orc | Lich |
| Pit Demon + Pit Demon | Rat |
| Lizard Man + Lizard Man | Skeleton |
| Orc + Cultist + Cave Hornet, any order (6 rows) | Workers |
| Pit Demon + Dark Elf + Cultist, any order (6 rows) | Workers |

The queue matches in throw order, so the two three-input worker recipes are listed in all six
orders. The two-creature rows are order free anyway.

## Check

`source/tests/check_temple_sacrifice.py`
