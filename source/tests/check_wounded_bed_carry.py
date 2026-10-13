"""Workers pull hurt creatures into their bed by the legs.

Pure source wiring checks (no compiler, no game): server authority, configuration, one worker per creature, abort
rules, the creature is pulled over the ground (never put into the carry node of the worker), the movement of the
two (speed, direction, path), the clip names with their fallbacks, nothing new to save or send. The carrying of things that are really carried (gold, bodies, prisoners, traps) must stay as it was."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


rooms_cfg = read('config/rooms.cfg')
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
entity_h = read('source/entities/GameEntity.h')
movable = read('source/entities/MovableGameEntity.cpp')
movable_h = read('source/entities/MovableGameEntity.h')
dormitory = read('source/rooms/RoomDormitory.cpp')
carry = read('source/creatureaction/CreatureActionCarryEntity.cpp')
carry_h = read('source/creatureaction/CreatureActionCarryEntity.h')
render = read('source/render/RenderManager.cpp')
config_manager = read('source/utils/ConfigManager.cpp')

# Every number is in the configuration: documented, set, read with a default
number_keys = ['DormitoryWoundedCarryHpPercent', 'DormitoryWoundedCarryRadius', 'DormitoryWoundedCarryPriority',
               'DormitoryWoundedCarryCooldown', 'DormitoryWoundedCarryEnemyRadius', 'DormitoryWoundedCarryMaxTurns',
               'DormitoryWoundedCarryTempKo', 'DormitoryWoundedDragGap', 'DormitoryWoundedDragWorkerSpeedFactor',
               'DormitoryWoundedDragFollowSpeedFactor', 'DormitoryWoundedDragMaxDistance',
               'DormitoryWoundedDragLayTurns']
for key in number_keys:
    assert re.search(r'^# ' + key + r'\s', rooms_cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\t[\d.]+', rooms_cfg, re.M), key + ' not set'
    assert re.search(r'getRoomConfigDoubleOrDefault\(\s*"' + key + '"', creature + carry), key + ' not read'

# The clip names are in the code and in the configuration, the fallbacks too
clip_keys = {'DormitoryWoundedDragWorkerClip': 'Drag', 'DormitoryWoundedDragCreatureClip': 'Dragged',
             'DormitoryWoundedDragWorkerFallbackClip': 'Walk', 'DormitoryWoundedDragCreatureFallbackClip': 'Die'}
for key, value in clip_keys.items():
    assert re.search(r'^# ' + key + r'\s', rooms_cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\t' + value + r'\s*$', rooms_cfg, re.M), key + ' not set to ' + value
    assert '"' + key + '"' in render, key + ' not read by the renderer'
assert 'drag_anim = "Drag"' in movable_h and 'dragged_anim = "Dragged"' in movable_h
assert 'std::string ConfigManager::getRoomConfigStringOrDefault(' in config_manager

# A new carry priority exists, below the other carryable things
assert re.search(r'corpse,\s+woundedCreature,\s+skillEntity', entity_h)

# The creature decides on the server: own seat, radius, priority from the config, nothing for other seats
carry_type = function_body(creature, 'EntityCarryType Creature::getEntityCarryType(')
assert 'carrier->getSeat() == getSeat()' in carry_type and 'getIsOnServerMap()' in carry_type
assert 'isWoundedForBedCarry()' in carry_type and 'DormitoryWoundedCarryRadius' in carry_type

wounded = function_body(creature, 'bool Creature::isWoundedForBedCarry() const')
for needle in ('getIsOnServerMap()', 'isAlive()', 'isInPrison()', 'mIsBeingDragged',
               'mWoundedCarryNextTurn', 'CreatureActionType::fight', 'CreatureActionType::flee',
               'isHostileNear(', 'RoomType::dormitory', 'myTile == mHomeTile'):
    assert needle in wounded, needle
# No dormitory with an own bed of the seat = not pulled at all (a worker never moved a hurt creature before either)
assert 'mHomeTile == nullptr' in wounded and 'RoomType::dormitory' in wounded

# One worker per creature: the existing carry lock; the creature stands still, the pause after the pulling
assert 'getCarryLock' in read('source/entities/Tile.cpp')
start = function_body(creature, 'void Creature::notifyDragStart(')
assert 'mIsBeingDragged = true;' in start and 'clearActionQueue();' in start
end = function_body(creature, 'void Creature::notifyDragEnd(')
assert 'mWoundedCarryNextTurn =' in end and 'mIsBeingDragged = false;' in end
assert 'if(mIsBeingDragged && (mKoTurnCounter == 0))' in creature

# The carrying of real carried things is as it was: the entity is taken out of the map, nothing of the pulling in it
on = function_body(creature, 'void Creature::notifyEntityCarryOn(')
off = function_body(creature, 'void Creature::notifyEntityCarryOff(')
assert 'mIsBeingDragged' not in on + off and 'mWoundedCarryNextTurn' not in on + off
assert 'mIsBeingCarried' not in creature + creature_h, 'old carry flag of the carried wounded is dead code'
handler = function_body(carry, 'bool CreatureActionCarryEntity::handleCarryEntity(')
assert 'nbTurnsActive' not in handler and 'isHostileNear' not in handler

# Pulled, not carried: the hurt creature never goes into the carry node of the worker
assert 'static bool isPulledOverGround(GameEntity& entity);' in carry_h
pulled = function_body(carry, 'bool CreatureActionCarryEntity::isPulledOverGround(')
assert 'isAlive()' in pulled and 'getKoTurnCounter() >= 0' in pulled
ctor = function_body(carry, 'CreatureActionCarryEntity::CreatureActionCarryEntity(')
assert re.search(r'if\(mIsDrag\)\s*\{\s*startDrag\(\);\s*\}\s*else\s*\{\s*mEntityToCarry->notifyEntityCarryOn\(&mCreature\);'
                 r'\s*mCreature\.carryEntity\(mEntityToCarry\);', ctor), 'carrying only for the other things'
drag_start = function_body(carry, 'void CreatureActionCarryEntity::startDrag(')
assert 'notifyDragStart()' in drag_start and 'carryEntity' not in drag_start and 'notifyEntityCarryOn' not in drag_start
assert 'dragged_anim' in drag_start
dtor = function_body(carry, 'CreatureActionCarryEntity::~CreatureActionCarryEntity(')
assert 'notifyDragEnd()' in dtor and re.search(r'if\(!mIsDrag\)\s*mCreature\.releaseCarriedEntity\(\);', dtor)
release = function_body(carry, 'void CreatureActionCarryEntity::releaseEntity(')
assert 'notifyDragEnd()' in release and 'releaseCarriedEntity' in release and 'notifyEntityCarryOff' in release
assert 'handleDragCreature, this' in carry

# Pulling: the worker walks with its own clip and without a walk action (it keeps its turn), the creature follows the
# way of the worker at a fixed distance (config), and the worker lets go when a hostile creature comes close, after the
# longest time, when it died or was knocked out to death, when it is left behind
drag = function_body(carry, 'bool CreatureActionCarryEntity::handleDragCreature(')
for needle in ('isHostileNear(', 'DormitoryWoundedCarryEnemyRadius', 'DormitoryWoundedCarryMaxTurns',
               'DormitoryWoundedDragMaxDistance', 'DormitoryWoundedDragGap', 'DormitoryWoundedDragLayTurns',
               'followTrail(', 'setDragDestination(', 'startLaying(', 'isAlive()', 'getKoTurnCounter() < 0',
               'getNbTurnsActive()'):
    assert needle in drag, needle
assert 'popAction' not in drag and 'setDestination(' not in drag, 'only stopDragging may pop the action'
stop = function_body(carry, 'bool CreatureActionCarryEntity::stopDragging(')
assert stop.rstrip().endswith('mCreature.popAction();\n    return false;'), 'nothing of the action is used after popAction'
assert 'clearDestinations(EntityAnimation::idle_anim' in stop
follow = function_body(carry, 'void CreatureActionCarryEntity::followTrail(')
assert 'setWalkPath(EntityAnimation::dragged_anim, EntityAnimation::dragged_anim, true, false, path, false)' in follow
assert 'getWalkQueue()' in follow and 'appendTrailSection(' in follow
laying = function_body(carry, 'void CreatureActionCarryEntity::startLaying(')
assert 'getHomeTile()' in laying and 'dragged_anim' in laying
set_drag = function_body(creature, 'bool Creature::setDragDestination(')
assert 'EntityAnimation::drag_anim' in set_drag and 'pushAction' not in set_drag and 'setWalkPath(' in set_drag

# Speed and direction are the same on the server and the clients (both know the clip name): the worker is slower, the
# pulled creature slides a bit faster than the worker, both look against the way they move, the walk clip keeps up
speed = function_body(creature, 'double Creature::getMoveSpeed(Tile* tile) const')
assert 'EntityAnimation::drag_anim' in speed and 'EntityAnimation::dragged_anim' in speed
assert 'DormitoryWoundedDragFollowSpeedFactor' in speed and 'getDragWorkerSpeedFactor()' in speed
assert 'DormitoryWoundedDragWorkerSpeedFactor' in function_body(creature, 'double Creature::getDragWorkerSpeedFactor(')
pose = function_body(creature, 'double Creature::getClientPoseSpeedFactor() const')
assert 'EntityAnimation::drag_anim' in pose
update = function_body(movable, 'void MovableGameEntity::update(')
assert 'EntityAnimation::drag_anim' in update and 'EntityAnimation::dragged_anim' in update
assert update.count('* facing') >= 4

# The dormitory accepts the hurt creature, lays it in its bed (sleep heals) and copes with a creature that is gone
has_spot = function_body(dormitory, 'bool RoomDormitory::hasCarryEntitySpot(')
assert 'isWoundedForBedCarry()' in has_spot and 'getHomeTile()' in has_spot
notify = function_body(dormitory, 'void RoomDormitory::notifyCarryingStateChanged(')
assert 'carriedEntity == nullptr' in notify and 'carrierTile' in notify
assert 'creature->sleep();' in notify and 'if(creature->getKoTurnCounter() < 0)\n        creature->resetKoTurns();' in notify

# Nothing new to save or to send: not saved, and the clients are told by the walk paths and clips that exist
for forbidden in ('mWoundedCarryNextTurn <<', '>> mWoundedCarryNextTurn', 'mIsBeingDragged <<', '>> mIsBeingDragged'):
    assert forbidden not in creature, forbidden
notification_h = read('source/network/ServerNotification.h')
enum_text = notification_h[notification_h.index('enum class ServerNotificationType'):notification_h.index('};')]
assert not re.search(r'drag(?!gable)', enum_text.lower()), 'no new message for the pulling'
assert 'carryEntity' in read('source/network/ODClient.cpp')

# The renderer: own clips with fallbacks (the worker the walk clip, the creature the last frame of its death clip),
# said once in the log, nothing else changes for other clips
choose = function_body(render, 'std::string chooseDragClip(')
assert 'hasAnimation(clip)' in choose and 'sMissingClipsSaid' in choose and 'OD_LOG_INF' in choose
assert 'EntityAnimation::walk_anim' in choose and 'EntityAnimation::die_anim' in choose
assert 'needsCreatureDropFallback(entity)' in choose and 'EntityAnimation::idle_anim' in choose
assert 'chooseDragClip(objectEntity,' in render and 'freezeOnLastFrame' in render
assert 'setTimePosition(animState->getLength())' in render

print('wounded bed pulling: ok')
