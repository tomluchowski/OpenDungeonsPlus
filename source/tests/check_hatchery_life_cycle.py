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

# Eggs are laid in a free place of a coop nest (closest coop first), the place rules are the pure pickNestPlace
cycle_cpp = (root / 'source/rooms/HatcheryCycle.cpp').read_text()
cycle_h = (root / 'source/rooms/HatcheryCycle.h').read_text()
coop_h = (root / 'source/rooms/HatcheryCoopHouse.h').read_text()
assert 'pickNestPlace' in cycle_h and 'int32_t HatcheryCycle::pickNestPlace' in cycle_cpp
nest = body(room_cpp, 'bool RoomHatchery::findNestSpot')
assert 'HatcheryCoopHouse::nestEggSpotWorld' in nest and 'HatcheryCycle::pickNestPlace' in nest
assert 'getCoveringRoom() != this' in nest, 'a nest place outside of the hatchery is not used'
assert 'mCentralActiveSpotTiles' in nest and 'squaredDistance' in nest, 'closest coop first'
assert 'HatcheryNestEggs' in nest and 'HatcheryNestSameRadius' in nest
assert 'HatcheryNestEggs' in cfg and 'HatcheryNestSameRadius' in cfg
assert 'nestEggSpot' in coop_h and 'nestCount' in coop_h and 'eggsPerNest' in coop_h
lay = doUpkeep[doUpkeep.index('hen->countDownLay()'):doUpkeep.index('Eggs hatch while there is a rooster')]
assert 'findNestSpot(' in lay and 'spawnAnimal(ChickenKind::egg, eggSpot, settings)' in lay
assert 'eggPositions.push_back' in lay, 'an egg laid this turn takes its place at once'
assert 'HatcheryCycle::canLay(counts, capacity)' in lay, 'capacity still limits the eggs'
assert 'ChickenPose::lay' in lay, 'the hen sits down where she is (robust variant)'
# The hen walks to the place next to the nest first and lays there (the egg lies in the nest)
assert 'getNestStandPoint(eggSpot, standing)' in lay and 'trip.mHen = hen->getName()' in lay and 'setFollowTarget(standing' in lay
assert doUpkeep.index('isOnNestTrip(*hen)') < doUpkeep.index('hen->countDownLay()'), 'a hen on her way does not start a second egg'
trips = body(room_cpp, 'void RoomHatchery::updateNestTrips')
assert 'HatcheryNestArrive' in trips and 'ChickenPose::lay' in trips and 'it->mWalkTurns' in trips, 'arrival or timeout starts the Lay pose'
assert 'hen == nullptr' in trips and '--counts.mEggs' in trips, 'a hen that is gone takes her egg with her'
assert 'standingPosition' in body(room_cpp, 'bool RoomHatchery::getNestStandPoint')
assert '!it->mHen.empty()' in body(room_cpp, 'void RoomHatchery::releasePendingEggs'), 'the egg waits for the hen'
assert doUpkeep.index('updateNestTrips(hens, counts)') < doUpkeep.index('releasePendingEggs(settings)')
assert 'HatcheryNestWalkTurns' in cfg and 'HatcheryNestArrive' in cfg
# The walk must not delay the hatching (balance parity): the egg takes the turns since the hen started as its age
assert '++it->mAge' in body(room_cpp, 'void RoomHatchery::releasePendingEggs') and '->setAge(it->mAge)' in room_cpp
assert 'void setAge(' in chicken_h
assert 'eggs.erase(eggIt)' in doUpkeep and doUpkeep.index('eggs.erase(eggIt)') < doUpkeep.index('eggPositions.push_back'),     'trampled eggs free their place'
# The chick from a nest stands next to the coop, the nest lies in the footprint of the coop
assert 'leaveNest(egg)' in doUpkeep and 'chick->teleport(' in body(room_cpp, 'void RoomHatchery::leaveNest')
assert 'standingPosition' in body(room_cpp, 'void RoomHatchery::leaveNest')
teleport_pos = chicken_h.index('void teleport(')
assert 'private:' not in chicken_h[chicken_h.index('void hopDown('):teleport_pos], 'teleport is public'
# Egg save/load: the position (with the height) is saved by the entity, nothing new is saved
assert 'mPosition.z' in body(chicken, 'void ChickenEntity::exportToStream') or 'mPosition.z' in chicken
# Client: the egg in a nest has no straw of its own, the coop mesh has the nests
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
assert 'releasePendingEggs(settings)' in laying and 'eggs.size() + mPendingEggs.size()' in laying
assert laying.index('releasePendingEggs(settings)') < laying.index('hen->countDownLay()')
release = body(room_cpp, 'void RoomHatchery::releasePendingEggs')
assert 'spawnAnimal(ChickenKind::egg' in release and 'erase(it)' in release
# the egg is created in one place only (here or at once without delay), never in both
assert laying.count('spawnAnimal(ChickenKind::egg') == 1
assert '"HatcheryLays "' in room_cpp[room_cpp.index('void RoomHatchery::exportToStream'):][:900]
assert 'tag == "HatcheryLays"' in room_cpp[room_cpp.index('bool RoomHatchery::importFromStream'):][:2600]
print('hatchery delayed egg checks passed')
