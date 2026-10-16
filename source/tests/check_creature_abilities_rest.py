"""Check the creature abilities added after freeze, drain and invisible.

Whirlwind, Teleport, GasCloud, HailStorm, GuidedBolt, Grenade, RaiseDead and SkeletonArmy are skills of their own,
Disruption is a slow, strong MissileLaunch. Every skill must be registered under its config name, be part of the
build, and every config line must have as many values as the column comment above it names."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding="utf-8")


cmake = read("CMakeLists.txt")
skill_sources = {
    "Whirlwind": "creatureskill/CreatureSkillWhirlwind",
    "Teleport": "creatureskill/CreatureSkillTeleport",
    "GasCloud": "creatureskill/CreatureSkillAreaDamage",
    "HailStorm": "creatureskill/CreatureSkillAreaDamage",
    "GuidedBolt": "creatureskill/CreatureSkillGuidedBolt",
    "Grenade": "creatureskill/CreatureSkillGrenade",
    "RaiseDead": "creatureskill/CreatureSkillRaiseDead",
    "SkeletonArmy": "creatureskill/CreatureSkillSkeletonArmy",
}
for name, base in skill_sources.items():
    assert f"{{SRC}}/{base}.cpp".replace("{SRC}", "${SRC}") in cmake, f"{base}.cpp is part of the build"
    source = read(f"source/{base}.cpp")
    assert f'"{name}"' in source, f"{name} is registered by its config name"
    assert "CreatureSkillRegister" in source, f"{name} registers a factory"
for extra in ("creatureeffect/CreatureEffectAreaDamage", "creatureeffect/CreatureEffectTemporary",
              "creatureskill/CreatureSkillSummon", "entities/MissileBlast"):
    assert f"${{SRC}}/{extra}.cpp" in cmake, f"{extra}.cpp is part of the build"

# The missile type is known to the loader and the homing hook is used by the missile loop
missile = read("source/entities/MissileObject.cpp")
assert "MissileObjectType::blast" in missile and "MissileBlast::getMissileBlastFromStream" in missile
assert "MissileBlast::getMissileBlastFromPacket" in missile
assert "updateDirection();" in missile, "the missile loop calls the homing hook"
assert "blast" in read("source/entities/MissileObject.h")

# The creature helpers
creature_h = read("source/entities/Creature.h")
for decl in ("void teleportTo(Tile* tile);", "Tile* getWalkDestinationTile() const;", "bool takeCorpse();"):
    assert decl in creature_h, decl

# The config: every ability sits on its creature and has as many values as its column comment
creatures = read("config/creatures.cfg")
blocks = {}
for block in re.findall(r"^\[Creature\]\n(.*?)^\[/Creature\]", creatures, re.M | re.S):
    blocks[re.search(r"^\s+Name\s+(\S+)", block, re.M).group(1)] = block

expected = {
    "CaveHornet": ["Whirlwind"],
    "Kobold": ["Teleport"],
    "DwarfWorker": ["Teleport"],
    "Slime": ["GasCloud"],
    "Dragon": ["MissileLaunch", "HailStorm", "SkeletonArmy"],
    "DarkElf": ["GuidedBolt"],
    "Elf": ["GuidedBolt", "Grenade"],
    "Lich": ["RaiseDead"],
}
for creature, skills in expected.items():
    block = blocks[creature]
    section = block[block.index("[CreatureSkills]"):block.index("[/CreatureSkills]")]
    lines = [l.strip() for l in section.splitlines()[1:] if l.strip()]
    for skill in skills:
        found = False
        for i, line in enumerate(lines):
            if line.startswith("#") or line.split()[0] != skill:
                continue
            if skill == "MissileLaunch" and "none\tMissileMagic\t1.7" not in line:
                continue
            found = True
            comment = lines[i - 1]
            assert comment.startswith("#"), f"{creature} {skill} has a column comment"
            assert len(comment.lstrip("# ").split()) == len(line.split()), f"{creature} {skill} column count"
        assert found, f"{creature} has {skill}"

# Spot values taken from the reference tables (fork scale where the values are gameplay numbers)
dragon = blocks["Dragon"]
assert re.search(r"^\s+HailStorm\t21\t2\t4\t8\t2\t11\t1\.5\t", dragon, re.M), "hail storm: 8 s of 1.4 turns, radius 2"
assert re.search(r"^\s+SkeletonArmy\t21\t2\t10\t3\t42\tSkeleton$", dragon, re.M), "three skeletons for 30 s"
assert re.search(r"^\s+RaiseDead\t24\t2\t10\t4\t3\t42\tSkeleton$", blocks["Lich"], re.M), "raised skeletons last 30 s"
assert re.search(r"^\s+Whirlwind\t14\t1\t2\t8\t3\t3$", blocks["CaveHornet"], re.M), "pushed back by three tiles"
assert re.search(r"^\s+GasCloud\t7\t1\t2\t4\t1\.5\t2\t", blocks["Slime"], re.M), "gas cloud from level 4"
for worker in ("Kobold", "DwarfWorker"):
    assert re.search(r"^\s+Teleport\t7\t0\t8\t8\t30$", blocks[worker], re.M), "these workers teleport from level 8"

print("ok")
