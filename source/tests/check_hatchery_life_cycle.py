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
# There is no day and night: no day length, night share, sleep mood or day state in the rooster rules, the room, the
# config, the entities, the client and the unit tests. Old saves with the day line still load (the number is dropped).
night_terms = ('isNight', 'isNewDay', 'dayNumber', 'newDayCrowOwed', 'mDayTurns', 'mNightPercent', 'mRoostDivisor',
               'mCrowDay', 'mLastCrowDay', 'HatcheryNightPercent', 'HatcheryDayTurns', 'HatcheryRoosterRoostDivisor',
               'HatcheryChickSnuggleGap', 'mSnuggleGap', 'setCalm', 'mCalm', 'RoosterMood::roost', 'ChickenPose::roost')
for path in ('source/rooms/HatcheryRooster.h', 'source/rooms/HatcheryRooster.cpp', 'source/rooms/RoomHatchery.h',
             'source/rooms/RoomHatchery.cpp', 'source/rooms/HatcheryCycle.h', 'source/rooms/HatcheryCycle.cpp',
             'source/entities/ChickenEntity.h', 'source/entities/ChickenEntity.cpp', 'source/entities/ChickenPose.h',
             'source/render/RenderManagerChickens.cpp', 'config/rooms.cfg', 'source/tests/test_HatcheryCycle.cpp'):
    text = (root / path).read_text()
    for term in night_terms:
        assert term not in text, (path, term)
assert 'night' not in room_cpp.lower() and 'night' not in rooster_cpp.lower()
# the egg plan runs on its own turn counters and does not look at the time of day
cycle_cpp = (root / 'source/rooms/HatcheryCycle.cpp').read_text()
assert 'getTurnNumber' not in cycle_cpp and 'getTurnNumber' not in (root / 'source/rooms/HatcheryCycle.h').read_text()
assert 'getTurnNumber' not in room_cpp[room_cpp.index('void RoomHatchery::doUpkeep'):room_cpp.index('void RoomHatchery::updateFight')]
# a save of an older version has the day line after the waiting counters: it is read, dropped and not written again
exp = room_cpp[room_cpp.index('void RoomHatchery::exportToStream'):][:900]
assert '"HatcheryDay' not in exp and '"HatcheryWaits "' in exp
imp_day = room_cpp[room_cpp.index('bool RoomHatchery::importFromStream'):][:2600]
assert 'tag == "HatcheryDay"' in imp_day and 'seekg(pos)' in imp_day
assert 'test_RoosterCrowTimer' in (root / 'source/tests/test_HatcheryCycle.cpp').read_text()
print('hatchery no day and night checks passed')

# The hens roam: they walk to free points anywhere in the room, they neither sit in the coops nor follow the rooster
assert 'updateFlock(hens);' in room_cpp and 'planWanderPath' in (root / 'source/entities/ChickenEntity.cpp').read_text()
assert 'updateCoopSitting' not in room_cpp and 'HatcheryCoopSit' not in config and 'HatcheryCallFollowGap' not in config

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
assert enum_body.rstrip().endswith('hatcheryNests')
assert 'ServerNotificationType::chickenFight' in chicken and 'ServerNotificationType::chickenFight' in client
assert 'case ServerNotificationType::chickenFight' in (root / 'source/network/ServerNotification.cpp').read_text()
print('hatchery rooster fight checks passed')
# The numbers of the rooster and the flock are settings read from the config, not fixed numbers in the room code
rooster_h = (root / 'source/rooms/HatcheryRooster.h').read_text()
rooster_cpp = (root / 'source/rooms/HatcheryRooster.cpp').read_text()
for member in ('mCrowTurns', 'mGuardFar', 'mGuardNear', 'mGuardApproachGap', 'mCatchDistance',
               'mWalkGap', 'mHopDistance', 'mLeadScratchChance',
               'mCallScratchChance', 'mFightStandFactor'):
    assert member in rooster_h and ('settings.' + member in room_cpp or 'Settings.' + member in room_cpp), member
assert 'settings.mCrowTurns' in rooster_cpp
assert 'mTurns = 4;' not in rooster_cpp and '/ 10)' not in rooster_cpp
acting = room_cpp[room_cpp.index('void RoomHatchery::actRoosterMood'):room_cpp.index('void RoomHatchery::updateRooster')]
for number in ('2.2', '0.9', '1.8', '0.55f', 'Random::Int(1, 3)', 'Random::Int(1, 2)'):
    assert number not in acting, number
print('hatchery rooster settings checks passed')

