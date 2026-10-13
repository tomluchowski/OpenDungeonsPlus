"""Workers take over enemy rooms only when nobody guards them, and the taker can be charged.

Pure source wiring checks (no compiler, no game): server authority, configuration with defaults,
the guard and price rules in Room, their use in Tile (both the search check and the dance), the
client reactions. The probe of the two pure rules in RoomClaim.h is compiled and run only with
--probe (needs the MSVC compiler on the path)."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


rooms_cfg = read('config/rooms.cfg')
room = read('source/rooms/Room.cpp')
room_h = read('source/rooms/Room.h')
claim_h = read('source/rooms/RoomClaim.h')
tile = read('source/entities/Tile.cpp')

# Configuration: documented, set, read with a default; the defaults keep the old behaviour except the guard
defaults = {'RoomTakeoverGuardRadius': '5', 'RoomTakeoverCostPercent': '0'}
for key, value in defaults.items():
    assert re.search(r'^\s+' + key + r'\s+' + value + r'\s*$', rooms_cfg, re.M), key
    assert re.search(r'^\s+# ' + key + r'\s', rooms_cfg, re.M), key + ' is not documented'
    assert '"' + key + '", ' + value + '.0)' in room, key + ' needs the same default in the source'

# Server decides; rooms of nobody, the client and the heart are not touched by the new rules
blocked = function_body(room, 'bool Room::isTakeoverBlocked(')
for needle in ('isServerGameMap()', 'isRogueSeat()', 'RoomTakeoverGuardRadius', 'isWorker()', 'isKo()', 'isInContainment()', 'isAlliedSeat(getSeat())',
               'RoomClaim::isGuardClose(', 'getTakeoverPrice()', 'getGold() < price'):
    assert needle in blocked, needle
assert 'dungeonTemple' not in blocked, 'the heart exception stays in RoomClaim::isClaimableBy'
# Every kind of room can be taken over: portals and bridges are not excluded, the switches are gone
assert 'RoomType::portal' not in blocked and 'RoomType::bridge' not in blocked, 'no room type is excluded'
for source_text in (rooms_cfg, room, room_h, claim_h, tile):
    assert 'TakeoverExclude' not in source_text, 'the takeover exclude switches no longer exist'
price = function_body(room, 'int32_t Room::getTakeoverPrice() const')
assert 'RoomClaim::takeoverPrice(' in price and 'RoomManager::costPerTile(' in price and 'isRogueSeat()' in price
assert 'RoomTakeoverCostPercent' in price

# Use: the search check and the dance both ask, the price is paid after the change of owner
ground = function_body(tile, 'bool Tile::isGroundClaimable(')
assert 'isTakeoverBlocked(seat, this)' in ground
dance = function_body(tile, 'void Tile::claimForSeat(')
assert dance.index('isTakeoverBlocked(seat, this)') < dance.index('->claimForSeat(seat, this, nDanceRate)')
assert dance.index('->claimForSeat(seat, this, nDanceRate)') < dance.index('withdrawFromTreasuries(takeoverPrice, seat)')
assert 'getSeat() != ownerBefore' in dance
# Level scripts and the heart reward call Room::claimForSeat directly and stay free and unguarded
assert 'isTakeoverBlocked' not in function_body(room, 'void Room::claimForSeat(')

# The dungeon heart can only be destroyed: every path that changes the owner of a room or of a tile
# of a room leaves it out
heart = 'RoomType::dungeonTemple'
claim_room = function_body(room, 'void Room::claimForSeat(')
assert heart in claim_room and claim_room.index(heart) < claim_room.index('mClaimHealth -='), 'Room::claimForSeat'
change = function_body(room, 'void Room::changeOwner(')
assert heart in change and change.index(heart) < change.index('handTilesOverToSeat('), 'Room::changeOwner'
hand = function_body(room, 'Room* Room::handTilesOverToSeat(')
assert heart in hand and hand.index(heart) < hand.index('RoomManager::createRoom('), 'Room::handTilesOverToSeat'
assert 'return nullptr' in hand[:hand.index('RoomManager::createRoom(')]
assert 'Room* Room::handTileOverToSeat(' in room and 'return handTilesOverToSeat(' in room  # bridges go through it
assert heart in function_body(tile, 'void Tile::claimForSeat(') and     function_body(tile, 'void Tile::claimForSeat(').index(heart) < function_body(tile, 'void Tile::claimForSeat(').index('isClaimable(seat)')
tile_claim = function_body(tile, 'void Tile::claimTile(')
assert heart in tile_claim and tile_claim.index(heart) < tile_claim.index('setSeat(seat)') and 'isServerGameMap()' in tile_claim
assert heart in function_body(room, 'bool Room::isClaimable(')
assert 'isTakeoverBlocked' in room_h and 'getTakeoverPrice' in room_h

# The two pure rules
assert 'inline bool isGuardClose(' in claim_h and 'inline int32_t takeoverPrice(' in claim_h

# The room is taken over at once: one pool of tiles x duration of one tile, lowered by every dancer on any tile
assert 'inline double takeoverSeconds(double secondsPerTile, uint32_t numTiles)' in claim_h


def inline_body(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n    }\n', start)]


assert 'secondsPerTile * static_cast<double>(numTiles)' in inline_body(claim_h, 'inline double takeoverSeconds(')
for rule in ('inline double healthLostPerDance(', 'inline double healthRepairedPerDance('):
    assert 'takeoverSeconds(secondsPerTile, numTiles)' in inline_body(claim_h, rule), rule + ' uses the pool size'
# One duration value in the config (no second key with the same meaning), documented with the pool formula
assert len(re.findall(r'^\s+RoomConvertSecondsPerTile\s', rooms_cfg, re.M)) == 1, 'one set value'
assert len(re.findall(r'^\s+# RoomConvertSecondsPerTile\s', rooms_cfg, re.M)) == 1, 'one documented value'
assert not re.search(r'RoomTakeover(Seconds|Turns)PerTile', rooms_cfg + room + room_h + claim_h), 'no double of RoomConvertSecondsPerTile'
assert 'tiles times this value' in rooms_cfg and 'at once' in rooms_cfg
dance_room = function_body(room, 'void Room::claimForSeat(')
assert 'numCoveredTiles()' in dance_room, 'the size of the pool is read at every dance'
assert dance_room.index('mClaimHealth -=') < dance_room.index('changeOwner(seat)'), 'the whole room changes owner only after the pool is empty'
assert 'handTilesOverToSeat(seat, tiles)' in function_body(room, 'void Room::changeOwner(') and 'mCoveredTiles' in function_body(room, 'void Room::changeOwner(')
# A room that grows or shrinks while it is worn down keeps its share of the pool: merging averages by tiles,
# the part that breaks off keeps the share, the taker's new room starts full
assert 'mClaimHealth = (mClaimHealth * nbTilesThis' in function_body(room, 'void Room::absorbRoom(')
assert 'newRoom->mClaimHealth = mClaimHealth;' in function_body(room, 'void Room::checkForSplit(')
# Bridges keep their own square by square rule and are not rooms of one pool
bridge = read('source/rooms/RoomBridge.cpp')
bridge_claim = function_body(bridge, 'void RoomBridge::claimForSeat(')
assert 'mClaimedValue' in bridge_claim and 'handTileOverToSeat(seat, tile)' in bridge_claim and 'mClaimHealth' not in bridge_claim
# The portal changes hands as a whole and restarts at a full pool
assert 'mClaimHealth = 1.0;' in function_body(read('source/rooms/RoomPortal.cpp'), 'void RoomPortal::changeOwner(')
if '--probe' in sys.argv:
    probe = r'''
#include "rooms/RoomClaim.h"
#include <iostream>
static int gFailures = 0;
static void check(bool ok, const char* msg)
{
    if(!ok)
    {
        ++gFailures;
        std::cout << "FAIL: " << msg << '\n';
    }
}
int main()
{
    check(RoomClaim::isGuardClose(3, 4, 5.0), "a guard exactly at the radius guards");
    check(!RoomClaim::isGuardClose(4, 4, 5.0), "a guard beyond the radius does not");
    check(!RoomClaim::isGuardClose(0, 0, 0.0), "radius 0 switches the guard rule off");
    check(RoomClaim::takeoverPrice(100, 10, 0.0) == 0, "no share, no price");
    check(RoomClaim::takeoverPrice(100, 10, 25.0) == 250, "a quarter of 10 tiles at 100");
    check(RoomClaim::takeoverPrice(0, 10, 25.0) == 0, "a free room costs nothing");
    check(RoomClaim::takeoverPrice(100, 0, 25.0) == 0, "no tiles, no price");
    check(RoomClaim::takeoverSeconds(2.5, 25) == 62.5, "the pool is tiles times the duration of one tile");
    check(RoomClaim::takeoverSeconds(2.5, 0) == 0.0, "no tiles, no pool");
    // One worker at the reference rate: the pool of 25 tiles is empty after 62.5 s of dancing
    double lost = RoomClaim::healthLostPerDance(0.42, 0.42, 2.5, 1.4, 25);
    check(lost > 0.0 && (lost * 1.4 * 62.5 > 0.999) && (lost * 1.4 * 62.5 < 1.001), "one dance is a 1/(seconds x turns) share");
    std::cout << "FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='odp-room-takeover-') as directory:
        work = Path(directory)
        (work / 'check.cpp').write_text(probe)
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/I', str(root / 'source'), 'check.cpp', '/Fecheck.exe'],
                       cwd=work, check=True, stdout=subprocess.DEVNULL)
        result = subprocess.run([str(work / 'check.exe')], cwd=work, capture_output=True, text=True)
        sys.stdout.write(result.stdout)
        assert result.returncode == 0, 'probe failed'

print('room takeover: ok')
