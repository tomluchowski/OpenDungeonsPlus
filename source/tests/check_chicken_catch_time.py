#!/usr/bin/env python3
"""Catch time of a chicken that runs from a hungry creature: the free hop (RoomHatchery::planFleePath) must not make the
chase longer than the old hop to a neighbouring tile (ChickenEntity::collectMovePositions, commit OLD_COMMIT).

A deterministic simulation (fixed seeds) of one hungry creature and one chicken in grid rooms of several shapes, with coop
footprints as obstacles. All numbers are READ from the sources and from config/rooms.cfg (nothing is copied):
flight limits from ChickenFlight.h, hop reach / spread / wander values from rooms.cfg, speeds from ChickenEntity.h and
creatures.cfg, turns per second from ODApplication.cpp. A text check keeps the simulated logic tied to the code.
Pure Python, compiles nothing."""
import math
import random
import re
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
OLD_COMMIT = '6e3c91480'


def read(path):
    return (root / path).read_text(encoding='utf-8')


def body(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start)]


# ---------------------------------------------------------------- parameters read from the sources
flight_h = read('source/entities/ChickenFlight.h')


def flight_constant(name):
    match = re.search(r'const\s+(?:double|int32_t)\s+' + name + r'\s*=\s*([0-9.]+);', flight_h)
    assert match, name
    return float(match.group(1))


TRIGGER_MAX = flight_constant('TRIGGER_DISTANCE_MAX')
TRIGGER_MIN = flight_constant('TRIGGER_DISTANCE_MIN')
COOLDOWN_S = flight_constant('COOLDOWN_SECONDS')
MAX_HOPS = int(flight_constant('MAX_HOPS_IN_ROW'))
HOLD_S = flight_constant('HOLD_STILL_SECONDS')

TPS = float(re.search(r'double ODApplication::turnsPerSecond\s*=\s*([0-9.]+);', read('source/ODApplication.cpp')).group(1))
CHICKEN_SPEED = float(re.search(r'getMoveSpeed\(\) const override\s*\{\s*return\s*([0-9.]+);', read('source/entities/ChickenEntity.h')).group(1))

cfg_text = read('config/rooms.cfg')


def cfg(key):
    match = re.search(r'^[ \t]+' + key + r'[ \t]+([0-9.]+)\s*$', cfg_text, re.M)
    assert match, 'rooms.cfg has no ' + key
    return float(match.group(1))


WANDER_PAUSE = cfg('HatcheryWanderPausePercent')
WANDER_REACH = cfg('HatcheryWanderReach')
WANDER_MINLEG = cfg('HatcheryWanderMinLeg')
WANDER_EDGE = cfg('HatcheryWanderEdge')
WANDER_ATTEMPTS = int(cfg('HatcheryWanderAttempts'))
FLEE_REACH = cfg('HatcheryFleeReach')
FLEE_MINLEG = cfg('HatcheryFleeMinLeg')
FLEE_GAIN = cfg('HatcheryFleeGain')
FLEE_SPREAD = math.radians(cfg('HatcheryFleeSpread'))
FLEE_ATTEMPTS = int(cfg('HatcheryFleeAttempts'))
SCATTER_RADIUS = cfg('HatcheryScatterRadius')
SCATTER_TURNS = int(cfg('HatcheryScatterTurns'))
SCATTER_ATTEMPTS = int(cfg('HatcheryScatterAttempts'))
SCATTER_MARGIN = cfg('HatcheryScatterMargin')

