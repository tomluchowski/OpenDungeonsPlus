"""Every shipped level must carry only 5x5 dungeon hearts (25 tiles, a 3x3 core plus the
16-tile treasury ring). Only old savegames may still load a 3x3 heart.

Usage: check_level_hearts.py [levels-directory]"""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[2]
levels = Path(sys.argv[1]) if len(sys.argv) > 1 else repo / 'levels'

HEART_TILES = 25
failures = 0
hearts = 0
files = sorted(levels.rglob('*.level'))
if not files:
    print('FAIL: no level files found in %s' % levels)
    sys.exit(1)

for path in files:
    lines = path.read_text(errors='replace').splitlines()
    i = 0
    while i < len(lines):
        if lines[i].strip() != '[Room]':
            i += 1
            continue
        header = lines[i + 1].split('\t') if i + 1 < len(lines) else []
        tiles = []
        i += 2
        while i < len(lines) and lines[i].strip() != '[/Room]':
            match = re.match(r'^(\d+)\s+(\d+)\s*$', lines[i])
            if match:
                tiles.append((int(match.group(1)), int(match.group(2))))
            i += 1
        if len(header) < 2 or 'DungeonTemple' not in header[1]:
            continue
        hearts += 1
        name = '%s %s' % (path.relative_to(levels).as_posix(), header[1])
        xs = [t[0] for t in tiles]
        ys = [t[1] for t in tiles]
        square = (len(set(tiles)) == HEART_TILES and max(xs) - min(xs) == 4 and max(ys) - min(ys) == 4)
        if len(tiles) != HEART_TILES or not square:
            print('FAIL: %s has %d tiles, expected a 5x5 heart of %d' % (name, len(tiles), HEART_TILES))
            failures += 1

print('%d hearts in %d levels, %d failures' % (hearts, len(files), failures))
sys.exit(1 if failures else 0)
