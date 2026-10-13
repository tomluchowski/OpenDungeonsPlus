#!/usr/bin/env python3
"""Opening check for campaign levels: the first minute must not be decided by the layout.

For every level the state at the START of the game is checked (no trigger has fired yet):

1. Enemy walk path. The sources of the enemy team are the creatures of other teams, their
   wave portals, their portals and their dungeon temples. For every source the walk path
   to the dungeon temple of the human seat is measured (4-neighbourhood, like the game):
   only over tiles that can be walked right now (open dirt, claimed or built tiles, rooms,
   bridges, doors; doors count as walkable). Every such connection is a FAIL, whatever its
   length: a source must always have to dig to reach the temple. Solid rock, water and lava
   never count as passable. A tile that is rock at the start and opened later by a trigger
   therefore separates, as it should. The shortest path that may also cross solid dirt,
   gold and gem tiles is only reported (shortest_dig_path) and has no threshold. Levels
   outside the campaign (skirmish, realms) are exempt from this part.

2. Open areas between the rooms of the human seat. Within OPEN_RADIUS tiles of the temple
   and within the bounding box of all rooms of the human seat, a square of at least
   OPEN_SQUARE x OPEN_SQUARE walkable (not dug) tiles that belong to no room, i.e. an open
   area larger than 4x4, is a large empty hall: FAIL (the largest one is reported with
   its corner). Furthermore, rooms
   placed for the human seat must be separated from each other by rock: two such rooms
   that touch each other, or a block of WIDE_PASSAGE x WIDE_PASSAGE open non-room tiles
   next to one of them (passage wider than MAX_PASSAGE_WIDTH), is a FAIL.

3. Safe zone around heart and nearest portal (SAFE_RADIUS, default 20 tiles, option
   --safe-radius N). The seat of the human player must be able to build up before it has
   to fight. The zone is every tile that is at most SAFE_RADIUS tiles away, over walkable
   and diggable tiles (dirt, gold, gem, rooms, doors; rock, water and lava block), from the
   dungeon heart, from the nearest portal or from the dig path between the two. Portals of
   that seat count, and so do unclaimed portals (owner seat 0) because the player claims
   them on the way.
   Inside the zone there must be no creature of an enemy seat (other team; neutral seat 0
   does not count), no trap or door of an enemy seat, no room of an enemy seat, no wave
   portal of an enemy seat and no script spawn action of an enemy seat whose spawn position
   lies in the zone (triggered or not). A level without such a portal is only
   checked around the heart. Applies to every level with a human (or, failing that,
   a choice) seat, campaign or not.

Tiles that the level file does not list are treated like the game does: full dirt
(fullness 100), diggable but not walkable until dug. This applies to every rule above.

Output is one line per level (PASS or FAIL, file, numbers). Levels listed in
scripts/check-campaign-opening.allowlist report FAIL but do not change the exit code
("known, to be fixed"); the exit code is 0 only if every level passes or is allowlisted.
This script contains no level data.

Usage:
  check-campaign-opening.py [--no-allowlist] [--safe-radius N] [--full] [--all] [<file> ...]
  check-campaign-opening.py --self-test

Without files all levels of levels/campaign/Campaign.cfg are checked; --all checks every
level file below levels/ (campaign, skirmish, multiplayer). --full lists every violation
(by default at most MAX_EXAMPLES examples per kind). A summary block ends the output.
"""

import os
import shutil
import sys
import tempfile
from collections import deque

# --- Constants ---------------------------------------------------------------------------
OPEN_RADIUS = 12
OPEN_SQUARE = 5
MAX_PASSAGE_WIDTH = 3
WIDE_PASSAGE = MAX_PASSAGE_WIDTH + 1
# A room of the human seat counts as "next to" a wide passage within this many tiles.
PASSAGE_ROOM_DISTANCE = 2
# Safe zone around heart and nearest portal of the human seat (walk/dig path in tiles).
SAFE_RADIUS = 20
MAX_EXAMPLES = 5

TILE_DIRT = 1
TILE_GOLD = 2
TILE_ROCK = 3
TILE_WATER = 4
TILE_LAVA = 5
TILE_GEM = 6
DIGGABLE_TYPES = (TILE_DIRT, TILE_GOLD, TILE_GEM)

ROOM_TEMPLE = 1
ROOM_PORTAL = 4
ROOM_PORTAL_WAVE = 10

