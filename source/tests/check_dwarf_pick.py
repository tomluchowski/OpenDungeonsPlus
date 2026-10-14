#!/usr/bin/env python3
# Build-free check of the dwarf worker pickaxe: the model files exist, are named in CREDITS, and the render manager
# attaches the pick to the right-hand weapon bone of a worker without a weapon (and removes it again).
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[2]


def read(path):
    """Returns the text of a file of the repository."""
    return (root / path).read_text(encoding='utf-8', errors='replace')


for path in ('models/DwarfPick.mesh', 'materials/scripts/DwarfPick.material',
              'materials/textures/DwarfPickSurface.png', 'tools/gen_dwarf_pick_texture.py'):
    assert (root / path).is_file(), path
    assert re.search(r'^' + re.escape(path) + r'\s', read('CREDITS'), re.M), 'no CREDITS row for ' + path
assert 'texture DwarfPickSurface.png' in read('materials/scripts/DwarfPick.material')

render = read('source/render/RenderManager.cpp')
assert 'const std::string WORKER_TOOL_MESH = "DwarfPick.mesh";' in render
create = render[render.index('void RenderManager::rrCreateWorkerTool('):]
create = create[:create.index('void RenderManager::rrDestroyWorkerTool(')]
for word in ('isWorker()', 'getWeaponR() != nullptr', 'hasBone("Pick")', 'hasBone("Weapon_R")',
             'resourceExistsInAnyGroup(WORKER_TOOL_MESH)', 'getWeaponMount(skeleton, "R", WORKER_TOOL_MESH, mount)',
             'attachObjectToBone('):
    assert word in create, word
destroy = render[render.index('void RenderManager::rrDestroyWorkerTool('):]
destroy = destroy[:destroy.index('void RenderManager::rrCreateMapLight(')]
assert 'detachFromParent()' in destroy and 'destroyEntity(' in destroy

header = read('source/render/RenderManager.h')
assert 'void rrCreateWorkerTool(Creature* curCreature);' in header
assert 'void rrDestroyWorkerTool(Creature* curCreature);' in header
creature = read('source/entities/Creature.cpp')
assert creature.count('rrCreateWorkerTool(this);') == 1 and creature.count('rrDestroyWorkerTool(this);') == 1
print('check_dwarf_pick: ok')
sys.exit(0)
