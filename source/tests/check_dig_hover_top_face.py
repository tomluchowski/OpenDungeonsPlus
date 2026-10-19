"""Check that the dig hover box and the dig drag rectangle mark only the top face of a tile.

Text check, no compile and no game start. It fails for the old behaviour, where hovering an
earth block outlined a 3D block (bottom edges, vertical edges and framed side faces).
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


render = read('source/render/RenderManager.cpp')
preview = render[render.index('void RenderManager::rrDrawTilePreview('):render.index('void RenderManager::entitySlapped()')]
rectangle = render[render.index('static void drawTileRectangle('):render.index('void RenderManager::rrDrawTilePreview(')]
single = preview[preview.index('mTilePreview->begin("debug_draw", Ogre::RenderOperation::OT_LINE_LIST'):]

# Drag rectangle: every vertex of the outline and of the thick frame lies on one plane (z), the top.
for name, text in (('drag outline', rectangle),):
    assert 'Ogre::Vector3(minX - PREVIEW_TILE_HALF, minY - PREVIEW_TILE_HALF, z)' in text, name + ' corner is not on the top plane'
    assert 'Ogre::Vector3(minX - PREVIEW_BORDER_INNER_HALF, minY - PREVIEW_BORDER_INNER_HALF, z)' in text, name + ' frame is not on the top plane'
    assert '0.04f' not in text, name + ' has a vertex below the top plane'

# Single-tile hover (dig): the outline of a full tile is four top edges only.
line_block = single[single.index('OT_LINE_LIST'):single.index('mTilePreview->end();')]
cube = line_block[line_block.index('if(tile->isFullTile()'):]
assert re.search(r'if\(tile->isFullTile\(\) && !digging\)', cube),     'bottom and vertical edges of a full tile must not be drawn for a dig selection'

# Single-tile hover (dig): the thick frame is the top ring only, no side faces.
tri_block = single[single.index('OT_TRIANGLE_LIST'):]
assert 'Offset side frames' not in tri_block, 'side face frames of the wall are still drawn'
assert 'face[' not in tri_block and 'inset[' not in tri_block, 'side face frame vertices are still emitted'
assert 'getWallFramePoint' not in render, 'side face frame helper is still present'
assert re.search(r'outer\[\] = \{\{x-0\.5f,y-0\.5f,z\}', tri_block), 'frame ring is not on the top plane z'
assert 'x-0.44f,y-0.44f,z' in tri_block, 'inner frame edge is not on the top plane z'
# The frame stays thick: same 0.06 band as before.
assert 'x-0.5f' in tri_block and 'x-0.44f' in tri_block

# A dig selection (hover and drag) reaches the preview as digging.
game_mode = read('source/modes/GameMode.cpp')
assert re.search(r'const bool digging = mPlayerSelection\.getCurrentAction\(\) == SelectedAction::none \|\|\s*'
                 r'mPlayerSelection\.getCurrentAction\(\) == SelectedAction::selectTile;', game_mode)
print('check_dig_hover_top_face ok: dig hover and drag mark only the top face, frame width unchanged')