NEIGHBOURS = ((1, 0), (-1, 0), (0, 1), (0, -1))


class Level(object):
    def __init__(self, path):
        self.path = path
        self.width = 0
        self.height = 0
        self.types = {}
        self.fullness = {}
        self.seats = {}          # seatId -> dict of fields
        self.rooms = []          # (roomType, seatId, [(x, y)])
        self.doors = []          # (x, y)
        self.creatures = []      # (seatId, x, y)
        self.room_lines = []     # (name, line number), parallel to rooms
        self.creature_info = []  # (name, line number), parallel to creatures
        self.traps = []          # (name, seatId, x, y, line number)
        self.spawns = []         # (trigger name, seatId, x, y, line number)


def to_int(text):
    try:
        return int(float(text))
    except ValueError:
        return None


def parse_level(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = [l.rstrip("\r") for l in handle.read().split("\n")]
    level = Level(path)
    section = ""
    seat = None
    trigger_name = ""
    index = 0
    while index < len(lines):
        line = lines[index]
        index += 1
        stripped = line.strip()
        if stripped == "":
            continue
        if stripped.startswith("["):
            if stripped == "[Seat]":
                seat = {}
                section = "Seat"
            elif stripped == "[/Seat]":
                if seat is not None and "seatId" in seat:
                    level.seats[seat["seatId"]] = seat
                seat = None
                section = "Seats"
            elif stripped in ("[/Tiles]", "[/Rooms]", "[/Traps]", "[/Creatures]",
                              "[/Triggers]"):
                section = ""
            elif stripped in ("[Tiles]", "[Rooms]", "[Traps]", "[Creatures]", "[Triggers]"):
                section = stripped[1:-1]
                trigger_name = ""
            continue
        if stripped.startswith("#") and section != "Tiles":
            continue
        if section == "Seat":
            fields = line.split("\t")
            if len(fields) == 2:
                if fields[0] == "seatId" or fields[0] == "teamId":
                    value = to_int(fields[1])
                    if value is not None:
                        seat[fields[0]] = value
                elif fields[0] in ("player", "faction"):
                    seat[fields[0]] = fields[1].strip()
                elif fields[0] in ("startingX", "startingY"):
                    value = to_int(fields[1])
                    if value is not None:
                        seat[fields[0]] = value
        elif section == "Tiles":
            body = line.split("#", 1)[0].strip()
            if body == "":
                continue
            fields = body.split()
            if len(fields) == 1 and level.width == 0:
                level.width = int(fields[0])
            elif len(fields) == 1 and level.height == 0:
                level.height = int(fields[0])
            elif len(fields) >= 4:
                x = int(fields[0])
                y = int(fields[1])
                level.types[(x, y)] = int(fields[2])
                level.fullness[(x, y)] = float(fields[3])
        elif section in ("Rooms", "Traps"):
            fields = line.split("\t")
            if len(fields) == 4 and to_int(fields[0]) is not None:
                count = to_int(fields[3])
                tiles = []
                entry_line = index
                for _ in range(count):
                    if index >= len(lines):
                        break
                    tile = lines[index].split("\t")
                    index += 1
                    if len(tile) >= 2:
                        tiles.append((int(tile[0]), int(tile[1])))
                if section == "Rooms":
                    level.rooms.append((to_int(fields[0]), to_int(fields[2]), tiles))
                    level.room_lines.append((fields[1], entry_line))
                else:
                    for pos in tiles:
                        level.traps.append((fields[1], to_int(fields[2]), pos[0], pos[1],
                                            entry_line))
                    if "Door" in fields[1]:
                        level.doors.extend(tiles)
        elif section == "Creatures":
            fields = line.split("\t")
            if len(fields) >= 6 and to_int(fields[0]) is not None:
                x = to_int(fields[3])
                y = to_int(fields[4])
                if x is not None and y is not None:
                    level.creatures.append((to_int(fields[0]), x, y))
                    level.creature_info.append((fields[1], index))
        elif section == "Triggers":
            fields = line.split("	")
            if fields[0] == "Name" and len(fields) >= 2:
                trigger_name = fields[1].strip()
            elif fields[0] == "Action" and len(fields) >= 6 and fields[1] == "spawn":
                seat_value = to_int(fields[2])
                x = to_int(fields[3])
                y = to_int(fields[4])
                if seat_value is not None and x is not None and y is not None:
                    level.spawns.append((trigger_name, seat_value, x, y, index))
    if level.width <= 0 or level.height <= 0:
        return None
    # A tile that is not listed is full, diggable dirt in the game (new maps start as dirt
    # with fullness 100), not solid rock.
    for x in range(level.width):
        for y in range(level.height):
            if (x, y) not in level.types:
                level.types[(x, y)] = TILE_DIRT
                level.fullness[(x, y)] = 100.0
    return level


def human_seat(level):
    for seat_id in sorted(level.seats):
        if level.seats[seat_id].get("player") == "Human":
            return seat_id
    return None


def build_grids(level):
    """Returns (walkable, digpassable, room_tiles) as sets of (x, y)."""
    walkable = set()
    passable = set()
    for pos, tile_type in level.types.items():
        if tile_type in DIGGABLE_TYPES:
            passable.add(pos)
            if level.fullness.get(pos, 100.0) <= 0:
                walkable.add(pos)
    room_tiles = set()
    for room_type, _, tiles in level.rooms:
        for pos in tiles:
            walkable.add(pos)
            passable.add(pos)
            room_tiles.add(pos)
    for pos in level.doors:
        walkable.add(pos)
        passable.add(pos)
    return walkable, passable, room_tiles


def bfs(sources, allowed, width, height):
    dist = {}
    queue = deque()
    for pos in sources:
        if pos in allowed and pos not in dist:
            dist[pos] = 0
            queue.append(pos)
    while queue:
        x, y = queue.popleft()
        d = dist[(x, y)]
        for dx, dy in NEIGHBOURS:
            nxt = (x + dx, y + dy)
            if 0 <= nxt[0] < width and 0 <= nxt[1] < height and nxt in allowed \
                    and nxt not in dist:
                dist[nxt] = d + 1
                queue.append(nxt)
    return dist


def nearest(dist, tiles):
    best = None
    for pos in tiles:
        if pos in dist and (best is None or dist[pos] < best):
            best = dist[pos]
    return best


def heart_tiles(level, seat_id):
    tiles = []
    for room_type, owner, room in level.rooms:
        if room_type == ROOM_TEMPLE and owner == seat_id:
            tiles.extend(room)
    if not tiles:
        seat = level.seats[seat_id]
        if "startingX" in seat and "startingY" in seat:
            tiles.append((seat["startingX"], seat["startingY"]))
    return tiles


def enemy_sources(level, seat_id):
    team = level.seats[seat_id].get("teamId")
    sources = []
    for other_id, seat in level.seats.items():
        if other_id == seat_id or seat.get("teamId") == team:
            continue
        for room_type, owner, tiles in level.rooms:
            if owner == other_id and room_type in (ROOM_TEMPLE, ROOM_PORTAL) and tiles:
                sources.append(("seat %d room type %d" % (other_id, room_type), tiles[0]))
    for room_type, owner, tiles in level.rooms:
        if room_type == ROOM_PORTAL_WAVE and tiles:
            owner_team = level.seats.get(owner, {}).get("teamId")
            if owner_team != team:
                sources.append(("wave portal of seat %d" % owner, tiles[0]))
    for owner, x, y in level.creatures:
        owner_team = level.seats.get(owner, {}).get("teamId")
        if owner in level.seats and owner_team != team:
            sources.append(("creature of seat %d at %d,%d" % (owner, x, y), (x, y)))
    return sources


def check_enemy_distance(level, seat_id):
    walkable, passable, _ = build_grids(level)
    hearts = heart_tiles(level, seat_id)
    walk = bfs(hearts, walkable, level.width, level.height)
    dig = bfs(hearts, passable, level.width, level.height)
    reasons = []
    shortest = None
    connected = []
    for name, pos in enemy_sources(level, seat_id):
        if pos in walk:
            connected.append((walk[pos], name))
        d = dig.get(pos)
        if d is not None:
            if shortest is None or d < shortest:
                shortest = d
    if connected:
        connected.sort()
        reasons.append("%d enemy source(s) walk to the heart without digging, nearest: %s "
                       "in %d tiles" % (len(connected), connected[0][1], connected[0][0]))
    return reasons, shortest


def largest_open_square(open_tiles, x0, y0, x1, y1):
    """Largest all-open square inside the box; returns (size, x, y) of its corner."""
    best = (0, 0, 0)
    side = {}
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if (x, y) not in open_tiles:
                side[(x, y)] = 0
                continue
            value = 1 + min(side.get((x - 1, y), 0), side.get((x, y - 1), 0),
                            side.get((x - 1, y - 1), 0))
            side[(x, y)] = value
            if value > best[0]:
                best = (value, x - value + 1, y - value + 1)
    return best


def check_open_areas(level, seat_id):
    walkable, _, room_tiles = build_grids(level)
    hearts = heart_tiles(level, seat_id)
    cx = sum(p[0] for p in hearts) // len(hearts)
    cy = sum(p[1] for p in hearts) // len(hearts)
    x0 = max(0, cx - OPEN_RADIUS)
    x1 = min(level.width - 1, cx + OPEN_RADIUS)
    y0 = max(0, cy - OPEN_RADIUS)
    y1 = min(level.height - 1, cy + OPEN_RADIUS)
    own_tiles = [pos for room_type, owner, tiles in level.rooms
                 if owner == seat_id for pos in tiles]
    if own_tiles:
        x0 = min(x0, min(p[0] for p in own_tiles) - 1)
        x1 = max(x1, max(p[0] for p in own_tiles) + 1)
        y0 = min(y0, min(p[1] for p in own_tiles) - 1)
        y1 = max(y1, max(p[1] for p in own_tiles) + 1)
        x0 = max(0, x0)
        y0 = max(0, y0)
        x1 = min(level.width - 1, x1)
        y1 = min(level.height - 1, y1)
    open_tiles = set(p for p in walkable if p not in room_tiles
                     and x0 <= p[0] <= x1 and y0 <= p[1] <= y1)
    size, sx, sy = largest_open_square(open_tiles, x0, y0, x1, y1)
    reasons = []
    if size >= OPEN_SQUARE:
        reasons.append("open area %dx%d at %d,%d" % (size, size, sx, sy))
    own_rooms = [tiles for room_type, owner, tiles in level.rooms
                 if owner == seat_id]
    # rooms that touch each other
    owner_of = {}
    for number, tiles in enumerate(own_rooms):
        for pos in tiles:
            owner_of[pos] = number
    touching = set()
    for pos, number in owner_of.items():
        for dx, dy in NEIGHBOURS:
            other = owner_of.get((pos[0] + dx, pos[1] + dy))
            if other is not None and other != number:
                touching.add((min(number, other), max(number, other)))
    if touching:
        reasons.append("%d room pair(s) touch without rock" % len(touching))
    # wide passage next to an own room
    wide = 0
    wide_at = None
    for (x, y) in open_tiles:
        block = [(x + i, y + j) for i in range(WIDE_PASSAGE) for j in range(WIDE_PASSAGE)]
        if not all(p in open_tiles for p in block):
            continue
        near = False
        for bx, by in block:
            for ox in range(-PASSAGE_ROOM_DISTANCE, PASSAGE_ROOM_DISTANCE + 1):
                for oy in range(-PASSAGE_ROOM_DISTANCE, PASSAGE_ROOM_DISTANCE + 1):
                    if (bx + ox, by + oy) in owner_of:
                        near = True
        if near:
            wide += 1
            if wide_at is None or (y, x) < (wide_at[1], wide_at[0]):
                wide_at = (x, y)
    if wide:
        reasons.append("wide passage (>%d) next to a room, first at %d,%d"
                       % (MAX_PASSAGE_WIDTH, wide_at[0], wide_at[1]))
    return reasons, size, (sx, sy)


def player_seats(level):
    """Human seats; if there is none, the choice seats (any of them may be the player)."""
    humans = [i for i in sorted(level.seats) if level.seats[i].get("player") == "Human"]
    if humans:
        return humans
    return [i for i in sorted(level.seats) if level.seats[i].get("player") == "Choice"]


def is_enemy_seat(level, seat_id, owner):
    """Enemy = a real seat of another team. Neutral seat 0 does not count."""
    if owner == 0 or owner not in level.seats or owner == seat_id:
        return False
    return level.seats[owner].get("teamId") != level.seats[seat_id].get("teamId")


def bfs_parents(sources, allowed, width, height):
    dist = {}
    parent = {}
    queue = deque()
    for pos in sources:
        if pos in allowed and pos not in dist:
            dist[pos] = 0
            parent[pos] = None
            queue.append(pos)
    while queue:
        x, y = queue.popleft()
        d = dist[(x, y)]
        for dx, dy in NEIGHBOURS:
            nxt = (x + dx, y + dy)
            if 0 <= nxt[0] < width and 0 <= nxt[1] < height and nxt in allowed \
                    and nxt not in dist:
                dist[nxt] = d + 1
                parent[nxt] = (x, y)
                queue.append(nxt)
    return dist, parent


def safe_zone(level, seat_id, radius):
    """Returns (zone, portal_text): zone maps tile -> dig/walk distance (<= radius) from the
    heart, the nearest portal of the seat and the path between them."""
    _, passable, _ = build_grids(level)
    hearts = list(heart_tiles(level, seat_id))
    portals = [pos for room_type, owner, tiles in level.rooms
               if room_type == ROOM_PORTAL and owner in (seat_id, 0) for pos in tiles]
    sources = list(hearts)
    portal_text = "no portal of the seat or unclaimed"
    if portals:
        dist, parent = bfs_parents(hearts, passable, level.width, level.height)
        reachable = sorted((dist[p], p) for p in portals if p in dist)
        if reachable:
            best_dist, best = reachable[0]
            for room_type, owner, tiles in level.rooms:
                if room_type == ROOM_PORTAL and owner in (seat_id, 0) and best in tiles:
                    sources.extend(tiles)
            pos = best
            while pos is not None:
                sources.append(pos)
                pos = parent[pos]
            portal_text = "nearest portal %d,%d path %d tiles" % (best[0], best[1], best_dist)
        else:
            by_air = sorted((min(abs(p[0] - h[0]) + abs(p[1] - h[1]) for h in hearts), p)
                            for p in portals)
            best = by_air[0][1]
            for room_type, owner, tiles in level.rooms:
                if room_type == ROOM_PORTAL and owner in (seat_id, 0) and best in tiles:
                    sources.extend(tiles)
            portal_text = "portal %d,%d not reachable by digging" % (best[0], best[1])
    zone, _ = bfs_parents(sources, passable, level.width, level.height)
    return dict((pos, d) for pos, d in zone.items() if d <= radius), portal_text


def check_safe_zone(level, seat_id, radius):
    """Returns (violations, portal_text); violations are (kind, text) tuples."""
    zone, portal_text = safe_zone(level, seat_id, radius)
    name = os.path.basename(level.path)
    found = []
    for number, (owner, x, y) in enumerate(level.creatures):
        if is_enemy_seat(level, seat_id, owner) and (x, y) in zone:
            label = level.creature_info[number]
            found.append(("enemy creature", "%s:%d seat %d %s at %d,%d (%d tiles)" % (
                name, label[1], owner, label[0], x, y, zone[(x, y)])))
    for trap_name, owner, x, y, line in level.traps:
        if is_enemy_seat(level, seat_id, owner) and (x, y) in zone:
            kind = "enemy door" if "Door" in trap_name else "enemy trap"
            found.append((kind, "%s:%d seat %d %s at %d,%d (%d tiles)" % (
                name, line, owner, trap_name, x, y, zone[(x, y)])))
    for number, (room_type, owner, tiles) in enumerate(level.rooms):
        if room_type == ROOM_PORTAL_WAVE:
            relevant = owner is not None and owner != seat_id and (
                owner not in level.seats or is_enemy_seat(level, seat_id, owner))
            kind = "enemy wave portal"
        else:
            relevant = is_enemy_seat(level, seat_id, owner)
            kind = "enemy room"
        if not relevant:
            continue
        inside = [p for p in tiles if p in zone]
        if inside:
            label = level.room_lines[number]
            found.append((kind, "%s:%d seat %s %s at %d,%d (%d tiles, %d of %d tiles inside)"
                          % (name, label[1], owner, label[0], inside[0][0], inside[0][1],
                             min(zone[p] for p in inside), len(inside), len(tiles))))
    for trigger, owner, x, y, line in level.spawns:
        if is_enemy_seat(level, seat_id, owner) and (x, y) in zone:
            found.append(("enemy spawn", "%s:%d trigger %s seat %d at %d,%d (%d tiles)" % (
                name, line, trigger, owner, x, y, zone[(x, y)])))
    return found, portal_text


def summarize_safe_zone(found, full):
    """Reasons for the output: one text per kind with a few examples."""
    kinds = []
    for kind, _ in found:
        if kind not in kinds:
            kinds.append(kind)
    reasons = []
    for kind in kinds:
        items = [text for k, text in found if k == kind]
        shown = items if full else items[:MAX_EXAMPLES]
        more = "" if len(shown) == len(items) else " +%d more" % (len(items) - len(shown))
        reasons.append("safe zone: %d %s [%s%s]" % (len(items), kind, "; ".join(shown), more))
    return reasons


def check_level(path, campaign_index, radius=SAFE_RADIUS, full=False):
    """Returns (status, reasons, numbers, violations)."""
    level = parse_level(path)
    if level is None:
        return "SKIP", ["no tiles"], "", []
    seat_id = human_seat(level)
    safe_seats = [i for i in player_seats(level) if heart_tiles(level, i)]
    if not safe_seats:
        return "SKIP", ["no human seat"], "", []
    reasons = []
    shortest = None
    numbers = ""
    if seat_id is not None and heart_tiles(level, seat_id):
        if campaign_index is not None:
            found, shortest = check_enemy_distance(level, seat_id)
            reasons.extend(found)
        found, size, corner = check_open_areas(level, seat_id)
        reasons.extend(found)
        numbers = "shortest_dig_path=%s largest_open=%dx%d@%d,%d " % (
            "none" if shortest is None else shortest, size, size, corner[0], corner[1])
    violations = []
    portal_texts = []
    for other in safe_seats:
        found, portal_text = check_safe_zone(level, other, radius)
        violations.extend(found)
        portal_texts.append(portal_text)
    reasons.extend(summarize_safe_zone(violations, full))
    numbers += "safe_zone(r=%d): %s" % (radius, "; ".join(portal_texts))
    return ("FAIL" if reasons else "PASS"), reasons, numbers, violations


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def campaign_files(root):
    cfg = os.path.join(root, "levels", "Campaign.cfg")
    if not os.path.isfile(cfg):
        cfg = os.path.join(root, "levels", "campaign", "Campaign.cfg")
    result = []
    with open(cfg, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if line.startswith("File="):
                result.append(os.path.join(root, "levels", line[5:].replace("/", os.sep)))
    return result


def read_allowlist(root):
    names = set()
    path = os.path.join(root, "scripts", "check-campaign-opening.allowlist")
    if os.path.isfile(path):
        with open(path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.split("#", 1)[0].strip()
                if line:
                    names.add(line)
    return names


def all_level_files(root):
    result = []
    for folder, _, names in os.walk(os.path.join(root, "levels")):
        for name in sorted(names):
            if name.endswith(".level") and name != "template.level":
                result.append(os.path.join(folder, name))
    return sorted(result)


def run(files, root, use_allowlist, order, radius=SAFE_RADIUS, full=False):
    allow = read_allowlist(root) if use_allowlist else set()
    failed = 0
    counts = {"PASS": 0, "FAIL": 0, "FAIL known": 0, "SKIP": 0}
    kinds = {}
    for path in files:
        name = os.path.basename(path)
        norm = os.path.normcase(os.path.abspath(path))
        index = order.get(norm)
        if index is None and os.path.basename(os.path.dirname(norm)) == "campaign":
            index = len(order)
        status, reasons, numbers, violations = check_level(path, index, radius, full)
        text = "; ".join(reasons)
        for kind, _ in violations:
            kinds[kind] = kinds.get(kind, 0) + 1
        if status == "FAIL" and name in allow:
            counts["FAIL known"] += 1
            print("FAIL (known, to be fixed) %s %s | %s" % (name, numbers, text))
        elif status == "FAIL":
            failed += 1
            counts["FAIL"] += 1
            print("FAIL %s %s | %s" % (name, numbers, text))
        elif status == "SKIP":
            counts["SKIP"] += 1
            print("SKIP %s %s" % (name, text))
        else:
            counts["PASS"] += 1
            note = " (allowlist entry obsolete)" if name in allow else ""
            print("PASS %s %s%s" % (name, numbers, note))
    print("")
    print("Summary: %d level(s): %d PASS, %d FAIL, %d FAIL (known, allowlisted), %d SKIP" % (
        len(files), counts["PASS"], counts["FAIL"], counts["FAIL known"], counts["SKIP"]))
    if kinds:
        print("Safe zone violations by kind: " + ", ".join(
            "%s %d" % (k, kinds[k]) for k in sorted(kinds)))
    return 1 if failed else 0


# --- Self test ---------------------------------------------------------------------------
def synthetic_level(path, heart_x, enemy_x, rooms_gap, open_hall, portal_x=None, spawn_x=None,
                    trap_x=None, rock_wall_x=None, neutral_x=None, omit_dirt=False):
    """40x20 map, rock border, human temple around (heart_x, 10), enemy at enemy_x.
    omit_dirt leaves the full dirt tiles out of the file (the game fills them as dirt)."""
    width, height = 80, 20
    lines = ["OpenDungeons_Version:0.7.1", "", "[Seats]"]
    for seat_id, team, player, x in ((1, 1, "Human", heart_x), (2, 2, "Inactive", enemy_x)):
        lines += ["[Seat]", "seatId\t%d" % seat_id, "teamId\t%d" % team, "player\t%s" % player,
                  "startingX\t%d" % x, "startingY\t10", "[/Seat]"]
    lines += ["[/Seats]", "", "[Tiles]", "%d # MapSizeX" % width, "%d # MapSizeY" % height]
    open_tiles = set()
    for x in range(heart_x - 2, heart_x + 3):
        for y in range(8, 13):
            open_tiles.add((x, y))
    for x in range(enemy_x - 1, enemy_x + 2):
        for y in range(9, 12):
            open_tiles.add((x, y))
    if enemy_x - heart_x < 20:
        for x in range(heart_x, enemy_x):
            open_tiles.add((x, 10))
    room2 = [(heart_x - 3 - rooms_gap - i, y) for i in range(3) for y in range(8, 11)]
    if open_hall:
        for x in range(heart_x - 12, heart_x - 6):
            for y in range(4, 16):
                open_tiles.add((x, y))
    open_tiles.update(room2)
    portal = []
    if portal_x is not None:
        portal = [(portal_x + i, y) for i in range(3) for y in range(9, 12)]
        open_tiles.update(portal)
    for y in range(height):
        for x in range(width):
            if rock_wall_x is not None and x == rock_wall_x:
                lines.append("%d\t%d\t3\t100" % (x, y))
            elif (x, y) in open_tiles:
                lines.append("%d\t%d\t1\t0" % (x, y))
            elif x in (0, width - 1) or y in (0, height - 1):
                lines.append("%d\t%d\t3\t100" % (x, y))
            else:
                lines.append("%d\t%d\t1\t100" % (x, y))
    lines += ["[/Tiles]", "", "[Rooms]", "[Room]", "1\tDungeonTemple1\t1\t25"]
    for x in range(heart_x - 2, heart_x + 3):
        for y in range(8, 13):
            lines.append("%d\t%d" % (x, y))
    lines += ["[/Room]", "[Room]", "2\tDormitory1\t1\t9"]
    for pos in room2:
        lines.append("%d\t%d" % pos)
    if portal:
        lines += ["[/Room]", "[Room]", "4\tPortal1\t1\t%d" % len(portal)]
        for pos in portal:
            lines.append("%d\t%d" % pos)
    lines += ["[/Room]", "[/Rooms]", "", "[Traps]"]
    if trap_x is not None:
        lines += ["1\tCannon1\t2\t1", "%d\t10\t1" % trap_x]
    lines += ["[/Traps]", "", "[Creatures]",
              "2\tHero1\tKnight.mesh\t%d\t10\t0\tKnight\t1\t0\tmax\t100\t0\t0" % enemy_x]
    if neutral_x is not None:
        lines.append("0\tRogue1\tKnight.mesh\t%d\t10\t0\tKnight\t1\t0\tmax\t100\t0\t0"
                     % neutral_x)
    lines += ["[/Creatures]", "", "[Triggers]"]
    if spawn_x is not None:
        lines += ["[Trigger]", "Name\tambush", "Mode\tonce", "Cond\ttime\t5",
                  "Action\tspawn\t2\t%d\t10\t1\tKnight:1" % spawn_x, "[/Trigger]"]
    lines += ["[/Triggers]", ""]
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines))


def self_test():
    directory = tempfile.mkdtemp()
    try:
        good = os.path.join(directory, "good.level")
        near = os.path.join(directory, "near.level")
        open_area = os.path.join(directory, "open.level")
        touching = os.path.join(directory, "touch.level")
        synthetic_level(good, 60, 4, 1, False)
        synthetic_level(near, 14, 30, 1, False)
        synthetic_level(open_area, 60, 4, 1, True)
        synthetic_level(touching, 60, 4, 0, False)
        safe_ok = os.path.join(directory, "safe_ok.level")
        safe_far = os.path.join(directory, "safe_far.level")
        safe_spawn = os.path.join(directory, "safe_spawn.level")
        safe_trap = os.path.join(directory, "safe_trap.level")
        safe_rock = os.path.join(directory, "safe_rock.level")
        safe_neutral = os.path.join(directory, "safe_neutral.level")
        synthetic_level(safe_ok, 60, 4, 1, False, portal_x=70, neutral_x=64)
        synthetic_level(safe_far, 60, 4, 1, False, portal_x=70, spawn_x=30)
        synthetic_level(safe_spawn, 60, 4, 1, False, portal_x=70, spawn_x=78)
        synthetic_level(safe_trap, 60, 4, 1, False, portal_x=70, trap_x=75)
        synthetic_level(safe_rock, 60, 4, 1, False, rock_wall_x=66, spawn_x=70)
        synthetic_level(safe_neutral, 60, 4, 1, False, portal_x=70, neutral_x=75)
        sparse_spawn = os.path.join(directory, "sparse_spawn.level")
        sparse_ok = os.path.join(directory, "sparse_ok.level")
        sparse_rock = os.path.join(directory, "sparse_rock.level")
        synthetic_level(sparse_spawn, 60, 4, 1, False, portal_x=70, spawn_x=78, omit_dirt=True)
        synthetic_level(sparse_ok, 60, 4, 1, False, portal_x=70, spawn_x=30, omit_dirt=True)
        synthetic_level(sparse_rock, 60, 4, 1, False, rock_wall_x=66, spawn_x=70,
                        omit_dirt=True)
        expectations = [(good, 5, "PASS"), (near, 5, "FAIL"), (open_area, 5, "FAIL"),
                        (touching, 5, "FAIL"), (safe_ok, 5, "PASS"), (safe_far, 5, "PASS"),
                        (safe_spawn, 5, "FAIL"), (safe_trap, 5, "FAIL"), (safe_rock, 5, "PASS"),
                        (safe_neutral, 5, "PASS"), (sparse_spawn, 5, "FAIL"),
                        (sparse_ok, 5, "PASS"), (sparse_rock, 5, "PASS")]
        ok = True
        for path, index, expected in expectations:
            status, reasons, numbers, _ = check_level(path, index)
            print("%s (expected %s) %s | %s" % (status, expected, os.path.basename(path),
                                                  "; ".join(reasons)))
            if status != expected:
                ok = False
        # the connected variant must report the walk and the distance reason
        status, reasons, _, _ = check_level(near, 5)
        if not any("walk to the heart" in r for r in reasons):
            ok = False
        status, reasons, _, _ = check_level(open_area, 5)
        if not any("open area" in r for r in reasons):
            ok = False
        status, reasons, _, _ = check_level(touching, 5)
        if not any("touch" in r for r in reasons):
            ok = False
        status, reasons, _, _ = check_level(safe_spawn, 5)
        if not any("enemy spawn" in r for r in reasons):
            ok = False
        status, reasons, _, _ = check_level(safe_trap, 5)
        if not any("enemy trap" in r for r in reasons):
            ok = False
        status, reasons, _, _ = check_level(sparse_spawn, 5)
        if not any("enemy spawn" in r for r in reasons):
            ok = False
        print("self-test %s" % ("passed" if ok else "FAILED"))
        return 0 if ok else 1
    finally:
        shutil.rmtree(directory, ignore_errors=True)


def main(argv):
    if "--self-test" in argv:
        return self_test()
    use_allowlist = "--no-allowlist" not in argv
    full = "--full" in argv
    radius = SAFE_RADIUS
    rest = []
    skip = False
    for position, arg in enumerate(argv):
        if skip:
            skip = False
        elif arg == "--safe-radius" and position + 1 < len(argv):
            radius = int(argv[position + 1])
            skip = True
        else:
            rest.append(arg)
    files = [a for a in rest if not a.startswith("--")]
    root = repo_root()
    order = {}
    for number, path in enumerate(campaign_files(root)):
        order[os.path.normcase(os.path.abspath(path))] = number
    if not files:
        files = all_level_files(root) if "--all" in argv else campaign_files(root)
    return run(files, root, use_allowlist, order, radius, full)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
