"""Check that unexplored tiles cannot be marked for digging and that a fogged tile never shows a mark colour,
so the yellow mark does not reveal gold or dirt walls in the unexplored area. Text check, no game start."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


game_mode = read('source/modes/GameMode.cpp')
assert 'tile->getEverVisible() && tile->isDiggable(player->getSeat())' in game_mode

render = read('source/render/RenderManager.cpp')
assert render.count('createInstancedEntity("DirtInstanced"), false)') == 2
assert 'createInstancedEntity("DirtInstanced"), isMarked)' not in render

print('check_unexplored_marks ok')
