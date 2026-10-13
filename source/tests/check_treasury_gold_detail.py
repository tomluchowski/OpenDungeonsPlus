"""Wiring checks for the finer treasury gold visuals: coins and gems on the piles, overflow at the edges, scattered
coins on bare floors, sliding, rolling and sparkling coins, the dent and the settling of a pile that gains or loses
gold, the level of detail by distance and the live "Treasury detail" option. Client only, within the option and
its budgets (no compiler needed)."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


render = read('source/render/RenderManager.cpp')
mesh = read('source/render/TreasuryGoldMesh.cpp')
rules = read('source/render/TreasuryCreatureRules.h')
layer = read('source/rooms/TreasuryGoldLayer.h')
particles = read('particles/TreasuryCoins.particle')
treasury = read('source/rooms/RoomTreasury.cpp')
material = read('materials/scripts/TreasuryGoldPile.material')
config = read('config/treasury.cfg')
settings = read('source/rooms/TreasurySettings.h')

# Coins, gems and spilled coins: a second mesh section, only at the full detail, derived from the pile name alone.
assert 'material TreasuryGoldDetail' in material and 'ambient vertexcolour' in material
assert 'DetailMaterial' in mesh and 'buildPileMesh(sceneManager, name + ".mesh", shape, reduced ? ReducedRings : FullRings, !reduced)' in mesh
for name in ('topCoinCount', 'gemCount', 'edgeOpen', 'spillCoinsPerEdge', 'hasFloorScatter'):
    assert name in layer and name in mesh + treasury, name
assert 'maxTopCoins' in layer and 'maxGems' in layer and 'maxSpillCoins' in layer

# The numbers are settings of config/treasury.cfg
for key in ('MaxTopCoins', 'TopCoinMinLevel', 'MaxGems', 'SpillCoinsFull', 'ScatterCoins'):
    assert ('    ' + key + '\t') in config and ('#   ' + key) in config and key in settings, key

# Bare floor: the server names a level 0 pile for some empty tiles; the client draws nothing for it unless full.
assert 'hasFloorScatter(tile->getX(), tile->getY())' in treasury
assert 'mGoldChanged = true;\n    return new RoomTreasuryTileData' in treasury.replace('\r\n', '\n')
assert 'shape.mLevel == 0 && (currentDetail != Detail::full || farAway)' in mesh

# Reduced and off keep the round surface, without coins and gems
assert 'const bool reduced = (currentDetail != Detail::full) || (farAway && currentDetail == Detail::full);' in mesh
assert 'withDetail && hasDetail(shape)' in mesh

# Sliding, rolling and sparkling coins.
for name in ('TreasuryCoinSlide', 'TreasuryCoinRoll', 'TreasuryGlint'):
    assert 'particle_system ' + name in particles
    assert '"' + name + '"' in render

# Dent and rolling coins on a pile that loses gold, growth on a pile that gains it (from the pile name only).
assert 'startTreasuryPileChange(node' in render and 'updateTreasuryPileSettles(timeSinceLastFrame)' in render
assert 'previousPileLevel' in render and 'int registerPile(' in read('source/render/TreasuryGoldMesh.h')
assert 'pileSettleScale' in rules and 'dentDepth' in rules

# Local dent: at the full detail a taken pile shows a bowl where the gold was taken (a dynamic copy of the pile
# with a vertex offset, drawn in place of the entity while it settles), which fills up again; the entity is given
# back when the settle ends, when the entity goes away and when the renderer is cleared.
assert 'createDentedPile' in mesh and 'updateDentedPile' in mesh and 'dentedHeight' in mesh
assert 'object->setDynamic(true)' in mesh and 'object->beginUpdate(section)' in mesh
assert 'fillPile(object, shape, DentDivisions, true, &dent, true)' in mesh
change = render.split('void RenderManager::startTreasuryPileChange')[1].split('float RenderManager::getTreasuryCameraDistance')[0]
assert 'createDentedPile(mSceneManager, pileMeshName' in change and 'node->detachObject(entity)' in change
assert 'settle.mLocalDent' in change and 'dentLocalDepth' in change
dents = render.split('void RenderManager::updateTreasuryDents')[1].split('void RenderManager::updateTreasuryPileSettles')[0]
assert 'localDentFactor' in dents
assert 'attachObject(it->mEntity)' in dents and 'destroyManualObject(it->mObject)' in dents
assert 'finishTreasuryDent(entityName);' in render.split('void RenderManager::cancelTreasuryPileSettle')[1].split('void RenderManager::registerBuriedObject')[0]
assert 'finishTreasuryDent(mTreasuryPileDents.back().mEntityName)' in render.split('void RenderManager::clearTreasuryEffects')[1]
assert 'updateTreasuryDents(timeSinceLastFrame)' in render
assert 'it->mTaken && !it->mLocalDent' in render
assert 'localDentFactor' in rules and 'dentRadius' in rules and 'dentLocalDepth' in rules

# Level of detail by distance: piles beyond lodFarDistance use the reduced mesh (no coins and gems, nothing on a bare
# tile), return below lodFarDistance - lodHysteresis (no flipping), at most a few per check, never while settling;
# the new entity stays hidden while the room batch still shows the old one.
assert 'farAway && currentDetail == Detail::full' in mesh and 'bool farAway = false' in read('source/render/TreasuryGoldMesh.h')
lod = render.split('void RenderManager::updateTreasuryLod')[1].split('void RenderManager::updateTreasuryDents')[0]
assert 'lodReducedAt(it->second.mFar' in lod and 'lodSwitchesPerUpdate' in lod and 'isTreasuryPileSettling' in lod
assert 'destroyMesh(nodeType)' in lod and 'createMesh(nodeType)' in lod and 'hideUntilBatched' in lod
assert 'Detail::full' in lod.split('lodReducedAt')[0]
assert 'mTreasuryPiles.erase(curRenderedMovableEntity)' in render and 'updateTreasuryLod(timeSinceLastFrame)' in render
assert 'lodReducedAt(false' in render and 'prepareMesh(mSceneManager, meshName, pileFar)' in render
assert 'inline bool lodReducedAt' in rules and 'lodHysteresis' in rules
assert 'void TreasuryGoldBatch::hideUntilBatched' in read('source/render/TreasuryGoldBatch.cpp')

# Live option: changing the "Treasury detail" setting rebuilds what exists (piles, batches, buried objects follow
# through the piles, floor gold, thief sacks), a few per frame, and only when the setting really changed.
settings_window = read('source/modes/SettingsWindow.cpp')
assert 'previousTreasuryDetail != TreasuryGoldMesh::getDetail()' in settings_window
assert 'RenderManager::getSingleton().rrTreasuryDetailChanged()' in settings_window
rebuild = render.split('void RenderManager::rrTreasuryDetailChanged')[1].split('void RenderManager::updateTreasuryRebuild')[0]
assert 'mTreasuryPiles.begin()' in rebuild and 'GameEntityType::treasuryObject' in rebuild
assert 'rrRefreshCreatureGoldSack(creature)' in rebuild and 'finishTreasuryDent' in rebuild
rebuild_step = render.split('void RenderManager::updateTreasuryRebuild')[1].split('void RenderManager::rrRefreshCreatureGoldSack')[0]
assert 'destroyMesh(nodeType)' in rebuild_step and 'createMesh(nodeType)' in rebuild_step and 'rebuildPerFrame' in rebuild_step
assert 'updateTreasuryRebuild();' in render and 'mTreasuryRebuildQueue.clear()' in render.split('void RenderManager::clearTreasuryEffects')[1]
assert 'static const int& rebuildPerFrame' in rules

# Option and budgets: every new effect goes through the view test and a per-room budget; off has no effects.
assert 'TreasuryEffectKind::ambient' in render and 'ambientBudget(TreasuryGoldMesh::getDetail())' in render
assert 'case TreasuryGoldMesh::Detail::full:' in rules.split('inline int ambientBudget')[1]
assert 'camera->isVisible' in render.split('void RenderManager::startTreasuryPileChange')[1].split('float RenderManager::getTreasuryCameraDistance')[0]
print('ok')
