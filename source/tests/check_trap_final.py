#!/usr/bin/env python3
"""Static check of the cannon turn and the rolling boulder with its view shake (no compiler needed).

    python source/tests/check_trap_final.py

Checks that the config reader knows the kinds Roll and Turn and the keys Mesh and EndSystem, that the
declarations in RoomAmbience.h match the definitions in the source, that the turn pauses after a shot and is
undone by stopAll, that the effects exist in the trap config with the limits of tools/check_room_ambience.py,
that the particle systems exist and that the guard banner still never fires.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


problems = []
config_h = read("source", "render", "RoomAmbienceConfig.h")
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
header = read("source", "render", "RoomAmbience.h")
source = read("source", "render", "RoomAmbience.cpp")
traps = read("config", "roomAmbienceTraps.cfg")
particles = read("particles", "RoomAmbienceTraps.particle")
checker = read("tools", "check_room_ambience.py")

for word, name in (("Roll", "roll"), ("Turn", "turn")):
    if 'words[1] == "%s"' % word not in config_cpp or "AmbienceKind::%s" % name not in config_cpp:
        problems.append("config reader does not know Kind %s" % word)
    if not re.search(r"^\s+%s[,\r\n]" % name, config_h, re.M):
        problems.append("AmbienceKind::%s missing" % name)
    if '"%s"' % word not in checker:
        problems.append("check_room_ambience.py does not know Kind %s" % word)
for key, member in (("Mesh", "mMesh"), ("EndSystem", "mEndSystem")):
    if 'key == "%s"' % key not in config_cpp or member not in config_h:
        problems.append("config key %s not read" % key)
    if '"%s"' % key not in checker:
        problems.append("check_room_ambience.py does not know the key %s" % key)

for decl in ("updateTurrets", "restoreTurret", "startRoll", "updateRollers", "finishRoller"):
    if not re.search(r"\b%s\(" % decl, header):
        problems.append("%s not declared" % decl)
    if "RoomAmbience::%s(" % decl not in source:
        problems.append("%s not defined" % decl)
for needle in ("updateTurrets(dt);", "updateRollers(dt);", "TURRET_HOLD_SECONDS", "mTurretHoldUntil",
               "mTurrets.clear();", "mRollers.clear();", "restoreTurret(it->second);", "finishRoller(roller, false);",
               "Ogre::Node::TS_WORLD", "AmbienceKind::turn", "AmbienceKind::roll", "isAlliedSeat",
               "NEGATIVE_UNIT_Y", "MAX_ROLLERS"):
    if needle not in source:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
# The turn must continue from the orientation the node has, never store and rewrite one (the server aims the cannon)
turn = source[source.index("void turnNodeToward"):source.index("double hashPhase")]
if "setOrientation" in turn:
    problems.append("turnNodeToward must not overwrite the orientation")
fired = source[source.index("case 0:", source.index("void RoomAmbience::notifyTrapEffect")):]
if fired.index("mTurretHoldUntil") > fired.index('triggerEvent("TrapFired"'):
    problems.append("the hold of the turn must be set before TrapFired is shown")

effects = {}
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", traps, re.S):
    entry = {}
    for line in block.splitlines():
        words = line.split("#", 1)[0].split()
        if len(words) >= 2:
            entry[words[0]] = words[1:]
    if "Name" in entry:
        effects[entry["Name"][0]] = entry

turn_effect = effects.get("CannonTurn")
if turn_effect is None:
    problems.append("effect CannonTurn missing")
else:
    if turn_effect.get("Match") != ["trap:Cannon"] or turn_effect.get("Kind") != ["Turn"] or turn_effect.get("Target") != ["Object"]:
        problems.append("CannonTurn must be a Turn on trap:Cannon objects")
    if not 0.0 < float(turn_effect["Speed"][0]) <= 45.0:
        problems.append("the cannon must turn slowly")
    if turn_effect.get("Reduced") != ["yes"]:
        problems.append("the turn of the cannon should stay in the mode reduced")

roll = effects.get("BoulderRoll")
if roll is None:
    problems.append("effect BoulderRoll missing")
else:
    if roll.get("Match") != ["Boulder"] or roll.get("Event") != ["TrapFired"] or roll.get("Kind") != ["Roll"]:
        problems.append("BoulderRoll must be a Roll on the event TrapFired of Boulder")
    for key in ("Mesh", "System", "EndSystem"):
        if key not in roll:
            problems.append("BoulderRoll lacks %s" % key)
    if not os.path.exists(os.path.join(ROOT, "models", roll["Mesh"][0] + ".mesh")):
        problems.append("mesh of BoulderRoll missing")
    for key in ("System", "EndSystem"):
        if not re.search(r"^particle_system %s\s*$" % re.escape(roll[key][0]), particles, re.M):
            problems.append("particle system %s missing" % roll[key][0])

shake = effects.get("BoulderRollShake")
if shake is None:
    problems.append("effect BoulderRollShake missing")
else:
    if shake.get("Kind") != ["Shake"] or shake.get("Event") != ["TrapFired"] or shake.get("Match") != ["Boulder"]:
        problems.append("BoulderRollShake must be a Shake on the event TrapFired of Boulder")
    if float(shake["Amount"][0]) > 0.5 or float(shake["Duration"][0]) > 2.0:
        problems.append("BoulderRollShake is too strong or too long")

# The boulder keeps its sounds and particles from before
for name in ("TrapBoulderFireSound", "TrapBoulderReloadSound", "TrapBoulderIdleSound", "BoulderEmptyNiche"):
    if name not in effects:
        problems.append("effect %s was removed" % name)

# The guard banner never fires, so it has no reload look and nothing was added for it
banner = read("source", "traps", "TrapWatchBanner.h")
if "bool shoot(Tile* tile) override" not in banner or "return false;" not in banner:
    problems.append("the guard banner no longer overrides shoot() with false")
if re.search(r"trap:WatchBanner\s+When\s+Reloading", traps) or any(
        e.get("Match") == ["trap:WatchBanner"] and e.get("When") == ["Reloading"] for e in effects.values()):
    problems.append("the guard banner has a reload look")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
