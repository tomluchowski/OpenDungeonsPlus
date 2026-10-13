#!/usr/bin/env python3
# Static checks of the server side chicken life cycle wiring (the rules themselves are covered by
# the 00-HatcheryCycle unit test).
from pathlib import Path

root = Path(__file__).resolve().parents[2]
chicken_h = (root / 'source/entities/ChickenEntity.h').read_text()
chicken = (root / 'source/entities/ChickenEntity.cpp').read_text()
room = (root / 'source/rooms/RoomHatchery.cpp').read_text()
food = (root / 'source/creatureaction/CreatureActionSearchFood.cpp').read_text()
client = (root / 'source/network/ODClient.cpp').read_text()
notif = (root / 'source/network/ServerNotification.h').read_text()
config = (root / 'config/rooms.cfg').read_text()


def body(text, start):
    i = text.index(start)
    return text[i:text.index('\n}\n', i)]


# Only a free hen is food: the rooster, chicks and eggs are skipped everywhere food is chosen.
assert 'isFree() && (mKind == ChickenKind::hen)' in chicken_h
assert '!isEdible()' in body(chicken, 'bool ChickenEntity::eatChicken')
assert '!chicken->isEdible()' in body(room, 'bool RoomHatchery::useRoom')
assert '!chicken->isEdible()' in food

# Old saves have no kind: the chicken stays a hen (the kind is read optionally after the position).
stream_reader = body(chicken, 'bool ChickenEntity::importFromStream')
assert 'mKind = static_cast<ChickenKind>(kind)' in stream_reader and 'is.clear()' in stream_reader
assert 'mKind(ChickenKind::hen)' in chicken

# Clients get the kind with the entity and a small event when it changes.
assert 'chickenKindChanged' in notif and 'chickenKindChanged' in client
assert 'ServerNotificationType::chickenKindChanged' in body(chicken, 'void ChickenEntity::setKind')
assert 'exportToPacket' in chicken_h and 'importFromPacket' in chicken_h

# Nothing appears without a source: coop spawn only for an empty hatchery or a missing rooster.
doUpkeep = body(room, 'void RoomHatchery::doUpkeep')
assert 'needCoopHen' in doUpkeep and 'needCoopRooster' in doUpkeep
assert doUpkeep.count('spawnFromCoop(') == 2
assert 'new ChickenEntity' not in doUpkeep

# Values come from the config.
for key in ('HatcheryLayMin', 'HatcheryLayMax', 'HatcheryHatchTurns', 'HatcheryGrowTurns',
            'HatcheryTilesPerChicken', 'HatcheryChickenSpawnRate'):
    assert key in config and key in room, key

print('hatchery life cycle checks passed')

# Breeding needs care: lay faster when claimed, lit and without enemies; eggs wait while enemies stand in the hatchery
cycle = (root / 'source/rooms/HatcheryCycle.cpp').read_text()
room_cpp = (root / 'source/rooms/RoomHatchery.cpp').read_text()
cfg = (root / 'config/rooms.cfg').read_text()
assert 'carePercent' in cycle and 'canHatch' in cycle
assert 'HatcheryCycle::withCare' in room_cpp and 'HatcheryCycle::canHatch(counts, false)' in room_cpp
hatching = room_cpp[room_cpp.index('// Eggs hatch while'):room_cpp.index('// Chicks grow up')]
assert 'isEnemyOnEggTile(*egg, enemies)' in hatching
assert hatching.index('isEnemyOnEggTile') < hatching.index('egg->incrementAge()')
assert 'enemy->getPositionTile() == egg.getPositionTile()' in room_cpp
assert 'HatcheryCareLightPercent' in cfg and 'HatcheryCareCalmPercent' in cfg and 'HatcheryCareLightRadius' in cfg
assert 'HatcheryCareLayPercent' not in cfg

# Enemies trample eggs, own creatures never eat them
cycle_h = (root / 'source/rooms/HatcheryCycle.h').read_text()
assert 'tramples' in cycle_h and 'egg->trample(enemy)' in room_cpp
assert 'HatcheryTramplePercent' in cfg and 'HatcheryTrampleRadius' in cfg
assert 'bool ChickenEntity::trample' in chicken and 'ChickenKind::egg' in chicken[chicken.index('bool ChickenEntity::trample'):][:200]
# only enemies are collected for trampling (allied seats are skipped)
assert 'isAlliedSeat' in room_cpp[room_cpp.index('void RoomHatchery::collectEnemies'):][:600]

