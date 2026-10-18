"""Check exact ground and room-ownership source contracts; does not execute C++ or gameplay."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
runner = (root / 'source/gamemap/LevelScriptRunner.cpp').read_text(encoding='utf-8')
door = (root / 'source/traps/TrapDoor.h').read_text(encoding='utf-8')
tiles = (root / 'source/gamemap/TileContainer.cpp').read_text(encoding='utf-8')
parser = (root / 'source/gamemap/LevelScript.cpp').read_text(encoding='utf-8')

# Ground proofs reject walls, open water/lava, physical door locks and arena fences without needing an actor.
dry = runner[runner.index('bool isDryGroundPassable('):runner.index('bool areGroundPointsConnected(')]
assert 'tile->getFullness() != 0.0' in dry
assert 'TileType::water' in dry and 'TileType::lava' in dry and '!tile->getHasBridge()' in dry
assert 'trap->isDoor()' in dry and '!static_cast<TrapDoor*>(trap)->isGroundPassageOpen(tile)' in dry
assert 'trap->appearsAsWallForSeat(tile, seat)' in dry
assert 'RoomType::arena' in dry and 'getActualRoomTiles()' in dry
for surface in ('dirt', 'gold', 'rock', 'manaWell'):
    assert 'tile->getType() == TileType::' + surface in dry
assert '!isActivated(tile) || (!mIsLocked && !mIsLockedState)' in door

# The BFS uses only the current map and existing cardinal neighbors, even with no surviving workers.
bfs = runner[runner.index('bool areGroundPointsConnected('):runner.index('bool isConditionMet(')]
assert 'gameMap.getCreatures()' not in bfs and 'isAlive()' not in bfs and 'Creature*' not in bfs
assert 'gameMap.getSeatById(cond.mSeatId)' in bfs
assert 'isDryGroundPassable(seat, start, start)' in bfs and 'isDryGroundPassable(seat, start, end)' in bfs
assert 'tile->getAllNeighbors()' in bfs and 'visited.count(neighbor)' in bfs
assert 'isDryGroundPassable(seat, start, neighbor)' in bfs
assert 'isDiggable' not in bfs and 'throughDiggableTiles' not in bfs
neighbors = tiles[tiles.index('void TileContainer::setTileNeighbors('):]
neighbors = neighbors[:neighbors.index('\n}\n')]
assert 'i < 2' in neighbors and '--tempX;' in neighbors and '--tempY;' in neighbors

# A room footprint needs every original coordinate to be open, completely claimed and consistently room-owned.
mask = runner[runner.index('case LevelScriptConditionType::roomFootprintClaimed:'):runner.index('case LevelScriptConditionType::trapGone:')]
for required in ('tile->getFullness() != 0.0', 'tile->getSeat() != seat',
                 'tile->getClaimedPercentage() < 1.0', '!tile->isClaimedForSeat(seat)',
                 'tile->getCoveringRoom()', 'room == nullptr || room->getSeat() != seat'):
    assert required in mask, required
assert 'std::min(cond.mX1, cond.mX2)' in mask and 'std::min(cond.mY1, cond.mY2)' in mask
assert 'if(type == "groundconnected" || type == "roomclaimed")' in parser
assert 'os << "groundconnected\\t"' in parser and 'os << "roomclaimed\\t"' in parser
print('campaign exact ground/room source contracts: ok; C++ and gameplay not executed')
