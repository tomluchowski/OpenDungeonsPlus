#!/usr/bin/env python3
"""Static check of the trap and door sounds of the room ambience (no compiler needed).

    python source/tests/check_trap_door_sounds.py

Checks that every sound family of the config has a sound file, that every sound file of the traps and doors is
used by the config and has a CREDITS line, that the config reader knows the new keys, that the declarations in the
headers match the definitions in the sources, that doors report opening and closing, and that the sounds of a
secret door are only heard by its keeper.
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


problems = []
config = read("config", "roomAmbienceTraps.cfg")
families = set(re.findall(r"^\s*Family\s+(\S+)", config, re.M))
for family in sorted(families):
    folder = os.path.join(ROOT, "sounds", "Spatial", *family.split("/"))
    if not glob.glob(os.path.join(folder, "*.ogg")):
        problems.append("no sound file for family %s" % family)

credits = read("CREDITS")
for kind in ("Traps", "Doors"):
    for path in sorted(glob.glob(os.path.join(ROOT, "sounds", "Spatial", kind, "*", "*", "*.ogg"))):
        relative = os.path.relpath(path, ROOT).replace(os.sep, "/")
        family = "/".join(relative.split("/")[2:5])
        # The shot of the cannon is a sound of the server, which has its own CREDITS entry
        if family == "Traps/Cannon/Fire":
            continue
        if family not in families:
            problems.append("sound %s is not used by the config" % relative)
        if relative not in credits:
            problems.append("sound %s has no CREDITS line" % relative)
        with open(path, "rb") as handle:
            if handle.read(4) != b"OggS":
                problems.append("%s is not an Ogg file" % relative)

# Every trap has a shot, an idle and a reload sound (the cannon fires with a sound of the server), the banner
# has an idle and a call sound
for name in ("Spike", "Boulder", "Alarm", "Fear", "Gas", "Lightning", "Fireburst", "Freeze", "Trigger"):
    for role in ("Fire", "Idle", "Reload"):
        if "Traps/%s/%s" % (name, role) not in families:
            problems.append("trap %s has no %s sound in the config" % (name, role))
for role in ("Idle", "Reload"):
    if "Traps/Cannon/%s" % role not in families:
        problems.append("the cannon has no %s sound in the config" % role)
for role in ("Idle", "Fire"):
    if "Traps/WatchBanner/%s" % role not in families:
        problems.append("the watch banner has no %s sound in the config" % role)
for name in ("Wooden", "Ironbound", "Steel", "Secret", "Runed"):
    for role in ("Open", "Close", "Hit", "Break"):
        if "Doors/%s/%s" % (name, role) not in families:
            problems.append("door %s has no %s sound in the config" % (name, role))
for role in ("Hit", "Break"):
    if "Doors/Barricade/%s" % role not in families:
        problems.append("barricade has no %s sound in the config" % role)
for family in ("Doors/Door/Sold", "Traps/Trap/Sold"):
    if family not in families:
        problems.append("no sold sound %s in the config" % family)

# The sounds of a secret door are heard by its keeper only (they would give the door away)
effects = {}
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", config, re.S):
    entry = {}
    for line in block.splitlines():
        words = line.split("#", 1)[0].split()
        if len(words) >= 2:
            entry[words[0]] = words[1:]
    if "Name" in entry:
        effects[entry["Name"][0]] = entry
for name in ("DoorSecretOpenSound", "DoorSecretCloseSound"):
    if effects.get(name, {}).get("OwnerOnly") != ["yes"]:
        problems.append("the effect %s is not OwnerOnly" % name)

reader = read("source", "render", "RoomAmbienceConfig.cpp")
for text in ('"Sound"', '"Family"', '"Delay"'):
    if text not in reader:
        problems.append("the config reader does not know %s" % text)

ambience_h = read("source", "render", "RoomAmbience.h")
ambience = read("source", "render", "RoomAmbience.cpp")
for text in ("void playSound(", "void updatePendingSounds();", "mPendingSounds;", "const Seat* owner = nullptr"):
    if text not in ambience_h:
        problems.append("RoomAmbience.h lacks %s" % text)
for text in ("void RoomAmbience::playSound(", "void RoomAmbience::updatePendingSounds()", "updatePendingSounds();",
             "AmbienceKind::sound", "effect.mOwnerOnly && !forced", "mPendingSounds.clear();"):
    if text not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % text)
if '#include "sound/SoundEffectsManager.h"' not in ambience:
    problems.append("RoomAmbience.cpp does not include the sound manager")

door_h = read("source", "entities", "DoorEntity.h")
door = read("source", "entities", "DoorEntity.cpp")
if "setAnimationState(const std::string& state" not in door_h or "using MovableGameEntity::setAnimationState;" not in door_h:
    problems.append("DoorEntity.h does not declare the setAnimationState override")
if "void DoorEntity::setAnimationState(" not in door or '"DoorOpen"' not in door or '"DoorClose"' not in door:
    problems.append("DoorEntity.cpp does not raise DoorOpen and DoorClose")
if "getSeat()" not in door[door.index("void DoorEntity::setAnimationState"):door.index("std::string DoorEntity::getListenerName")]:
    problems.append("the door events do not pass the owner")

if problems:
    for problem in problems:
        print("PROBLEM: " + problem)
    sys.exit(1)
print("fine")
