#!/usr/bin/env python3
"""Static check of the room ambience data of the the temple bell, the converted flash and the hurt heart (no compiler needed).

    python source/tests/check_room_client_moments.py

Checks that config/roomAmbienceDeferred.cfg is included by config/roomAmbience.cfg, that it has the effects the group is
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
EXPECTED = ("TempleGongRing", "TempleBellSparks", "ConvertedFlash", "ConvertedRing", "ConvertedSparks", "HeartHurtPulse", "HeartHurtDrops", "HeartLowBeatSound")
# Files below the repository root that the group brings
FILES = ("sounds/Spatial/Rooms/Temple/Gong/Gong01.ogg", "materials/textures/RoomAmbSoftRing.png")
# Meshes that need a CREDITS line
MESHES = ("sounds/Spatial/Rooms/Temple/Gong/Gong01.ogg",)


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

if "Include roomAmbienceDeferred.cfg" not in read("config", "roomAmbience.cfg"):
    problems.append("roomAmbience.cfg does not include roomAmbienceDeferred.cfg")

names = []
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", read("config", "roomAmbienceDeferred.cfg"), re.S):
    found = re.search(r"^\s*Name\s+(\S+)", block, re.M)
    if found is None:
        problems.append("an effect without a name in roomAmbienceDeferred.cfg")
    else:
        names.append(found.group(1))
for name in EXPECTED:
    if name not in names:
        problems.append("roomAmbienceDeferred.cfg lacks the effect %s" % name)

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

# The key Sound is read and played with the particles of an event
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
if 'key == "Sound"' not in config_cpp or "mSound" not in read("source", "render", "RoomAmbienceConfig.h"):
    problems.append("the key Sound is not read from the config")
ambience = read("source", "render", "RoomAmbience.cpp")
if "playSound(effect.mSound, position)" not in ambience or "mExtras.reset();" not in ambience:
    problems.append("RoomAmbience.cpp does not play the sound of an event effect or does not reset the extras")
# The moments are started by the watcher, from what the client already knows
extras = read("source", "render", "RoomAmbienceExtras.cpp")
for needle in ('"TempleHealed"', '"ConvertedFlash"', '"HeartHurt"', '"HeartLowBeat"', "getOverlayHealthValue()", "getSeatPrison()",
               "mInitialized && ((clock - snapshot.mLastEvent) >= CREATURE_EVENT_COOLDOWN)", "fraction < HEART_HURT_FRACTION"):
    if needle not in extras:
        problems.append("RoomAmbienceExtras.cpp lacks %s" % needle)
# The low beat is a sound of the heart hit
if "Family      Rooms/Heart/Hit" not in read("config", "roomAmbienceDeferred.cfg"):
    problems.append("the low beat does not use the sound of the heart hit")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine (%d effects)" % len(names))
