#!/usr/bin/env python3
"""Static check of the room ambience data of the dungeon heart, portals, bridges and doors (no compiler needed).

    python source/tests/check_room_core_objects.py

Checks that config/roomAmbienceCoreObjects.cfg is included by config/roomAmbience.cfg, that it has the effects the group is
meant to have, that no effect name is used twice in the room ambience files, that the files and textures
named below exist and (for meshes) have a CREDITS line, and that the heart rate is read and used. The generic checks of every config
file (keys, meshes, particle systems, textures) are in tools/check_room_ambience.py.
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# Effects the config file must contain
EXPECTED = ("HeartGlow", "HeartSparks", "HeartBeat", "HeartVeins", "HeartFlare", "PortalSwirl", "PortalGlow", "PortalSparks", "PortalArrivalFlash", "PortalArrivalSparks", "WoodBridgeRock", "WoodBridgeDust", "WoodBridgeMotes", "WoodBridgeSway", "WoodBridgeSplinters", "StoneBridgePebbles", "StoneBridgeDust", "StoneBridgeGlint", "StoneBridgeCrumble", "BridgeLavaEmbers", "BridgeLavaPool", "BridgeWaterGlint", "BridgeWaterMist", "DoorLockGlint", "DoorFrameDust", "DoorCreak", "DoorPassing", "DoorPassingDust")
# Files below the repository root that the group brings
FILES = ("particles/RoomAmbienceCoreObjects.particle", "materials/scripts/RoomAmbienceCoreObjects.material", "source/render/RoomAmbienceExtras.h", "source/render/RoomAmbienceExtras.cpp")
# Meshes that need a CREDITS line
MESHES = ()


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

if "Include roomAmbienceCoreObjects.cfg" not in read("config", "roomAmbience.cfg"):
    problems.append("roomAmbience.cfg does not include roomAmbienceCoreObjects.cfg")

names = []
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", read("config", "roomAmbienceCoreObjects.cfg"), re.S):
    found = re.search(r"^\s*Name\s+(\S+)", block, re.M)
    if found is None:
        problems.append("an effect without a name in roomAmbienceCoreObjects.cfg")
    else:
        names.append(found.group(1))
for name in EXPECTED:
    if name not in names:
        problems.append("roomAmbienceCoreObjects.cfg lacks the effect %s" % name)

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

# The heart rate: the key is read, the beat is summed up, and the extras tell the rates after every scan
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
if 'key == "HeartRate"' not in config_cpp or "mHeartRate" not in read("source", "render", "RoomAmbienceConfig.h"):
    problems.append("the key HeartRate is not read from the config")
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("double RoomAmbience::getHeartRateFactor", "emitter.mCycle +=", "instance.mCycle +=",
               "mExtras.scan(*this, mGameMap"):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
extras = read("source", "render", "RoomAmbienceExtras.cpp")
for needle in ("ambience.clearHeartRates()", "ambience.addHeartRate(", "getHeartBadge()"):
    if needle not in extras:
        problems.append("RoomAmbienceExtras.cpp lacks %s" % needle)
if "render/RoomAmbienceExtras.cpp" not in read("CMakeLists.txt"):
    problems.append("CMakeLists.txt does not build RoomAmbienceExtras.cpp")

# Every effect of the heart that has HeartRate beats at a calm speed that is a number
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", read("config", "roomAmbienceCoreObjects.cfg"), re.S):
    if re.search(r"^\s*HeartRate\s+yes", block, re.M) and not re.search(r"^\s*Speed\s+[0-9.]+\s*$", block, re.M):
        problems.append("an effect with HeartRate has no Speed")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine (%d effects)" % len(names))
