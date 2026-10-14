"""Check the gold shown outside the treasury: a coin heap for gold on the floor, a sack for gold carried
by a worker or a thief, the gold of the thief in the creature packets, and the wiring of these into the
renderer (no game start and no compiler needed)."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


render = read('source/render/RenderManager.cpp')
loose = read('source/render/LooseGoldMesh.cpp')
header = read('source/render/LooseGoldMesh.h')

# Floor gold becomes a heap, carried gold a sack, both only while the treasury detail is not off.
assert 'LooseGoldMesh::prepareHeap(mSceneManager, meshName)' in render
assert 'GameEntityType::treasuryObject' in render
assert 'LooseGoldMesh::prepareSack(mSceneManager, carried->getMeshName())' in render
assert loose.count('TreasuryGoldMesh::Detail::off') == 2
# The sack and its visibility swap are undone when the gold is put down and when the object goes away.
assert '_sack' in render and render.count('"_sack"') >= 3
assert 'carriedEnt->setVisible(true)' in render
# Four sizes follow the four classic stack names the server already sends; nothing else is replicated.
assert 'sizeCount = 4' in header
assert 'GoldstackLv' in loose
assert 'GoldstackLv1' in read('source/entities/TreasuryObject.cpp')
# Assets: material, texture, generator, credits, build.
assert 'material GoldSack' in read('materials/scripts/GoldSack.material')
assert (repo / 'materials/textures/GoldSack.png').exists()
assert (repo / 'tools/treasury-gold/generate_gold_sack.py').exists()
assert 'GoldSack.png' in read('CREDITS')
assert 'LooseGoldMesh.cpp' in read('CMakeLists.txt')

# Thief gold: one trailing field in the creature packets (new and update), written and read at the end,
# only sent again when the amount changed; the client sack follows it.
creature = read('source/entities/Creature.cpp')
for name in ('exportToPacket(ODPacket& os, const Seat* seat) const', 'exportToPacketForUpdate(ODPacket& os, Seat* seat)'):
    body = creature.split('void Creature::' + name)[1].split('\n}\n')[0]
    assert body.rstrip().endswith('os << mGoldCarried;'), name
for name in ('importFromPacket(ODPacket& is)', 'updateFromPacket(ODPacket& is)'):
    body = creature.split('void Creature::' + name)[1].split('\n}\n')[0]
    assert 'OD_ASSERT_TRUE(is >> mGoldCarried);' in body, name
    assert body.index('importActivityFromPacket(is);') < body.index('is >> mGoldCarried'), name
assert 'mGoldCarried != mGoldCarriedNotified' in creature
assert 'rrRefreshCreatureGoldSack(this)' in creature
assert 'rrRefreshCreatureGoldSack(curCreature)' in render
assert 'removeTreasuryThiefSack(curCreature)' in render
assert 'getMeshNameForGold(creature->getGoldCarried())' in render
# The sack never moves the creature itself (render node only)
sack = render.split('void RenderManager::rrRefreshCreatureGoldSack')[1].split('void RenderManager::removeTreasuryThiefSack')[0]
assert 'creature->setPosition' not in sack

print('check_gold_elsewhere ok')
