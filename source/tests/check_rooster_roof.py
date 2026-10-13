#!/usr/bin/env python3
# Checks the rooster on the coop roof: after a random time he jumps on the roof of a coop of his own hatchery, sits
# there and crows (pose and sound) and jumps down again; he never stays up there (time limit, loading, pick up).
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
chicken = (root / 'source/entities/ChickenEntity.cpp').read_text()
room = (root / 'source/rooms/RoomHatchery.cpp').read_text()
rooster_h = (root / 'source/rooms/HatcheryRooster.h').read_text()
rooster_cpp = (root / 'source/rooms/HatcheryRooster.cpp').read_text()
config = (root / 'config/rooms.cfg').read_text()


def body(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start)]


# Random time from the config (with defaults), a crow lasts a limited number of turns
assert 'static_cast<uint32_t>' in body(room, 'RoosterSettings RoomHatchery::getRoosterSettings(') or 'mCrowMin' in room
for key in ('HatcheryRoosterCrowMin', 'HatcheryRoosterCrowMax', 'HatcheryRoosterCrowTurns', 'HatcheryCoopRoofHeight',
            'HatcheryCoopPerchOffset'):
    assert re.search(r'^# ' + key + r'\s', config, re.M) and re.search(r'^\s+' + key + r'\s', config, re.M), key
assert 'Random::Uint(0, 1000)' in body(room, 'void RoomHatchery::beginRoosterMood(')
assert 'mSinceCrow >= context.mCrowInterval' in rooster_cpp and 'settings.mCrowTurns' in rooster_cpp

# The server computes the roof place, the client only shows the position it gets
roost = body(room, 'bool RoomHatchery::roostOnRoof(')
assert 'getNearestCoop(' in roost and 'getPerchSpot(' in roost and 'getRoofHeight(' in roost and 'hopToRoof(' in roost
assert 'if(coopTile == nullptr)' in roost, 'a hatchery without a coop does not crash'
# Walk to the ground next to the coop first, flutter up only from there (no flight from far away), time limit on the way
assert 'getGroundSpot(*coopTile, approach)' in roost and 'rooster->walkToward(approach' in roost
assert 'if(position.distance(approach) < mRoosterSettings.mHopDistance && RoomObjectPath::clearPoint(obstacles, position))' in roost, 'hop only from next to the coop'
assert roost.index('if(position.distance(approach) < mRoosterSettings.mHopDistance && RoomObjectPath::clearPoint(obstacles, position))') < roost.index('rooster->hopToRoof('), 'up only after the arrival test'
assert 'hopFromFar' not in room + (root / 'source/rooms/RoomHatchery.h').read_text()
assert re.search(r'^\s+HatcheryRoosterHopDistance\s', config, re.M), 'distance for the hop from the config'
update_body = body(room, 'void RoomHatchery::updateRooster(')
assert 'countApproachTurn()' in update_body and '"HatcheryRoosterApproachTurns", 15.0' in update_body, 'time limit on the way, from the config'
assert re.search(r'^# HatcheryRoosterApproachTurns\s', config, re.M) and re.search(r'^\s+HatcheryRoosterApproachTurns\s+15$', config, re.M)
assert 'resetApproachTurns()' in body(room, 'void RoomHatchery::beginRoosterMood(')
limit_part = update_body[update_body.index('countApproachTurn()'):]
assert 'setMood(RoosterMood::strut, 0)' in limit_part[:300] and 'setRoomDriven(false)' in limit_part[:300], 'he gives up the crow when the limit is over'
assert 'if(roostOnRoof(rooster, ChickenPose::crow))' in room, 'the crow pose only where he crows, not while he walks or flutters'
assert roost.count('setAnimationState(pose, true)') == 2, 'the crow pose never overwrites the flutter or the walk'
# Landing right next to the coop (distance from the config)
ground = body(room, 'bool RoomHatchery::getGroundSpot(')
assert '"HatcheryRoosterLandReach", 1.0' in ground and 'spot.distance(coopCenter) <= reach' in ground
assert re.search(r'^# HatcheryRoosterLandReach\s', config, re.M) and re.search(r'^\s+HatcheryRoosterLandReach\s+1$', config, re.M)
nearest = body(room, 'Tile* RoomHatchery::getNearestCoop(')
assert 'mCentralActiveSpotTiles' in nearest, 'only a coop of the own hatchery'
assert 'RoosterMood::crow' in room and 'ChickenPose::crow' in room and 'Hatchery/Crow' in room

