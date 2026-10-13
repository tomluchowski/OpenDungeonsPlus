#!/usr/bin/env python3
"""Progression check for campaign levels against a local folder of other level files.

The folder (default: ..\\OpenDungeonsPlus-private next to the main working tree) is never
part of the repository. If it does not exist the check is skipped and the exit code is 0,
so clones without it stay green. This script contains no level data: the assignment of
a campaign level to the level it was derived from is read from
<folder>\\rework\\progression-map.json at run time, a JSON object that maps the file name of
a campaign level (without ".level") to {"source": "<name of the source level>"}, to
{"source_file": "<path of the source level relative to the folder>"} (skirmish maps) or, for a
level that continues the one before it, to {"previous": "<file name of our level>"}. The
entry "source_dir" holds the folder with the source levels, relative to <folder>.

A rebuilt level keeps the unlocks of the level it was derived from: for the human keeper
seat the rooms, spells, traps and doors of the lists [SkillDone], [SkillPending] and
[SkillNotAllowed] have to be the same, and the skills that a script gives to that seat
("make") have to be the same. A level that continues the one before it is compared with
that level. Reward skills that no source list names are not compared.

Creature types are compared as well: a creature type is allowed for the human keeper seat
unless the level blocks it with a "Block <seatId> <class>" line of the [Triggers] section or
a script action "available <seatId> <class> 0" (an action with 1 allows it again). The types
that come through the portal of a keeper (config/factions.cfg) have to be allowed or blocked
exactly like in the level it is compared with.

One line per level: PASS or FAIL, the file and the differences.

Usage:
  python scripts/check-campaign-progression.py [--against <dir>] [files]
  python scripts/check-campaign-progression.py --self-test
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

PRIVATE_DIRECTORY_NAME = "OpenDungeonsPlus-private"
MAP_FILE = os.path.join("rework", "progression-map.json")
CAMPAIGN_DIRECTORY = os.path.join("levels", "campaign")

# FNV-1a hash (see source/utils/NameAliases.cpp) of an older skill name -> current name
SKILL_ALIASES = {
    0xfea599dbea82e978: "spellHexenHen",
    0xaf1772cc154463d5: "spellDefector",
    0xd93144790c52abbf: "trapDoorIronbound",
    0xa6a9ead8659e219d: "trapDoorRuned",
    0x979dc0f5d17eadfd: "trapWatchBanner",
}


def hash_name(name):
    value = 14695981039346656037
    for byte in name.lower().encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & 0xffffffffffffffff
    return value


def resolve_skill(name):
    return SKILL_ALIASES.get(hash_name(name), name)


def repository_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run_git(args, cwd):
    try:
        proc = subprocess.run(["git"] + args, cwd=cwd, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
    except OSError:
        return 1, ""
    return proc.returncode, proc.stdout.decode("utf-8", errors="replace")


def default_against():
    """The private folder next to the main working tree (not next to a worktree)."""
    root = repository_root()
    code, out = run_git(["rev-parse", "--git-common-dir"], root)
    if code == 0:
        common = os.path.abspath(os.path.join(root, out.strip()))
        root = os.path.dirname(common)
    return os.path.join(os.path.dirname(root), PRIVATE_DIRECTORY_NAME)


class Progression:
    """The unlock data of one level file."""

    def __init__(self):
        self.done = set()
        self.pending = set()
        self.not_allowed = set()
        self.makes = set()
        self.blocked = set()
        self.unblocked = set()
        self.has_human = False

    def state(self, skill):
        if skill in self.done:
            return "done"
        if skill in self.pending:
            return "pending"
        if skill in self.not_allowed:
            return "not allowed"
        return "unlisted"

    def listed(self):
        return self.done | self.pending | self.not_allowed


def parse_level(path):
    """Reads the seat blocks and the script actions of a level file."""
    result = Progression()
    seats = []
    seat = None
    section = None
    makes = []
    blocks = []
    availability = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.strip()
            if line == "[Seat]":
                seat = {"id": None, "player": None, "done": [], "pending": [], "na": []}
                seats.append(seat)
                section = None
            elif line == "[/Seat]":
                seat = None
                section = None
            elif seat is not None and line in ("[SkillDone]", "[SkillPending]",
                                               "[SkillNotAllowed]"):
                section = {"[SkillDone]": "done", "[SkillPending]": "pending",
                           "[SkillNotAllowed]": "na"}[line]
            elif line in ("[/SkillDone]", "[/SkillPending]", "[/SkillNotAllowed]"):
                section = None
            elif seat is not None and section is not None and line != "":
                seat[section].append(resolve_skill(line))
            elif seat is not None and line.startswith("seatId"):
                seat["id"] = int(line.split()[1])
            elif seat is not None and line.startswith("player"):
                seat["player"] = line.split()[1]
            else:
                fields = line.split()
                if len(fields) >= 4 and fields[0] == "Action" and fields[1] == "make":
                    makes.append((int(fields[2]), resolve_skill(fields[3])))
                elif len(fields) == 3 and fields[0] == "Block":
                    blocks.append((int(fields[1]), fields[2]))
                elif len(fields) == 5 and fields[0] == "Action" and fields[1] == "available":
                    availability.append((int(fields[2]), fields[3], fields[4]))
    humans = [s for s in seats if s["player"] == "Human"]
    if not humans:
        # skirmish maps leave the seats open ("Choice"): the first seat stands for the player
        humans = [s for s in seats if s["player"] == "Choice"][:1]
    if not humans:
        return result
    human = humans[0]
    result.has_human = True
    result.done = set(human["done"])
    result.pending = set(human["pending"])
    result.not_allowed = set(human["na"])
    result.makes = set(skill for (seat_id, skill) in makes if seat_id in (human["id"], -1))
    result.blocked = set(name for (seat_id, name) in blocks if seat_id == human["id"])
    for (seat_id, name, value) in availability:
        if seat_id == human["id"]:
            (result.unblocked if value == "1" else result.blocked).add(name)
    return result


def keeper_pool(root):
    """The creature classes that come through the portal of a keeper (config/factions.cfg)."""
    path = os.path.join(root, "config", "factions.cfg")
    if not os.path.isfile(path):
        return []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    start = text.find("Name\tKeeper")
    if start < 0:
        return []
    begin = text.find("[SpawnPool]", start)
    end = text.find("[/SpawnPool]", begin)
    if begin < 0 or end < 0:
        return []
    names = []
    for line in text[begin + len("[SpawnPool]"):end].split("\n"):
        line = line.strip()
        if line != "" and not line.startswith("#"):
            names.append(line)
    return names


def compare(reference, level):
    """The list of differences of the unlocks of a level and its reference."""
    differences = []
    for skill in sorted(reference.listed()):
        wanted = reference.state(skill)
        found = level.state(skill)
        if wanted != found:
            differences.append("%s: %s, expected %s" % (skill, found, wanted))
    for skill in sorted(reference.makes - level.makes):
        differences.append("%s: script does not make it available" % skill)
    for skill in sorted(level.makes - reference.makes):
        differences.append("%s: script makes it available but the source does not" % skill)
    return differences


def allowed_creatures(progression, pool):
    return [c for c in pool if c not in progression.blocked or c in progression.unblocked]


def compare_creatures(reference, level, pool):
    """The differences of the creature types that may come to the human keeper seat."""
    wanted = set(allowed_creatures(reference, pool))
    found = set(allowed_creatures(level, pool))
    differences = []
    for name in pool:
        if name in wanted and name not in found:
            differences.append("creature %s: blocked, expected allowed" % name)
        elif name in found and name not in wanted:
            differences.append("creature %s: allowed, expected blocked" % name)
    return differences


def load_map(against):
    path = os.path.join(against, MAP_FILE)
    if not os.path.isfile(path):
        return None
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def check_file(path, against, mapping, levels_dir):
    """Returns (line, failed) for one level file."""
    name = os.path.basename(path)
    base = name[:-len(".level")]
    entry = mapping.get(base)
    if entry is None or not isinstance(entry, dict):
        return "SKIP %s (not part of the rebuilt campaign)" % name, False
    if "source_file" in entry:
        source_path = os.path.join(against, entry["source_file"])
    elif "source" in entry:
        source_dir = os.path.join(against, mapping.get("source_dir", ""))
        source_path = os.path.join(source_dir, entry["source"] + ".level")
    else:
        source_path = os.path.join(levels_dir, entry["previous"] + ".level")
        earlier = mapping.get(entry["previous"])
        if not os.path.isfile(source_path) and isinstance(earlier, dict) and "source" in earlier:
            # the level before is not part of this checkout yet: compare with what it is made from
            source_dir = os.path.join(against, mapping.get("source_dir", ""))
            source_path = os.path.join(source_dir, earlier["source"] + ".level")
    if not os.path.isfile(source_path):
        return "SKIP %s (no level to compare with)" % name, False
    reference = parse_level(source_path)
    level = parse_level(path)
    if not reference.has_human or not level.has_human:
        return "SKIP %s (no human seat)" % name, False
    differences = compare(reference, level)
    pool = keeper_pool(repository_root())
    differences += compare_creatures(reference, level, pool)
    line = "%s %s unlocks: %d differences" % ("FAIL" if differences else "PASS", name,
                                              len(differences))
    if differences:
        line += "\n" + "\n".join("  " + text for text in differences)
    return line, bool(differences)


def default_files(levels_dir):
    files = []
    if os.path.isdir(levels_dir):
        for name in sorted(os.listdir(levels_dir)):
            if name.endswith(".level"):
                files.append(os.path.join(levels_dir, name))
    return files


def check_files(files, against, levels_dir):
    mapping = load_map(against)
    if mapping is None:
        return ["check-campaign-progression: skipped, %s does not exist" %
                os.path.join(against, MAP_FILE)], False
    lines = []
    failed = False
    for path in files:
        if not path.endswith(".level"):
            continue
        line, bad = check_file(path, against, mapping, levels_dir)
        lines.append(line)
        failed = failed or bad
    return lines, failed


# ---------------------------------------------------------------------------------------
# Self test with generated levels
# ---------------------------------------------------------------------------------------

def make_level(done, pending, not_allowed, make_lines=(), blocks=()):
    lines = ["[Seats]", "[Seat]", "seatId\t1", "player\tHuman", "[SkillDone]"]
    lines += done + ["[/SkillDone]", "[SkillNotAllowed]"] + not_allowed
    lines += ["[/SkillNotAllowed]", "[SkillPending]"] + pending + ["[/SkillPending]", "[/Seat]"]
    lines += ["[/Seats]", "[Triggers]"]
    for name in blocks:
        lines.append("Block	1	" + name)
    for skill in make_lines:
        lines += ["[Trigger]", "Name\tt", "Mode\tonce", "Cond\ttime\t1",
                  "Action\tmake\t1\t" + skill, "[/Trigger]"]
    lines.append("[/Triggers]")
    return "\n".join(lines) + "\n"


def self_test():
    failures = []
    work = tempfile.mkdtemp(prefix="progression-selftest-")
    try:
        private = os.path.join(work, "private")
        os.makedirs(os.path.join(private, "rework"))
        os.makedirs(os.path.join(private, "src"))
        levels = os.path.join(work, "levels")
        os.makedirs(levels)
        with open(os.path.join(private, MAP_FILE), "w") as handle:
            json.dump({"source_dir": "src", "One": {"source": "S1"},
                       "Two": {"previous": "One"}, "Three": {"source": "S1"}}, handle)

        def write(path, text):
            with open(path, "w") as handle:
                handle.write(text)

        write(os.path.join(private, "src", "S1.level"),
              make_level(["roomA"], ["roomB"], ["roomC"], ["roomB"]))
        write(os.path.join(levels, "One.level"),
              make_level(["roomA"], ["roomB"], ["roomC"], ["roomB"]))
        write(os.path.join(levels, "Two.level"),
              make_level(["roomA"], ["roomB"], ["roomC"], ["roomB"]))
        write(os.path.join(levels, "Three.level"),
              make_level(["roomA", "roomC"], ["roomB"], [], ["roomB", "roomC"]))
        lines, failed = check_files([os.path.join(levels, n) for n in
                                     ("One.level", "Two.level")], private, levels)
        if failed or not all(line.startswith("PASS") for line in lines):
            failures.append("equal levels: %s" % lines)
        lines, failed = check_files([os.path.join(levels, "Three.level")], private, levels)
        if not failed or "roomC" not in lines[0]:
            failures.append("different level not reported: %s" % lines)
        pool = keeper_pool(repository_root())
        if len(pool) >= 2:
            write(os.path.join(private, "src", "S1.level"),
                  make_level(["roomA"], ["roomB"], ["roomC"], ["roomB"], pool[:1]))
            write(os.path.join(levels, "One.level"),
                  make_level(["roomA"], ["roomB"], ["roomC"], ["roomB"], pool[:1]))
            write(os.path.join(levels, "Two.level"),
                  make_level(["roomA"], ["roomB"], ["roomC"], ["roomB"], pool[:1]))
            write(os.path.join(levels, "Three.level"),
                  make_level(["roomA"], ["roomB"], ["roomC"], ["roomB"], pool[1:2]))
            lines, failed = check_files([os.path.join(levels, "One.level"),
                                         os.path.join(levels, "Two.level")], private, levels)
            if failed or not all(line.startswith("PASS") for line in lines):
                failures.append("equal creature blocks: %s" % lines)
            lines, failed = check_files([os.path.join(levels, "Three.level")], private, levels)
            if (not failed or "creature %s: blocked, expected allowed" % pool[1] not in lines[0] or
                    "creature %s: allowed, expected blocked" % pool[0] not in lines[0]):
                failures.append("different creature blocks not reported: %s" % lines)
        lines, failed = check_files([os.path.join(levels, "One.level")],
                                    os.path.join(work, "missing"), levels)
        if failed or "skipped" not in lines[0]:
            failures.append("missing folder not skipped: %s" % lines)
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
        print("check-campaign-progression: skipped, folder %s does not exist" % against)
        return 0
    levels_dir = os.path.join(repository_root(), CAMPAIGN_DIRECTORY)
    if not files:
        files = default_files(levels_dir)
    lines, failed = check_files(files, against, levels_dir)
    for line in lines:
        print(line)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
