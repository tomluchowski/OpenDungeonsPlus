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
               'isHostileNear(', 'hasOwnBedInDormitory()', 'myTile == mHomeTile'):
    assert needle in wounded, needle
# No dormitory with an own bed of the seat = not pulled at all (a worker never moved a hurt creature before either)
own_bed = function_body(creature, 'bool Creature::hasOwnBedInDormitory() const')
assert 'mHomeTile != nullptr' in own_bed and 'RoomType::dormitory' in own_bed and 'getSeat() == getSeat()' in own_bed
assert '!hasOwnBedInDormitory()' in wounded

# One worker per creature: the existing carry lock; the creature stands still, the pause after the pulling
assert 'getCarryLock' in read('source/entities/Tile.cpp')
start = function_body(creature, 'void Creature::notifyDragStart(')
assert 'mIsBeingDragged = true;' in start and 'clearActionQueue();' in start
end = function_body(creature, 'void Creature::notifyDragEnd(')
assert 'mWoundedCarryNextTurn =' in end and 'mIsBeingDragged = false;' in end
assert 'if(mIsBeingDragged && (mKoTurnCounter == 0) && isAlive())' in creature

# The carrying of real carried things is as it was: the entity is taken out of the map, nothing of the pulling in it
on = function_body(creature, 'void Creature::notifyEntityCarryOn(')
off = function_body(creature, 'void Creature::notifyEntityCarryOff(')
assert 'mIsBeingDragged' not in on + off and 'mWoundedCarryNextTurn' not in on + off
assert 'mIsBeingCarried' not in creature + creature_h, 'old carry flag of the carried wounded is dead code'
handler = function_body(carry, 'bool CreatureActionCarryEntity::handleCarryEntity(')
assert 'nbTurnsActive' not in handler and 'isHostileNear' not in handler

# Pulled, not carried: the hurt creature never goes into the carry node of the worker
assert 'static bool isPulledOverGround(const Creature& carrier, GameEntity& entity);' in carry_h
pulled = function_body(carry, 'bool CreatureActionCarryEntity::isPulledOverGround(')
assert 'isAlive()' in pulled and 'getSeat() == carrier.getSeat()' in pulled
assert 'getKoTurnCounter' not in pulled, 'knocked out to death is pulled like the other hurt ones'
assert 'isPulledOverGround(creature, entityToCarry)' in carry
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
               'followTrail(', 'setDragDestination(', 'startLaying(', 'isAlive()',
               'getNbTurnsActive()'):
    assert needle in drag, needle
assert 'getKoTurnCounter() < 0' not in drag, 'a creature knocked out to death is pulled on, not let go'
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
assert 'isWoundedForBedCarry()' in has_spot and 'isKoToDeathForBedPull()' in has_spot and 'getHomeTile()' in has_spot
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

# Own bed only (a): the pulling target is the bed the hurt creature owns in the dormitory, never another free bed
ask_spot = function_body(dormitory, 'Tile* RoomDormitory::askSpotForCarriedEntity(')
assert 'bed.getCreature() == creature' in ask_spot and 'return bed.getOwningTile();' in ask_spot
assert 'return nullptr;' in ask_spot.split('return bed.getOwningTile();')[1]
assert 'isFree' not in ask_spot and 'getTileData' not in ask_spot, 'no free bed or any dormitory tile as the target'
# (b) no own bed = not picked up, not pulled: the search/grab ask hasCarryEntitySpot, which wants the own bed to exist
assert 'askSpotForCarriedEntity(carriedEntity) == nullptr' in has_spot
assert 'homeTile == nullptr' in has_spot and 'getCoveringRoom() != this' in has_spot
assert 'hasCarryEntitySpot(' in read('source/creatureaction/CreatureActionSearchEntityToCarry.cpp')
assert 'hasCarryEntitySpot(' in read('source/creatureaction/CreatureActionGrabEntity.cpp')
# a bed lost while pulling (destroyed, given to someone else, other seat) makes the worker let go where it lies
assert re.search(r'dragged->getSeat\(\) != mBuildingDest->getSeat\(\)\) \|\| \(mBuildingDest->askSpotForCarriedEntity\(dragged\) != mTileDest\)\)\s*'
                 r'return stopDragging\(true\);', drag)
