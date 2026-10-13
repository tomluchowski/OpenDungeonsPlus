"""Wiring checks for the room batches and the level of detail of the treasury gold: the piles are drawn by one
static batch per room instead of one scene object per tile; a change rebuilds only its own room, at most every
interval; piles far from the camera use the reduced mesh (with hysteresis, a few per check); a change of the
"Treasury detail" option draws the existing piles again. Client only, text only (no compiler needed)."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8').replace('\r\n', '\n')


render = read('source/render/RenderManager.cpp')
header = read('source/render/RenderManager.h')
batch = read('source/render/TreasuryGoldBatch.cpp')
batch_h = read('source/render/TreasuryGoldBatch.h')
settings = read('source/rooms/TreasurySettings.h')
layer = read('source/rooms/TreasuryGoldLayer.h')
mesh = read('source/render/TreasuryGoldMesh.cpp')
cmake = read('CMakeLists.txt')

# Technique: one Ogre::StaticGeometry per room, built from the pile entities.
assert 'createStaticGeometry' in batch and 'destroyStaticGeometry' in batch
assert 'addEntity(member.mEntity, member.mNode->getPosition(), member.mNode->getOrientation()' in batch
assert 'render/TreasuryGoldBatch.cpp' in cmake
assert '#include "render/TreasuryGoldBatch.h"' in header and 'TreasuryGoldBatch mTreasuryBatch;' in header

# One batch per room: the key is the room only, the rebuild interval is a setting.
interval = float(re.search(r'batchRebuildInterval = ([0-9.]+)f;', settings).group(1))
assert 0.1 <= interval <= 0.5
assert 'typedef const void* ChunkKey;' in batch_h
assert 'member.mKey = roomKey;' in batch
assert 'setOrigin(Ogre::Vector3(-500.0f, -500.0f, -500.0f))' in batch
assert 'TreasurySettings::current().batchRebuildInterval' in batch and 'mSinceRebuild' in batch

# Dirty flag per room: add, settle end and removal of a batched pile mark only their own room.
add = batch.split('void TreasuryGoldBatch::addPile')[1].split('void TreasuryGoldBatch::pileSettled')[0]
assert 'mDirty = true' in add
settled = batch.split('void TreasuryGoldBatch::pileSettled')[1].split('void TreasuryGoldBatch::hideUntilBatched')[0]
assert 'mChunks[it->second.mKey].mDirty = true' in settled
removal = batch.split('void TreasuryGoldBatch::removePile')[1].split('void TreasuryGoldBatch::update')[0]
assert 'mBatched' in removal and 'mDirty = true' in removal
update = batch.split('void TreasuryGoldBatch::update')[1].split('void TreasuryGoldBatch::rebuild')[0]
assert '!chunk.mDirty || chunk.mSinceRebuild < TreasurySettings::current().batchRebuildInterval' in update
rebuild = batch.split('void TreasuryGoldBatch::rebuild')[1].split('void TreasuryGoldBatch::clear')[0]
# only the room's own members are added, settling piles stay visible entities, build before hiding
assert 'chunk.mMembers' in rebuild and 'if(member.mSettling)' in rebuild and 'setVisible(true)' in rebuild
assert rebuild.index('->build()') < rebuild.index('setVisible(false)')
assert 'reset()' in rebuild
# clear() never touches the entities (they may be destroyed already)
assert 'mEntity' not in batch.split('void TreasuryGoldBatch::clear')[1].split('size_t TreasuryGoldBatch::getBatchCount')[0]

# Wiring in the render manager: created piles are listed and handed over to the batch, removal before the entity
# is destroyed, the per frame update, clear with the scene.
create = render.split('void RenderManager::rrCreateRenderedMovableEntity')[1].split('void RenderManager::rrDestroyRenderedMovableEntity')[0]
assert 'TreasuryGoldLayer::parseMeshName(meshName, pileShape)' in create
assert 'mTreasuryPiles[renderedMovableEntity] = pileInfo;' in create
assert 'mTreasuryBatch.addPile(mSceneManager, renderedMovableEntity->getName(), batchKey, ent, node, false)' in create
destroy = render.split('void RenderManager::rrDestroyRenderedMovableEntity')[1]
assert destroy.index('mTreasuryBatch.removePile(') < destroy.index('destroyEntity(ent)')
assert 'mTreasuryPiles.erase(curRenderedMovableEntity)' in destroy
assert 'mTreasuryBatch.update(timeSinceLastFrame)' in render
assert 'mTreasuryBatch.clear()' in render.split('void RenderManager::clearTreasuryPiles')[1]
assert render.count('clearTreasuryPiles();') == 2

# Level of detail by distance: piles beyond lodFarDistance use the reduced mesh, return below lodFarDistance -
# lodHysteresis (no flipping), at most a few per check; the new entity stays hidden while the batch shows the old one.
assert 'farAway && currentDetail == Detail::full' in mesh and 'bool farAway = false' in read('source/render/TreasuryGoldMesh.h')
lod = render.split('void RenderManager::updateTreasuryLod')[1].split('void RenderManager::rrTreasuryDetailChanged')[0]
assert 'lodReducedAt(it->second.mFar' in lod and 'lodSwitchesPerUpdate' in lod
assert 'destroyMesh(nodeType)' in lod and 'createMesh(nodeType)' in lod and 'hideUntilBatched' in lod
assert 'Detail::full' in lod.split('lodReducedAt')[0]
assert 'lodReducedAt(false' in create and 'prepareMesh(mSceneManager, meshName, pileFar)' in create
assert 'inline bool lodReducedAt' in layer and 'lodHysteresis' in layer
assert 'void TreasuryGoldBatch::hideUntilBatched' in batch
assert 'updateTreasuryLod(timeSinceLastFrame);' in render

# Live option: changing the "Treasury detail" setting draws the existing piles again, a few per frame, and only
# when the setting really changed.
settings_window = read('source/modes/SettingsWindow.cpp')
assert 'previousTreasuryDetail != TreasuryGoldMesh::getDetail()' in settings_window
assert 'RenderManager::getSingleton().rrTreasuryDetailChanged()' in settings_window
rebuild_pile = render.split('void RenderManager::updateTreasuryRebuild')[1].split('void RenderManager::clearTreasuryPiles')[0]
assert 'destroyMesh(nodeType)' in rebuild_pile and 'createMesh(nodeType)' in rebuild_pile and 'rebuildPerFrame' in rebuild_pile
assert 'updateTreasuryRebuild();' in render
print('ok')