# Down again: free ground spot at the coop, with and without a coop
down = body(room, 'void RoomHatchery::climbDown(')
assert 'if(!rooster->isOnRoof())' in down and 'getGroundSpot(' in down and 'hopDown(' in down
assert 'if(coopTile != nullptr)' in down, 'no crash when the coop is gone'
begin = body(room, 'void RoomHatchery::beginRoosterMood(')
assert 'climbDown(rooster)' in begin

# He never stays on the roof: not after the mood, not after loading (the mood is not saved), not when picked up
update = body(room, 'void RoomHatchery::updateRooster(')
guard = 'if(rooster->isOnRoof() && (rooster->getMood() != RoosterMood::crow))'
assert guard in update and 'climbDown(rooster);' in update[update.index(guard):update.index(guard) + 200]
assert update.index(guard) < update.index('HatcheryRooster::decide('), 'checked before the rooster decides'
assert 'mOnRoof = (mPosition.z > 0.3);' in chicken, 'the roof state is rebuilt after loading'
assert chicken.count('mOnRoof = false;') >= 3, 'picked up, lost a fight and jumped down reset the roof state'

# Up and down use a complete roof-flight clip and continuous client movement.
chicken_h = (root / 'source/entities/ChickenEntity.h').read_text()
pose_h = (root / 'source/entities/ChickenPose.h').read_text()
assert 'static const std::string flutter = "Flutter";' in pose_h
start_hop = body(chicken, 'void ChickenEntity::startHop(')
assert 'clearDestinations(ChickenPose::roofFlight, false, false)' in start_hop, 'complete flight pose on the way up and down'
assert 'getRoomConfigDoubleOrDefault("HatcheryRoosterHopTurns", 4.0)' in start_hop and 'std::max(1.0' in start_hop, 'duration from the config, at least one turn'
assert re.search(r'^# HatcheryRoosterHopTurns\s', config, re.M) and re.search(r'^\s+HatcheryRoosterHopTurns\s+4$', config, re.M)
up = body(chicken, 'void ChickenEntity::hopToRoof(')
dn = body(chicken, 'void ChickenEntity::hopDown(')
assert 'startHop(' in up and 'startHop(' in dn and 'teleport(' not in up + dn, 'no instant change of place'
step = body(chicken, 'void ChickenEntity::continueHop(')
assert '--mHopTurnsLeft;' in step and 'roofFlightPosition(mHopFrom, mHopTo,' in step, 'server samples the shared curve'
assert 'moveTo(' not in step, 'no tick teleports during the flight'
client_step = body(chicken, 'void ChickenEntity::update(')
assert 'timeSinceLastFrame' in client_step and 'roofFlightPosition(mHopFrom, mHopTo,' in client_step
assert 'mHopElapsed >= mHopTurns' in client_step, 'bounded flight ending at the goal'
upkeep = body(chicken, 'void ChickenEntity::doUpkeep(')
assert 'continueHop();' in upkeep and upkeep.index('mHopTurnsLeft > 0') < upkeep.index('mBusyTurns > 0'), 'the flight is stepped before the pose wait'
# Time limit and reset: the flight counts down on the server every turn; teleport, pick up and a lost fight end it
assert 'mHopTurnsLeft = 0;' in body(chicken, 'void ChickenEntity::teleport(')
assert 'mHopTurnsLeft = 0;' in body(chicken, 'void ChickenEntity::pickup(') and 'mHopTurnsLeft = 0;' in body(chicken, 'bool ChickenEntity::loseFight(')
assert 'mHopTurnsLeft > 0' in chicken_h and 'isHopping()' in update
assert update.index('isHopping()') < update.index('countDownMood()'), 'the crow only counts down after landing'
# The sound stays at the start of the crow mood (see the sound check)
assert 'fireAnimalSound(*rooster, "Hatchery/Crow")' in begin

# Not wanted any more: night, sleep, crow at a new day
for text in (rooster_h, rooster_cpp, room):
    for needle in ('isNight', 'newDayCrowOwed', 'RoosterMood::sleep', 'RoosterMood::call', 'RoosterMood::lead'):
        assert needle not in text, needle

print('rooster roof checks passed')
