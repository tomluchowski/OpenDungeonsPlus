"""Static checks of the sandbox start state: only the starting rooms and the worker summon spell are
researched, the other rooms become available over time, spells and traps are researched in the library.

Run from any directory. It does not start a game.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
sandbox = (root / "levels/skirmish/Sandbox.level").read_text(encoding="utf-8")
everything = (root / "levels/skirmish/SandboxEverything.level").read_text(encoding="utf-8")
header = (root / "source/gamemap/SandboxMode.h").read_text(encoding="utf-8")
code = (root / "source/gamemap/SandboxMode.cpp").read_text(encoding="utf-8")
rooms_cfg = (root / "config/rooms.cfg").read_text(encoding="utf-8")
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def skills_done(level):
    seat = level[level.index("[Seat]"):level.index("[/Seat]")]
    block = seat[seat.index("[SkillDone]"):seat.index("[/SkillDone]")]
    return [line.strip() for line in block.splitlines()[1:] if line.strip()]


start = skills_done(sandbox)
check(start == ["roomTreasury", "roomDormitory", "roomHatchery", "spellSummonWorker"],
      "Sandbox.level must start with Treasury, Dormitory, Hatchery and the worker summon spell only: %s" % start)

# The unlock order of the code
order_block = code[code.index("ROOM_UNLOCK_ORDER["):]
order_block = order_block[order_block.index("{"):order_block.index("};")]
order = re.findall(r"SkillType::(room\w+)", order_block)
check(len(order) == len(set(order)), "the room unlock order has a room twice")
check("NB_UNLOCK_ROOMS = %d;" % len(order) in code, "NB_UNLOCK_ROOMS does not match the unlock order (%d rooms)" % len(order))
check(not set(order) & set(start), "a room of the unlock order is already researched at the start")
check(set(order) | set(start) >= {"roomLibrary", "roomWorkshop", "roomPrison", "roomTorture", "roomTemple",
                                  "roomCrypt", "roomCasino", "roomArena", "roomBridgeStone", "roomBridgeWooden",
                                  "roomTrainingHall", "roomGuardRoom"},
      "a base room is neither a start room nor in the unlock order")

# The variant with everything available has no room left to unlock and no room timer to wait for
check(set(order) <= set(skills_done(everything)), "SandboxEverything.level must give every room of the unlock order")

# The interval is a config key with a positive value, read by the code
match = re.search(r"^\s*SandboxRoomUnlockIntervalSeconds\s+(\d+(?:\.\d+)?)\s*$", rooms_cfg, re.M)
check(match is not None and float(match.group(1)) > 0, "rooms.cfg needs a positive SandboxRoomUnlockIntervalSeconds")
check('"SandboxRoomUnlockIntervalSeconds"' in code, "SandboxMode.cpp does not read SandboxRoomUnlockIntervalSeconds")

check("void updateRoomUnlocks();" in header, "SandboxMode.h does not declare updateRoomUnlocks")

# The unlock goes through the seat, which sends the usual notice, and runs on the server turn
check("seat->addSkill(ROOM_UNLOCK_ORDER[" in code, "the unlock must use Seat::addSkill")
check("updateRoomUnlocks();" in code[code.index("void SandboxMode::doTurn()"):], "doTurn must call updateRoomUnlocks")

if failures:
    print("\n".join(failures))
    sys.exit(1)
print("sandbox start checks passed")
