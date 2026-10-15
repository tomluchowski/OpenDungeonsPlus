#!/usr/bin/env python3
"""Static check of the sounds of the spells (no compiler needed).

    python source/tests/check_spell_sounds.py

Checks that every spell has an own cast sound family that the server plays, that the end sounds of the defector
and hen spells and the impact and end sounds that the config plays exist, that every sound file is a real Ogg
file with a CREDITS entry, that no two sounds are the same file, and that the config plays every impact sound.
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
credits = read("CREDITS")

spell_dir = os.path.join(ROOT, "source", "spells")
server_code = "".join(read("source", "spells", name) for name in sorted(os.listdir(spell_dir)) if name.endswith(".cpp"))
server_code += read("source", "creatureeffect", "CreatureEffectHexenHen.cpp") + \
    read("source", "creatureeffect", "CreatureEffectDefector.cpp") + \
    "".join(read("source", "creatureskill", name) for name in ("CreatureSkillHasteSelf.cpp", "CreatureSkillHealSelf.cpp",
                                                                "CreatureSkillDefenseSelf.cpp"))
casts = ("SummonWorkerCast", "CallToWarCast", "HealCast", "ExplosionCast", "HasteCast", "DefenseCast", "SlowCast",
         "StrengthCast", "WeakCast", "EyeEvilCast", "GoldCast", "LightningCast", "TremorCast", "DefectorCast",
         "HexenHenCast", "InfernoCast", "PossessCast", "ChampionCast")
if len(casts) != 18:
    problems.append("expected 18 spells")
others = ("ExplosionImpact", "LightningImpact", "TremorImpact", "InfernoImpact", "GoldImpact", "HealImpact",
          "HasteImpact", "PossessEnd", "DefectorEnd", "HexenHenEnd")
digests = {}
for family in casts + others:
    path = os.path.join(ROOT, "sounds", "Spatial", "Spells", family, "Fx%s01.ogg" % family)
    if not os.path.exists(path) or os.path.getsize(path) < 1000:
        problems.append("sound %s is missing or too small" % family)
        continue
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:4] != b"OggS":
        problems.append("sound %s is not an Ogg file" % family)
    if ("sounds/Spatial/Spells/%s/Fx%s01.ogg" % (family, family)) not in credits:
        problems.append("CREDITS has no entry for sound %s" % family)
    if data in digests:
        problems.append("sounds %s and %s are identical" % (family, digests[data]))
    digests[data] = family

for family in casts + ("DefectorEnd", "HexenHenEnd"):
    if '"%s"' % family not in server_code:
        problems.append("the server does not play the sound %s" % family)

fx_cfg = read("config", "roomAmbienceSpellsFx.cfg")
for family in others:
    if family in ("DefectorEnd", "HexenHenEnd"):
        continue
    if ("Spells/%s" % family) not in fx_cfg:
        problems.append("config has no effect that plays %s" % family)
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", fx_cfg, re.S):
    family = re.search(r"^\s*Family\s+Spells/(\S+)", block, re.M)
    if family is not None and family.group(1) not in casts + others:
        problems.append("the config plays the unknown sound family %s" % family.group(1))

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
