"""Workers carry hurt creatures into their bed.

Pure source wiring checks (no compiler, no game): server authority, configuration, one carrier per
creature, abort rules, nothing new to save or send, client reaction for the gentle lift."""
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
reactions_cfg = read('config/creatureReactions.cfg')
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
entity_h = read('source/entities/GameEntity.h')
dormitory = read('source/rooms/RoomDormitory.cpp')
carry = read('source/creatureaction/CreatureActionCarryEntity.cpp')
worker = read('source/render/WorkerReactions.cpp')

# Every value is in the configuration: documented, set, read with a default
keys = ['DormitoryWoundedCarryHpPercent', 'DormitoryWoundedCarryRadius', 'DormitoryWoundedCarryPriority',
        'DormitoryWoundedCarryCooldown', 'DormitoryWoundedCarryEnemyRadius', 'DormitoryWoundedCarryMaxTurns',
        'DormitoryWoundedCarryTempKo']
for key in keys:
    assert re.search(r'^# ' + key + r'\s', rooms_cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\t\d+', rooms_cfg, re.M), key + ' not set'
    assert re.search(r'getRoomConfigDoubleOrDefault\(\s*"' + key + '"', creature + carry), key + ' not read'

# A new carry priority exists, below the other carryable things
assert re.search(r'corpse,\s+woundedCreature,\s+skillEntity', entity_h)

# The creature decides on the server: own seat, radius, priority from the config, nothing for other seats
carry_type = function_body(creature, 'EntityCarryType Creature::getEntityCarryType(')
assert 'carrier->getSeat() == getSeat()' in carry_type and 'getIsOnServerMap()' in carry_type
assert 'isWoundedForBedCarry()' in carry_type and 'DormitoryWoundedCarryRadius' in carry_type

wounded = function_body(creature, 'bool Creature::isWoundedForBedCarry() const')
for needle in ('getIsOnServerMap()', 'isAlive()', 'isPossessed()', 'isInPrison()', 'mIsBeingCarried',
               'mWoundedCarryNextTurn', 'CreatureActionType::fight', 'CreatureActionType::flee',
               'isHostileNear(', 'RoomType::dormitory', 'myTile == mHomeTile'):
    assert needle in wounded, needle

# One carrier per creature: the existing carry lock, the carried creature stands still, the pause after a carry
assert 'getCarryLock' in read('source/entities/Tile.cpp')
on = function_body(creature, 'void Creature::notifyEntityCarryOn(')
assert 'mIsBeingCarried = true;' in on and 'clearActionQueue();' in on
off = function_body(creature, 'void Creature::notifyEntityCarryOff(')
assert 'mWoundedCarryNextTurn =' in off and 'mIsBeingCarried = false;' in off
assert 'if(mIsBeingCarried && (mKoTurnCounter == 0))' in creature

# The dormitory accepts the creature and lays it in its bed (sleep heals); KO to death keeps working
has_spot = function_body(dormitory, 'bool RoomDormitory::hasCarryEntitySpot(')
assert 'isWoundedForBedCarry()' in has_spot and 'getHomeTile()' in has_spot
notify = function_body(dormitory, 'void RoomDormitory::notifyCarryingStateChanged(')
assert 'creature->sleep();' in notify and 'if(creature->getKoTurnCounter() < 0)\n        creature->resetKoTurns();' in notify

# Abort: hostile close or too long, the creature is put down where the worker stands
handler = function_body(carry, 'bool CreatureActionCarryEntity::handleCarryEntity(')
assert 'isHostileNear(' in handler and 'DormitoryWoundedCarryMaxTurns' in handler and 'nbTurnsActive' in handler

# Nothing new to save or to send: no stream operator or notification of the carry state
for forbidden in ('mWoundedCarryNextTurn <<', '>> mWoundedCarryNextTurn', 'mIsBeingCarried <<', '>> mIsBeingCarried'):
    assert forbidden not in creature, forbidden
assert 'carryEntity' in read('source/network/ODClient.cpp')

# Client: gentle lift and put down, both events exist in the reaction config
for name in ('PickWounded', 'PutWoundedDown'):
    assert '"' + name + '"' in worker, name
    assert re.search(r'^\s*Name\s+' + name + r'\s*$', reactions_cfg, re.M), name
assert 'isKoDeath()' in function_body(worker, 'void WorkerReactions::noteCarry(')

print('wounded bed carry: ok')
