#!/usr/bin/env python3
"""Static check of the final spell effects: bolts and flying charges, own sounds, end of possession and defector.

    python source/tests/check_spell_final.py

No compiler is needed. Checks that no network message was added, that the kinds Beam and Projectile are wired
from the config to the code, that the bolt mesh, its material and texture exist with CREDITS entries, that every
spell has an own cast sound family (with .ogg and CREDITS entry) that the server uses, that the possession aura
is ended on the client, that the end of the defector spell is shown, and that heal, haste, defense and slow raise
their own events with config effects.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

# 1. No new network message: trapEffect is followed only by the later values timeLimit and campaignVictory.
header = read("source", "network", "ServerNotification.h")
body = header[header.index("enum class ServerNotificationType"):]
body = body[:body.index("};")]
names = re.findall(r"^\s*([A-Za-z_]\w*)\s*,?\s*(?://.*)?$", body, re.M)
if names[-3:] != ["trapEffect", "timeLimit", "campaignVictory"]:
    problems.append("trapEffect is not followed by exactly timeLimit and campaignVictory")

# 2. Beam and projectile are wired
config_h = read("source", "render", "RoomAmbienceConfig.h")
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
kinds = re.search(r"enum class AmbienceKind\s*\{(.*?)\};", config_h, re.S).group(1)
order = re.findall(r"^\s*(\w+),?\s*$", re.sub(r"//.*", "", kinds), re.M)
if order != ["particle", "motion", "clip", "shake", "mark", "sound", "roll", "turn", "beam", "projectile"]:
    problems.append("AmbienceKind order changed: %s" % order)
for word in ("Beam", "Projectile", "Mesh", "Land", "From", "MaxFlights"):
    if '"%s"' % word not in config_cpp:
        problems.append("%s is not read from the config" % word)
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("void RoomAmbience::startFlight", "void RoomAmbience::updateFlights", "void RoomAmbience::destroyFlight",
               "updateFlights(dt)", "getMaxFlights()", "destroyFlight(flight)", "mFlights.clear()", "mLand"):
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

# 3. Bolt mesh, material, texture, credits
credits = read("CREDITS")
if not os.path.exists(os.path.join(ROOT, "models", "SpellBolt.mesh")):
    problems.append("models/SpellBolt.mesh is missing")
material = read("materials", "scripts", "RoomAmbienceSpells.material")
if not re.search(r"^material SpellBolt\b", material, re.M) or "texture SpellBolt.png" not in material:
    problems.append("material SpellBolt is missing or has another texture")
if not os.path.exists(os.path.join(ROOT, "materials", "textures", "SpellBolt.png")):
    problems.append("SpellBolt.png is missing")
for needle in ("models/SpellBolt.mesh", "materials/textures/SpellBolt.png", "tools/gen_spell_bolt.py",
               "tools/blender-assets/spell_bolt.py", "particles/RoomAmbienceSpellsExtra.particle"):
    if needle not in credits:
        problems.append("CREDITS has no entry for %s" % needle)
mesh = open(os.path.join(ROOT, "models", "SpellBolt.mesh"), "rb").read()
if b"SpellBolt" not in mesh:
    problems.append("SpellBolt.mesh does not name the material SpellBolt")
for line in credits.splitlines():
    if line.startswith("models/SpellBolt.mesh") and "AI-generated" not in line:
        problems.append("the bolt mesh must be marked AI-generated")

# 4. Own cast sounds of all spells, used by the server
spell_files = "".join(read("source", "spells", f) for f in sorted(os.listdir(os.path.join(ROOT, "source", "spells")))
                      if f.endswith(".cpp"))
server_code = spell_files + read("source", "creatureeffect", "CreatureEffectHexenHen.cpp") + \
    read("source", "creatureeffect", "CreatureEffectDefector.cpp") + \
    "".join(read("source", "creatureskill", f) for f in ("CreatureSkillHasteSelf.cpp", "CreatureSkillHealSelf.cpp",
                                                         "CreatureSkillDefenseSelf.cpp"))
casts = ("SummonWorkerCast", "CallToWarCast", "HealCast", "ExplosionCast", "HasteCast", "DefenseCast", "SlowCast",
         "StrengthCast", "WeakCast", "EyeEvilCast", "GoldCast", "LightningCast", "TremorCast", "DefectorCast",
         "HexenHenCast", "InfernoCast", "PossessCast", "ChampionCast")
if len(casts) != 18:
    problems.append("expected 18 spells")
others = ("ExplosionImpact", "LightningImpact", "TremorImpact", "InfernoImpact", "GoldImpact", "HealImpact",
          "HasteImpact", "PossessEnd", "DefectorEnd", "HexenHenEnd")
for family in casts + others:
    path = os.path.join(ROOT, "sounds", "Spatial", "Spells", family, "Fx%s01.ogg" % family)
    if not os.path.exists(path) or os.path.getsize(path) < 1000:
        problems.append("sound %s is missing or too small" % family)
    if ("sounds/Spatial/Spells/%s/Fx%s01.ogg" % (family, family)) not in credits:
        problems.append("CREDITS has no entry for sound %s" % family)
for family in casts + ("DefectorEnd", "HexenHenEnd"):
    if '"%s"' % family not in server_code:
        problems.append("the server does not play the sound %s" % family)
fx_cfg = read("config", "roomAmbienceSpellsFx.cfg")
for family in others:
    if family in ("DefectorEnd", "HexenHenEnd"):
        continue
    if ("Spells/%s" % family) not in fx_cfg:
        problems.append("config has no effect that plays %s" % family)

# 5. Each spell has a distinct sound (the files differ)
digests = {}
for family in casts + others:
    path = os.path.join(ROOT, "sounds", "Spatial", "Spells", family, "Fx%s01.ogg" % family)
    if os.path.exists(path):
        data = open(path, "rb").read()
        if data in digests:
            problems.append("sounds %s and %s are identical" % (family, digests[data]))
        digests[data] = family

# 6. Possession aura ends with the spell
spell_possess = read("source", "spells", "SpellPossess.cpp")
if not re.search(r"addParticleEffect\(Creature::POSSESSION_AURA_SCRIPT,\s*-1\)", spell_possess):
    problems.append("the possession aura must last until the possession ends")
creature = read("source", "entities", "Creature.cpp")
end_possession = creature[creature.index("void Creature::endPossession"):creature.index("void Creature::formPossessionGroup")]
if "endParticleEffectsByScript(POSSESSION_AURA_SCRIPT)" not in end_possession:
    problems.append("the server does not end the possession aura")
# The aura is permanent on the clients, a refresh never removes a missing effect and the turn deletes the ended effect
# before the refresh of the turn is built: the other keepers are told at once, while the effect is still there
if "fireCreatureRefreshIfNeeded()" not in end_possession or end_possession.index("fireCreatureRefreshIfNeeded()") < end_possession.index("endParticleEffectsByScript"):
    problems.append("endPossession must send the refresh with the ended aura at once")
if "mPacket << lost" not in end_possession or "const bool lost = !isAlive() || isKo() || !getIsOnMap()" not in end_possession:
    problems.append("the server must tell the client whether the creature fell")
client = read("source", "network", "ODClient.cpp")
end = client[client.index("case ServerNotificationType::possessionEnd:"):]
end = end[:end.index("case ServerNotificationType::trapEffect:")]
if 'POSSESSION_END_EVENT = "SpellFxPossessEnd"' not in client or 'POSSESSION_LOST_EVENT = "SpellFxPossessLost"' not in client:
    problems.append("the possession end events are not named SpellFxPossessEnd and SpellFxPossessLost")
for needle in ("endParticleEffectsByScript(Creature::POSSESSION_AURA_SCRIPT)", "creatureLost", "POSSESSION_LOST_EVENT",
               "POSSESSION_END_EVENT"):
    if needle not in end:
        problems.append("possessionEnd lacks %s" % needle)
if "packetReceived >> creatureLost" not in end or "isAlive()" in end:
    problems.append("possessionEnd must take the fallen creature from the server message, not guess it")
if end.index("POSSESSION_END_EVENT") > end.index("setPossessedCreatureName"):
    problems.append("the effect must be raised before the possessed name is cleared")
entity = read("source", "entities", "GameEntity.cpp")
part = entity[entity.index("uint32_t GameEntity::endParticleEffectsByScript"):entity.index("void GameEntity::restoreEntityState")]
if "rrEntityRemoveParticleEffect" not in part or "getIsOnServerMap()" not in part:
    problems.append("endParticleEffectsByScript must remove on the client and let the server effect run out")

# 7. Defector end
defector = read("source", "creatureeffect", "CreatureEffectDefector.cpp")
if 'fireSpellEffect(*posTile, "DefectorEnd"' not in defector or "changeSeat(originalSeat)" not in defector:
    problems.append("the end of the defector effect shows nothing")
if defector.index("changeSeat(originalSeat)") > defector.index('fireSpellEffect(*posTile, "DefectorEnd"'):
    problems.append("the defector end effect must come after the seat change")

# 8. Heal, haste, defense, slow raise own events with effects of all four moments
cfg_all = fx_cfg + read("config", "roomAmbienceSpells.cfg")
for event, marker in (("Heal", "Mark"), ("Haste", "Mark"), ("Defense", "Mark"), ("Slow", "Mark")):
    if 'fireSpellEffect(' not in server_code or ('"%s", "%sCast"' % (event, event if event != "Heal" else "Heal")) not in server_code:
        problems.append("%s does not raise its own event" % event)
    block = re.compile(r"Event\s+SpellFx" + event + r"\s*\n\s*Kind\s+Particle\s*\n\s*System\s+(\S+)")
    systems = set(block.findall(cfg_all))
    if len(systems) < 3:
        problems.append("SpellFx%s has less than 3 particle effects (cast marker, area, impact)" % event)
for event in ("SpellFxLightning", "SpellFxExplosion", "SpellFxInferno"):
    if not re.search(r"Event\s+%s\s*\n\s*Kind\s+(Beam|Projectile)" % event, fx_cfg):
        problems.append("%s has no beam or projectile" % event)
if not re.search(r"Land\s+SpellFxExplosionLand", fx_cfg) or not re.search(r"Event\s+SpellFxExplosionLand", cfg_all):
    problems.append("explosion projectile has no landing event with effects")
for event in ("SpellFxDefectorEnd", "SpellFxHenEnd", "SpellFxPossessLost"):
    if event not in fx_cfg:
        problems.append("config has no effect for %s" % event)

# 9. Settings
settings = read("config", "roomAmbience.cfg")
if not re.search(r"^\s*MaxFlights\s+\d+\s*$", settings, re.M):
    problems.append("MaxFlights is not set")
if "roomAmbienceSpellsFx.cfg" not in settings:
    problems.append("roomAmbience.cfg does not include roomAmbienceSpellsFx.cfg")

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
