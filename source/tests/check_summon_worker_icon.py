"""Check the summon worker icon: shared image binding and the warm worker emblem in the icon atlas."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
import numpy as np
from PIL import Image

repo = Path(__file__).resolve().parents[2]
gui = (repo / 'source/render/Gui.cpp').read_text()
game = (repo / 'source/modes/GameMode.cpp').read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


refresh = function(game, 'void GameMode::refreshSkillButtonState(')
assert 'getCreatureHandIconImage' not in refresh
assert 'getWorkerClassToSpawn' not in refresh
assert 'mRootWindow->getChild(button)->getProperty("NormalImage")' in game
for filename, prop in [('WindowTabSpells.layout', 'NormalImage'), ('WindowSkillTree.layout', 'ButtonImage')]:
    button = ET.parse(repo / 'gui' / filename).find('.//Window[@name="SummonWorkerButton"]')
    assert button.find(f'Property[@name="{prop}"]').get('value') == 'OpenDungeonsIcons/SummonWorkerButton'
assert 'createSummonWorkerIcon' not in gui and 'colourNavigationAtlas' not in gui
m = re.search(r'height="(\d+)" name="SummonWorkerButton" width="(\d+)" xPos="(\d+)" yPos="(\d+)"', (repo / 'gui/ODIcons.imageset').read_text())
h, w, x, y = (int(m[i]) for i in (1, 2, 3, 4))
tile = np.array(Image.open(repo / 'gui/ODIcons.png').convert('RGBA'), dtype=np.float32)[y:y + h, x:x + w]
opaque = tile[..., 3] > 200
assert (w, h) == (128, 128) and opaque.sum() > 0.6 * w * h
assert tile[..., 3][0, 0] == 0 and tile[..., 3][16, 16] > 240 and opaque.sum() > 0.9 * w * h, 'square tile with slightly rounded corners, no round medallion'
rgb = tile[opaque][:, :3]
assert (rgb[:, 2] > rgb[:, 0] + 12).mean() < 0.01, 'no cold blue enamel'
centre = tile[40:80, 40:88, :3].reshape(-1, 3)
assert centre[:, 0].mean() > centre[:, 2].mean() + 40, 'ember coloured worker'
print('SUMMON ICON OK')
