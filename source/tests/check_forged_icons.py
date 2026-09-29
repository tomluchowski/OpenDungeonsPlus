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
print('FORGED ICONS OK: %d icons checked, %d square tiles' % (checked, len(squares)))
