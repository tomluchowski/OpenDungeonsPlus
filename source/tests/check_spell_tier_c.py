#!/usr/bin/env python3
"""Static check of the spell tier C effects: ring textures, view shake, ground marks, end of possession.

    python source/tests/check_spell_tier_c.py

No compiler is needed. Checks that no network message was added
(trapEffect is the last value), that the new kinds are wired from the config
to the code, that the shake is only applied while a frame is rendered and taken away again, that every
texture exists with a material and a CREDITS entry, and that every event of the config is raised.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

# 1. Existing network values keep their order: only timeLimit and campaignVictory follow trapEffect.
header = read("source", "network", "ServerNotification.h")
body = header[header.index("enum class ServerNotificationType"):]
body = body[:body.index("};")]
names = re.findall(r"^\s*([A-Za-z_]\w*)\s*,?\s*(?://.*)?$", body, re.M)
if names[-3:] != ["trapEffect", "timeLimit", "campaignVictory"]:
    problems.append("trapEffect is not followed by exactly timeLimit and campaignVictory")

# 2. Config kinds
config_h = read("source", "render", "RoomAmbienceConfig.h")
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
kinds = re.search(r"enum class AmbienceKind\s*\{(.*?)\};", config_h, re.S).group(1)
order = re.findall(r"^\s*(\w+),?\s*$", re.sub(r"//.*", "", kinds), re.M)
if order != ["particle", "motion", "clip", "shake", "mark", "sound", "roll", "turn", "beam", "projectile"]:
    problems.append("AmbienceKind values changed: %s" % order)
for word, name in (("Shake", "shake"), ("Mark", "mark")):
    if 'words[1] == "%s"' % word not in config_cpp or "AmbienceKind::%s" % name not in config_cpp:
        problems.append("kind %s is not read from the config" % word)
if '"MaxMarks"' not in config_cpp:
    problems.append("MaxMarks is not read")

# 3. Code
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("AmbienceKind::shake", "AmbienceKind::mark", "void RoomAmbience::applyShake", "void RoomAmbience::clearShake",
               "mMarks.push_back", "getMaxMarks()"):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
apply_part = ambience[ambience.index("void RoomAmbience::applyShake"):ambience.index("void RoomAmbience::clearShake")]
if "getGamePaused" not in apply_part:
    problems.append("the shake must not be applied while the game is paused")
if "mShakeApplied = offset" not in apply_part:
    problems.append("applyShake does not remember the offset")
listener = read("source", "render", "ODFrameListener.cpp")
started = listener[listener.index("bool ODFrameListener::frameStarted"):]
if "mRoomAmbience->applyShake()" not in started:
    problems.append("frameStarted does not apply the shake")
ended = listener[listener.index("bool ODFrameListener::frameEnded"):listener.index("bool ODFrameListener::frameStarted")]
if "mRoomAmbience->clearShake()" not in ended:
    problems.append("frameEnded does not clear the shake")
if "stopAll" in ambience and "clearShake();" not in ambience[ambience.index("void RoomAmbience::stopAll"):ambience.index("void RoomAmbience::update")]:
    problems.append("stopAll does not clear the shake")

# 4. End of possession uses the existing message
client = read("source", "network", "ODClient.cpp")
end = client[client.index("case ServerNotificationType::possessionEnd:"):]
end = end[:end.index("default:")]
if 'POSSESSION_END_EVENT = "SpellFxPossessEnd"' not in client or "POSSESSION_END_EVENT" not in end or "displayText" not in end:
    problems.append("possessionEnd shows no effect or message")
if end.index("POSSESSION_END_EVENT") > end.index("setPossessedCreatureName"):
    problems.append("the effect must be raised before the possessed name is cleared")

# 5. Config events, textures, materials, credits
cfg = read("config", "roomAmbienceSpells.cfg")
events = set(re.findall(r"^\s*Event\s+(\S+)", cfg, re.M))
raised = set("SpellFx" + name for name in re.findall(r'fireSpellEffect\([^;]*?"(\w+)"', "".join(
    read("source", "spells", f) for f in os.listdir(os.path.join(ROOT, "source", "spells")) if f.endswith(".cpp"))))
raised.add("SpellFxPossessEnd")
for event in ("SpellFxTremor", "SpellFxExplosion", "SpellFxInferno", "SpellFxLightning", "SpellFxPossessEnd"):
    if event not in events:
        problems.append("config has no effect for %s" % event)
    if event not in raised:
        problems.append("nothing raises %s" % event)
for kind in ("Shake", "Mark"):
    if not re.search(r"^\s*Kind\s+%s\s*$" % kind, cfg, re.M):
        problems.append("no %s effect in the spell config" % kind)

materials = read("materials", "scripts", "RoomAmbienceSpells.material")
credits = read("CREDITS")
particles = read("particles", "RoomAmbienceSpells.particle")
textures = ("RoomAmbRing", "RoomAmbRingRunes", "RoomAmbRingDust", "RoomAmbMarkScorch", "RoomAmbMarkCracks", "RoomAmbMarkBolt")
for name in textures:
    if not os.path.exists(os.path.join(ROOT, "materials", "textures", name + ".png")):
        problems.append("texture %s.png is missing" % name)
    if not re.search(r"material %s\b" % name, materials) or ("texture %s.png" % name) not in materials:
        problems.append("material %s is missing or has another texture" % name)
    if ("materials/textures/%s.png" % name) not in credits:
        problems.append("CREDITS has no entry for %s.png" % name)
    if ("material        %s" % name) not in particles:
        problems.append("no particle system uses %s" % name)
if "tools/gen_spell_marks.py" not in credits:
    problems.append("CREDITS has no entry for tools/gen_spell_marks.py")
# The ring textures are white where they are transparent: plain "add" ignores the alpha and draws a bright square
for name in ("RoomAmbRing", "RoomAmbRingRunes"):
    start = materials.find("material %s\n" % name)
    block = materials[start:materials.find("\nmaterial ", start + 1)] if start >= 0 else ""
    if "scene_blend src_alpha one" not in block:
        problems.append("material %s has to blend with src_alpha one (no bright square)" % name)

# 6. The limit of one-shot systems is unchanged and the marks have their own limit
settings = read("config", "roomAmbience.cfg")
if not re.search(r"^\s*MaxOneShots\s+16\s*$", settings, re.M):
    problems.append("MaxOneShots is not 16")
if not re.search(r"^\s*MaxMarks\s+\d+\s*$", settings, re.M):
    problems.append("MaxMarks is not set")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
