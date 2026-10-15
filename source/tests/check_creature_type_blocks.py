"""Creature type blocks per seat: format, save path, spawn filter and the level data.

A creature class that cannot come to a seat is a "Block <seatId> <class>" line in the [Triggers]
section (written by the game into saves as well). The script action "available <seatId> <class>
<0|1>" blocks or releases a class later. Levels without such a line allow every type.

Static checks of the sources and the level files; the parser and writer round trip is compiled
and run by check_level_script.py (test_LevelScript.cpp), which is started here when a compiler is
available and reported as not runnable otherwise.
"""
from pathlib import Path
import re
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
failures = []


def text(path):
    return (repo / path).read_text(encoding="utf-8", errors="replace")


def expect(condition, message):
    if not condition:
        failures.append(message)


# Format: reading, writing, runtime access
script = text("source/gamemap/LevelScript.cpp")
expect('key == "Block"' in script, "the Block line is not read")
expect('os << "Block' + chr(92) + 't" << block.first' in script, "the Block line is not written")
expect("LevelScriptActionType::creatureAvailable" in script, "no script action to block or release a type")
runner = text("source/gamemap/LevelScriptRunner.cpp")
expect("setCreatureBlocked(action.mSeatId, action.mText, action.mNumber == 0)" in runner,
       "the available action does not change the block")
expect("getLevelScript().isEmpty()" in text("source/gamemap/MapHandler.cpp"),
       "saves do not write the script section")

# Effect: the portal pick filters blocked types
seat = text("source/game/Seat.cpp")
spawn = seat[seat.index("Seat::getNextFighterClassToSpawn"):]
spawn = spawn[:spawn.index("\n}\n")]
expect("isCreatureBlocked(getId(), def.first->getClassName())" in spawn, "the portal pick does not skip blocked types")
expect(spawn.index("isCreatureBlocked") < spawn.index("computePointsForSeat"),
       "the block is checked after the spawn conditions")
expect("getNextFighterClassToSpawn" in text("source/rooms/RoomPortal.cpp"), "the portal does not use the pick")
expect(len(re.findall(r"getNextFighterClassToSpawn", "".join(
    p.read_text(encoding="utf-8", errors="replace") for p in (repo / "source").rglob("*.cpp")
    if "tests" not in p.parts))) == 2, "another place picks fighter classes without the block")

# Data: old format loads (no Block line, no change), campaign blocks only name known keeper types
factions = text("config/factions.cfg")
start = factions.index("Name\tKeeper")
pool = [l.strip() for l in factions[factions.index("[SpawnPool]", start) + 11:factions.index("[/SpawnPool]", start)]
        .split("\n") if l.strip() and not l.strip().startswith("#")]
expect(len(pool) >= 12, "keeper spawn pool not found")
blocked_levels = 0
for path in sorted((repo / "levels").glob("*/*.level")):
    seat_players = {}
    current = None
    in_triggers = False
    seen_trigger = False
    blocks = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        fields = line.split()
        if line == "[Seat]":
            current = {}
        elif line == "[/Seat]":
            if current is not None and "id" in current:
                seat_players[current["id"]] = current.get("player")
            current = None
        elif current is not None and len(fields) == 2 and fields[0] in ("seatId", "player"):
            current["id" if fields[0] == "seatId" else "player"] = fields[1]
        elif line == "[Triggers]":
            in_triggers = True
        elif line == "[Trigger]":
            seen_trigger = True
        elif in_triggers and fields[:1] == ["Block"]:
            blocks.append((fields, seen_trigger))
    if not blocks:
        continue
    blocked_levels += 1
    names = set()
    for fields, late in blocks:
        expect(len(fields) == 3 and not late, "%s: misplaced Block line %s" % (path.name, fields))
        if len(fields) == 3:
            expect(fields[2] in pool, "%s: unknown creature type %s" % (path.name, fields[2]))
            expect(fields[1] in seat_players, "%s: Block names a missing seat" % path.name)
            expect((fields[1], fields[2]) not in names, "%s: duplicate Block %s" % (path.name, fields))
            names.add((fields[1], fields[2]))
    expect(len(names) < len(pool), "%s: every type is blocked" % path.name)
expect(blocked_levels >= 20, "expected the campaign levels to carry blocks, found %d" % blocked_levels)
expect("Block" not in text("levels/skirmish/TestLegacy.level"), "legacy level changed")

# Runtime round trip through the compiled script test
if shutil.which("cl") is None:
    print("check_creature_type_blocks: parser round trip not runnable (cl not in PATH), static checks only")
else:
    result = subprocess.run([sys.executable, str(repo / "source/tests/check_level_script.py")])
    expect(result.returncode == 0, "check_level_script.py failed")

if failures:
    for failure in failures:
        print("FAIL: " + failure)
    sys.exit(1)
print("check_creature_type_blocks: ok (%d levels carry blocks)" % blocked_levels)
