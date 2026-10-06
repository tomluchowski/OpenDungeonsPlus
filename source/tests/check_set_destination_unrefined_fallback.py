"""Structure check: Creature::setDestination must not fail when only the room object refinement removed the walk path.

setWalkPath refines the tile path around room objects. When the refinement finds no route it clears the
whole path, so isMoving() is false although a tile path exists. setDestination then has to walk the
unrefined tile path (pathAlreadyRefined = true) and may only return false when the tile path itself is empty.
Optional argument: path of a Creature.cpp to check (default: the one in this repository).
"""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[2]
source = Path(sys.argv[1]) if len(sys.argv) > 1 else repo / 'source/entities/Creature.cpp'
text = source.read_text(encoding='utf-8')

match = re.search(r'bool Creature::setDestination\(Tile\* tile\)\n\{.*?\n\}\n', text, re.S)
if match is None:
    print('FAIL missing Creature::setDestination')
    sys.exit(1)
body = match[0]

failures = []
guard = re.search(r'if\(!isMoving\(\) && posTile != tile\)\s*(\{.*?\n    \}|[^\n]*\n[^\n]*\n)', body, re.S)
if guard is None:
    failures.append('setDestination lost the not-moving check')
else:
    block = guard[1]
    first_walk = body.index('setWalkPath(')
    if body.index('if(!isMoving()') < first_walk:
        failures.append('the not-moving check must come after the first setWalkPath')
    if 'path.empty()' not in block:
        failures.append('a refinement that cleared the path must be told apart from an empty tile path (path.empty())')
    retry = re.search(r'setWalkPath\([^;]*,\s*path,\s*true,\s*true\)', block)
    if retry is None:
        failures.append('setDestination must walk the unrefined tile path (setWalkPath with pathAlreadyRefined = true) when refinement removed the route')
    elif 'path.empty()' in block and block.index('path.empty()') > retry.start():
        failures.append('the empty tile path test must come before the unrefined retry')
    if not re.search(r'setWalkPath\([^;]*\);\s*\n\s*if\(!isMoving\(\)\)\s*\n\s*return false;', block):
        failures.append('the result of the unrefined retry must still be checked with isMoving()')

for failure in failures:
    print('FAIL ' + failure)
if failures:
    sys.exit(1)
print('OK setDestination walks the unrefined tile path when refinement removes the route')
