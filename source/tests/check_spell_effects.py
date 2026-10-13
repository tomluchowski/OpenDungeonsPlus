#!/usr/bin/env python3
"""Static check of the spell effects: bolts and flying charges, the own looks of heal, haste, defense and
slow, the ends of the defector, hen and possession spells, and the spark of the keeper's hand (no compiler needed).

    python source/tests/check_spell_effects.py

Checks that no network message was added, that the kinds Beam and Projectile are wired from the config to the
code, that the bolt mesh, its material and texture exist with CREDITS entries, that the server reports the
spell effects with the prefix "SpellFx/" and the client starts the matching events, that the end of the defector
and hen spells and of a possession is shown, that heal, haste, defense and slow raise their own events with
config effects, and that the hand spark is only sent for a spell the server accepted.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

# 1. No new network message: trapEffect is still the last value (the spell effects use the sound message)
header = read("source", "network", "ServerNotification.h")
body = header[header.index("enum class ServerNotificationType"):]
body = body[:body.index("};")]
names = re.findall(r"^\s*([A-Za-z_]\w*)\s*,?\s*(?://.*)?$", body, re.M)
if names[-1] != "trapEffect":
    problems.append("trapEffect is not the last server notification")

# 2. Beam and projectile are wired
config_h = read("source", "render", "RoomAmbienceConfig.h")
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
kinds = re.search(r"enum class AmbienceKind\s*\{(.*?)\};", config_h, re.S).group(1)
order = re.findall(r"^\s*(\w+),?\s*$", re.sub(r"//.*", "", kinds), re.M)
if order[-2:] != ["beam", "projectile"]:
    problems.append("AmbienceKind order changed: %s" % order)
for word in ("Beam", "Projectile", "Mesh", "Land", "From", "MaxFlights"):
    if '"%s"' % word not in config_cpp:
        problems.append("%s is not read from the config" % word)
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("void RoomAmbience::startFlight", "void RoomAmbience::updateFlights", "void RoomAmbience::destroyFlight",
               "updateFlights(dt)", "getMaxFlights()", "destroyFlight(flight)", "mFlights.clear()", "mLand",
               "void RoomAmbience::noteHandCast"):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
stop = ambience[ambience.index("void RoomAmbience::stopAll"):ambience.index("void RoomAmbience::update(")]
if "destroyFlight" not in stop or "mFlights.clear()" not in stop:
    problems.append("stopAll does not remove the flights")
update = ambience[ambience.index("void RoomAmbience::updateFlights"):ambience.index("void RoomAmbience::destroyFlight")]
if "triggerEvent(arrival.first" not in update:
    problems.append("a landing projectile does not raise its Land event")
if update.index("triggerEvent(arrival.first") < update.index("it = mFlights.erase(it)"):
    problems.append("Land events must be raised after the loop over the flights (the list changes)")

# 3. Bolt mesh, material, texture, rings, credits
credits = read("CREDITS")
if not os.path.exists(os.path.join(ROOT, "models", "SpellBolt.mesh")):
    problems.append("models/SpellBolt.mesh is missing")
material = read("materials", "scripts", "RoomAmbienceSpells.material")
for name, texture in (("SpellBolt", "SpellBolt.png"), ("RoomAmbRing", "RoomAmbRing.png"),
                      ("RoomAmbRingRunes", "RoomAmbRingRunes.png")):
    if not re.search(r"^material %s\b" % name, material, re.M) or ("texture %s" % texture) not in material:
        problems.append("material %s is missing or has another texture" % name)
    if not os.path.exists(os.path.join(ROOT, "materials", "textures", texture)):
        problems.append("%s is missing" % texture)
for needle in ("models/SpellBolt.mesh", "materials/textures/SpellBolt.png", "tools/gen_spell_bolt.py",
               "tools/blender-assets/spell_bolt.py", "particles/RoomAmbienceSpellsExtra.particle",
               "materials/textures/RoomAmbRing.png", "materials/textures/RoomAmbRingRunes.png", "tools/gen_spell_marks.py"):
    if needle not in credits:
        problems.append("CREDITS has no entry for %s" % needle)
with open(os.path.join(ROOT, "models", "SpellBolt.mesh"), "rb") as handle:
    if b"SpellBolt" not in handle.read():
        problems.append("SpellBolt.mesh does not name the material SpellBolt")
for line in credits.splitlines():
    if line.startswith("models/SpellBolt.mesh") and "AI-generated" not in line:
        problems.append("the bolt mesh must be marked AI-generated")

# 4. The server reports the effects with the prefix SpellFx/, the client starts the events
spell_h = read("source", "spells", "Spell.h")
spell_cpp = read("source", "spells", "Spell.cpp")
if "static void fireSpellEffect(" not in spell_h or "void Spell::fireSpellEffect(" not in spell_cpp:
    problems.append("Spell::fireSpellEffect is missing")
if 'SPELL_EFFECT_PREFIX = "SpellFx/"' not in spell_cpp or "SPELL_EFFECT_PREFIX + effectName" not in spell_cpp:
    problems.append("the effects do not travel with the prefix SpellFx/")
client = read("source", "network", "ODClient.cpp")
sound_case = client[client.index("case ServerNotificationType::playSpatialSound:"):]
sound_case = sound_case[:sound_case.index("case ServerNotificationType::playRelativeSound:")]
for needle in ("Spell::SPELL_EFFECT_PREFIX", '"SpellFx" + family.substr(', "noteHandCast"):
    if needle not in sound_case:
        problems.append("the client does not handle the spell effects: %s is missing" % needle)
server = read("source", "network", "ODServer.cpp")
cast = server[server.index("case ClientNotificationType::askCastSpell:"):]
cast = cast[:cast.index("case ClientNotificationType::askPossessMove:")]
if "HAND_CAST_EFFECT_NAME" not in cast or cast.index("HAND_CAST_EFFECT_NAME") < cast.index("SpellManager::castSpell("):
    problems.append("the hand spark must only be sent after the server accepted the spell")

# 5. Own events of heal, haste, defense and slow, the three flying spells and the ends
server_code = "".join(read("source", "spells", f) for f in sorted(os.listdir(os.path.join(ROOT, "source", "spells")))
                      if f.endswith(".cpp"))
server_code += read("source", "creatureeffect", "CreatureEffectHexenHen.cpp") + \
    read("source", "creatureeffect", "CreatureEffectDefector.cpp") + \
    "".join(read("source", "creatureskill", f) for f in ("CreatureSkillHasteSelf.cpp", "CreatureSkillHealSelf.cpp",
                                                         "CreatureSkillDefenseSelf.cpp"))
for effect in ("Heal", "Haste", "Defense", "Slow", "Lightning", "Explosion", "Inferno", "DefectorEnd", "HenEnd"):
    if 'fireSpellEffect(' not in server_code or ('"%s"' % effect) not in server_code:
        problems.append("the server does not raise the spell effect %s" % effect)
fx_cfg = read("config", "roomAmbienceSpellsFx.cfg")
cfg_all = fx_cfg + read("config", "roomAmbienceSpells.cfg") + read("config", "roomAmbienceFixEffects.cfg")
for event in ("Heal", "Haste", "Defense", "Slow"):
    block = re.compile(r"Event\s+SpellFx" + event + r"\s*\n\s*Kind\s+Particle\s*\n\s*System\s+(\S+)")
    if len(set(block.findall(cfg_all))) < 3:
        problems.append("SpellFx%s has less than 3 particle effects (cast marker, area, impact)" % event)
for event in ("SpellFxLightning", "SpellFxExplosion", "SpellFxInferno"):
    if not re.search(r"Event\s+%s\s*\n\s*Kind\s+(Beam|Projectile)" % event, fx_cfg):
        problems.append("%s has no beam or projectile" % event)
if not re.search(r"Land\s+SpellFxExplosionLand", fx_cfg) or not re.search(r"Event\s+SpellFxExplosionLand", cfg_all):
    problems.append("explosion projectile has no landing event with effects")
for event in ("SpellFxDefectorEnd", "SpellFxHenEnd", "SpellFxPossessLost", "SpellFxPossessEnd", "SpellFxHandCast"):
    if event not in cfg_all:
        problems.append("config has no effect for %s" % event)

defector = read("source", "creatureeffect", "CreatureEffectDefector.cpp")
if 'fireSpellEffect(*posTile, "DefectorEnd"' not in defector or "changeSeat(originalSeat)" not in defector:
    problems.append("the end of the defector effect shows nothing")
if defector.index("changeSeat(originalSeat)") > defector.index('fireSpellEffect(*posTile, "DefectorEnd"'):
    problems.append("the defector end effect must come after the seat change")

# 6. The end of a possession: effect, message, fallen creature taken from the server message
end = client[client.index("case ServerNotificationType::possessionEnd:"):]
end = end[:end.index("case ServerNotificationType::trapEffect:")]
for needle in ("creatureLost", "POSSESSION_LOST_EVENT", "POSSESSION_END_EVENT", "displayText"):
    if needle not in end:
        problems.append("possessionEnd lacks %s" % needle)
if "packetReceived >> creatureLost" not in end or "isAlive()" in end:
    problems.append("possessionEnd must take the fallen creature from the server message, not guess it")
if end.index("triggerEvent(") > end.index("setPossessedCreatureName"):
    problems.append("the effect must be raised before the possessed name is cleared")

# 7. Settings
settings = read("config", "roomAmbience.cfg")
if not re.search(r"^\s*MaxFlights\s+\d+\s*$", settings, re.M):
    problems.append("MaxFlights is not set")
for name in ("roomAmbienceSpells.cfg", "roomAmbienceSpellsFx.cfg"):
    if name not in settings:
        problems.append("roomAmbience.cfg does not include %s" % name)

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
