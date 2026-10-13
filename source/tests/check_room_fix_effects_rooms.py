#!/usr/bin/env python3
"""Static check of the room ambience data of the dungeon heart hits, wave portal, crypt, prison and torture chamber (no compiler needed).

    python source/tests/check_room_fix_effects_rooms.py

Checks that config/roomAmbienceFixEffects.cfg is included by config/roomAmbience.cfg, that it has the effects the group is
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
EXPECTED = ("HeartHitBurst", "HeartHitFlash", "HeartHitPulse", "HeartHitSound", "WaveRumbleDust", "WaveSurgeLight", "WaveSurgeShockwave", "WaveSurgeShake", "WaveSurgeSound", "CryptRaiseEarth", "CryptRaiseClods", "CryptRaiseSpirit", "CryptRaiseShake", "CryptRaiseSound", "PrisonerDoorDust", "PrisonerDoorSparks", "PrisonerDoorClang", "TortureConvertFlash", "TortureConvertShackles", "TortureConvertSparks", "TortureConvertSound", "WatchBannerWave", "WatchBannerDroop", "WatchBannerFlare", "WatchBannerSparks", "WatchBannerAlertSound", "SpellHandCastSpark")
# Files below the repository root that the group brings
FILES = ("particles/RoomAmbienceFixEffects.particle", "sounds/Spatial/Rooms/Heart/Hit/FxHeartHit01.ogg", "sounds/Spatial/Rooms/WavePortal/Surge/FxWavePortalSurge01.ogg", "sounds/Spatial/Rooms/Crypt/Raise/FxCryptRaise01.ogg", "sounds/Spatial/Rooms/Prison/Clang/FxPrisonClang01.ogg", "sounds/Spatial/Rooms/Torture/Shackles/FxTortureShackles01.ogg")
# Meshes that need a CREDITS line
MESHES = ("sounds/Spatial/Rooms/Heart/Hit/FxHeartHit01.ogg", "sounds/Spatial/Rooms/WavePortal/Surge/FxWavePortalSurge01.ogg", "sounds/Spatial/Rooms/Crypt/Raise/FxCryptRaise01.ogg", "sounds/Spatial/Rooms/Prison/Clang/FxPrisonClang01.ogg", "sounds/Spatial/Rooms/Torture/Shackles/FxTortureShackles01.ogg")


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

if "Include roomAmbienceFixEffects.cfg" not in read("config", "roomAmbience.cfg"):
    problems.append("roomAmbience.cfg does not include roomAmbienceFixEffects.cfg")

names = []
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", read("config", "roomAmbienceFixEffects.cfg"), re.S):
    found = re.search(r"^\s*Name\s+(\S+)", block, re.M)
    if found is None:
        problems.append("an effect without a name in roomAmbienceFixEffects.cfg")
    else:
        names.append(found.group(1))
for name in EXPECTED:
    if name not in names:
        problems.append("roomAmbienceFixEffects.cfg lacks the effect %s" % name)

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

# The events of the effects are raised by the scans
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("void RoomAmbience::scanHeartHit", "scanHeartHit();", "mHitUntil[getTileKey(tileX, tileY)]", '"HeartHit"',
               '"CryptRaised"', '"PrisonerArrived"', '"TortureConverted"', "mLastHeartHP = -1.0;", "isInContainment()"):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
# The heart is hit when its health goes down: no hit for an unknown health or a health that did not fall
scan = ambience[ambience.index("void RoomAmbience::scanHeartHit"):ambience.index("void RoomAmbience::scanCreatureEvents")]
for needle in ("hp < 0.0", "previous < 0.0", "hp >= previous"):
    if needle not in scan:
        problems.append("scanHeartHit does not test %s" % needle)
used = set(re.findall(r"^\s*Event\s+(\S+)", read("config", "roomAmbienceFixEffects.cfg"), re.M))
for event in ("HeartHit", "CryptRaised", "PrisonerArrived", "TortureConverted"):
    if event not in used:
        problems.append("no effect for the event %s" % event)

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine (%d effects)" % len(names))
