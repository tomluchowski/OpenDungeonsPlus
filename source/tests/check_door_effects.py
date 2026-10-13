#!/usr/bin/env python3
"""Static check of the door effects of the room ambience (no compiler needed).

    python source/tests/check_door_effects.py

Checks that the doors send their damage to the clients, that the client keeps the hit and destroyed states,
shows the remains of a destroyed door with the clip of its own skeleton (the barricade falls into a heap, every
other door breaks), takes the length of the collapse from the clip, shows the effects of a secret door only to
its keeper, and that every door type has the looks the config promises with particle systems that exist.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


problems = []
ambience = read("source", "render", "RoomAmbience.cpp")
header = read("source", "render", "RoomAmbience.h")
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
config_h = read("source", "render", "RoomAmbienceConfig.h")
traps = read("config", "roomAmbienceTraps.cfg")
particles = "".join(read("particles", name) for name in os.listdir(os.path.join(ROOT, "particles"))
                    if name.startswith("RoomAmbience") and name.endswith(".particle"))

# The server: every hit door tells the clients how much health it has left, a destroyed one says so
door = read("source", "traps", "TrapDoor.cpp")
damage = door[door.index("double TrapDoor::takeDamage"):door.index("TrapDoor::TrapDoor(")]
for needle in ("TrapEffectKind::doorHit", "TrapEffectKind::doorWrecked", "getDefaultTileHP()", "damageDone > 0.0"):
    if needle not in damage:
        problems.append("TrapDoor::takeDamage lacks %s" % needle)
if "TrapEffectKind::fired" not in door[door.index("bool TrapDoor::shoot"):]:
    problems.append("the runed door does not tell the clients when it fires")

# The client: states, the wreck and the clip length
for needle in ("mHitUntil", "mWreckedUntil", "AmbienceWhen::hit", "AmbienceWhen::locked", "getAnimationStateName()",
               "startCollapse(tileX, tileY, typeName)", "updateCollapses(dt);", "destroyCollapse(collapse)",
               '"TrapBuilt"', '"TrapSold"', '"DoorHit"', '"DoorHurt"', '"DoorWrecked"'):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
for needle in ("void startCollapse(", "void updateCollapses(", "void destroyCollapse(", "std::string mClip;"):
    if needle not in header:
        problems.append("RoomAmbience.h lacks %s" % needle)
update = ambience[ambience.index("void RoomAmbience::updateCollapses"):ambience.index("void RoomAmbience::destroyCollapse")]
if "getAnimationState(it->mClip)->getLength()" not in update:
    problems.append("updateCollapses does not take the clip length from the skeleton")
start = ambience[ambience.index("void RoomAmbience::startCollapse"):ambience.index("void RoomAmbience::updateCollapses")]
if "BARRICADE_TYPE) ? COLLAPSE_CLIP : DESTROYED_CLIP" not in start:
    problems.append("startCollapse does not pick Collapse for the barricade and Destroyed for every other door")
if "hasAnimationState(clipName)" not in start:
    problems.append("startCollapse does not check the clip on the ghost copy of the door mesh")
if "destroyCollapse(collapse)" not in ambience[ambience.index("void RoomAmbience::stopAll"):ambience.index("void RoomAmbience::update(")]:
    problems.append("stopAll does not remove the collapsing doors")
wrecked = ambience[ambience.index("case TrapEffectKind::doorWrecked:", ambience.index("void RoomAmbience::notifyTrapEffect")):]
wrecked = wrecked[:wrecked.index("break;")]
if "startCollapse(tileX, tileY, typeName)" not in wrecked or "mWreckedUntil" not in wrecked:
    problems.append("doorWrecked does not keep the state and start the wreck")

# Keys of the config
if 'key == "OwnerOnly"' not in config_cpp or "mOwnerOnly" not in config_h:
    problems.append("the config reader does not read OwnerOnly")
for word in ("Hit", "Locked"):
    if 'text == "%s"' % word not in config_cpp:
        problems.append("the config reader does not know When %s" % word)
if "effect.mOwnerOnly" not in ambience or "entity->getSeat() != localPlayer->getSeat()" not in ambience:
    problems.append("scanObjects does not filter OwnerOnly effects by the seat")

# The effects
effects = {}
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", traps, re.S):
    entry = {}
    for line in block.splitlines():
        words = line.split("#", 1)[0].split()
        if len(words) >= 2:
            entry[words[0]] = words[1:]
    if "Name" in entry:
        effects[entry["Name"][0]] = entry

for name in ("SecretDoorGlint", "SecretDoorDust", "SecretDoorLockGlow"):
    if effects.get(name, {}).get("OwnerOnly") != ["yes"]:
        problems.append("the effect %s is not OwnerOnly (it would give the secret door away)" % name)
for name in ("DoorHitSplinters", "DoorHitSparks", "DoorHitDust", "DoorHitRunes", "DoorHurtDust", "DoorWreckedDust",
             "DoorWreckedSplinters", "DoorWreckedSparks"):
    if effects.get(name, {}).get("Target") != ["Event"] or effects[name].get("OwnerOnly") is not None:
        problems.append("%s must be an event for everybody" % name)
shake = effects.get("DoorShake")
if shake is None or shake.get("When") != ["Hit"] or shake.get("Kind") != ["Motion"]:
    problems.append("DoorShake must be a motion while a door is hit")
lock = effects.get("DoorLockGlow")
if lock is None or lock.get("When") != ["Locked"]:
    problems.append("DoorLockGlow must show while a door is locked")
for door_type in ("DoorIronbound", "DoorSteel", "DoorBarricade", "DoorSecret", "DoorRuned"):
    if not any(("trap:" + door_type) in effect.get("Match", []) for effect in effects.values()):
        problems.append("%s has no look of its own" % door_type)
for effect in effects.values():
    system = effect.get("System", [None])[0]
    if system is not None and not re.search(r"^particle_system %s\s*$" % re.escape(system), particles, re.M):
        problems.append("particle system %s of %s is missing" % (system, effect["Name"][0]))

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
