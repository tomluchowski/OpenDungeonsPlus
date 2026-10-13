#!/usr/bin/env python3
# Checks of the straw nest places of a hatchery (source/rooms/HatcheryNestField.h): a python port of the rules
# calculates the places for some layouts and checks that no nest overlaps a coop, its apron or a wall edge, that the
# places keep their distance and do not depend on the order of the tiles, and that the port and the header agree.
# Also checks the wiring in the server code, the client and the config (text checks only).
import itertools
import math
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
header = (root / 'source/rooms/HatcheryNestField.h').read_text()
room_cpp = (root / 'source/rooms/RoomHatchery.cpp').read_text()
room_h = (root / 'source/rooms/RoomHatchery.h').read_text()
bounds = (root / 'source/gamemap/RoomObjectBounds.h').read_text()
looks = (root / 'source/render/RenderManagerChickens.cpp').read_text()
config = (root / 'config/rooms.cfg').read_text()

MASK = 0xFFFFFFFF
DEFAULTS = {'mTilesPerNest': 3, 'mMaxNests': 16, 'mEdge': 0.25, 'mCoopClearance': 0.45, 'mLaneLength': 1.0,
            'mLaneHalfWidth': 0.5, 'mLaneClearance': 0.2, 'mPathHalfWidth': 0.35, 'mPathClearance': 0.2,
            'mSpacing': 0.6}
COOP = (-0.203275, 0.796725, -0.4, 0.4)


def hash_tile(x, y, attempt):
    h = 2166136261
    h = ((h ^ (x & MASK)) * 16777619) & MASK
    h = ((h ^ (y & MASK)) * 16777619) & MASK
    h = ((h ^ attempt) * 16777619) & MASK
    h ^= h >> 15
    h = (h * 2246822519) & MASK
    h ^= h >> 13
    h = (h * 3266489917) & MASK
    h ^= h >> 16
    return h


def rect_dist2(x, y, min_x, min_y, max_x, max_y):
    dx = max(max(min_x - x, 0.0), x - max_x)
    dy = max(max(min_y - y, 0.0), y - max_y)
    return dx * dx + dy * dy


def seg_dist2(x, y, ax, ay, bx, by):
    sx, sy = bx - ax, by - ay
    length2 = sx * sx + sy * sy
    t = 0.0
    if length2 > 0.0:
        t = min(1.0, max(0.0, ((x - ax) * sx + (y - ay) * sy) / length2))
    dx, dy = x - (ax + t * sx), y - (ay + t * sy)
    return dx * dx + dy * dy


def apron_mid(c, s):
    return (c[0] + COOP[1] + s['mLaneLength'] * 0.5, c[1])


def entrances_of(room, open_tiles):
    # a tile of the room next to (sharing an edge) a walkable tile that is not part of the room
    return sorted(t for t in room if any((t[0] + dx, t[1] + dy) in open_tiles and (t[0] + dx, t[1] + dy) not in room
                                         for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))))


def place_is_free(x, y, tx, ty, room, coops, entrances, taken, s):
    for dx, dy in itertools.product((-1, 0, 1), repeat=2):
        if (dx, dy) == (0, 0) or (tx + dx, ty + dy) in room:
            continue
        nx, ny = tx + dx, ty + dy
        if rect_dist2(x, y, nx - 0.5, ny - 0.5, nx + 0.5, ny + 0.5) < s['mEdge'] ** 2:
            return False
    for cx, cy in coops:
        if rect_dist2(x, y, cx + COOP[0], cy + COOP[2], cx + COOP[1], cy + COOP[3]) < s['mCoopClearance'] ** 2:
            return False
        if rect_dist2(x, y, cx + COOP[1], cy - s['mLaneHalfWidth'], cx + COOP[1] + s['mLaneLength'],
                      cy + s['mLaneHalfWidth']) < s['mLaneClearance'] ** 2:
            return False
        clear = s['mPathHalfWidth'] + s['mPathClearance']
        mx, my = apron_mid((cx, cy), s)
        for ex, ey in entrances:
            if seg_dist2(x, y, ex, ey, mx, my) < clear ** 2:
                return False
    for px, py, _ in taken:
        if (px - x) ** 2 + (py - y) ** 2 < s['mSpacing'] ** 2:
            return False
    return True


