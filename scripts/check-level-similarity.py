#!/usr/bin/env python3
"""Similarity check for level files against a local folder of other level files.

The folder (default: ..\\OpenDungeonsPlus-private next to the main working tree) is never
part of the repository. If it does not exist the check is skipped and the exit code is 0,
so clones without it stay green. This script contains no level data.

For every checked level the terrain is compared with every level found in the folder at
the best alignment. The search covers shifts, mirroring, the four 90 degree rotations and
scales from 0.7 to 1.4: a coarse search on a pooled grid (FFT correlation), then an exact
search around the best coarse candidates, plus a search that aligns the placements.

Filler is ignored: solid rock and dirt, everything outside the map and the inside of
regions. Only contour tiles count (floor next to a wall, the edge of a lake, a lava field
or a gold seam), so large caves and lakes do not match by chance. The terrain match is
corrected by the number of tiles that coincide by chance, and the placements are corrected
by what the same search finds in a copy of the other level whose placements were moved by
a few tiles. Levels with fewer than 200 contour tiles are judged on placements and texts
only. A run of identical tiles has to be at least three tiles wide and has to change its
cross section, and a run of more than three identical words only counts with at least
three content words.

A level FAILs if, at the best alignment,

  * more than 25 % of its non-filler terrain tiles are identical,
  * more than 10 % of its placements (heart, keepers, creatures, rooms, traps, doors,
    treasures) are within 2 tiles of a placement of the other level,
  * the longest identical row or column run is longer than 8 tiles,
  * a text (name, description, messages, titles, briefings) has more than 3 identical
    words in a row with a text of the other levels.

Output is one line per level: PASS or FAIL, then the four numbers.

Usage:
  check-level-similarity.py [--against <dir>] [<file> ...]
  check-level-similarity.py --self-test

Without files all levels under levels/ of this checkout are checked.
"""

import hashlib
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np

PRIVATE_DIRECTORY_NAME = "OpenDungeonsPlus-private"

MAX_TERRAIN_FRACTION = 0.25
MAX_PLACEMENT_FRACTION = 0.10
MAX_RUN_LENGTH = 8
MAX_IDENTICAL_WORDS = 3
# Own test levels without a source that have too few contour tiles to judge the terrain
# (a match is mostly chance). Every other level always gets the full terrain and run check.
SMALL_LEVEL_EXCEPTIONS = [
    "aa.level",
    "ab.level",
    "Sandbox.level",
    "SandboxEverything.level",
    "TestSingleplayerSmallPassability.level",
    "TestWeaponsShowcase.level",
]
# Own test levels without a source: a placement match against any other level is mostly
# chance for these tiny setups, so the placement check is not applied to them. Reworked
# levels that have a source never appear here and always get the full check.
PLACEMENT_EXCEPTIONS = [  # own test levels without a source
    "TestMultiplayerSmall1v1.level",
    "ab.level",
    "TestModelsFair.level",
    "TestSingleplayerSmallPassability.level",
    "TestWeaponsShowcase.level",
]
PLACEMENT_DISTANCE = 2.0
DECOY_JITTER = 6.0
CHANCE_MARGIN = 2

SCALES = [0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4]
COARSE_FACTOR = 4
COARSE_CANDIDATES = 4
PLACEMENT_CANDIDATES = 3

# Terrain classes after normalisation, 0 is filler.
CLASS_FILLER = 0
CLASS_FLOOR_EDGE = 1
CLASS_GOLD = 2
CLASS_WATER = 3
CLASS_LAVA = 4
CLASS_GEM = 5
CLASS_COUNT = 6
CLASS_WEIGHTS = [0.0, 1.0, 2.0, 2.0, 2.0, 2.0]

TYPE_GOLD = 2
TYPE_WATER = 4
TYPE_LAVA = 5
TYPE_GEM = 6

