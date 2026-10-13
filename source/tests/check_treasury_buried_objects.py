"""Checks for objects standing in the treasury gold (partly buried, settling smoothly with the pile). Client render
offset only: the server, the object bounds, the paths and the gold amounts are not touched. Text and a mirror of the
pure rules, no compiler needed."""
import math
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


render = read('source/render/RenderManager.cpp')
header = read('source/render/RenderManager.h')
rules = read('source/render/TreasuryCreatureRules.h')
layer = read('source/rooms/TreasuryGoldLayer.h')
settings = read('source/rooms/TreasurySettings.h')

# Pure rules exist
for name in ('buryShare', 'buriedLift', 'buriedStep', 'buryShareFirst', 'buryShareFull', 'buriedSettleTime'):
    assert name in rules, name

# Mirror of the rules
max_level = int(layer.split('maxLevel = ')[1].split(';')[0])
# The numbers are the defaults of the settings struct (config/treasury.cfg overrides them)
step = float(settings.split('float levelHeight = ')[1].split('f;')[0])
first = float(settings.split('float buryShareFirst = ')[1].split('f;')[0])
full = float(settings.split('float buryShareFull = ')[1].split('f;')[0])
settle = float(settings.split('float pileSettleTime = ')[1].split('f;')[0])
snap = float(rules.split('buriedSnapDistance = ')[1].split('f;')[0])
assert 0.0 < first < full <= 0.6 and settle > 0.0 and snap > 0.0


def share(level):
    if level <= 0:
        return 0.0
    top = min(level, max_level)
    return first + (full - first) * (top - 1) / (max_level - 1)


def lift(surface, level, height):
    if level <= 0 or surface <= 0.0:
        return 0.0
    return max(0.0, surface - share(level) * max(height, 0.0))


def level_height(level):
    return step * min(level, max_level) if level > 0 else 0.0


# The share of the buried height grows with the fill and never exceeds the maximum
shares = [share(level) for level in range(0, max_level + 1)]
assert shares[0] == 0.0 and shares[1] == first and abs(shares[max_level] - full) < 1e-6
assert all(a < b for a, b in zip(shares[1:], shares[2:]))
# No pile (level 0 or no surface): no offset; detail off registers no piles, so objects are not moved
assert lift(0.0, 0, 0.4) == 0.0 and lift(0.3, 0, 0.4) == 0.0 and lift(0.0, 3, 0.4) == 0.0
# A full room: about half of an object stays above the gold, the base never goes below the floor
for height in (0.2, 0.4, 0.8, 1.5):
    for level in range(1, max_level + 1):
        value = lift(level_height(level), level, height)
        assert value >= 0.0
        buried = level_height(level) - value
        assert buried <= level_height(level) + 1e-6
        assert value + height >= level_height(level) + (1.0 - share(level)) * height - 1e-5 or value == 0.0
# The offset follows the pile height without jumps between neighbouring levels larger than one step
for height in (0.3, 0.6):
    values = [lift(level_height(level), level, height) for level in range(1, max_level + 1)]
    assert all(abs(b - a) <= step + 1e-6 for a, b in zip(values, values[1:]))


def settle_step(current, target, elapsed):
    rate = 1.0 - math.exp(-3.0 * elapsed / settle)
    nxt = current + (target - current) * rate
    return target if abs(target - nxt) < snap else nxt


# Smooth settling: monotone, no overshoot, reaches the target within 1.5 seconds at 60 frames per second
for start, target in ((0.0, 0.3), (0.3, 0.0), (0.1, 0.1)):
    value = start
    previous = abs(target - value)
    for frame in range(90):
        value = settle_step(value, target, 1.0 / 60.0)
        assert abs(target - value) <= previous + 1e-9
        previous = abs(target - value)
        assert min(start, target) - 1e-9 <= value <= max(start, target) + 1e-9
    assert value == target

# Wiring: registered for objects and loose gold on treasuries, not for the piles themselves
assert 'registerBuriedObject(renderedMovableEntity' in render
assert 'nt == NodeType::MTILES_NODE && ent != nullptr && !isPileEntity' in render
assert 'isPileEntity = TreasuryGoldLayer::parseMeshName(meshName, pileShape)' in render
assert 'GameEntityType::treasuryObject))' in render.split('registerBuriedObject(renderedMovableEntity')[0][-200:]
assert 'ent->getBoundingBox().getSize().z * node->getScale().z' in render

# Follows pile changes: created, replaced or removed piles refresh the objects of their tile
assert render.count('refreshBuriedObjectsOnTile(') >= 3
assert 'updateTreasuryBuriedObjects(timeSinceLastFrame)' in render
for name in ('registerBuriedObject', 'refreshBuriedObjectsOnTile', 'updateTreasuryBuriedObjects', 'getBuriedLift',
             'mTreasuryBuriedObjects'):
    assert name in header, name

# Cleanup: forgotten with the entity and with the game renderer
assert 'mTreasuryBuriedObjects.erase(curRenderedMovableEntity)' in render
assert 'mTreasuryBuriedObjects.clear()' in render

# Animation only in view; a carried object (child of its carrier) is never moved here
update = render.split('void RenderManager::updateTreasuryBuriedObjects')[1].split('float RenderManager::getBuriedLift')[0]
assert 'getParent() == mRoomSceneNode' in update
refresh = render.split('void RenderManager::refreshBuriedObjectsOnTile')[1].split('void RenderManager::updateTreasuryBuriedObjects')[0]
assert 'camera->isVisible(position)' in refresh and 'Detail::off' in refresh

# Render only: moving an entity keeps its position, the offset goes on the node; nothing is written to entities
body = update + refresh + render.split('float RenderManager::getBuriedLift')[1].split('void RenderManager::rrRefreshCreatureGoldSack')[0]
for forbidden in ('setPosition(', 'setMeshName', 'getGameMap'):
    assert forbidden not in body.replace('node->setPosition(', '').replace('entity->getEntityNode()->setPosition(', ''), forbidden
assert 'buriedLift = getBuriedLift(static_cast<RenderedMovableEntity*>(entity), true)' in render
assert 'entity->getEntityNode()->setPosition(position + Ogre::Vector3(0.0f, 0.0f, buriedLift))' in render

# Bounds, navigation and paths: the logical obstacle uses the entity position, not the node, so it is unchanged.
step_block = render.split('void RenderManager::updateCreatureStep')[1].split('void RenderManager::cancelCreatureStep')[0]
assert 'obstacle.maximumHeight = candidate->getPosition().z + bounds.maxZ' in step_block
assert 'Buried' not in step_block
# The scale block cut by check_room_object_bounds.py still holds only the scale code and runs before our hook
scale_block = render.split('    if(renderedMovableEntity->getObjectType() == GameEntityType::buildingObject)', 1)[1].split('    Ogre::Entity* ent = nullptr;', 1)[0]
assert 'Buried' not in scale_block and 'furnitureScale' in scale_block
# Server and packets untouched by this change
for path in ('source/rooms/RoomTreasury.cpp', 'source/gamemap/RoomObjectBounds.h'):
    assert 'Buried' not in read(path), path
print('ok')
