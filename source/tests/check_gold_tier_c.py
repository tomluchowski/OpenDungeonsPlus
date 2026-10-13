"""Wiring checks for the pouring worker: the worker climbs the heap and tips its gold out, with the skeleton clips
when the mesh has them and with a procedural motion of the render node when it has not (client only, no compiler
needed)."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


render = read('source/render/RenderManager.cpp')
rules = read('source/render/TreasuryCreatureRules.h')

# Pour motion: render node only, started by the deposit message, reset when it ends or is cancelled.
assert 'startTreasuryPour(tile, level)' in render
assert 'updateTreasuryPours(timeSinceLastFrame)' in render
assert 'lift += goldHeight + pourRise' in render
assert 'cancelTreasuryPour(creature)' in render
assert 'creature->setPosition' not in render.split('void RenderManager::updateTreasuryPours')[1].split(
    'void RenderManager::cancelTreasuryPour')[0]
for name in ('pourRise', 'pourLean', 'pourSway', 'pourDuration'):
    assert name in rules

# Pour clips: the worker skeletons carry ClimbGold and PourGold, the code plays them and keeps the procedural
# motion as the fallback when a mesh has no such clips.
assert 'pourClimbClip = "ClimbGold"' in rules and 'pourTipClip = "PourGold"' in rules
assert 'pourClipPhase' in rules and 'pourClipTime' in rules
assert 'TreasuryCreatureRules::pourClipTime' in render and 'newPour.mEntity = entity' in render
assert 'it->mEntity == nullptr' in render
skeleton = (repo / 'models/Kobold.skeleton').read_bytes()
assert b'ClimbGold' in skeleton and b'PourGold' in skeleton
dwarf = (repo / 'models/Dwarf1.skeleton').read_bytes()
assert b'ClimbGold' in dwarf and b'PourGold' in dwarf

# The clips are credited and their Blender scripts are in the repository.
credits = read('CREDITS')
assert 'Kobold.skeleton (clips ClimbGold, PourGold)' in credits and 'Dwarf1.skeleton (clips ClimbGold, PourGold)' in credits
for name in ('kobold_gold_clips.py', 'dwarf_gold_clips.py', 'odp_fk.py', 'odp_ogre_io.py', 'splice_anims.py'):
    assert (repo / 'tools' / 'blender-assets' / name).exists(), name
print('ok')