# The waiting counters of the room are saved (old saves without the line still load), one hen per coop comes out
room_h = (root / 'source/rooms/RoomHatchery.h').read_text()
assert 'exportToStream' in room_h and 'importFromStream' in room_h
exp = room_cpp[room_cpp.index('void RoomHatchery::exportToStream'):][:300]
assert 'HatcheryWaits' in exp and 'mCoopHenWait' in exp and 'mCoopRoosterWait' in exp
stream_reader = room_cpp[room_cpp.index('bool RoomHatchery::importFromStream'):][:900]
assert 'seekg(pos)' in stream_reader and 'HatcheryWaits' in stream_reader
assert 'coopHenCount' in room_cpp and 'HatcheryCoopBatch' in cfg
print('hatchery saved waits checks passed')

doUpkeep = body(room_cpp, 'void RoomHatchery::doUpkeep')
# Pickup captures the keeper before detach; allied hatcheries cannot adopt young animals.
pickup = body(chicken, 'void ChickenEntity::pickup')
assert 'mHomeSeat = tile->getSeat();' in pickup
assert pickup.index('mHomeSeat = tile->getSeat();') < pickup.index('RenderedMovableEntity::pickup();')
upkeep = body(chicken, 'void ChickenEntity::doUpkeep')
assert '(room->getSeat() == mHomeSeat)' in upkeep and '(mHomeSeat == nullptr)' in upkeep
assert 'HatcheryYoungLostTurns' in upkeep and '(currentHatchery == nullptr)' in upkeep
assert 'chicken->getHomeSeat() != getSeat()' in doUpkeep
assert doUpkeep.index('chicken->getHomeSeat() != getSeat()') < doUpkeep.index('switch(chicken->getKind())')
print('young animal same-keeper lifecycle source contracts passed; C++ not executed')

# A hen outside a hatchery walks back to one like the rooster: the search and the way are limited by settings, the
# hen gives up after a while and the old turn rule applies again
return_walk = body(chicken, 'bool ChickenEntity::runBackToHatchery')
for key in ('HatcheryReturnPathTiles', 'HatcheryReturnSearchTiles', 'HatcheryReturnRetryTurns', 'HatcheryReturnMaxTurns'):
    assert key in config and key in chicken, key
assert 'henMayReturn' in chicken and 'failReturn()' in return_walk
assert 'mReturnTurns' in return_walk and 'mReturnRetryTurns' in return_walk
print('hatchery hen return checks passed')