creatures = read('config/creatures.cfg')
speeds = sorted(set(float(v) for v in re.findall(r'^[ \t]+GroundMoveSpeed[ \t]+([0-9.]+)', creatures, re.M)))
assert speeds
CREATURE_SPEEDS = [speeds[0], speeds[len(speeds) // 2], speeds[-1]]

nest_h = read('source/rooms/HatcheryNestField.h')
COOP = [float(re.search(r'coop' + n + r'\s*=\s*(-?[0-9.]+);', nest_h).group(1)) for n in ('MinX', 'MaxX', 'MinY', 'MaxY')]
OBSTACLE_MARGIN = 0.1   # RoomObjectNavigation::collect(..., 0.1f) in the wander and flight code

# ---------------------------------------------------------------- the code matches what is simulated
chicken_cpp = read('source/entities/ChickenEntity.cpp')
chicken_h = read('source/entities/ChickenEntity.h')
room_cpp = read('source/rooms/RoomHatchery.cpp')
room_h = read('source/rooms/RoomHatchery.h')

assert 'collectMovePositions' not in chicken_cpp + chicken_h, 'the flight is not tied to the neighbour tiles any more'
flee = body(chicken_cpp, 'bool ChickenEntity::tryFlee(')
for needle in ('ChickenFlight::shouldFlee(mFlight, distance, true)', 'planFleePath(start, eaterPosition, path)',
               'setWalkPath(EntityAnimation::walk_anim', 'ChickenFlight::registerFlight(mFlight, ODApplication::turnsPerSecond)'):
    assert needle in flee, needle
upkeep = body(chicken_cpp, 'void ChickenEntity::doUpkeep()')
assert upkeep.index('if(isMoving())') < upkeep.index('tryFlee(tile, currentHatchery)') < upkeep.index('wander(currentHatchery);'), 'order of the turn'
assert 'double getMoveSpeed() const override' in chicken_h.replace('virtual ', '')

plan = body(room_cpp, 'bool RoomHatchery::planFleePath(')
for needle in ('"HatcheryFleeReach", 1.2', '"HatcheryFleeMinLeg", 0.4', '"HatcheryFleeGain", 0.3', '"HatcheryFleeSpread", 70.0',
               '"HatcheryFleeAttempts", 16.0', 'Random::Double(-spread, spread)', 'Random::Double(minLeg, reach)',
               'goal.distance(threat) < distanceNow + gain', 'isFreeWanderPoint(goal, obstacles, edge, nestClearance)',
               'RoomObjectPath::clearSegment(obstacles, from, goal, true) && isSegmentInRoom(from, goal)'):
    assert needle in plan, needle
# the defaults in the code are the values of rooms.cfg
for key, value in (('HatcheryFleeReach', FLEE_REACH), ('HatcheryFleeMinLeg', FLEE_MINLEG), ('HatcheryFleeGain', FLEE_GAIN),
                   ('HatcheryFleeSpread', math.degrees(FLEE_SPREAD)), ('HatcheryFleeAttempts', FLEE_ATTEMPTS)):
    default = float(re.search(r'"' + key + r'",\s*([0-9.]+)', plan).group(1))
    assert abs(default - value) < 1e-9, key
assert 'bool planFleePath(' in room_h
wander = body(room_cpp, 'bool RoomHatchery::planWanderPath(')
for needle in ('distance < minLeg', 'distance > reach', 'isFreeWanderPoint(goal, obstacles, edge, nestClearance)'):
    assert needle in wander, needle
assert 'ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryWanderPausePercent"' in body(chicken_cpp, 'void ChickenEntity::wander(')
flock = body(room_cpp, 'void RoomHatchery::updateFlock(')
for needle in ('henPos.distance(creaturePos) > radius', 'spot.distance(creaturePos) < radius + scatterMargin', 'hen->scatterTo(spot, scatterTurns)'):
    assert needle in flock, needle
# the flight state port below: the same lines as in the header
for needle in ('(distance > TRIGGER_DISTANCE_MAX) || (distance <= TRIGGER_DISTANCE_MIN)', 'return state.mCooldownTurns <= 0;',
               'state.mHopsInRow >= MAX_HOPS_IN_ROW', 'HOLD_STILL_SECONDS * turnsPerSecond', 'COOLDOWN_SECONDS * turnsPerSecond'):
    assert needle in flight_h, needle
# the chase of the creature (CreatureActionEatChicken): catch at squared tile distance <= 1 with a clear reach, 80% of the way else
eat = read('source/creatureaction/CreatureActionEatChicken.cpp')
for needle in ('dist > 1 || !clearReach || !clearBody', 'chase.resize(8 * chase.size() / 10)', 'if(chase.size() > 2)'):
    assert needle in eat, needle

# the old logic that is simulated is the one of OLD_COMMIT
try:
    old_cpp = subprocess.run(['git', 'show', OLD_COMMIT + ':source/entities/ChickenEntity.cpp'], cwd=str(root), capture_output=True,
                             check=True).stdout.decode('utf-8')
except (OSError, subprocess.CalledProcessError):
    old_cpp = None
    print('note: commit ' + OLD_COMMIT + ' not available, the old logic is not compared to the history')
if old_cpp is not None:
    old_flee = body(old_cpp, 'bool ChickenEntity::tryFlee(')
    old_collect = body(old_cpp, 'void ChickenEntity::collectMovePositions(')
    for needle in ('collectMovePositions(tile, currentHatchery, positions)', 'double bestDistance = distance + 0.3;', 'candidateDistance > bestDistance'):
        assert needle in old_flee, needle
    for needle in ('addTileToListIfPossible(posChickenX - 1, posChickenY', 'addTileToListIfPossible(posChickenX + 1, posChickenY',
                   'addTileToListIfPossible(posChickenX, posChickenY - 1', 'addTileToListIfPossible(posChickenX, posChickenY + 1',
                   'RoomObjectNavigation::standingPosition(', 'RoomObjectPath::clearSegment(obstacles, start, point, true)'):
        assert needle in old_collect, needle

# ---------------------------------------------------------------- geometry
Vec = tuple


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


class Room:
    def __init__(self, name, tiles, coops):
        self.name = name
        self.tiles = set(tiles)
        self.tile_list = sorted(self.tiles)
        # coop footprint around the middle of its tile, grown by the margin of the navigation
        self.boxes = [(x + COOP[0] - OBSTACLE_MARGIN, y + COOP[2] - OBSTACLE_MARGIN, x + COOP[1] + OBSTACLE_MARGIN, y + COOP[3] + OBSTACLE_MARGIN)
                      for (x, y) in coops]
        self.bare = [(x + COOP[0], y + COOP[2], x + COOP[1], y + COOP[3]) for (x, y) in coops]

    def in_room(self, p):
        return (int(math.floor(p[0] + 0.5)), int(math.floor(p[1] + 0.5))) in self.tiles

    def clear_point(self, p):
        for (x0, y0, x1, y1) in self.boxes:
            if x0 <= p[0] <= x1 and y0 <= p[1] <= y1:
                return False
        return True

    def segment_hits(self, a, b, boxes):
        for (x0, y0, x1, y1) in boxes:
            t0, t1 = 0.0, 1.0
            ok = True
            for (s, d, lo, hi) in ((a[0], b[0] - a[0], x0, x1), (a[1], b[1] - a[1], y0, y1)):
                if abs(d) < 1e-12:
                    if s < lo or s > hi:
                        ok = False
                        break
                else:
                    ta, tb = (lo - s) / d, (hi - s) / d
                    if ta > tb:
                        ta, tb = tb, ta
                    t0, t1 = max(t0, ta), min(t1, tb)
                    if t0 > t1:
                        ok = False
                        break
            if ok:
                return True
        return False

    def clear_segment(self, a, b, allow_exit=False):
        for box in self.boxes:
            if allow_exit and box[0] <= a[0] <= box[2] and box[1] <= a[1] <= box[3] and not (box[0] <= b[0] <= box[2] and box[1] <= b[1] <= box[3]):
                continue
            if self.segment_hits(a, b, [box]):
                return False
        return True

    def segment_in_room(self, a, b):
        steps = max(1, int(math.ceil(dist(a, b) / 0.2)))
        for i in range(1, steps + 1):
            f = i / float(steps)
            if not self.in_room((a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f)):
                return False
        return True

    def is_free_point(self, p):
        tx, ty = int(math.floor(p[0] + 0.5)), int(math.floor(p[1] + 0.5))
        if (tx, ty) not in self.tiles:
            return False
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if (tx + dx, ty + dy) in self.tiles:
                    continue
                gx = max((tx + dx - 0.5) - p[0], 0.0, p[0] - (tx + dx + 0.5))
                gy = max((ty + dy - 0.5) - p[1], 0.0, p[1] - (ty + dy + 0.5))
                if gx * gx + gy * gy < WANDER_EDGE * WANDER_EDGE:
                    return False
        return self.clear_point(p)

    def pick_free_point(self, rng):
        for _ in range(WANDER_ATTEMPTS):
            tx, ty = rng.choice(self.tile_list)
            p = (tx + rng.uniform(-0.5, 0.5), ty + rng.uniform(-0.5, 0.5))
            if self.is_free_point(p):
                return p
        return None

    def plan_wander(self, start, rng):
        for _ in range(WANDER_ATTEMPTS):
            tx, ty = rng.choice(self.tile_list)
            goal = (tx + rng.uniform(-0.5, 0.5), ty + rng.uniform(-0.5, 0.5))
            d = dist(start, goal)
            if d < WANDER_MINLEG or (WANDER_REACH > 0 and d > WANDER_REACH):
                continue
            if not self.is_free_point(goal):
                continue
            if self.clear_segment(start, goal, True) and self.segment_in_room(start, goal):
                return goal
        return None

    # RoomHatchery::planFleePath (the soft bend only bends the way a little and is left out)
    def plan_flee_new(self, start, threat, rng):
        away = (start[0] - threat[0], start[1] - threat[1])
        base = math.atan2(away[1], away[0]) if math.hypot(*away) > 0.001 else 0.0
        now = dist(start, threat)
        for _ in range(FLEE_ATTEMPTS):
            angle = base + rng.uniform(-FLEE_SPREAD, FLEE_SPREAD)
            length = rng.uniform(FLEE_MINLEG, FLEE_REACH)
            goal = (start[0] + math.cos(angle) * length, start[1] + math.sin(angle) * length)
            if dist(goal, threat) < now + FLEE_GAIN:
                continue
            if not self.is_free_point(goal):
                continue
            if self.clear_segment(start, goal, True) and self.segment_in_room(start, goal):
                return goal
        return None

    # ChickenEntity::collectMovePositions + the choice in tryFlee at OLD_COMMIT
    def plan_flee_old(self, start, threat, rng):
        tile = (int(math.floor(start[0] + 0.5)), int(math.floor(start[1] + 0.5)))
        now = dist(start, threat)
        best, best_distance = None, now + 0.3
        for (dx, dy) in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            neighbour = (tile[0] + dx, tile[1] + dy)
            if neighbour not in self.tiles:
                continue
            point = (float(neighbour[0]), float(neighbour[1]))
            if not self.clear_point(point) or not self.clear_segment(start, point, True):
                continue
            if dist(threat, point) > best_distance:
                best_distance = dist(threat, point)
                best = point
        return best


def make_rooms():
    def rect(w, h, x0=0, y0=0):
        return [(x0 + x, y0 + y) for x in range(w) for y in range(h)]
    rooms = [Room('3x3', rect(3, 3), []),
             Room('5x5', rect(5, 5), []),
             Room('10x10', rect(10, 10), []),
             Room('2x8 strip', rect(2, 8), []),
             Room('5x5 + coop', rect(5, 5), [(2, 2)]),
             Room('10x10 + 3 coops', rect(10, 10), [(2, 2), (7, 3), (4, 7)])]
    return rooms


# ---------------------------------------------------------------- simulation
class Mover:
    def __init__(self, pos, speed):
        self.pos = pos
        self.speed = speed
        self.path = []

    def moving(self):
        return bool(self.path)

    def advance(self):
        left = self.speed
        while self.path and left > 0:
            d = dist(self.pos, self.path[0])
            if d > left:
                f = left / d
                self.pos = (self.pos[0] + (self.path[0][0] - self.pos[0]) * f, self.pos[1] + (self.path[0][1] - self.pos[1]) * f)
                return
            self.pos = self.path.pop(0)
            left -= d


def tile_of(p):
    return (int(math.floor(p[0] + 0.5)), int(math.floor(p[1] + 0.5)))


def run(room, logic, creature_speed, hen, seed):
    start_rng = random.Random('start-%s-%s' % (room.name, seed))
    rng = random.Random('run-%s-%s-%s-%s' % (room.name, creature_speed, hen, seed))
    chicken_pos = None
    while chicken_pos is None:
        chicken_pos = room.pick_free_point(start_rng)
    # the creature starts far from the chicken: the farthest of a few random free points, at most 3.5 tiles away
    candidates = [p for p in (room.pick_free_point(start_rng) for _ in range(40)) if p is not None]
    creature_pos = max(candidates, key=lambda p: min(dist(p, chicken_pos), 3.5) + start_rng.random() * 0.5)

    chicken = Mover(chicken_pos, CHICKEN_SPEED)
    creature = Mover(creature_pos, creature_speed)
    cooldown, hops, scatter = 0, 0, 0
    for turn in range(1, TIMEOUT_TURNS + 1):
        # RoomHatchery::updateFlock: the hen runs off when the creature comes close
        if hen and scatter == 0 and dist(chicken.pos, creature.pos) <= SCATTER_RADIUS:
            for _ in range(SCATTER_ATTEMPTS):
                spot = room.pick_free_point(rng)
                if spot is None or dist(spot, creature.pos) < SCATTER_RADIUS + SCATTER_MARGIN:
                    continue
                if room.clear_segment(chicken.pos, spot, True) and room.in_room(spot):
                    chicken.path = [spot]
                    scatter = SCATTER_TURNS
                    break
        # ChickenEntity::doUpkeep
        if cooldown > 0:
            cooldown -= 1
        if scatter > 0:
            scatter -= 1
        if not chicken.moving():
            distance = dist(chicken.pos, creature.pos)
            fled = False
            if cooldown <= 0 and TRIGGER_MIN < distance <= TRIGGER_MAX and scatter == 0:
                goal = (room.plan_flee_new if logic == 'new' else room.plan_flee_old)(chicken.pos, creature.pos, rng)
                if goal is not None:
                    chicken.path = [goal]
                    hops += 1
                    if hops >= MAX_HOPS:
                        cooldown, hops = int(HOLD_S * TPS), 0
                    else:
                        cooldown = int(COOLDOWN_S * TPS)
                    fled = True
            if not fled and not (rng.randrange(100) < WANDER_PAUSE):
                goal = room.plan_wander(chicken.pos, rng)
                if goal is not None:
                    chicken.path = [goal]
        # CreatureActionEatChicken::handleEatChicken (when the creature stands still)
        if not creature.moving():
            ct, ht = tile_of(creature.pos), tile_of(chicken.pos)
            squared = (ct[0] - ht[0]) ** 2 + (ct[1] - ht[1]) ** 2
            reach = not room.segment_hits(creature.pos, chicken.pos, room.bare)
            if squared <= 1 and reach:
                return turn
            if squared <= 1:
                target, fraction = chicken.pos, 1.0
            else:
                n = abs(ct[0] - ht[0]) + abs(ct[1] - ht[1])
                target = (float(ht[0]), float(ht[1]))
                fraction = (8 * n // 10) / float(n) if n > 2 else 1.0
            creature.path = [(creature.pos[0] + (target[0] - creature.pos[0]) * fraction,
                              creature.pos[1] + (target[1] - creature.pos[1]) * fraction)]
        chicken.advance()
        creature.advance()
    return None


RUNS = 2000
TIMEOUT_TURNS = 1500


def percentile(values, q):
    values = sorted(values)
    return values[min(len(values) - 1, int(math.ceil(q * len(values))) - 1)]


results = {}
for room in make_rooms():
    for hen in (False, True):
        for speed in CREATURE_SPEEDS:
            for logic in ('old', 'new'):
                times = [run(room, logic, speed, hen, seed) for seed in range(RUNS)]
                results[(room.name, hen, speed, logic)] = times

print('speeds: chicken %.2f, creatures %s, turns per second %.1f, runs per case %d' % (CHICKEN_SPEED, CREATURE_SPEEDS, TPS, RUNS))
print('%-16s %-6s %5s | %-17s | %-17s' % ('room', 'kind', 'speed', 'old mean / p95', 'new mean / p95'))
failures = []


def mean(values):
    return sum(values) / float(len(values))


for room in make_rooms():
    for hen in (False, True):
        for speed in CREATURE_SPEEDS:
            row = []
            for logic in ('old', 'new'):
                times = results[(room.name, hen, speed, logic)]
                caught = [t for t in times if t is not None]
                row.append((mean(caught), percentile([t if t is not None else TIMEOUT_TURNS for t in times], 0.95), len(times) - len(caught)))
            print('%-16s %-6s %5.2f | %7.2f / %6.1f  | %7.2f / %6.1f%s' % (room.name, 'hen' if hen else 'chick', speed, row[0][0], row[0][1],
                  row[1][0], row[1][1], '  TIMEOUTS old=%d new=%d' % (row[0][2], row[1][2]) if row[0][2] or row[1][2] else ''))
            if row[1][2]:
                failures.append('no catch (timeout) in %d runs: %s hen=%s speed=%s' % (row[1][2], room.name, hen, speed))
# strict: per room shape and kind, over all creature speeds, the new mean catch time is not longer than the old one
for room in make_rooms():
    for hen in (False, True):
        old_all = [t for s in CREATURE_SPEEDS for t in results[(room.name, hen, s, 'old')] if t is not None]
        new_all = [t for s in CREATURE_SPEEDS for t in results[(room.name, hen, s, 'new')] if t is not None]
        print('%-16s %-6s all speeds: old mean %.2f p95 %d | new mean %.2f p95 %d' % (room.name, 'hen' if hen else 'chick', mean(old_all),
              percentile(old_all, 0.95), mean(new_all), percentile(new_all, 0.95)))
        if mean(new_all) > mean(old_all):
            failures.append('new mean catch time %.2f > old %.2f: %s hen=%s' % (mean(new_all), mean(old_all), room.name, hen))
everything = [(logic, t) for (name, hen, s, logic), times in results.items() for t in times if t is not None]
old_mean = mean([t for l, t in everything if l == 'old'])
new_mean = mean([t for l, t in everything if l == 'new'])
print('all cases: old mean %.2f, new mean %.2f' % (old_mean, new_mean))
if new_mean > old_mean:
    failures.append('new mean over all cases longer than old')
# the hop is not longer than the longest old hop (a neighbour tile centre seen from a point of the own tile)
assert FLEE_REACH <= math.hypot(1.5, 0.5), 'the hop is longer than the old one'
assert FLEE_REACH >= FLEE_MINLEG > 0
assert not failures, '; '.join(failures)
print('chicken catch time checks passed')
