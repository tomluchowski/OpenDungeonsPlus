"""Check the forged icon atlas gui/ODIcons.png and the forged bar of gui/ODSkin.png.

Every icon named in the layouts must exist in the imageset inside the atlas, be opaque where it
has a body, and use warm colours: no cold (blue dominant) pixels in the frames and symbols.
"""
from pathlib import Path
import re
import sys
import numpy as np
from PIL import Image

repo = Path(__file__).resolve().parents[2]
text = (repo / 'gui/ODIcons.imageset').read_text()
rects = {m[2]: tuple(int(m[i]) for i in (4, 5, 3, 1)) for m in re.finditer(
    r'<Image height="(\d+)" name="([^"]+)" width="(\d+)" xPos="(\d+)" yPos="(\d+)"', text)}
atlas = np.array(Image.open(repo / 'gui/ODIcons.png').convert('RGBA'), dtype=np.float32)
used = set()
for layout in (repo / 'gui').glob('*.layout'):
    used |= set(re.findall(r'OpenDungeonsIcons/([A-Za-z0-9_]+)', layout.read_text()))
runtime = {'GoldBadge', 'ManaBadge', 'NavHelp', 'NavSell', 'NavOptions', 'MapZoom', 'MiniMapRim', 'MiniMapNorth'}
runtime |= {n for n in used if n.startswith('MiniMapCorner')}
for name in sorted(used - runtime):
    assert name in rects, 'missing in imageset: ' + name
for name in ('NavigationCreatures', 'NavigationRooms', 'NavigationSpells', 'NavigationWorkshop', 'NavigationPanel',
             'NavigationObjectives', 'NavigationMessages', 'NavigationMessagesRead', 'MenuReturn',
             'HourglassIcon', 'CogIcon', 'HammerAnvilIcon'):
    assert name in rects, name
terrain = {'GoldButton', 'LavaButton', 'RockButton', 'WaterButton', 'DirtButton', 'ClaimedButton', 'GemButton'}
checked = 0
for name, (x, y, w, h) in rects.items():
    assert x + w <= 1024 and y + h <= 1024, name
    if name in terrain:
        continue
    tile = atlas[y:y + h, x:x + w]
    opaque = tile[..., 3] > 200
    assert opaque.sum() > w * h * 0.04, name + ' has no body'
    rgb = tile[opaque][:, :3]
    cold = (rgb[:, 2] > rgb[:, 0] + 12).mean()
    assert cold < 0.02, '%s has cold pixels: %.3f' % (name, cold)
    assert rgb[:, 0].mean() >= rgb[:, 2].mean(), name + ' is not warm'
    checked += 1