# Eggs are laid in a free straw nest of the nest field (closest nest first), the place rules are the pure pickNestPlace
# (the places of the field are checked in check_hatchery_nest_field.py)
cycle_cpp = (root / 'source/rooms/HatcheryCycle.cpp').read_text()
cycle_h = (root / 'source/rooms/HatcheryCycle.h').read_text()
assert 'pickNestPlace' in cycle_h and 'int32_t HatcheryCycle::pickNestPlace' in cycle_cpp
nest = body(room_cpp, 'bool RoomHatchery::findNestSpot')
assert 'getNestPlaces()' in nest and 'HatcheryCycle::pickNestPlace(occupied, 1)' in nest
assert 'squaredDistance' in nest, 'closest nest first'
assert 'HatcheryNestEggs' in nest and 'HatcheryNestSameRadius' in nest
assert 'HatcheryNestEggs' in cfg and 'HatcheryNestSameRadius' in cfg
assert (root / 'source/rooms/HatcheryNestField.h').exists()
lay = doUpkeep[doUpkeep.index('Hens lay eggs while the hatchery is not full'):doUpkeep.index('Eggs hatch while there is a rooster')]
assert 'findNestSpot(' in lay and 'eggs.push_back(spawnAnimal(ChickenKind::egg, eggSpot, settings))' in lay
assert 'eggPositions.push_back' in lay, 'an egg laid this turn takes its place at once'
assert 'HatcheryCycle::canLay(counts, capacity)' in lay, 'capacity still limits the eggs'
assert 'ChickenPose::lay' in lay, 'the hen sits down where she is when the egg has no nest'
# The hen plans her egg early, walks to the place next to the nest (real distance, real walking speed) and lays there
assert 'getNestStandPoint(eggSpot, standing)' in lay and 'plan.mHen = hen->getName()' in lay
walk_body = body(room_cpp, 'uint32_t RoomHatchery::nestWalkTurns')
assert 'HatcheryCycle::walkTurns(' in walk_body and 'getMoveSpeed()' in walk_body and 'ODApplication::turnsPerSecond' in walk_body, 'the walk window follows the real distance and speed'
assert 'nestWalkTurns(*hen, standing)' in lay and 'HatcheryCycle::walkFits(walk, hen->getLayTimer(), settings)' in lay
assert 'leadTurns' not in room_cpp and 'mNestWalkTurns' not in room_cpp and 'mNestWalkTurns' not in cycle
assert 'planned->mDue = true' in lay, 'the egg is laid when the laying timer runs out, not when the hen arrives'
trips = body(room_cpp, 'void RoomHatchery::updateNestTrips')
assert 'HatcheryNestArrive' in trips and 'ChickenPose::lay' in trips and 'HatcheryCycle::tripDue(' in trips, 'she sets off when the turns left are as many as walk and pose'
assert 'setFollowTarget(it->mStand' in trips and 'hen == nullptr' in trips and 'it->mDue' in trips, 'a hen that is gone takes a planned egg with her, a laid one still appears'
assert 'uint32_t HatcheryCycle::walkTurns' in cycle_cpp and 'bool HatcheryCycle::tripDue' in cycle_cpp and 'layDelay' not in cycle_cpp
assert 'standingPosition' in body(room_cpp, 'bool RoomHatchery::getNestStandPoint')
assert doUpkeep.index('updateNestTrips(hens, settings)') < doUpkeep.index('hen->countDownLay()') < doUpkeep.index('releasePendingEggs(settings, eggs)')
assert 'HatcheryNestWalkTurns' not in cfg and 'HatcheryNestArrive' in cfg and 'HatcheryLayFactor' in cfg and 'HatcheryLayFactor' in room_cpp
# A late egg (the walk was longer than the time left) gets the age it would have had, and so does the chick: the rhythm
# of the cycle does not depend on the way to the nest. The parity test in the unit tests models exactly this.
assert 'egg->setAge(it->mLate)' in body(room_cpp, 'void RoomHatchery::releasePendingEggs')
assert 'setAge(eggAge - settings.mHatchTurns)' in doUpkeep and 'chicks.push_back(egg)' in doUpkeep
assert 'void setAge' in chicken_h or 'inline void setAge' in chicken_h
assert 'eggs.erase(eggIt)' in doUpkeep and doUpkeep.index('eggs.erase(eggIt)') < doUpkeep.index('eggPositions.push_back'),     'trampled eggs free their place'
# The chick from a nest (and a hen that sat in a coop) stands on the ground at a free spot next to where it is
assert 'leaveNest(egg)' in doUpkeep and 'chick->teleport(' in body(room_cpp, 'void RoomHatchery::leaveNest')
assert 'standingPosition' in body(room_cpp, 'void RoomHatchery::leaveNest')
teleport_pos = chicken_h.index('void teleport(')
assert 'private:' not in chicken_h[chicken_h.index('void hopDown('):teleport_pos], 'teleport is public'
# Egg save/load: the position (with the height) is saved by the entity, nothing new is saved
assert 'mPosition.z' in body(chicken, 'void ChickenEntity::exportToStream') or 'mPosition.z' in chicken
# Client: the egg in a nest has no straw of its own, the nest mesh has the straw
render = (root / 'source/render/RenderManagerChickens.cpp').read_text()
assert 'hideEggStraw' in render and 'mNestEgg' in render and 'ChickenStraw' in render

# Trampling: shell pieces, yolk and feathers, shown by the clients, no new network value
assert 'fireEggTrample(*egg)' in doUpkeep
fx = body(room_cpp, 'void RoomHatchery::fireEggTrample')
assert 'HatcheryFx/EggTrample' in fx and 'ServerNotificationType::playSpatialSound' in fx
assert 'HatcheryFx/EggTrample' in client and 'rrEggTrampled' in client
assert 'void RenderManager::rrEggTrampled' in render and '"ChickenEggTrample"' in render
particles = (root / 'particles/ChickenEggShell.particle').read_text()
trample = particles[particles.index('particle_system ChickenEggTrample'):]
assert trample.count('emitter Point') == 2, 'shell emitter and yolk emitter'
print('hatchery nest egg and trample checks passed')

# The hen shows herself laying: the egg appears after HatcheryLayShowTurns turns, a pending egg is saved and counted
assert 'mLayShowTurns' in (root / 'source/rooms/HatcheryCycle.h').read_text()
assert 'HatcheryLayShowTurns' in room_cpp and 'HatcheryLayShowTurns' in config
laying = body(room_cpp, 'void RoomHatchery::doUpkeep')
assert 'mPendingEggs.push_back(PendingEgg(eggSpot, settings.mLayShowTurns))' in laying
assert 'releasePendingEggs(settings, eggs)' in laying and 'if(pending.mDue)' in laying
assert laying.index('hen->countDownLay()') < laying.index('releasePendingEggs(settings, eggs)')
release = body(room_cpp, 'void RoomHatchery::releasePendingEggs')
assert 'spawnAnimal(ChickenKind::egg' in release and 'erase(it)' in release
# the egg is created in one place only (here or at once without delay), never in both
assert laying.count('spawnAnimal(ChickenKind::egg') == 1
assert '"HatcheryLays "' in room_cpp[room_cpp.index('void RoomHatchery::exportToStream'):][:900]
assert 'tag == "HatcheryLays"' in room_cpp[room_cpp.index('bool RoomHatchery::importFromStream'):][:2600]
print('hatchery delayed egg checks passed')
