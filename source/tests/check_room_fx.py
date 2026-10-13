#!/usr/bin/env python3
"""Static check of the room ambience data of the casino results, the cheering arena and the heart at low health (no compiler needed).

    python source/tests/check_room_fx.py

Checks that config/roomAmbienceRoomFx.cfg is included by config/roomAmbience.cfg, that it has the effects the group is
meant to have, that no effect name is used twice in the room ambience files, that the files and textures
named below exist and (for meshes) have a CREDITS line. The generic checks of every config
file (keys, meshes, particle systems, textures) are in tools/check_room_ambience.py.
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# Effects the config file must contain
EXPECTED = ("HeartLowFlare", "HeartLowPulse", "HeartLowBurst", "HeartLowCritical", "HeartLowPulseHard", "CasinoWinCoins", "CasinoWinFlash", "CasinoWinRain", "CasinoWinSound", "CasinoLossSmoke", "CasinoLossSparks", "CasinoLossGroan", "ArenaCheerConfetti", "ArenaCheerSound")
# Files below the repository root that the group brings
FILES = ("particles/RoomAmbienceRoomFx.particle", "sounds/Spatial/Rooms/Casino/Win/FxCasinoWin01.ogg", "sounds/Spatial/Rooms/Casino/Loss/FxCasinoLoss01.ogg", "sounds/Spatial/Rooms/Arena/Cheer/FxArenaCheer01.ogg")
# Meshes that need a CREDITS line
MESHES = ("sounds/Spatial/Rooms/Casino/Win/FxCasinoWin01.ogg", "sounds/Spatial/Rooms/Casino/Loss/FxCasinoLoss01.ogg", "sounds/Spatial/Rooms/Arena/Cheer/FxArenaCheer01.ogg")


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

if "Include roomAmbienceRoomFx.cfg" not in read("config", "roomAmbience.cfg"):
    problems.append("roomAmbience.cfg does not include roomAmbienceRoomFx.cfg")

names = []
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", read("config", "roomAmbienceRoomFx.cfg"), re.S):
    found = re.search(r"^\s*Name\s+(\S+)", block, re.M)
    if found is None:
        problems.append("an effect without a name in roomAmbienceRoomFx.cfg")
    else:
        names.append(found.group(1))
for name in EXPECTED:
    if name not in names:
        problems.append("roomAmbienceRoomFx.cfg lacks the effect %s" % name)

# An effect name is used once in all room ambience files (the log and the check use it to tell effects apart)
seen = {}
for path in sorted(glob.glob(os.path.join(ROOT, "config", "roomAmbience*.cfg"))):
    for found in re.finditer(r"^\s*Name\s+(\S+)", open(path, encoding="utf-8", errors="replace").read(), re.M):
        other = seen.setdefault(found.group(1), os.path.basename(path))
        if other != os.path.basename(path):
            problems.append("effect %s is in %s and in %s" % (found.group(1), other, os.path.basename(path)))

for path in FILES:
    if not os.path.exists(os.path.join(ROOT, *path.split("/"))):
        problems.append("missing file %s" % path)

credits = read("CREDITS")
for mesh in MESHES:
    if mesh not in credits:
        problems.append("no CREDITS line for %s" % mesh)

# The condition LowHealth: read from the config, decided from the heart badge of the local keeper
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
for needle in ('text == "LowHealth"', 'key == "Below"', "AmbienceWhen::lowHealth"):
    if needle not in config_cpp:
        problems.append("RoomAmbienceConfig.cpp lacks %s" % needle)
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("bool RoomAmbience::isLocalHeartBelow", "AmbienceWhen::lowHealth", "badge.mFraction", "badge.mHP < 0.0"):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
# Every LowHealth effect is on the heart and has a Below that is a fraction
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", read("config", "roomAmbienceRoomFx.cfg"), re.S):
    if re.search(r"^\s*When\s+LowHealth", block, re.M):
        found = re.search(r"^\s*Below\s+(\S+)", block, re.M)
        if found is None or not 0.0 < float(found.group(1)) < 1.0:
            problems.append("a LowHealth effect has no Below between 0 and 1")
        if "DungeonTempleObject" not in block:
            problems.append("a LowHealth effect is not on the dungeon heart")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine (%d effects)" % len(names))
