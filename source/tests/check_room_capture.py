"""Checks of taking over enemy and neutral rooms and portals (task M10). It does not start a game.

The pool maths and the claim rule are compiled from the real header source/rooms/RoomClaim.h
with the values of config/rooms.cfg; the wiring in Room.cpp, RoomPortal.cpp and the config is
checked on the sources. Needs the MSVC compiler (cl) on the path, like check_level_statistics.py.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

repo = Path(__file__).resolve().parents[2]


def read(relative):
    return (repo / relative).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


rooms_cfg = read('config/rooms.cfg')
room_source = read('source/rooms/Room.cpp')
room_header = read('source/rooms/Room.h')
portal_source = read('source/rooms/RoomPortal.cpp')
portal_header = read('source/rooms/RoomPortal.h')
search_source = read('source/creatureaction/CreatureActionSearchGroundTileToClaim.cpp')
wave_source = read('source/rooms/RoomPortalWave.cpp')


def config_value(key):
    match = re.search(r'^\s+' + key + r'\s+([0-9.]+)\s*$', rooms_cfg, re.M)
    check(match is not None, 'config/rooms.cfg has no value for ' + key)
    return match.group(1) if match else '0'


# Configuration: the default behaviour
check(config_value('RoomsClaimableByEnemies') == '1', 'RoomsClaimableByEnemies must be 1 (claimable and destructible)')
check(config_value('RoomConvertSecondsPerTile') == '2.5', 'an enemy room takes 2.5 seconds per tile')
check(config_value('RoomConvertNeutralSecondsPerTile') == '0.5', 'a neutral room takes 0.5 seconds per tile')
creatures_cfg = read('config/creatures.cfg')
worker_claim = re.search(r'^\s+ClaimRate\s+([0-9.]+)\s*$', creatures_cfg, re.M).group(1)
check(config_value('RoomConvertClaimRate') == worker_claim,
      'RoomConvertClaimRate must be the claim rate of the worker (' + worker_claim + ')')
check(config_value('RoomRepairFactor') == '5.0', 'an own worker repairs 5 times faster than an enemy wears down (20000 against 4000)')
check(config_value('PortalFirstSpawnSeconds') == '25', 'the first creature of a taken over portal comes after 25 seconds')
check('"RoomsClaimableByEnemies", DEFAULT_ROOMS_CLAIMABLE_BY_ENEMIES)' in room_source
      and 'static const double DEFAULT_ROOMS_CLAIMABLE_BY_ENEMIES = 1.0;' in room_source,
      'without the setting the default behaviour applies')
turns_per_second = re.search(r'double ODApplication::turnsPerSecond = ([0-9.]+);', read('source/ODApplication.cpp')).group(1)

# Wiring
is_claimable = function(room_source, 'bool Room::isClaimable(')
check('RoomClaim::isClaimableBy(' in is_claimable and 'RoomType::dungeonTemple' in is_claimable,
      'Room::isClaimable must keep the dungeon heart exception through RoomClaim::isClaimableBy')
check('isClaimable' not in portal_header and 'isClaimable' not in portal_source,
      'the portal follows the rule of Room::isClaimable and has no override of its own')
check('isAttackable' in portal_header and 'return false;' in portal_header[portal_header.index('isAttackable'):][:160],
      'the portal can still not be destroyed')
check('return false' in function(wave_source, 'bool RoomPortalWave::isClaimable('), 'the hero portal is still not claimable')

claim = function(room_source, 'void Room::claimForSeat(')
check('RoomConvertNeutralSecondsPerTile' in claim and 'RoomConvertSecondsPerTile' in claim and 'isRogueSeat()' in claim,
      'Room::claimForSeat must use the neutral rate for a room of the rogue seat')
check('mClaimHealth -= RoomClaim::healthLostPerDance(' in claim, 'every dance lowers the one pool of the room')
check(claim.index('mClaimHealth -= ') < claim.index('changeOwner(seat);'), 'the owner changes only after the pool is lowered')
check('if(mClaimHealth > 0.0)' in claim, 'the room only changes hands when the pool is empty')
check('mClaimedValue' not in function(room_source, 'void Room::claimForSeat('), 'the per tile claim value is not used for rooms')

change_owner = function(room_source, 'void Room::changeOwner(')
check('handTilesOverToSeat(seat, tiles)' in change_owner and 'std::vector<Tile*> tiles = mCoveredTiles;' in change_owner,
      'Room::changeOwner hands every tile over at once')
check('notifyOwnerChanged(oldSeat, seat)' in change_owner, 'Room::changeOwner tells both seats')
hand_over = function(room_source, 'Room* Room::handTilesOverToSeat(')
check(hand_over.index('addToGameMap(gameMap)') < hand_over.index('tile->claimTile(seat)'),
      'the tiles are claimed after the new room is on the map')
notify = function(room_source, 'void Room::notifyOwnerChanged(')
check('You have claimed a room' in notify and 'You have captured a room' in notify and 'A room has been lost' in notify,
      'the new and the old owner get a message')

portal_change = function(portal_source, 'void RoomPortal::changeOwner(')
check('setSeat(seat);' in portal_change and 'claimTile(seat)' in portal_change, 'a taken over portal changes seat and tiles')
check('mSpawnCreatureCountdown =' in portal_change and 'PortalFirstSpawnSeconds' in portal_change,
      'a taken over portal restarts its spawn timer for the new owner')
check('mClaimHealth = 1.0;' in portal_change, 'a taken over portal is full again')
check('mRoomsCaptured++' in portal_change, 'a taken over portal counts as a captured room')
check('mClaimedValue' not in portal_source and 'mClaimedValue' not in portal_header, 'the portal uses the pool of Room')
check('numCoveredTiles()' in function(portal_source, 'void RoomPortal::exportToStream('),
      'the portal file format still stores the claim value as a number of tiles')

# A bridge is one room with one pool: no square by square claim value, the research only scales the dance
bridge_source = read('source/rooms/RoomBridge.cpp')
bridge_claim = function(bridge_source, 'void RoomBridge::claimForSeat(')
check('Room::claimForSeat(seat, tile, danceRate)' in bridge_claim and 'getResearchValue(' in bridge_claim,
      'a bridge is taken over with the pool of Room, slowed by the research of its owner')
check('mClaimedValue' not in bridge_source and 'mClaimedValue' not in read('source/rooms/RoomBridge.h'),
      'a bridge has no claim value per square any more')
check('numCoveredTiles()' in function(bridge_source, 'void RoomBridge::exportToStream('),
      'the bridge file format still stores the claim value as a number of squares')

# The pool is saved for every kind of room, as an optional field on the first tile line (older versions skip it,
# a save without it loads full, an untouched room writes nothing extra)
tile_export = function(room_source, 'void Room::exportTileDataToStream(')
tile_import = function(room_source, 'bool Room::importTileDataFromStream(')
check('RoomClaim::writeClaimPool(os, mClaimHealth)' in tile_export and 'mCoveredTiles.front() == tile' in tile_export,
      'the pool is written with the first tile of every room')
check(tile_export.index('seatsToSave') < tile_export.index('writeClaimPool'), 'the pool follows the fields older versions know')
check('mClaimHealth = RoomClaim::readClaimPool(is, mClaimHealth)' in tile_import, 'the pool is read back for every room')
check(tile_import.index('mSeatsVision.push_back(seat)') < tile_import.index('readClaimPool'), 'the pool is read behind the known fields')
claim_header = read('source/rooms/RoomClaim.h')
check('if(health < 1.0)' in function(claim_header, 'inline void writeClaimPool(')
      and 'std::min(1.0, std::max(0.0, health))' in function(claim_header, 'inline double readClaimPool('),
      'only a worn down pool is written, a read value stays between empty and full')
check('mClaimHealth(1.0)' in function(room_source, 'Room::Room('), 'a new room starts with a full pool')
check('double getClaimHealth() const' in room_header and 'double mClaimHealth;' in room_header
      and 'virtual void changeOwner(Seat* seat);' in room_header, 'Room.h declares the pool and changeOwner')
check('virtual void changeOwner(Seat* seat) override;' in portal_header, 'RoomPortal.h declares its changeOwner')
check('mClaimHealth = (mClaimHealth * nbTilesThis' in function(room_source, 'void Room::absorbRoom('),
      'merging rooms averages the pool by tiles')
check('newRoom->mClaimHealth = mClaimHealth;' in function(room_source, 'void Room::checkForSplit('),
      'the part of a room that breaks off keeps the share of the pool')
check('inline double takeoverSeconds(' in read('source/rooms/RoomClaim.h')
      and 'numCoveredTiles()' in claim, 'the pool of a room is its number of tiles times the duration of one tile')

check('logPortalCandidate(creature, myTile, "standing")' in search_source and '"neighbor"' in search_source
      and '"sight"' in search_source, 'the claim search logs portal tiles')
check('RoomType::portal' in function(search_source, 'static void logPortalCandidate('), 'only portal tiles are logged')
# Repair of a worn down room by the own workers
repair = function(room_source, 'void Room::repairClaimHealth(')
check('RoomRepairFactor' in repair and 'RoomClaim::healthRepairedPerDance(' in repair, 'repairing uses RoomRepairFactor')
check('mClaimHealth > 1.0' in repair, 'the pool of a room never goes above full')
check('bool needsClaimRepair() const' in room_header and 'mClaimHealth < 1.0' in room_header, 'Room.h tells when a room needs repair')
claim_source = read('source/creatureaction/CreatureActionClaimGroundTile.cpp')
claim_action = function(claim_source, 'bool CreatureActionClaimGroundTile::handleCreatureActionClaimGroundTile(')
check('needsClaimRepair()' in claim_action and 'repairClaimHealth(creature.getClaimRate())' in claim_action
      and 'room->getSeat() == creature.getSeat()' in claim_action, 'a worker repairs a worn down room of its own seat')
check(claim_action.index('repairClaimHealth') < claim_action.index('isGroundClaimable'), 'repair is tried before claiming ground')
search_action = function(search_source, 'bool CreatureActionSearchGroundTileToClaim::handleSearchGroundTileToClaim(')
check('needsClaimRepair()' in search_action and 'room->getSeat() != creature.getSeat()' in search_action,
      'the claim search looks for own worn down rooms')
check(search_action.index('needsClaimRepair()') < search_action.index('logPortalCandidate(creature, myTile, "standing")'),
      'repairing own rooms comes before claiming ground')
check(room_header.count('dungeonTemple') <= 1, 'the heart exception must stay in one place')

# Probe: the pool maths and the claim rule, compiled from the real header
probe = r'''
#include "rooms/RoomClaim.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

static int gFailures = 0;
static void check(bool ok, const char* msg)
{
    if(!ok)
    {
        ++gFailures;
        std::cout << "FAIL: " << msg << '\n';
    }
}

// Turns of dancing until the pool of a room is empty, the way Room::claimForSeat counts
static int turnsToTake(double danceRate, double secondsPerTile, uint32_t tiles, int workers)
{
    double health = 1.0;
    int turns = 0;
    while(health > 0.0 && turns < 100000)
    {
        for(int i = 0; i < workers; ++i)
            health -= RoomClaim::healthLostPerDance(danceRate, REFERENCE, secondsPerTile, TPS, tiles);
        ++turns;
    }
    return turns;
}

int main()
{
    const double workerRate = REFERENCE;
    double seconds = turnsToTake(workerRate, ENEMY, 25, 1) / TPS;
    check(std::fabs(seconds - 62.5) < 1.0, "a 25 tile enemy room takes one worker about 62.5 seconds");
    seconds = turnsToTake(workerRate, ENEMY, 9, 1) / TPS;
    check(std::fabs(seconds - 22.5) < 1.0, "a 9 tile portal takes one worker about 22.5 seconds");
    seconds = turnsToTake(workerRate, NEUTRAL, 25, 1) / TPS;
    check(std::fabs(seconds - 12.5) < 1.0, "a 25 tile neutral room takes one worker about 12.5 seconds");
    check(turnsToTake(workerRate, ENEMY, 25, 1) > 4 * turnsToTake(workerRate, NEUTRAL, 25, 1), "neutral rooms are about 5 times faster");
    int one = turnsToTake(workerRate, ENEMY, 25, 1);
    int five = turnsToTake(workerRate, ENEMY, 25, 5);
    check(std::abs(one - 5 * five) <= 5, "five workers need a fifth of the time");
    check(turnsToTake(workerRate + 0.06 * 4, ENEMY, 25, 1) < turnsToTake(workerRate, ENEMY, 25, 1), "a level 5 worker is faster");
    check(turnsToTake(workerRate, ENEMY, 50, 1) > 1.9 * turnsToTake(workerRate, ENEMY, 25, 1), "twice the tiles take twice the time");
    check(RoomClaim::takeoverSeconds(ENEMY, 25) == ENEMY * 25.0, "the pool is the number of tiles times the duration of one tile");

    // A room that is worn down halfway and then shrinks to half the tiles keeps its share of the pool:
    // the half of 50 tiles is gone, so half of the 25 tiles are left
    double health = 1.0;
    int turns = 0;
    while(health > 0.5)
    {
        health -= RoomClaim::healthLostPerDance(workerRate, REFERENCE, ENEMY, TPS, 50);
        ++turns;
    }
    int turnsLeft = 0;
    while(health > 0.0 && turnsLeft < 100000)
    {
        health -= RoomClaim::healthLostPerDance(workerRate, REFERENCE, ENEMY, TPS, 25);
        ++turnsLeft;
    }
    check(std::fabs(turnsLeft / TPS - 0.5 * 25 * ENEMY) < 1.0, "the time left follows the new size of the room");
    check(std::fabs((turns + turnsLeft) / TPS - (0.5 * 50 + 0.5 * 25) * ENEMY) < 1.5, "half a 50 tile pool and half a 25 tile pool in all");

    // A bridge is one room: its pool is all its squares times the duration of one square
    check(std::fabs(RoomClaim::takeoverSeconds(ENEMY, 8) - 8 * ENEMY) < 1e-9, "a bridge of 8 squares has the pool of 8 tiles");
    check(RoomClaim::healthLostPerDance(workerRate, REFERENCE, ENEMY, TPS, 8) > RoomClaim::healthLostPerDance(workerRate, REFERENCE, ENEMY, TPS, 25),
          "a short bridge is taken faster than a big room");

    check(RoomClaim::healthLostPerDance(workerRate, REFERENCE, ENEMY, TPS, 25) > 0.0, "a dance lowers the pool");
    check(RoomClaim::healthLostPerDance(workerRate, REFERENCE, ENEMY, TPS, 0) >= 1.0, "a room without tiles is taken at once");

    // Repair: one worker repairs five times faster than one worker wears down, so one repairing worker
    // outweighs four enemy workers and the room refills from empty in a fifth of the time
    double lost = RoomClaim::healthLostPerDance(workerRate, REFERENCE, ENEMY, TPS, 25);
    double repaired = RoomClaim::healthRepairedPerDance(workerRate, REFERENCE, ENEMY, TPS, 25, REPAIR);
    check(std::fabs(repaired - 5.0 * lost) < 1e-12, "repairing is 5 times the wearing down");
    check(repaired > 4.0 * lost, "one repairing worker outweighs four enemy workers");
    check(repaired < 5.5 * lost, "but not much more");
    seconds = 1.0 / (repaired * TPS);
    check(std::fabs(seconds - 12.5) < 0.1, "a 25 tile room is repaired from empty by one worker in 12.5 seconds");
    check(RoomClaim::healthRepairedPerDance(workerRate, REFERENCE, ENEMY, TPS, 0, REPAIR) >= 1.0, "a room without tiles is repaired at once");

    check(RoomClaim::isClaimableBy(true, false, false), "an enemy room is claimable");
    check(!RoomClaim::isClaimableBy(true, true, false), "an allied room is not claimable");
    check(!RoomClaim::isClaimableBy(true, false, true), "the enemy dungeon heart is never claimable");
    check(!RoomClaim::isClaimableBy(true, true, true), "an allied heart is not claimable");
    check(!RoomClaim::isClaimableBy(false, false, false), "nothing is claimable when the setting is off");

    // The takeover pool in the file: a room nobody touched writes nothing extra, a worn down one writes the
    // field behind the data of its first tile line, and a version that does not know it reads its own fields
    // as before. A file without the field loads a full pool.
    std::ostringstream untouched;
    RoomClaim::writeClaimPool(untouched, 1.0);
    check(untouched.str().empty(), "an untouched room writes nothing extra");
    std::ostringstream worn;
    worn << "5\t6\t100\t1\t3";
    RoomClaim::writeClaimPool(worn, 0.375);
    std::istringstream older(worn.str());
    int tx = 0, ty = 0, seatsCount = 0, seatId = 0;
    double hp = 0.0;
    older >> tx >> ty >> hp >> seatsCount >> seatId;
    check(tx == 5 && ty == 6 && hp == 100.0 && seatsCount == 1 && seatId == 3, "an older version reads the fields it knows unchanged");
    std::istringstream newer(worn.str());
    newer >> tx >> ty >> hp >> seatsCount >> seatId;
    check(std::fabs(RoomClaim::readClaimPool(newer, 1.0) - 0.375) < 1e-6, "the pool of a worn down room survives the round trip");
    std::istringstream oldFile("5\t6\t100\t0");
    oldFile >> tx >> ty >> hp >> seatsCount;
    check(RoomClaim::readClaimPool(oldFile, 1.0) == 1.0, "a file without the field loads a full pool");
    std::istringstream other("SomethingElse 0.5");
    check(RoomClaim::readClaimPool(other, 1.0) == 1.0, "another field is not taken for the pool");
    std::istringstream broken("ClaimPool");
    check(RoomClaim::readClaimPool(broken, 1.0) == 1.0, "a cut field loads a full pool");
    std::istringstream outside("ClaimPool 7.5");
    check(RoomClaim::readClaimPool(outside, 1.0) == 1.0, "a value above full is held at full");
    std::istringstream negative("ClaimPool -2");
    check(RoomClaim::readClaimPool(negative, 1.0) == 0.0, "a value below empty is held at empty");

    std::cout << "FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
probe = (probe.replace('REFERENCE', config_value('RoomConvertClaimRate'))
         .replace('NEUTRAL', config_value('RoomConvertNeutralSecondsPerTile'))
         .replace('ENEMY', config_value('RoomConvertSecondsPerTile'))
         .replace('REPAIR', config_value('RoomRepairFactor'))
         .replace('TPS', turns_per_second))


def run_probe(code):
    with tempfile.TemporaryDirectory(prefix='odp-room-capture-') as directory:
        work = Path(directory)
        (work / 'check.cpp').write_text(code)
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/I', str(repo / 'source'), 'check.cpp', '/Fecheck.exe'],
                       cwd=work, check=True, stdout=subprocess.DEVNULL)
        for attempt in range(4):
            try:
                result = subprocess.run([str(work / 'check.exe')], cwd=work, capture_output=True, text=True)
                sys.stdout.write(result.stdout)
                if result.returncode != 0:
                    failures.append('probe failed')
                return
            except OSError as error:
                # Windows may block a freshly compiled fixture (WinError 4551) for a moment
                if getattr(error, 'winerror', None) != 4551 or attempt == 3:
                    raise
                time.sleep(2)


run_probe(probe)

if failures:
    for failure in failures:
        print('FAIL: ' + failure)
    sys.exit(1)
print('check_room_capture: OK')
