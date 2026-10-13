"""Wiring checks for the room batches of the treasury gold: settled piles are drawn by one static batch per room
instead of one scene object per tile; a change rebuilds only its own room, at most every interval; settling piles, the registry (creature heights), the detail option and the room object bounds slice stay
as they were. Client only, text only (no compiler needed)."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8').replace('\r\n', '\n')


render = read('source/render/RenderManager.cpp')
header = read('source/render/RenderManager.h')
batch = read('source/render/TreasuryGoldBatch.cpp')
batch_h = read('source/render/TreasuryGoldBatch.h')
rules = read('source/render/TreasuryCreatureRules.h')
cmake = read('CMakeLists.txt')

# Technique: one Ogre::StaticGeometry per room, built from the pile entities.
assert 'createStaticGeometry' in batch and 'destroyStaticGeometry' in batch
assert 'addEntity(member.mEntity, member.mNode->getPosition(), member.mNode->getOrientation()' in batch
assert 'render/TreasuryGoldBatch.cpp' in cmake
assert '#include "render/TreasuryGoldBatch.h"' in header and 'TreasuryGoldBatch mTreasuryBatch;' in header

# One batch per room: the key is the room only (no patch of tiles), the rule constant is the rebuild interval.
settings = read('source/rooms/TreasurySettings.h')
interval = float(re.search(r'batchRebuildInterval\s*=\s*([0-9.]+)f\s*;', settings).group(1))
assert 'batchRebuildInterval' in rules
assert 0.1 <= interval <= 0.5
assert 'batchChunkSize' not in rules and 'batchChunkIndex' not in rules
assert 'batchChunkSize' not in batch and 'batchChunkIndex' not in batch
assert 'typedef const void* ChunkKey;' in batch_h and 'tuple' not in batch_h
assert 'member.mKey = roomKey;' in batch and 'int tileX' not in batch_h
assert 'setOrigin(Ogre::Vector3(-500.0f, -500.0f, -500.0f))' in batch
assert 'batchRebuildInterval' in batch and 'mSinceRebuild' in batch

# Dirty flag per room: add, settle end and removal of a batched pile mark only their own room.
add = batch.split('void TreasuryGoldBatch::addPile')[1].split('void TreasuryGoldBatch::pileSettled')[0]
assert 'mDirty = true' in add
settled = batch.split('void TreasuryGoldBatch::pileSettled')[1].split('void TreasuryGoldBatch::removePile')[0]
assert 'mChunks[it->second.mKey].mDirty = true' in settled
removal = batch.split('void TreasuryGoldBatch::removePile')[1].split('void TreasuryGoldBatch::update')[0]
assert 'mBatched' in removal and 'mDirty = true' in removal
update = batch.split('void TreasuryGoldBatch::update')[1].split('void TreasuryGoldBatch::rebuild')[0]
assert '!chunk.mDirty || chunk.mSinceRebuild < TreasuryCreatureRules::batchRebuildInterval' in update
rebuild = batch.split('void TreasuryGoldBatch::rebuild')[1].split('void TreasuryGoldBatch::clear')[0]
# only the room's own members are added, settling piles stay visible entities, build before hiding
assert 'chunk.mMembers' in rebuild and 'if(member.mSettling)' in rebuild and 'setVisible(true)' in rebuild
assert rebuild.index('->build()') < rebuild.index('setVisible(false)')
assert 'reset()' in rebuild
# clear() never touches the entities (they may be destroyed already)
assert 'mEntity' not in batch.split('void TreasuryGoldBatch::clear')[1].split('size_t TreasuryGoldBatch::getBatchCount')[0]

# Wiring in the render manager: created piles are handed over after the settle started, removal before the
# entity is destroyed, settle end and the per frame update, clear with the other treasury state.
create = render.split('void RenderManager::rrCreateRenderedMovableEntity')[1].split('void RenderManager::rrDestroyRenderedMovableEntity')[0]
assert create.index('startTreasuryPileChange(node') < create.index('mTreasuryBatch.addPile(')
assert 'isTreasuryPileSettling(renderedMovableEntity->getName())' in create
destroy = render.split('void RenderManager::rrDestroyRenderedMovableEntity')[1]
assert destroy.index('mTreasuryBatch.removePile(') < destroy.index('destroyEntity(ent)')
assert 'mTreasuryBatch.pileSettled(it->mEntityName)' in render.split('void RenderManager::updateTreasuryPileSettles')[1]
assert 'mTreasuryBatch.update(timeSinceLastFrame)' in render
assert 'mTreasuryBatch.clear()' in render.split('void RenderManager::clearTreasuryEffects')[1].split('void RenderManager::cancelCreatureStep')[0]

# Detail option: only piles registered at full/reduced are batched; off keeps the classic per-object stacks.
batch_block = create.split('if(pileLevel >= 0)')[1].split('// Objects and gold lying')[0]
assert 'mTreasuryBatch.addPile(' in batch_block
assert 'TreasuryGoldMesh::getDetail() != TreasuryGoldMesh::Detail::off' in create.split('if(pileLevel >= 0)')[0]

# The registry that creatures, buried objects, dust and glow use is untouched by the batch.
assert 'TreasuryGoldMesh::registerPile(' in create and 'TreasuryGoldMesh::unregisterPile(' in destroy
assert 'TreasuryGoldMesh' not in batch and 'registerPile' not in batch

# check_room_object_bounds cuts the renderer at the first buildingObject block: no earlier occurrence.
marker = '    if(renderedMovableEntity->getObjectType() == GameEntityType::buildingObject)'
first = render.index(marker)
assert first > render.index('void RenderManager::rrCreateRenderedMovableEntity')
assert render[first:].split('    Ogre::Entity* ent = nullptr;', 1)[0].count('RoomObjectPath::furnitureScale') >= 1
print('ok')
