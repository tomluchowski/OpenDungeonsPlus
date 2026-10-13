#!/usr/bin/env python3
# Checks the free roaming of the hatchery animals: hens, chicks and the rooster walk to free points anywhere in the
# room (no step from tile to tile), the hens do not gather at the calling rooster, and the rooster sits on a roof only
# to crow (jumps up for the crow after a random time, jumps down again afterwards).
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
chicken = (root / 'source/entities/ChickenEntity.cpp').read_text()
chicken_h = (root / 'source/entities/ChickenEntity.h').read_text()
room = (root / 'source/rooms/RoomHatchery.cpp').read_text()
room_h = (root / 'source/rooms/RoomHatchery.h').read_text()
rooster_h = (root / 'source/rooms/HatcheryRooster.h').read_text()
rooster_cpp = (root / 'source/rooms/HatcheryRooster.cpp').read_text()
config = (root / 'config/rooms.cfg').read_text()


def body(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start)]


# The roaming walk is a free point in the room, not a neighbouring tile
wander = body(chicken, 'void ChickenEntity::wander(')
assert 'planWanderPath(' in wander and 'setWalkPath(' in wander
for forbidden in ('collectMovePositions', 'addTileToListIfPossible', 'getTile(', 'getX()', 'getY()', 'getPositionTile'):
    assert forbidden not in wander, 'wander is not tied to tiles: ' + forbidden
assert 'wander(currentHatchery);' in chicken and 'wander(tile' not in chicken
assert 'void wander(Room* currentHatchery);' in chicken_h
# the only user of the neighbour tile positions is the short flight from a hungry creature (see ChickenFlight.h)
assert len(re.findall(r'collectMovePositions\(', chicken)) == 2, 'definition and the flight only'

plan = body(room, 'bool RoomHatchery::planWanderPath(')
free = body(room, 'bool RoomHatchery::isFreeWanderPoint(')
pick = body(room, 'bool RoomHatchery::pickFreePoint(')
# Every covered tile can be hit and the point inside it is a random real position (not the tile middle)
for text in (plan, pick):
    assert 'mCoveredTiles[Random::Uint(0, mCoveredTiles.size() - 1)]' in text
    assert text.count('Random::Double(-0.5, 0.5)') == 2, 'a random position inside the tile in x and y'
    assert 'isFreeWanderPoint(' in text
# Free: on a room tile, at a distance from walls, not in a coop footprint, not in a nest
for needle in ('getCoveringRoom() != this', 'edge * edge', 'RoomObjectPath::clearPoint(obstacles, point)',
               'getNestPlaces()', 'nestClearance * nestClearance'):
    assert needle in free, needle
# A natural way: one soft bend, obstacles are avoided, the way stays in the room
for needle in ('RoomObjectPath::clearSegment(', 'isSegmentInRoom(', 'side *', 'path.push_back(middle)', 'reach'):
    assert needle in plan, needle
assert 'tile->getX()' in plan and 'Random::Double(-0.5, 0.5)' in plan
assert 'getX() +' not in plan.replace('tile->getX() + Random::Double', ''), 'no integer tile steps in the plan'
# The scatter of a hen and the run off of a guarding rooster use free points too
flock = body(room, 'void RoomHatchery::updateFlock(')
assert 'pickFreePoint(spot)' in flock and 'away->getX()' not in flock
guard = room[room.index('case RoosterMood::guard:'):]
guard = guard[:guard.index('\n        }\n')]
assert 'pickFreePoint(away)' in guard and 'mCoveredTiles[' not in guard

# Hens do not gather at the calling rooster any more
assert 'caller' not in room and 'mCallFollowGap' not in room + rooster_h and 'CallFollowGap' not in config
hen_follow = [line for line in room.splitlines() if 'hen->setFollowTarget(' in line]
assert hen_follow == ['                hen->setFollowTarget(it->mStand, 0.1);'], 'only the nest walk of a hen sets a follow target'

# The rooster does not stay on a roof: no perch mood, the roof only for the crow, then down again
assert 'perch' not in rooster_h and 'perch' not in rooster_cpp and 'RoosterMood::perch' not in room
assert 'Perch' not in rooster_h and 'RoosterPerch' not in config and 'RoosterPerch' not in room
assert 'RoosterMood::crow' in rooster_cpp and 'mSinceCrow >= context.mCrowInterval' in rooster_cpp
assert 'HatcheryRoosterCrowMin' in config and 'HatcheryRoosterCrowMax' in config
begin = body(room, 'void RoomHatchery::beginRoosterMood(')
assert 'bool roofMood = (plan.mMood == RoosterMood::crow);' in begin and 'climbDown(rooster)' in begin
assert 'roostOnRoof(rooster, ChickenPose::crow, true)' in room
assert room.count('roostOnRoof(rooster') == 1, 'the crow is the only reason for the roof'

# The numbers are settings with a comment in the config
for key in ('HatcheryWanderPausePercent', 'HatcheryWanderReach', 'HatcheryWanderMinLeg', 'HatcheryWanderBend',
            'HatcheryWanderEdge', 'HatcheryWanderNestClearance', 'HatcheryWanderAttempts'):
    assert re.search(r'^# ' + key + r'\s', config, re.M) and re.search(r'^\s+' + key + r'\s', config, re.M), key
    assert key in chicken or key in room, key
assert '"HatcheryWanderEdge", 0.35' in room and re.search(r'HatcheryWanderEdge\s+0\.35', config)
print('hatchery roaming checks passed')