rooster_h = (root / 'source/rooms/HatcheryRooster.h').read_text()
rooster_cpp = (root / 'source/rooms/HatcheryRooster.cpp').read_text()
# The new day crow is remembered as a state (the day he crowed for), it is saved, and old saves still load
assert 'newDayCrowOwed' in rooster_cpp and 'isNewDay(context.mTurn' not in rooster_cpp
assert 'int64_t mCrowDay' in rooster_h
assert 'context.mCrowDay = mLastCrowDay' in room_cpp
assert 'mLastCrowDay = std::max(mLastCrowDay' in body(room_cpp, 'void RoomHatchery::beginRoosterMood')
assert '"HatcheryDay "' in room_cpp[room_cpp.index('void RoomHatchery::exportToStream'):][:400]
imp_day = room_cpp[room_cpp.index('bool RoomHatchery::importFromStream'):][:1800]
assert 'HatcheryDay' in imp_day and 'seekg(pos)' in imp_day
assert 'test_RoosterNewDayCrow' in (root / 'source/tests/test_HatcheryCycle.cpp').read_text()
print('hatchery new day crow checks passed')

# The new rooster comes after the same wait as a hen (HatcheryChickenSpawnRate); the own rooster wait is gone,
# the saved counter stays so that old saves still load
assert 'HatcheryRoosterSpawnRate' not in config and 'HatcheryRoosterSpawnRate' not in room_cpp
assert 'mRoosterWait' not in room_cpp and 'mRoosterWait' not in cycle_h
assert 'mCoopRoosterWait >= settings.mCoopWait' in room_cpp
assert 'needCoopRooster(counts, mNumActiveSpots) && !mFightActive' in room_cpp
spawn_rooster = room_cpp[room_cpp.index('needCoopRooster(counts, mNumActiveSpots)'):][:400]
assert 'capacity' not in spawn_rooster

# Only one rooster per hatchery: two fight, the server draws the winner, the clients get a small event
assert 'needFight' in cycle_h and 'fightWinner' in cycle_h and 'fightContinues' in cycle_h
fight = room_cpp[room_cpp.index('void RoomHatchery::updateFight'):room_cpp.index('RoosterSettings RoomHatchery::getRoosterSettings')]
assert 'HatcheryCycle::fightWinner(Random::Uint(' in fight, 'winner is drawn by the server RNG'
assert fight.count('fightWinner(') == 1, 'drawn once per fight, never per client'
assert 'notifyFight(' in fight and 'loseFight()' in fight and 'ChickenPose::fight' in fight and 'ChickenPose::crow' in fight
assert 'climbDown' in fight and 'fightContinues' in fight
assert 'updateFight(roosters, settings, counts)' in room_cpp
assert 'if(!oneRooster->isFighting())' in room_cpp
for key in ('HatcheryFightTurns', 'HatcheryFightApproachTurns', 'HatcheryFightReach'):
    assert key in config, key
for key in ('HatcheryFightTurns', 'HatcheryFightApproachTurns', 'HatcheryFightReach'):
    assert key in room_cpp, key
assert 'bool ChickenEntity::loseFight' in chicken and 'ChickenState::dying' in body(chicken, 'bool ChickenEntity::loseFight')
pick = body(chicken, 'void ChickenEntity::pickup')
assert 'mFighting = false' in pick, 'a picked up rooster leaves the fight'
# the event is appended last, right after chickenKindChanged (no existing numeric value changes)
enum_body = notif[notif.index('enum class ServerNotificationType'):notif.index('};')]
assert enum_body.index('chickenKindChanged') < enum_body.index('chickenFight')
assert enum_body.rstrip().endswith('chickenFight')
assert 'ServerNotificationType::chickenFight' in chicken and 'ServerNotificationType::chickenFight' in client
assert 'case ServerNotificationType::chickenFight' in (root / 'source/network/ServerNotification.cpp').read_text()
print('hatchery rooster fight checks passed')
# The numbers of the rooster and the flock are settings read from the config, not fixed numbers in the room code
rooster_h = (root / 'source/rooms/HatcheryRooster.h').read_text()
rooster_cpp = (root / 'source/rooms/HatcheryRooster.cpp').read_text()
for member in ('mCrowTurns', 'mRoostDivisor', 'mGuardFar', 'mGuardNear', 'mGuardApproachGap', 'mCatchDistance',
               'mWalkGap', 'mHopDistance', 'mCallFollowGap', 'mSnuggleGap', 'mLeadScratchChance',
               'mCallScratchChance', 'mFightStandFactor'):
    assert member in rooster_h and ('settings.' + member in room_cpp or 'Settings.' + member in room_cpp), member
assert 'settings.mCrowTurns' in rooster_cpp and 'settings.mRoostDivisor' in rooster_cpp
assert 'mTurns = 4;' not in rooster_cpp and '/ 10)' not in rooster_cpp
acting = room_cpp[room_cpp.index('void RoomHatchery::actRoosterMood'):room_cpp.index('void RoomHatchery::updateRooster')]
for number in ('2.2', '0.9', '1.8', '0.55f', 'Random::Int(1, 3)', 'Random::Int(1, 2)'):
    assert number not in acting, number
print('hatchery rooster settings checks passed')
