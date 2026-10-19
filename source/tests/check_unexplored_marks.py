"""Check type-neutral marking of unexplored tiles and the fog-surface mark. Text check, no game start."""
from pathlib import Path
import ast
import math
import re

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
fog_shader = read('shaders/DirtTileInstanced.frag')
assert render.count('(isMarked ? mInstanceManagerMarkedFog : mInstanceManagerDirt)') == 3
assert 'tile.setFogOfWarMesh(tile.getFogOfWarMesh(), isMarked)' in render
rectangle = render[render.index('static void drawTileRectangle('):render.index('void RenderManager::rrDrawTilePreview(')]
assert 'if(!digging)' in rectangle and 'OT_TRIANGLE_LIST' in rectangle
assert 'PREVIEW_BORDER_INNER_HALF' in rectangle and 'PREVIEW_TILE_HALF' in rectangle
assert 'PREVIEW_BORDER_INNER_HALF = 0.44f' in render and 'PREVIEW_TILE_HALF = 0.5f' in render
assert 'minX - PREVIEW_BORDER_INNER_HALF' in rectangle and 'maxX + PREVIEW_BORDER_INNER_HALF' in rectangle
assert 'minX - PREVIEW_TILE_HALF' in rectangle and 'maxX + PREVIEW_TILE_HALF' in rectangle
tile = read('source/entities/Tile.cpp')
assert 'DIG_MARK_COLOUR(0.65f, 0.45f, 1.0f, 0.5f)' in tile
assert 'mix(texelColor, outputColor.rgb, outputColor.a)' in fog_shader

# Flat marked-fog geometry is independent of tile type and exactly shares every adjacent edge.
mesh = render[render.index('void createMarkedFogMesh('):render.index('RenderManager::RenderManager(')]
assert '"FogOfWarDirt.mesh"' in mesh and 'getBounds().getMaximum().z' in mesh
assert 'getTileVisual' not in mesh and 'getType' not in mesh
corner_text = re.search(r'const Ogre::Vector3 corners\[\] = \{(.*?)\n    \};', mesh, re.S).group(1)
corner_text = corner_text.replace('height', '1.33211').replace('f', '').replace('{', '[').replace('}', ']')
corners = ast.literal_eval('[' + corner_text + ']')
assert len(corners) == 8
assert {p[0] for p in corners} == {-0.5, 0.5}
assert {p[1] for p in corners} == {-0.5, 0.5}
assert {p[2] for p in corners} == {0, 1.33211}
for width, height in [(1, 1), (4, 1), (4, 4), (3, 7)]:
    # Every cell shares the same straight edge and top plane with its neighbours, without a gap/overlap.
    for x in range(width):
        for y in range(height):
            top = {(x + p[0], y + p[1], p[2]) for p in corners if p[2] > 0}
            if x + 1 < width:
                next_top = {(x + 1 + p[0], y + p[1], p[2]) for p in corners if p[2] > 0}
                assert len(top & next_top) == 2
            if y + 1 < height:
                next_top = {(x + p[0], y + 1 + p[1], p[2]) for p in corners if p[2] > 0}
                assert len(top & next_top) == 2
    thickness = 0.5 - 0.44
    assert math.isclose(thickness, 0.06)
    # One outer frame, with the same width as the existing single-tile frame, no internal tile lines.
    ring_area = width * height - (width - 2 * thickness) * (height - 2 * thickness)
    assert ring_area > 0
assert 'for(Tile* tile' not in rectangle[rectangle.index('if(!digging)'):]
vertex = read('shaders/DirtTileInstanced.vert')
flat_branch = vertex[vertex.index('if(final_color.a > 0.0)'):vertex.index('    vec3 local_normal')]
assert 'deform(' not in flat_branch and 'return;' in flat_branch
assert 'outputColor.a > 0.0 ? FragPos.xy : out_UV0.st' in fog_shader
assert 'wasMarked != isMarked' in render
assert 'destroyInstanceManager(mInstanceManagerMarkedFog)' in render
# Releasing clears the temporary frame before the persistent marking is requested.
release = game_mode[game_mode.index('void GameMode::handlePlayerActionSelectTile()'):]
release = release[:release.index('void GameMode::resetSkillTree()')]
assert release.index('unselectAllTiles();') < release.index('ClientNotificationType::askMarkTiles')
assert 'markPass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA)' in render
<<<<<<< HEAD
assert 'Ogre::ColourValue markColor(Tile::DIG_MARK_COLOUR)' in render
=======
assert 'Ogre::ColourValue markColor(0.65f, 0.45f, 1.0f, 0.5f)' in render
# Every fog tile is markable everywhere, on the client as well as on the server, and the mark is shown.
set_mark = tile[tile.index('void Tile::setMarkedForDigging('):]
set_mark = set_mark[:set_mark.index('bool Tile::getMarkedForDigging')]
# The client never flags its players as human, so the client branch must not depend on getIsHuman().
assert 'setIsHuman' not in read('source/network/ODClient.cpp'), 'client sets human flags: re-check the client branch'
server_branch = set_mark[set_mark.index('if(getGameMap()->isServerGameMap())'):set_mark.index('else')]
client_branch = set_mark[set_mark.index('else'):set_mark.index('if (ss && !unexplored')]
assert 'getIsHuman' in server_branch and 'everVisitedFlagPool' in server_branch
assert 'getIsHuman' not in client_branch, 'client fog marks must not depend on the human flag the client never has'
assert '!getEverVisible() || getHasFogOfWar()' in client_branch
assert 'if (ss && !unexplored && !isDiggable(pp->getSeat()))' in set_mark
# No adjacency, distance or neighbour restriction in the client selection or the server acceptance.
for name, text in (('client selection', release[:release.index('unselectAllTiles();')]),
                   ('server mark', player[player.index('void Player::markTilesForDigging('):player.index('void Player::upkeepPlayer(')])):
    for forbidden in ('getNeighbor', 'isAdjacent', 'adjacent', 'Adjacent', 'getAllNeighbors'):
        assert forbidden not in text, name + ' restricts marking by ' + forbidden
# The mark of a fogged tile is drawn whenever it exists: the fog surface is switched on isMarked alone.
assert 'if(tile.getHasFogOfWar() && tile.getFogOfWarMesh() != nullptr)' in render
assert 'const bool wasMarked = tile.getFogOfWarMesh()->getCustomParam(0).w > 0.0f;' in render
print('check_unexplored_marks ok: thick outer frame, seamless flat marked fog, neutral tooltip and transparent marking')
