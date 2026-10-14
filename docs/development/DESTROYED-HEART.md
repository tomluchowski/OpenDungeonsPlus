# Destroyed dungeon heart

When a dungeon heart is destroyed, the heart model disappears but its stone
platform with the circular emblem stays on the ground, dark and empty, for the
rest of the game (and through the defeat sequence).

## How it works

- `RoomDungeonTemple` is not removed any more. Once its heart health is 0,
  `doUpkeep` releases only the heart object (`removeAllBuildingObjects`), after
  `notifyRemoveAsked()` allows it. The floor tiles stay covered by the room, so
  their tile visual (`dungeonTempleRoom`, the emblem floor) is unchanged on the
  server and on every client.
- The room reports `getHP() == 0`. Every temple query (`GameMap::getRoomsByType`,
  `numRoomsByTypeAndSeat`, `Seat::computeSeatBeginTurn`) skips rooms with no
  health, so the seat has no dungeon temple and the existing last-temple defeat
  path (`Player::notifyNoMoreDungeonTemple`, `playerDefeated`, `levelStatistics`)
  runs exactly as before.
- The ruin cannot be attacked (`canAttackHeart` needs health and a heart object),
  cannot be sold, claimed, repaired or built upon (tiles stay covered), and its
  floor tiles are never released in game mode (`removeCoveredTile`).
- Saved games: the room is written with `HeartHealth10000 0`. On load the ruin gets no new
  heart object (`updateActiveSpots`) and restores only its floor
  (`restoreInitialEntityState`).
- The editor is unchanged.

## Check

`python source/tests/check_heart_ruin.py` (needs `cl` from the developer environment).