assert drag.index('askSpotForCarriedEntity(dragged)') < drag.index('if(mDragPhase == 1)'), 'checked in every phase'
# (c) death is never held back: the only hold-back of the upkeep is for a living, not knocked out creature, and
# resetKoTurns is only called at the bed (never on pick up) and only for a creature knocked out to death
assert 'if(mIsBeingDragged && (mKoTurnCounter == 0) && isAlive())' in creature
assert creature.index('if(mIsBeingDragged && (mKoTurnCounter == 0) && isAlive())') < creature.index('// If the counter reaches 0, the creature is dead')
assert 'resetKoTurns' not in carry and 'resetKoTurns' not in function_body(creature, 'void Creature::notifyDragStart(')
assert 'isAtBed' in notify and notify.index('if(!isAtBed)') < notify.index('creature->resetKoTurns();')
assert 'isAlive()' in drag

# Knocked out to death (13j): the same pulling as the other hurt ones, no carrying of them to a bed any more
# (1) search, pick up, target, pull, abort and lay down run over the pulling path
ko_pull = function_body(creature, 'bool Creature::isKoToDeathForBedPull() const')
for needle in ('getIsOnServerMap()', 'isAlive()', 'mKoTurnCounter >= 0', 'mIsBeingDragged',
               'isInPrison()', 'isWorker()', 'hasOwnBedInDormitory()', 'isHostileNear('):
    assert needle in ko_pull, needle
assert 'mWoundedCarryNextTurn' not in ko_pull and 'DormitoryWoundedCarryHpPercent' not in ko_pull, 'the counter is running'
assert re.search(r'if\(mKoTurnCounter < 0\)\s*\{\s*if\(\(carrier != nullptr\) && \(carrier->getSeat\(\) == getSeat\(\)\)\)\s*'
                 r'return isKoToDeathForBedPull\(\) \? EntityCarryType::koCreature : EntityCarryType::notCarryable;',
                 carry_type), 'own KO to death: pulled to the own bed or not touched'
assert re.search(r'EntityCarryType::notCarryable;\s*return EntityCarryType::koCreature;\s*\}', carry_type), 'enemies still go to a prison'
assert carry_type.index('mKoTurnCounter < 0') < carry_type.index('EntityCarryType::corpse')
assert 'bool pullable = (creature->getKoTurnCounter() < 0) ? creature->isKoToDeathForBedPull()' in has_spot
assert 'if(getIsOnServerMap())' in start and 'mKoTurnCounter >= 0' not in start, 'the pulled one lies down at once'
# (2) the counter and the death run on while it is pulled (the creature stays on the map, nothing holds it back)
assert 'mKoTurnCounter < 0' not in function_body(creature, 'void Creature::notifyDragStart(')
assert 'getIsOnMap()' in drag and not re.search(r'mKoTurnCounter\s*=\s*0', carry)
assert 'mIsBeingDragged && (mKoTurnCounter == 0) && isAlive()' in creature
assert 'if(!getIsOnMap())\n        return;' in creature, 'only a carried creature (off the map) stops the counter'
# let go on the way: a creature knocked out to death keeps lying and does not stand up
assert 'getKoTurnCounter() < 0' in stop and 'clearDestinations(EntityAnimation::die_anim, false, false)' in stop
# (3) revive and sleep in the own bed as before: only at the bed, the counter is reset there
assert 'creature->resetKoTurns();' in notify and 'creature->sleep();' in notify
assert notify.index('creature->resetKoTurns();') < notify.index('creature->sleep();')
# without an own bed a knocked out to death creature is not picked up and not pulled; the pull aborts if the bed is lost
assert 'hasOwnBedInDormitory()' in ko_pull and 'askSpotForCarriedEntity(carriedEntity) == nullptr' in has_spot
# the carrying of prisoners is not part of this: an enemy knocked out to death is still carried into a prison cell
prison = read('source/rooms/RoomPrison.cpp')
prison_spot = function_body(prison, 'bool RoomPrison::hasCarryEntitySpot(')
assert 'getKoTurnCounter() >= 0' in prison_spot and 'isAlliedSeat(creature->getSeat())' in prison_spot
assert 'isPulledOverGround(creature, entityToCarry)' in carry and 'getSeat() == carrier.getSeat()' in pulled
assert 'notifyEntityCarryOn(&mCreature)' in ctor, 'carrying stays for gold, bodies, traps and enemies for a prison'

print('wounded bed pulling: ok')