TEXT_KEYS = ["Name", "Description"]
CONFIG_TEXT_KEYS = ["Title", "Briefing", "Debriefing"]
# A run of more than three identical words only counts if it holds at least three words
# outside this list, so stock phrases ("it is said that") are not flagged.
STOP_WORDS = set([
    "a", "an", "the", "of", "in", "on", "at", "to", "for", "from", "by", "with", "and",
    "or", "but", "it", "is", "are", "was", "were", "be", "been", "that", "this", "these",
    "those", "as", "if", "you", "your", "he", "she", "they", "them", "his", "her", "its",
    "their", "we", "our", "i", "my", "me", "has", "have", "had", "will", "can", "all",
    "said", "so", "not", "no", "full", "complete", "campaign", "dungeon",
])
MIN_CONTENT_WORDS = 3

TEXT_ACTIONS = ["message", "objective", "subobjective"]


def run_git(args, cwd):
    proc = subprocess.run(["git"] + args, cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
    return proc.returncode, proc.stdout.decode("utf-8", errors="replace")


def repository_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def default_against():
    """The private folder next to the main working tree (not next to a worktree)."""
    root = repository_root()
    code, out = run_git(["rev-parse", "--git-common-dir"], root)
    if code == 0:
        common = os.path.abspath(os.path.join(root, out.strip()))
        root = os.path.dirname(common)
    return os.path.join(os.path.dirname(root), PRIVATE_DIRECTORY_NAME)


# ---------------------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------------------

class Level:
    def __init__(self, name):
        self.name = name
        self.grid = np.zeros((1, 1), dtype=np.int8)
        self.placements = []  # (kind, x, y)
        self.texts = []
        self.start = {}


def to_number(text):
    try:
        return float(text)
    except ValueError:
        return None


def derive_classes(types, fullness):
    """Maps raw tile types and fullness to terrain classes.

    Only tiles on the contour of a region count: a tile whose four neighbours all have its
    own class is filler, so large lakes, caves and gold seams do not match by chance.
    """
    height, width = types.shape
    raw = np.zeros((height, width), dtype=np.int8)
    solid = fullness > 0
    raw[(types == TYPE_WATER)] = CLASS_WATER
    raw[(types == TYPE_LAVA)] = CLASS_LAVA
    raw[(types == TYPE_GOLD) & solid] = CLASS_GOLD
    raw[(types == TYPE_GEM) & solid] = CLASS_GEM
    open_floor = (types > 0) & ~solid & (types != TYPE_WATER) & (types != TYPE_LAVA)
    raw[open_floor] = CLASS_FLOOR_EDGE
    padded = np.zeros((height + 2, width + 2), dtype=np.int8)
    padded[1:-1, 1:-1] = raw
    inside = ((padded[:-2, 1:-1] == raw) & (padded[2:, 1:-1] == raw)
              & (padded[1:-1, :-2] == raw) & (padded[1:-1, 2:] == raw))
    result = raw.copy()
    result[inside] = CLASS_FILLER
    return result


def parse_level(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.read().split("\n")
    level = Level(os.path.basename(path))
    width = 0
    height = 0
    tiles = []
    section = ""
    in_info = False
    index = 0
    while index < len(lines):
        line = lines[index].rstrip("\r")
        index += 1
        stripped = line.strip()
        if stripped == "":
            continue
        if stripped.startswith("["):
            if stripped == "[Info]":
                in_info = True
            elif stripped == "[/Info]":
                in_info = False
            elif stripped in ("[/Tiles]", "[/Rooms]", "[/Traps]", "[/Creatures]",
                              "[/TreasuryObject]", "[/Triggers]", "[/Seat]"):
                section = ""
            elif stripped in ("[Tiles]", "[Rooms]", "[Traps]", "[Creatures]",
                              "[TreasuryObject]", "[Triggers]", "[Seat]"):
                section = stripped[1:-1]
            continue
        if in_info:
            fields = line.split("\t", 1)
            if len(fields) == 2 and fields[0] in TEXT_KEYS:
                level.texts.append(fields[1])
            continue
        if section == "Tiles":
            body = line.split("#", 1)[0].strip()
            if body == "":
                continue
            fields = body.split()
            if len(fields) == 1 and width == 0:
                width = int(fields[0])
            elif len(fields) == 1 and height == 0:
                height = int(fields[0])
            elif len(fields) >= 4:
                tiles.append((int(fields[0]), int(fields[1]), int(fields[2]),
                              float(fields[3])))
        elif section == "Seat":
            fields = line.split("\t")
            if len(fields) == 2 and fields[0] in ("startingX", "startingY"):
                value = to_number(fields[1])
                if value is not None:
                    level.start[fields[0]] = value
                    if len(level.start) == 2:
                        level.placements.append(("heart", level.start["startingX"],
                                                 level.start["startingY"]))
                        level.start = {}
        elif section == "Rooms":
            fields = line.split("\t")
            if len(fields) == 4 and to_number(fields[0]) is not None:
                count = int(fields[3])
                xs = []
                ys = []
                for _ in range(count):
                    if index >= len(lines):
                        break
                    tile = lines[index].split("\t")
                    index += 1
                    if len(tile) >= 2:
                        xs.append(float(tile[0]))
                        ys.append(float(tile[1]))
                if xs:
                    kind = "heart" if fields[1].startswith("DungeonTemple") else "room"
                    level.placements.append((kind, sum(xs) / len(xs), sum(ys) / len(ys)))
        elif section == "Traps":
            fields = line.split("\t")
            if len(fields) == 4 and to_number(fields[0]) is not None:
                count = int(fields[3])
                kind = "door" if "Door" in fields[1] else "trap"
                for _ in range(count):
                    if index >= len(lines):
                        break
                    tile = lines[index].split("\t")
                    index += 1
                    if len(tile) >= 2:
                        level.placements.append((kind, float(tile[0]), float(tile[1])))
        elif section in ("Creatures", "TreasuryObject"):
            fields = line.split("\t")
            if fields[0].startswith("#") or len(fields) < 6:
                continue
            x = to_number(fields[3])
            y = to_number(fields[4])
            if x is not None and y is not None:
                kind = "creature" if section == "Creatures" else "treasure"
                level.placements.append((kind, x, y))
        elif section == "Triggers":
            fields = line.split("\t")
            if len(fields) >= 5 and fields[0] == "Action":
                if fields[1] == "spawn":
                    x = to_number(fields[3])
                    y = to_number(fields[4])
                    if x is not None and y is not None:
                        level.placements.append(("creature", x, y))
                elif fields[1] in TEXT_ACTIONS:
                    level.texts.append(fields[-1])
    if width <= 0 or height <= 0:
        return None
    types = np.zeros((height, width), dtype=np.int16)
    fullness = np.full((height, width), 100.0, dtype=np.float32)
    for x, y, tile_type, tile_fullness in tiles:
        if 0 <= x < width and 0 <= y < height:
            types[y, x] = tile_type
            fullness[y, x] = tile_fullness
    level.grid = derive_classes(types, fullness)
    return level


def parse_config_texts(path):
    texts = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.rstrip("\r\n")
            for key in CONFIG_TEXT_KEYS:
                if line.startswith(key + "="):
                    texts.extend(line[len(key) + 1:].split("\\n"))
    return texts


def words_of(text):
    return re.findall(r"[a-z0-9']+", text.lower())


# ---------------------------------------------------------------------------------------
# Transforms
# ---------------------------------------------------------------------------------------

class Transformed:
    def __init__(self, grid, points, decoy):
        self.grid = grid
        self.points = points  # kind -> array of (x, y)
        self.decoy = decoy  # the same points, each moved by a few tiles


def transform_level(level, rotation, scale):
    height, width = level.grid.shape
    indices = np.arange(height * width).reshape(height, width)
    indices = np.rot90(indices, rotation % 4)
    if rotation >= 4:
        indices = indices[:, ::-1]
    new_height, new_width = indices.shape
    grid = level.grid.ravel()[indices]
    where = np.empty(height * width, dtype=np.int64)
    where[indices.ravel()] = np.arange(indices.size)
    if scale != 1.0:
        scaled_height = max(1, int(round(new_height * scale)))
        scaled_width = max(1, int(round(new_width * scale)))
        rows = np.minimum((np.arange(scaled_height) / scale).astype(int), new_height - 1)
        columns = np.minimum((np.arange(scaled_width) / scale).astype(int), new_width - 1)
        grid = grid[np.ix_(rows, columns)]
    points = {}
    seen = set()
    for kind, x, y in level.placements:
        ix = int(round(x))
        iy = int(round(y))
        if not (0 <= ix < width and 0 <= iy < height) or (kind, ix, iy) in seen:
            continue
        seen.add((kind, ix, iy))
        place = where[iy * width + ix]
        ny = (place // new_width + 0.5) * scale - 0.5
        nx = (place % new_width + 0.5) * scale - 0.5
        points.setdefault(kind, []).append((nx, ny))
    decoy = {}
    generator = np.random.RandomState(7)
    for kind in points:
        points[kind] = np.array(points[kind], dtype=np.float64)
        decoy[kind] = points[kind] + generator.uniform(-DECOY_JITTER, DECOY_JITTER,
                                                       points[kind].shape)
    return Transformed(grid, points, decoy)


def all_transforms(level):
    result = []
    for rotation in range(8):
        for scale in SCALES:
            result.append(transform_level(level, rotation, scale))
    return result


# ---------------------------------------------------------------------------------------
# Comparison at one alignment
# ---------------------------------------------------------------------------------------

def overlap_slices(shape_a, shape_b, dy, dx):
    """Slices of a and b that overlap when b[y + dy, x + dx] lies on a[y, x]."""
    y0 = max(0, -dy)
    y1 = min(shape_a[0], shape_b[0] - dy)
    x0 = max(0, -dx)
    x1 = min(shape_a[1], shape_b[1] - dx)
    if y1 <= y0 or x1 <= x0:
        return None
    return (slice(y0, y1), slice(x0, x1)), (slice(y0 + dy, y1 + dy), slice(x0 + dx, x1 + dx))


def identical_mask(candidate, other, dy, dx):
    """Mask of identical non-filler tiles and the number expected by chance, or None."""
    slices = overlap_slices(candidate.shape, other.shape, dy, dx)
    if slices is None:
        return None, 0.0
    first = candidate[slices[0]]
    second = other[slices[1]]
    first_counts = np.bincount(first.ravel(), minlength=CLASS_COUNT)
    second_counts = np.bincount(second.ravel(), minlength=CLASS_COUNT)
    expected = float((first_counts[1:] * second_counts[1:]).sum()) / first.size
    return (first == second) & (first != CLASS_FILLER), expected


def longest_run(mask, reference):
    """Longest row or column run of identical tiles. A tile only counts if its neighbours
    on both sides across the run are identical too, and the run must change its cross
    section at least once, so straight walls, bridges and seams do not count."""
    best = 0
    padded = np.pad(mask, 1)
    across_rows = mask & padded[:-2, 1:-1] & padded[2:, 1:-1]
    across_columns = mask & padded[1:-1, :-2] & padded[1:-1, 2:]
    classes = np.pad(reference, 1).astype(np.int32)
    section_rows = (classes[:-2, 1:-1] * 36 + classes[1:-1, 1:-1] * 6 + classes[2:, 1:-1])
    section_columns = (classes[1:-1, :-2] * 36 + classes[1:-1, 1:-1] * 6 + classes[1:-1, 2:])
    for matrix, sections in ((across_rows, section_rows),
                             (across_columns.T, section_columns.T)):
        run = np.zeros(matrix.shape[0], dtype=np.int32)
        changes = np.zeros(matrix.shape[0], dtype=np.int32)
        for column in range(matrix.shape[1]):
            if column > 0:
                changed = sections[:, column] != sections[:, column - 1]
                changes = np.where(run > 0, changes + changed, 0)
            run = (run + 1) * matrix[:, column]
            changes = np.where(matrix[:, column], changes, 0)
            changing = changes > 0
            if changing.any():
                best = max(best, int(run[changing].max()))
    return best


def placements_near(candidate_points, other_points, dy, dx):
    """Number of placements within the distance of a placement of the same kind."""
    near = 0
    for kind, points in candidate_points.items():
        if kind not in other_points:
            continue
        shifted = points + np.array([dx, dy], dtype=np.float64)
        distance = np.abs(shifted[:, None, :] - other_points[kind][None, :, :]).max(axis=2)
        near += int((distance.min(axis=1) <= PLACEMENT_DISTANCE).sum())
    return near


class Metrics:
    def __init__(self):
        self.terrain = 0.0
        self.placement = 0.0
        self.near = 0
        self.run = 0

    def merge(self, other):
        self.terrain = max(self.terrain, other.terrain)
        self.placement = max(self.placement, other.placement)
        self.near = max(self.near, other.near)
        self.run = max(self.run, other.run)


def metrics_at(candidate, other, nonfiller, dy, dx):
    result = Metrics()
    mask, expected = identical_mask(candidate.grid, other.grid, dy, dx)
    if mask is not None and nonfiller > 0:
        result.terrain = max(0.0, float(mask.sum()) - expected) / nonfiller
        slices = overlap_slices(candidate.grid.shape, other.grid.shape, dy, dx)
        result.run = longest_run(mask, candidate.grid[slices[0]])
    result.near = placements_near(candidate.points, other.points, dy, dx)
    return result


# ---------------------------------------------------------------------------------------
# Alignment search
# ---------------------------------------------------------------------------------------

def pool(grid, factor):
    height, width = grid.shape
    padded_height = -(-height // factor) * factor
    padded_width = -(-width // factor) * factor
    stack = np.zeros((CLASS_COUNT - 1, padded_height, padded_width), dtype=np.float32)
    for cls in range(1, CLASS_COUNT):
        stack[cls - 1, :height, :width] = (grid == cls) * CLASS_WEIGHTS[cls]
    stack = stack.reshape(CLASS_COUNT - 1, padded_height // factor, factor,
                          padded_width // factor, factor)
    return stack.sum(axis=(2, 4))


def coarse_best(candidate_pool, other_pool):
    """Best coarse offset (dy, dx) and score for other shifted onto candidate."""
    ch, cw = candidate_pool.shape[1:]
    oh, ow = other_pool.shape[1:]
    shape = (ch + oh - 1, cw + ow - 1)
    first = np.fft.rfft2(candidate_pool, s=shape, axes=(1, 2))
    second = np.fft.rfft2(other_pool[:, ::-1, ::-1], s=shape, axes=(1, 2))
    correlation = np.fft.irfft2((first * second).sum(axis=0), s=shape)
    position = np.unravel_index(int(np.argmax(correlation)), correlation.shape)
    score = float(correlation[position])
    # out[i, j] pairs other[y + oh - 1 - i, x + ow - 1 - j] with candidate[y, x]
    return (oh - 1 - int(position[0]), ow - 1 - int(position[1])), score


def shift_votes(candidate_points, other_points):
    """Best (dy, dx) of other onto candidate by votes of equal placement kinds."""
    offsets = []
    for kind, points in candidate_points.items():
        if kind not in other_points:
            continue
        difference = other_points[kind][None, :, :] - points[:, None, :]
        offsets.append(np.rint(difference.reshape(-1, 2)).astype(np.int64))
    if not offsets:
        return None, 0
    offsets = np.concatenate(offsets)
    low = offsets.min(axis=0)
    span = offsets.max(axis=0) - low + 1
    histogram = np.bincount((offsets[:, 1] - low[1]) * span[0] + (offsets[:, 0] - low[0]),
                            minlength=int(span[0] * span[1])).reshape(span[1], span[0])
    padded = np.pad(histogram, 2)
    summed = np.zeros_like(histogram)
    for shift_y in range(5):
        for shift_x in range(5):
            summed += padded[shift_y:shift_y + span[1], shift_x:shift_x + span[0]]
    position = np.unravel_index(int(np.argmax(summed)), summed.shape)
    return (int(position[0] + low[1]), int(position[1] + low[0])), int(summed[position])


def compare_with(candidate, candidate_pool, nonfiller, transforms):
    """Metrics of the candidate against one other level over all its transforms."""
    result = Metrics()
    ranked = []
    voted = []
    decoyed = []
    for transform in transforms:
        offset, score = coarse_best(candidate_pool, pool(transform.grid, COARSE_FACTOR))
        ranked.append((score, offset, transform))
        shift, count = shift_votes(candidate.points, transform.points)
        if shift is not None:
            voted.append((count, shift, transform))
        shift, count = shift_votes(candidate.points, transform.decoy)
        if shift is not None:
            decoyed.append((count, shift, transform))
    ranked.sort(key=lambda entry: -entry[0])
    chance = 0
    for score, offset, transform in ranked[:COARSE_CANDIDATES]:
        best = None
        for dy in range(offset[0] * COARSE_FACTOR - COARSE_FACTOR,
                        offset[0] * COARSE_FACTOR + COARSE_FACTOR + 1):
            for dx in range(offset[1] * COARSE_FACTOR - COARSE_FACTOR,
                            offset[1] * COARSE_FACTOR + COARSE_FACTOR + 1):
                mask, expected = identical_mask(candidate.grid, transform.grid, dy, dx)
                if mask is None:
                    continue
                count = float(mask.sum()) - expected
                if best is None or count > best[0]:
                    best = (count, dy, dx)
        if best is not None:
            result.merge(metrics_at(candidate, transform, nonfiller, best[1], best[2]))
            chance = max(chance, placements_near(candidate.points, transform.decoy,
                                                 best[1], best[2]))
    voted.sort(key=lambda entry: -entry[0])
    for count, shift, transform in voted[:PLACEMENT_CANDIDATES]:
        result.merge(metrics_at(candidate, transform, nonfiller, shift[0], shift[1]))
    # What the same search finds in moved placements is chance, not similarity.
    decoyed.sort(key=lambda entry: -entry[0])
    for count, shift, transform in decoyed[:PLACEMENT_CANDIDATES]:
        chance = max(chance, placements_near(candidate.points, transform.decoy,
                                             shift[0], shift[1]))
    total = sum(len(points) for points in candidate.points.values())
    if total > 0:
        result.placement = max(0.0, result.near - chance - CHANCE_MARGIN) / total
    return result


# ---------------------------------------------------------------------------------------
# Texts
# ---------------------------------------------------------------------------------------

def build_word_index(texts):
    sequences = []
    index = {}
    for text in texts:
        words = words_of(text)
        sequences.append(words)
        for position, word in enumerate(words):
            index.setdefault(word, []).append((len(sequences) - 1, position))
    return sequences, index


def has_content(words):
    """True if the run has enough content words to be more than a stock phrase."""
    return sum(1 for word in words if word not in STOP_WORDS) >= MIN_CONTENT_WORDS


def longest_identical_words(texts, sequences, index):
    best = 0
    for text in texts:
        words = words_of(text)
        for position, word in enumerate(words):
            for sequence_id, other_position in index.get(word, []):
                other = sequences[sequence_id]
                length = 0
                while (position + length < len(words)
                       and other_position + length < len(other)
                       and words[position + length] == other[other_position + length]):
                    length += 1
                if (length > MAX_IDENTICAL_WORDS
                        and not has_content(words[position:position + length])):
                    length = MAX_IDENTICAL_WORDS
                best = max(best, length)
    return best


# ---------------------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------------------

def collect_sources(against):
    """Level files and config files below the folder, without rework folders."""
    levels = []
    configs = []
    seen = set()
    for directory, names, files in os.walk(against):
        names[:] = sorted(name for name in names if name != "rework")
        for name in sorted(files):
            if not (name.endswith(".level") or name == "Campaign.cfg"):
                continue
            path = os.path.join(directory, name)
            with open(path, "rb") as handle:
                digest = hashlib.sha1(handle.read()).hexdigest()
            if digest in seen:
                continue
            seen.add(digest)
            if name.endswith(".level"):
                levels.append(path)
            else:
                configs.append(path)
    return levels, configs


def check_files(files, against):
    """Returns the result lines and whether any level failed."""
    level_paths, config_paths = collect_sources(against)
    sources = []
    source_texts = []
    for path in level_paths:
        level = parse_level(path)
        if level is None:
            continue
        sources.append(level)
        source_texts.extend(level.texts)
    for path in config_paths:
        source_texts.extend(parse_config_texts(path))
    sequences, index = build_word_index(source_texts)
    source_transforms = [None] * len(sources)

    lines = []
    failed = False
    for path in files:
        if path.endswith(".cfg"):
            texts = parse_config_texts(path)
            words = longest_identical_words(texts, sequences, index)
            verdict = "PASS" if words <= MAX_IDENTICAL_WORDS else "FAIL"
            failed = failed or verdict == "FAIL"
            lines.append("%s %s terrain=- placement=- run=- words=%d" % (verdict, path, words))
            continue
        level = parse_level(path)
        if level is None:
            lines.append("SKIP %s (no tile data)" % path)
            continue
        candidate = transform_level(level, 0, 1.0)
        nonfiller = int((candidate.grid != CLASS_FILLER).sum())
        candidate_pool = pool(candidate.grid, COARSE_FACTOR)
        total = Metrics()
        worst = ""
        for position, other in enumerate(sources):
            if source_transforms[position] is None:
                source_transforms[position] = all_transforms(other)
            metrics = compare_with(candidate, candidate_pool, nonfiller,
                                   source_transforms[position])
            if metrics.terrain > total.terrain:
                worst = other.name
            total.merge(metrics)
        words = longest_identical_words(level.texts, sequences, index)
        small = os.path.basename(path) in SMALL_LEVEL_EXCEPTIONS
        placement_exempt = os.path.basename(path) in PLACEMENT_EXCEPTIONS
        verdict = "PASS"
        if ((not small and (total.terrain > MAX_TERRAIN_FRACTION or total.run > MAX_RUN_LENGTH))
                or (not placement_exempt and total.placement > MAX_PLACEMENT_FRACTION) or words > MAX_IDENTICAL_WORDS):
            verdict = "FAIL"
            failed = True
        note = " (small level: terrain and run not judged)" if small else ""
        if placement_exempt:
            note += " (own test level: placement not judged)"
        lines.append("%s %s terrain=%.1f%% placement=%.1f%% run=%d words=%d (closest: %s)%s"
                     % (verdict, path, total.terrain * 100.0, total.placement * 100.0,
                        total.run, words, worst, note))
    return lines, failed


def default_files():
    root = os.path.join(repository_root(), "levels")
    files = []
    for directory, names, names_files in os.walk(root):
        names.sort()
        for name in sorted(names_files):
            if name.endswith(".level") or name == "Campaign.cfg":
                files.append(os.path.join(directory, name))
    return files


# ---------------------------------------------------------------------------------------
# Self test with generated levels
# ---------------------------------------------------------------------------------------

def random_cave(width, height, seed):
    """A blob cave map as a list of (x, y, type, fullness) rows."""
    generator = random.Random(seed)
    solid = [[True] * width for _ in range(height)]
    for _ in range(14):
        cx = generator.randint(6, width - 7)
        cy = generator.randint(6, height - 7)
        radius = generator.randint(3, 6)
        for y in range(height):
            for x in range(width):
                if (x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2:
                    solid[y][x] = False
    rows = []
    for y in range(height):
        for x in range(width):
            if solid[y][x]:
                rows.append((x, y, TYPE_GOLD if (x * 7 + y * 3) % 23 == 0 else 3, 100))
            elif (x + y * 5) % 37 == 0:
                rows.append((x, y, TYPE_LAVA, 0))
            else:
                rows.append((x, y, 1, 0))
    return rows


def write_level(path, width, height, rows, markers, text):
    with open(path, "w") as handle:
        handle.write("[Info]\nName\tGenerated\nDescription\t%s\n[/Info]\n\n" % text)
        handle.write("[Tiles]\n# Map Size\n%d # MapSizeX\n%d # MapSizeY\n" % (width, height))
        for x, y, tile_type, fullness in rows:
            handle.write("%d\t%d\t%d\t%d\n" % (x, y, tile_type, fullness))
        handle.write("[/Tiles]\n\n[Traps]\n")
        for x, y in markers:
            handle.write("[Trap]\n1\tCannon_1\t1\t1\n%d\t%d\t1\n[/Trap]\n" % (x, y))
        handle.write("[/Traps]\n")


def self_test():
    failures = []
    work = tempfile.mkdtemp(prefix="level-similarity-selftest-")
    try:
        width = 60
        height = 50
        source_rows = random_cave(width, height, 1)
        markers = [(10, 12), (30, 20), (44, 33), (20, 40), (50, 8), (8, 30)]
        against = os.path.join(work, "private")
        os.makedirs(against)
        write_level(os.path.join(against, "source.level"), width, height, source_rows, markers,
                    "The first generated cave of the test series.")

        # the same map mirrored and shifted by three tiles in a larger map
        shifted_rows = [(width - 1 - x + 3, y + 3, t, f) for x, y, t, f in source_rows]
        shifted_markers = [(width - 1 - x + 3, y + 3) for x, y in markers]
        copy = os.path.join(work, "copy.level")
        write_level(copy, width + 6, height + 6, shifted_rows, shifted_markers,
                    "A mirrored cave that uses other words.")
        other = os.path.join(work, "other.level")
        write_level(other, width, height, random_cave(width, height, 2),
                    [(5, 5), (25, 14), (40, 40)], "Some second cavern with its own story.")

        lines, failed = check_files([copy], against)
        if not failed or not lines[0].startswith("FAIL"):
            failures.append("shifted and mirrored copy must fail: %s" % lines)
        lines, failed = check_files([other], against)
        if failed or not lines[0].startswith("PASS"):
            failures.append("unrelated map must pass: %s" % lines)
    finally:
        shutil.rmtree(work, ignore_errors=True)

    if failures:
        for failure in failures:
            sys.stderr.write("SELF-TEST FAIL: %s\n" % failure)
        return 1
    print("self-test passed")
    return 0


def main(argv):
    against = None
    files = []
    index = 1
    while index < len(argv):
        if argv[index] == "--self-test":
            return self_test()
        if argv[index] == "--against" and index + 1 < len(argv):
            against = argv[index + 1]
            index += 2
            continue
        files.append(argv[index])
        index += 1
    if against is None:
        against = default_against()
    if not os.path.isdir(against):
        print("check-level-similarity: skipped, folder %s does not exist" % against)
        return 0
    if not files:
        files = default_files()
    lines, failed = check_files(files, against)
    for line in lines:
        print(line)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
