"""Check the five fixed camera zoom levels of the camera.

The zoom must have exactly five levels by default. The first level is the old minimum camera height (3.0),
the last level is the old maximum (16.0) and the levels in between are evenly spaced. The number of levels,
the minimum and maximum height and the animation speed are read from the user config the camera already
reads (the input values), with these defaults. The input handlers must move the zoom through zoomStep and
not change the camera height directly.
"""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[2]
header = (repo / 'source/camera/CameraManager.h').read_text()
manager = (repo / 'source/camera/CameraManager.cpp').read_text()
config = (repo / 'source/utils/ConfigManager.h').read_text()
game_mode = (repo / 'source/modes/GameMode.cpp').read_text()

OLD_MIN = 3.0
OLD_MAX = 16.0


def constant(text, name):
    match = re.search(r'const\s+(?:Ogre::Real|int)\s+' + name + r'\s*=\s*([0-9.]+)\s*;', text)
    assert match, name + ' is missing'
    return float(match.group(1))


minimum = constant(header, 'MIN_CAMERA_Z')
maximum = constant(header, 'MAX_CAMERA_Z')
count = int(constant(manager, 'DEFAULT_ZOOM_LEVELS'))
constant(manager, 'DEFAULT_ZOOM_SPEED')

assert minimum == OLD_MIN, 'default minimum zoom changed: %s' % minimum
assert maximum == OLD_MAX, 'default maximum zoom changed: %s' % maximum
assert count == 5, 'expected 5 default zoom levels, found %d' % count

for key in ('ZOOM_LEVELS', 'ZOOM_MIN_HEIGHT', 'ZOOM_MAX_HEIGHT', 'ZOOM_SPEED'):
    assert re.search(r'const std::string ' + key + r' = "', config), key + ' is not a config key'
    assert 'Config::' + key in manager, key + ' is not read by the camera'

formula = re.search(r'CameraManager::getZoomLevelHeight\(int level\) const\s*\{\s*return\s+mZoomMinZ \+ '
    r'\(mZoomMaxZ - mZoomMinZ\) \* static_cast<Ogre::Real>\(level\)\s*/ '
    r'static_cast<Ogre::Real>\(mZoomLevels - 1\);', manager)
assert formula, 'getZoomLevelHeight is not the even-spacing formula'

levels = [minimum + (maximum - minimum) * i / (count - 1) for i in range(count)]
assert len(levels) == 5
assert levels[0] == OLD_MIN and levels[-1] == OLD_MAX
steps = [round(levels[i + 1] - levels[i], 6) for i in range(count - 1)]
assert len(set(steps)) == 1, 'zoom levels are not evenly spaced: %s' % steps
assert steps[0] == round((OLD_MAX - OLD_MIN) / 4, 6)

assert 'mZChange' not in manager and 'mZChange' not in header, 'continuous zoom height change is still there'
assert re.search(r'if\(zoom != 0\.0f && zoom != mControlZoom\)\s*zoomStep\(', manager), 'zoom keys do not step'
assert game_mode.count('getCameraManager()->zoomStep(') == 1, 'mouse wheel does not step'
assert 'zoomBy(' in game_mode, 'zoom drag handler is missing'

print('camera zoom levels ok:', ', '.join('%g' % level for level in levels))
sys.exit(0)
