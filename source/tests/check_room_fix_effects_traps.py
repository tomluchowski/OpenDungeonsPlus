#!/usr/bin/env python3
"""Static check of the room ambience data of the cannon, spike trap, lightning trap and the damaged doors (no compiler needed).

    python source/tests/check_room_fix_effects_traps.py

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
EXPECTED = ("HeartHitBurst", "HeartHitFlash", "HeartHitPulse", "HeartHitSound", "WaveRumbleDust", "WaveSurgeLight", "WaveSurgeShockwave", "WaveSurgeShake", "WaveSurgeSound", "CryptRaiseEarth", "CryptRaiseClods", "CryptRaiseSpirit", "CryptRaiseShake", "CryptRaiseSound", "PrisonerDoorDust", "PrisonerDoorSparks", "PrisonerDoorClang", "TortureConvertFlash", "TortureConvertShackles", "TortureConvertSparks", "TortureConvertSound", "CannonMuzzleFlash", "CannonSmokeRing", "SpikeShootDust", "LightningTrapBolt", "LightningTrapBoltFork", "WatchBannerWave", "WatchBannerDroop", "WatchBannerFlare", "WatchBannerSparks", "WatchBannerAlertSound", "RuneDoorCracks", "RuneDoorFlickerRunes", "RuneDoorFlicker", "SteelDoorHitDust", "SteelDoorDent", "SteelDoorRebound", "SecretDoorOpenGlow", "SpellHandCastSpark")
# Files below the repository root that the group brings
FILES = ("particles/RoomAmbienceFixEffects.particle",)
# Meshes that need a CREDITS line
MESHES = ()


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

# The shots and the damage of the doors use the events of the trap messages that exist, and a trap event is matched by
# the type of the trap or door
used = {}
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", read("config", "roomAmbienceFixEffects.cfg"), re.S):
    found = re.search(r"^\s*Name\s+(\S+)", block, re.M)
    event = re.search(r"^\s*Event\s+(\S+)", block, re.M)
    if found is not None and event is not None:
        used[found.group(1)] = event.group(1)
for name, event in (("CannonMuzzleFlash", "TrapFired"), ("SpikeShootDust", "TrapFired"), ("LightningTrapBolt", "TrapFired"),
                    ("RuneDoorCracks", "DoorHurt"), ("SteelDoorHitDust", "DoorHit"), ("SecretDoorOpenGlow", "DoorOpen")):
    if used.get(name) != event:
        problems.append("%s should be shown on the event %s" % (name, event))
# The bolt of the lightning trap is a beam with the bolt mesh of the spells
bolt = re.search(r"Name\s+LightningTrapBolt\s.*?\[/Effect\]", read("config", "roomAmbienceFixEffects.cfg"), re.S)
if bolt is None or "Kind        Beam" not in bolt.group(0) or "SpellBolt" not in bolt.group(0):
    problems.append("LightningTrapBolt is not a beam of the bolt mesh")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine (%d effects)" % len(names))
