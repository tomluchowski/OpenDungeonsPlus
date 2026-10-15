#!/usr/bin/env python3
"""Static check of the cosmetic effects of the other spells: casts, areas, impacts, lasting looks, ground marks (no compiler needed).

    python source/tests/check_spell_cosmetics.py

Checks that every spell with a look reports its effect to the clients and that the config has effects for
it, that the kind Mark is wired from the config to the code, that every texture exists with a material and a
CREDITS entry, that the lasting looks on creatures have their particle systems, that the aura of a possessed
creature lasts until the possession ends and is ended on the server and on the client, and that the limits
of the effects are set.
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

# 1. The spells report their effects, and every reported effect has effects in the config
spell_dir = os.path.join(ROOT, "source", "spells")
spell_code = "".join(read("source", "spells", name) for name in sorted(os.listdir(spell_dir)) if name.endswith(".cpp"))
spell_code += read("source", "creatureeffect", "CreatureEffectDefector.cpp") + \
    read("source", "creatureeffect", "CreatureEffectHexenHen.cpp")
cfg = read("config", "roomAmbienceSpells.cfg") + read("config", "roomAmbienceSpellsFx.cfg")
events = set(re.findall(r"^\s*Event\s+(\S+)", cfg, re.M))
reported = set("SpellFx" + name for name in re.findall(r'fireSpellEffect\([^;]*?"(\w+)"\s*,\s*"[^"]*"\)', spell_code))
for effect in ("Summon", "Champion", "CallToWar", "Explosion", "Strength", "Weak", "EyeEvil", "Gold", "Lightning",
               "Tremor", "Defector", "Hen", "Inferno", "InfernoHit", "Possess"):
    if "SpellFx" + effect not in reported:
        problems.append("no spell reports the effect %s" % effect)
    if "SpellFx" + effect not in events:
        problems.append("the config has no effect for SpellFx%s" % effect)

# 2. Mark is wired
config_h = read("source", "render", "RoomAmbienceConfig.h")
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
kinds = re.search(r"enum class AmbienceKind\s*\{(.*?)\};", config_h, re.S).group(1)
order = re.findall(r"^\s*(\w+),?\s*$", re.sub(r"//.*", "", kinds), re.M)
if order[-3:] != ["beam", "projectile", "mark"]:
    problems.append("AmbienceKind order changed: %s" % order)
if 'words[1] == "Mark"' not in config_cpp or "AmbienceKind::mark" not in config_cpp or '"MaxMarks"' not in config_cpp:
    problems.append("Mark and MaxMarks are not read from the config")
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("AmbienceKind::mark", "mMarks.push_back", "getMaxMarks()", "updateOneShots(mMarks, dt)"):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
stop = ambience[ambience.index("void RoomAmbience::stopAll"):ambience.index("void RoomAmbience::update(")]
if "mMarks.clear()" not in stop:
    problems.append("stopAll does not remove the marks")
for kind in ("Shake", "Mark"):
    if not re.search(r"^\s*Kind\s+%s\s*$" % kind, cfg, re.M):
        problems.append("no %s effect in the spell config" % kind)

# 3. Textures, materials and credits
materials = read("materials", "scripts", "RoomAmbienceSpells.material")
credits = read("CREDITS")
particles = read("particles", "RoomAmbienceSpells.particle") + read("particles", "RoomAmbienceSpellsExtra.particle")
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
generator = read("tools", "gen_spell_marks.py")
for name in textures:
    if '"%s.png"' % name not in generator:
        problems.append("tools/gen_spell_marks.py does not draw %s" % name)
art = read("tools", "gen_room_ambience_art.py")
for name in ("RoomAmbCoin", "RoomAmbFeather", "RoomAmbGrain", "RoomAmbSwirl"):
    if '"%s":' % name not in art:
        problems.append("tools/gen_room_ambience_art.py does not draw %s" % name)

# 4. Every system that an effect or a creature effect names exists
systems = set(re.findall(r"^particle_system\s+(\S+)", particles, re.M))
for system in re.findall(r"^\s*System\s+(\S+)", cfg, re.M):
    if system not in systems and not os.path.exists(os.path.join(ROOT, "particles", system + ".particle")):
        problems.append("particle system %s is missing" % system)
for script in ("SpellCreatureDefector", "SpellCreatureHexenHen", "SpellCreatureInferno", "SpellCreatureChampion",
               "SpellCreatureArrival", "SpellCreaturePossess"):
    if script not in systems:
        problems.append("the lasting look %s has no particle system" % script)

# 5. The aura of a possessed creature lasts until the possession ends
spell_possess = read("source", "spells", "SpellPossess.cpp")
if not re.search(r"addParticleEffect\(Creature::POSSESSION_AURA_SCRIPT,\s*-1\)", spell_possess):
    problems.append("the possession aura must last until the possession ends")
creature = read("source", "entities", "Creature.cpp")
end_possession = creature[creature.index("void Creature::endPossession"):creature.index("void Creature::formPossessionGroup")]
if "endParticleEffectsByScript(POSSESSION_AURA_SCRIPT)" not in end_possession:
    problems.append("the server does not end the possession aura")
# The aura is permanent on the clients, a refresh never removes a missing effect and the turn deletes the ended effect
# before the refresh of the turn is built: the other keepers are told at once, while the effect is still there
if "fireCreatureRefreshIfNeeded()" not in end_possession or \
        end_possession.index("fireCreatureRefreshIfNeeded()") < end_possession.index("endParticleEffectsByScript"):
    problems.append("endPossession must send the refresh with the ended aura at once")
client = read("source", "network", "ODClient.cpp")
end = client[client.index("case ServerNotificationType::possessionEnd:"):]
end = end[:end.index("case ServerNotificationType::trapEffect:")]
if "endParticleEffectsByScript(Creature::POSSESSION_AURA_SCRIPT)" not in end:
    problems.append("the client does not end the possession aura")
entity = read("source", "entities", "GameEntity.cpp")
part = entity[entity.index("uint32_t GameEntity::endParticleEffectsByScript"):entity.index("void GameEntity::restoreEntityState")]
if "rrEntityRemoveParticleEffect" not in part or "getIsOnServerMap()" not in part:
    problems.append("endParticleEffectsByScript must remove on the client and let the server effect run out")

# 6. Limits
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
