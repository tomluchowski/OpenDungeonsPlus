#!/usr/bin/env python3
"""Static check of the cannon turn, its recoil clip and the dust and shake of the boulder trap (no compiler needed).

    python source/tests/check_cannon_boulder.py

Checks that the config reader knows the kind Turn, that the declarations in RoomAmbience.h match the
definitions in the source, that the turn pauses after a shot and is undone by stopAll, that the cannon
mesh is linked to its skeleton with the clip Triggered and plays it when the cannon fires, that the
effects exist in the trap config with the limits of tools/check_room_ambience.py, and that the
particle systems and the files of the new sprite exist.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts, binary=False):
    """Returns the text (or the bytes) of a file below the repository root."""
    if binary:
        with open(os.path.join(ROOT, *parts), "rb") as handle:
            return handle.read()
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

if 'words[1] == "Turn"' not in config_cpp or "AmbienceKind::turn" not in config_cpp:
    problems.append("config reader does not know Kind Turn")
if not re.search(r"^\s+turn[,\r\n]", config_h, re.M) and not re.search(r"^\s+turn\s*$", config_h, re.M):
    problems.append("AmbienceKind::turn missing")
if '"Turn"' not in checker:
    problems.append("check_room_ambience.py does not know Kind Turn")

for decl in ("updateTurrets", "restoreTurret"):
    if not re.search(r"\b%s\(" % decl, header):
        problems.append("%s not declared" % decl)
    if "RoomAmbience::%s(" % decl not in source:
        problems.append("%s not defined" % decl)
for needle in ("updateTurrets(dt);", "TURRET_HOLD_SECONDS", "mTurretHoldUntil", "mTurrets.clear();",
               "restoreTurret(it->second);", "Ogre::Node::TS_WORLD", "AmbienceKind::turn", "isAlliedSeat",
               "NEGATIVE_UNIT_Y"):
    if needle not in source:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
# The turn must continue from the orientation the node has, never store and rewrite one (the server aims the cannon)
turn = source[source.index("void turnNodeToward"):source.index("std::string getTileKey")]
if "setOrientation" in turn:
    problems.append("turnNodeToward must not overwrite the orientation")
fired = source[source.index("case TrapEffectKind::fired:", source.index("void RoomAmbience::notifyTrapEffect")):]
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

# The real boulder is the server missile (TrapBoulder::shoot); the client must not roll a second one
if "BoulderRoll" in effects:
    problems.append("BoulderRoll would roll a second boulder next to the server missile")
dust = effects.get("BoulderMissileDust")
if dust is None:
    problems.append("effect BoulderMissileDust missing")
elif dust.get("Match") != ["trap:BoulderMoving"] or dust.get("Target") != ["Object"] or dust.get("Kind") != ["Particle"]:
    problems.append("BoulderMissileDust must be a particle on moving boulders")
burst = effects.get("BoulderLaunchBurst")
if burst is None:
    problems.append("effect BoulderLaunchBurst missing")
elif burst.get("Match") != ["Boulder"] or burst.get("Event") != ["TrapFired"] or burst.get("Kind") != ["Particle"]:
    problems.append("BoulderLaunchBurst must be a particle on the event TrapFired of Boulder")
for effect in (dust, burst):
    if effect is not None and not re.search(r"^particle_system %s\s*$" % re.escape(effect["System"][0]), particles, re.M):
        problems.append("particle system %s missing" % effect["System"][0])
if '"BoulderMoving"' not in source or "GameEntityType::missileObject" not in source or 'getMeshName() == "Boulder"' not in source:
    problems.append("scanObjects does not give moving boulder missiles (mesh Boulder) the kind BoulderMoving")
if '"MissileMoving"' in source:
    problems.append("the kind MissileMoving would give the dust to cannonballs and the other missiles too")

shake = effects.get("BoulderRollShake")
if shake is None:
    problems.append("effect BoulderRollShake missing")
else:
    if shake.get("Kind") != ["Shake"] or shake.get("Event") != ["TrapFired"] or shake.get("Match") != ["Boulder"]:
        problems.append("BoulderRollShake must be a Shake on the event TrapFired of Boulder")
    if float(shake["Amount"][0]) > 0.5 or float(shake["Duration"][0]) > 2.0:
        problems.append("BoulderRollShake is too strong or too long")

# The sprite of the fragments exists with its material, its generator entry and a particle system that uses it
if not os.path.exists(os.path.join(ROOT, "materials", "textures", "RoomAmbGrain.png")):
    problems.append("texture RoomAmbGrain.png is missing")
if "material RoomAmbGrain" not in read("materials", "scripts", "RoomAmbienceProduction.material"):
    problems.append("material RoomAmbGrain is missing")
if '"RoomAmbGrain": sprite_grain' not in read("tools", "gen_room_ambience_art.py"):
    problems.append("the generator does not draw RoomAmbGrain")

# The cannon recoil clip: the mesh is linked to the skeleton, the skeleton has the clip, the cannon plays it
if b"Cannon.skeleton" not in read("models", "Cannon.mesh", binary=True):
    problems.append("Cannon.mesh has no link to Cannon.skeleton")
skeleton = read("models", "Cannon.skeleton", binary=True)
for name in (b"Triggered", b"Barrel", b"Root"):
    if name not in skeleton:
        problems.append("Cannon.skeleton lacks " + name.decode())
cannon = read("source", "traps", "TrapCannon.cpp")
shoot = cannon[cannon.index("bool TrapCannon::shoot"):cannon.index("TrapEntity* TrapCannon::getTrapEntity")]
if 'setAnimationState("Triggered", false)' not in shoot:
    problems.append("the cannon does not play its clip when it fires")
if "models/Cannon.skeleton" not in read("CREDITS"):
    problems.append("models/Cannon.skeleton is not in CREDITS")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