def compute(room_tiles, coops_in, entrances_in=(), s=DEFAULTS):
    places = []
    room = set(room_tiles)
    coops = sorted(set(coops_in))
    entrances = sorted(set(entrances_in))
    wanted = max(min(len(room) // max(1, s['mTilesPerNest']), s['mMaxNests']), len(coops))
    for rnd in range(24):
        if len(places) >= wanted:
            break
        if rnd >= 6 and len(places) >= len(coops):
            break
        order = sorted((hash_tile(t[0], t[1], rnd), t) for t in room)
        for h, (tx, ty) in order:
            if len(places) >= wanted:
                break
            x = tx + (((h >> 8) % 81) - 40) / 100.0
            y = ty + (((h >> 16) % 81) - 40) / 100.0
            if place_is_free(x, y, tx, ty, room, coops, entrances, places, s):
                places.append((x, y, float(h % 360)))
    return places


def rect(x0, y0, w, h):
    return [(x, y) for x in range(x0, x0 + w) for y in range(y0, y0 + h)]


layouts = {
    '3x3 one coop': (rect(0, 0, 3, 3), [(1, 1)]),
    '5x5 two coops': (rect(10, 10, 5, 5), [(11, 12), (13, 12)]),
    '7x4 three coops': (rect(-4, 3, 7, 4), [(-3, 4), (-1, 4), (1, 4)]),
    'L shape': (rect(0, 0, 6, 2) + rect(0, 2, 2, 4), [(1, 1), (4, 1)]),
    '2x2 one coop': (rect(0, 0, 2, 2), [(0, 0)]),
    '9x9 four coops': (rect(0, 0, 9, 9), [(2, 2), (6, 2), (2, 6), (6, 6)]),
}
# Layouts with entrances: the walkable tiles next to the room (door, corridor)
layouts_open = {
    '5x5 door west': (rect(10, 10, 5, 5), [(11, 12), (13, 12)], {(9, 12)}),
    '9x9 doors south and north': (rect(0, 0, 9, 9), [(2, 2), (6, 2), (2, 6), (6, 6)], {(4, -1), (4, 9)}),
    '7x4 door east': (rect(-4, 3, 7, 4), [(-3, 4), (-1, 4), (1, 4)], {(3, 5)}),
    'L shape corridor': (rect(0, 0, 6, 2) + rect(0, 2, 2, 4), [(1, 1), (4, 1)], {(0, 6), (6, 0)}),
}
all_layouts = dict((n, (t, c, set())) for n, (t, c) in layouts.items())
all_layouts.update(layouts_open)
for name, (tiles, coops, open_tiles) in all_layouts.items():
    s = DEFAULTS
    entrances = entrances_of(set(tiles), open_tiles)
    if open_tiles:
        assert entrances, name
    places = compute(tiles, coops, entrances)
    room = set(tiles)
    # order of the tiles does not matter
    assert compute(list(reversed(tiles)), list(reversed(coops)), list(reversed(entrances))) == places, name
    wanted = max(min(len(room) // 3, 16), len(coops))
    assert len(places) <= wanted, name
    # a big enough hatchery gets about one nest per three tiles (fewer is fine when a walking strip takes the room)
    if len(room) >= 20 and not entrances:
        assert len(places) >= min(len(room) // 3, 16) - 1, (name, len(places))
    for i, (x, y, angle) in enumerate(places):
        tx, ty = int(math.floor(x + 0.5)), int(math.floor(y + 0.5))
        assert (tx, ty) in room, (name, 'nest not on a tile of the hatchery', x, y)
        assert 0.0 <= angle < 360.0
        for dx, dy in itertools.product((-1, 0, 1), repeat=2):
            if (tx + dx, ty + dy) not in room:
                assert rect_dist2(x, y, tx + dx - 0.5, ty + dy - 0.5, tx + dx + 0.5, ty + dy + 0.5) >= s['mEdge'] ** 2 - 1e-9, (name, 'too close to a wall edge', x, y)
        for cx, cy in coops:
            assert rect_dist2(x, y, cx + COOP[0], cy + COOP[2], cx + COOP[1], cy + COOP[3]) >= s['mCoopClearance'] ** 2 - 1e-9, (name, 'overlaps a coop', x, y)
            assert rect_dist2(x, y, cx + COOP[1], cy - 0.5, cx + COOP[1] + 1.0, cy + 0.5) >= s['mLaneClearance'] ** 2 - 1e-9, (name, 'on the apron of a coop', x, y)
            mx, my = apron_mid((cx, cy), s)
            for ex, ey in entrances:
                assert seg_dist2(x, y, ex, ey, mx, my) >= (s['mPathHalfWidth'] + s['mPathClearance']) ** 2 - 1e-9, (name, 'on a walking strip', x, y)
        for j in range(i):
            assert math.hypot(places[j][0] - x, places[j][1] - y) >= s['mSpacing'] - 1e-9, (name, 'nests too close')
    # every coop gets its nest when the layout is big enough (the walking strips may take places, but not all)
    if name in ('5x5 two coops', '5x5 door west', '9x9 doors south and north', '7x4 door east'):
        assert len(places) >= len(coops), (name, len(places))
    print('%s: %d nests for %d tiles, %d coops, %d entrances' % (name, len(places), len(room), len(coops), len(entrances)))

# Every coop of the usual layouts gets its nest
for name in ('3x3 one coop', '5x5 two coops', '7x4 three coops', '9x9 four coops'):
    tiles, coops = layouts[name]
    assert len(compute(tiles, coops)) >= len(coops), name
# The strips of the entrances move nests: with a door the places differ from those without one, and no nest is left on
# the way from the door to the apron of a coop
for name, (tiles, coops, open_tiles) in layouts_open.items():
    entrances = entrances_of(set(tiles), open_tiles)
    assert compute(tiles, coops, entrances) != compute(tiles, coops), name
# Many entrances can leave fewer nests than wanted, but never an error (and none on a strip)
wall_door = entrances_of(set(rect(0, 0, 3, 3)), {(x, -1) for x in range(3)} | {(-1, y) for y in range(3)})
few = compute(rect(0, 0, 3, 3), [(1, 1)], wall_door)
assert len(few) <= 1
# Without a coop the nests are still there (a coop that is not built yet does not take the eggs away)
assert len(compute(rect(0, 0, 6, 6), [])) >= 6

# The header has the strip rules and the entrance in the fingerprint
assert 'segmentDistanceSquared' in header and 'const std::vector<TileCoord>& entrances' in header
assert 'hashTile(it->first, it->second, 11u)' in header and 'extraRounds = 18' in header
assert 'collectEntrances' in room_h and 'getFullness() <= 0.0' in room_cpp
assert 'collectEntrances(mCoveredTiles)' in room_cpp and 'RoomHatchery::collectEntrances(coveredTiles)' in looks
for key in ('HatcheryNestPathHalfWidth', 'HatcheryNestPathClearance'):
    assert key in room_cpp and key in config, key

# The port and the header agree: same defaults, same footprint of the coop (the table of the object bounds), same hash
for key, value in DEFAULTS.items():
    m = re.search(r'%s\(([-0-9.u]+)\)' % key, header)
    assert m and float(m.group(1).rstrip('u')) == float(value), key
assert 'coopMinX = -0.203275' in header and 'coopMaxX = 0.796725' in header
assert 'coopMinY = -0.4;' in header and 'coopMaxY = 0.4;' in header
assert '{"ChickenCoop", -.203275f, -.4f, .796725f, .4f}' in bounds
for number in ('2166136261u', '16777619u', '2246822519u', '3266489917u', 'h >> 15', 'h >> 13', 'h >> 16',
               '% 81u', '(h >> 8)', '(h >> 16)', 'h % 360u', 'rounds = 6'):
    assert number in header, number
assert 'static const double eggHeight = 0.03;' in header and 'eggHeight' in room_cpp
# the header has no dependency on the game map or the engine, so the unit test can use it alone
assert '#include "' not in header and '#include <Ogre' not in header

# Server: the eggs lie in the nests of the field, the nest closest to the hen first, one egg per nest
find = room_cpp[room_cpp.index('bool RoomHatchery::findNestSpot'):room_cpp.index('void RoomHatchery::leaveNest')]
assert 'getNestPlaces()' in find and 'HatcheryCycle::pickNestPlace(occupied, 1)' in find
assert 'HatcheryNestField::eggHeight' in find and 'squaredDistance' in find
assert 'HatcheryNestEggs' in find and 'HatcheryNestSameRadius' in find
assert 'mCentralActiveSpotTiles' in room_cpp[room_cpp.index('RoomHatchery::getNestPlaces'):room_cpp.index('bool RoomHatchery::findNestSpot')]
assert 'HatcheryNestField::fingerprint' in room_cpp and 'HatcheryNestField::compute' in room_cpp
for key in ('HatcheryNestTilesPerNest', 'HatcheryNestMax', 'HatcheryNestEdge', 'HatcheryNestCoopClearance',
            'HatcheryNestLaneLength', 'HatcheryNestLaneHalfWidth', 'HatcheryNestLaneClearance', 'HatcheryNestSpacing'):
    assert key in room_cpp and key in config, key
assert not (root / 'source/rooms/HatcheryCoopHouse.h').exists(), 'the coop nest places are gone'
assert 'nestEggSpot' not in room_cpp and 'eggsPerNest' not in room_cpp

# Client: one entity of ChickenNest.mesh per place, no nest on the coops, the old code made nest mesh is gone
assert 'MeshNest = "ChickenNest"' in looks and 'HatcheryNestField::compute' in looks and 'updateNestFields' in looks
assert 'ChickenCoopNest' not in looks and 'buildNest' not in looks and 'mNest' not in looks.replace('mNestEgg', '').replace('mNestFields', '')
assert 'RoomHatchery::getNestFieldSettings()' in looks
print('hatchery nest field checks passed')