# Slot and status icons are square tiles, not round medallions: the corners of the tile body are opaque
# where a circle would be empty, the cell corner itself is rounded off, and the motif fills the tile.
sys.path.insert(0, str(repo / 'tools'))
import forged_motifs as fm
squares = [n for n, spec in fm.ICONS.items() if spec['kind'] is not None]
assert len(squares) >= 36, len(squares)
for name in squares:
    x, y, w, h = rects[name]
    a = atlas[y:y + h, x:x + w, 3]
    q = int(round(w * 0.13))
    assert a[0, 0] < 60, name + ' has an unrounded corner'
    assert min(a[q, q], a[q, w - 1 - q], a[h - 1 - q, q], a[h - 1 - q, w - 1 - q]) > 240, name + ' is not square: corners are empty'
    assert min(a[1, w // 2], a[h - 2, w // 2], a[h // 2, 1], a[h // 2, w - 2]) > 240, name + ' has no edge on all four sides'
    inner = atlas[y + h // 4:y + 3 * h // 4, x + w // 4:x + 3 * w // 4, :3]
    assert inner.std(axis=(0, 1)).max() > 12, name + ' has no motif in the tile'
skin = np.array(Image.open(repo / 'gui/ODSkin.png').convert('RGBA'), dtype=np.float32)
skin_set = (repo / 'gui/ODSkin.imageset').read_text()
m = re.search(r'height="(\d+)" name="ForgedBar" width="(\d+)" xPos="(\d+)" yPos="(\d+)"', skin_set)
h, w, x, y = (int(m[i]) for i in (1, 2, 3, 4))
bar = skin[y:y + h, x:x + w]
assert bar[..., 3].min() == 255 and (bar[..., 2] > bar[..., 0] + 5).mean() < 0.01
game = (repo / 'gui/ModeGame.layout').read_text()
assert game.count('OpenDungeonsSkin/ForgedBar') == 2 and 'FFB98B56' not in game
# The bar is a run of segments with fine joints: several segments differ, only some carry a vent slot.
skin_rects = {m[2]: tuple(int(m[i]) for i in (4, 5, 3, 1)) for m in re.finditer(
    r'<Image height="(\d+)" name="([^"]+)" width="(\d+)" xPos="(\d+)" yPos="(\d+)"', skin_set)}
assert w >= 384 and w % 64 == 0, 'the bar must be several segments wide'
segments = [bar[:, i * 64:(i + 1) * 64, :3] for i in range(w // 64)]
assert min(np.abs(segments[i] - segments[j]).mean() for i in range(len(segments)) for j in range(i + 1, len(segments))) > 1.0,     'bar segments repeat exactly'
mid = [seg[20:28, 16:48, :3].mean() for seg in segments]
vents = [m for m in mid if m < 0.85 * np.median(mid)]
assert 1 <= len(vents) <= len(segments) // 2, 'vent slots must be present in only some segments'
joint = bar[12:36, 64, :3].mean()
assert joint < bar[12:36, 60, :3].mean() * 0.8, 'joints between segments are fine dark lines'
# Research tiles: material states instead of flat colour squares.
def tile(name):
    x, y, tw, th = skin_rects[name]
    return skin[y:y + th, x:x + tw]
luma = lambda t: (t[..., :3] * [0.3, 0.59, 0.11]).sum(axis=2)
locked, learned, working, queued, focus = (tile('ResearchTile' + n) for n in ('Locked', 'Learned', 'Working', 'Queued', 'Focus'))
for name, t in (('Locked', locked), ('Learned', learned), ('Working', working), ('Queued', queued)):
    assert t[..., 3].min() > 0 or t[0, 0, 3] < 255, name
    body = t[6:-6, 6:-6, :3]
    assert body.std() > 2.0, 'research tile %s is a flat square' % name
    rgb = t[t[..., 3] > 200][:, :3]
    assert (rgb[:, 2] > rgb[:, 0] + 12).mean() < 0.01, 'research tile %s has cold pixels' % name
assert luma(locked)[8:-8, 8:-8].mean() < luma(learned)[8:-8, 8:-8].mean() * 0.6, 'locked must be much darker than learned'
assert luma(locked).mean() < luma(queued).mean() < luma(working).mean(), 'locked < queued < working brightness'
assert (working[..., 0] - working[..., 2]).mean() > 60, 'working glows in ember colours'
edge = learned[1:4, 10:-10, :3].mean(axis=(0, 1))
assert edge[0] > 150 and edge[0] > edge[2] * 1.6, 'learned has a warm gold or bronze edge'
assert focus[30, 30, 3] < 40 and focus[1, 10:-10, 3].min() > 200, 'focus is a rim with a clear centre'
assert (focus[1, 10:-10, 0] > 200).all(), 'focus rim is gold'
# Small stretched images have a ring of their own edge colour around them, so the texture filter never blends with a frame piece.
for name in ('StaticBackdrop', 'EditBoxMiddle', 'ComboboxEditBackground', 'MenuMiddle', 'MultiListMiddle', 'TooltipMiddle',
             'ComboboxListBackdrop', 'VerticalSliderMiddle', 'HorizontalSliderMiddle', 'TabContentPaneMiddle'):
    x, y, tw, th = skin_rects[name]
    inner = skin[y:y + th, x:x + tw, :3]
    assert inner.std(axis=(0, 1)).max() < 12, name + ' carries noise that stretches into streaks'
    assert np.abs(skin[y - 1, x:x + tw, :3] - skin[y, x:x + tw, :3]).max() < 1 and         np.abs(skin[y:y + th, x - 1, :3] - skin[y:y + th, x, :3]).max() < 1, name + ' has no clean edge ring'
    assert skin[y - 1:y + th + 1, x - 1:x + tw + 1, 3].min() == 255, name + ' ring is not opaque'
# Layout and look'n'feel wiring of the states
look = (repo / 'gui/OD.looknfeel').read_text()
scheme = (repo / 'gui/ODSkin.scheme').read_text()
game_code = (repo / 'source/modes/GameMode.cpp').read_text()
assert 'ResearchBackgroundColour' not in look and 'ResearchBackgroundColour' not in game_code
for n in ('Locked', 'Learned', 'Working', 'Queued'):
    assert 'OpenDungeonsSkin/ResearchTile' + n in game_code, n
assert 'ResearchTileFocus' in look and 'OD/EventPanel' in scheme
for f in ('WindowGameEvent.layout', 'WindowEvent.layout'):
    assert 'type="OD/EventPanel" name="GameEventText"' in (repo / 'gui' / f).read_text(), f
sym = look[look.index('<WidgetLook name="OD/MenuSymbolButton"'):]
sym = sym[:sym.index('</WidgetLook>')]
assert all(s in sym for s in ('plate_normal', 'plate_hover', 'plate_pushed', 'ButtonTopLeftHighlight', 'ButtonTopLeftPushed')),     'symbol buttons sit on forged plates with hover and pressed'
tree = (repo / 'gui/WindowSkillTree.layout').read_text()
assert 'FrameColours' not in tree and 'SelectionBrush' not in tree, 'skill tree columns use no flat tints'
material = (repo / 'materials/scripts/SquareSelector.material').read_text()
assert 'material SquareSelector' in material and 'emissive 1.0 0.70' in material
assert 'setMaterialName("SquareSelector")' in (repo / 'source/render/RenderManager.cpp').read_text()
# Third pass: one symbol style for every close and confirm button, iron scrollbars and list frames in the menus, warm text.
for layout in (repo / 'gui').glob('*.layout'):
    body = layout.read_text()
    assert 'OpenDungeonsSkin/CloseButton' not in body, layout.name + ' still uses the old close button image with its own frame'
    for button in re.finditer(r'<Window type="OD/MenuSymbolButton"[^>]*>(.*?)</Window>', body, re.S):
        assert set(re.findall(r'OpenDungeonsSkin/', button[1])) == set(), layout.name + ': symbol buttons use icon atlas images'
def look_block(name):
    a = look.index('<WidgetLook name="%s"' % name)
    end = look.find('</WidgetLook>', a)
    return look[a:] if end < 0 else look[a:end]
thumb = look_block('OD/MenuScrollbarThumb')
assert 'SelectionBrush' not in thumb and 'inherits="OD/VerticalScrollbarThumb"' in thumb, 'menu scrollbar thumb is the forged iron thumb'
for name in ('OD/MenuListbox', 'OD/MenuMultiColumnList'):
    block = look_block(name)
    assert 'section="main"' in block, name + ' has the forged inset frame'
assert 'FFE8DCC0' in look_block('OD/StaticText') and 'FFFFFFFF' not in look_block('OD/Tooltip').split('name="label"')[1].split('</TextComponent>')[0],     'labels and tooltips use the warm bone text colour'
for f in ('source/entities/Creature.cpp', 'source/entities/Tile.cpp', 'source/gamemap/GameMap.cpp', 'source/modes/GameMode.cpp',
          'source/network/ChatEventMessage.cpp'):
    code = (repo / f).read_text()
    assert 'CCBBBBFF' not in code and "colour='FFFFFFFF'" not in code, f + ' has a cold or hard white text colour'
print('FORGED ICONS OK: %d icons checked, %d square tiles' % (checked, len(squares)))
