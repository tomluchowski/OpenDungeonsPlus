"""Check type-neutral marking of unexplored tiles and the fog-surface mark. Text check, no game start."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


game_mode = read('source/modes/GameMode.cpp')
assert 'Unexplored. Click or drag to mark for digging.' in game_mode
assert '!tile->getEverVisible() || tile->getHasFogOfWar() || tile->isDiggable' in game_mode

player = read('source/game/Player.cpp')
assert 'if(unexplored || getSeat()->isTileDiggableForClient(tile))' in player
assert 'if(!unexplored && !tile->isDiggable(getSeat()))' in player
seat = read('source/game/Seat.cpp')
assert 'mPlayer->markTilesForDigging(false, std::vector<Tile*>(1, tile), false)' in seat

render = read('source/render/RenderManager.cpp')
assert render.count('createInstancedEntity("DirtInstanced"), isMarked)') == 2
assert 'tile.setFogOfWarMesh(tile.getFogOfWarMesh(), isMarked)' in render
preview = render[render.index('void RenderManager::rrDrawTilePreview('):render.index('bool RenderManager::getKeeperHandPosition')]
rectangle = preview[preview.index('if(singleRectangle)'):preview.index('    return;\n    }')]
assert 'OT_TRIANGLE_LIST' not in rectangle
assert 'minX - 0.5f' in rectangle and 'maxX + 0.5f' in rectangle

print('check_unexplored_marks ok')
