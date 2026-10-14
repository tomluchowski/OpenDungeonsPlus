"""Check the Summon champion spell and the champion creature.

The spell costs 108,000 mana and the champion then costs 2,250 mana per second after a free period of
price / drain = 48 seconds. The champion is the only creature that cannot be hurt. The spell is
unlocked by skill research."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding="utf-8")


# Spell values and the free period derived from them
spells = read("config/spells.cfg")
section = spells[spells.rindex("[Spells]"):spells.rindex("[/Spells]")]
values = dict(re.findall(r"^\s+(\w+)\s+([\d.]+)\s*$", section, re.M))
price = float(values["SummonChampionPrice"])
drain = float(values["SummonChampionDrainPerSecond"])
assert (price, drain) == (108000.0, 2250.0), "price and drain per second"
assert price / drain == 48.0, "free period of 48 seconds"
assert values["SummonChampionCooldown"] == "0"

# The creature definition
creatures = read("config/creatures.cfg")
blocks = re.findall(r"^\[Creature\]\n(.*?)^\[/Creature\]", creatures, re.M | re.S)
champions = [b for b in blocks if re.search(r"^\s+Champion\s+1\s*$", b, re.M)]
assert len(champions) == 1, "exactly one creature definition is the champion"
champion = champions[0]
assert re.search(r"^\s+Name\s+Champion\s*$", champion, re.M), "the spell looks the class up by this name"
for key in ("WakefulnessLost/Turn", "HungerGrowth/Turn", "FeeBase"):
    assert re.search(rf"^\s+{re.escape(key)}\s+0\s*$", champion, re.M), f"the champion has no {key}"
assert "Melee" in champion and "AttackEnemy" in champion
assert "FleeWhenWeak" not in champion and "LeaveDungeonWhenFurious" not in champion, "the champion never runs away"
faction = read("config/factions.cfg")
assert "Champion" not in faction, "no portal attracts the champion"

# The definition carries the flag through every path
definition = read("source/entities/CreatureDefinition.cpp")
for text in ("mChampion(def.mChampion)", "os << c->mChampion;", "is >> c->mChampion;",
             'nextParam == "Champion"', '"    Champion\\t"'):
    assert text in definition, text

# The champion rules
creature = read("source/entities/Creature.cpp")
damage = creature[creature.index("double Creature::takeDamage("):]
damage = damage[:damage.index("physicalDamage = std::max")]
assert "isChampion()" in damage and "return 0.0;" in damage, "no damage"
assert "handleChampionUpkeep()" in creature and "handleChampionIdle()" in creature
assert "SummonChampionPrice" in creature and "SummonChampionDrainPerSecond" in creature
assert "price / drainPerSecond" in creature, "free period is price divided by drain"
slap = creature[creature.index("void Creature::slap()"):]
assert "isChampion()" in slap[:slap.index("CreatureEffectSlap")], "a slap sends the champion away"
pickup = creature[creature.index("bool Creature::tryPickup("):]
assert "isChampion()" in pickup[:pickup.index("void Creature::pickup()")], "no pickup"
for spell in ("SpellDefector", "SpellHexenHen"):
    assert "isChampion()" in read(f"source/spells/{spell}.cpp"), f"{spell} skips the champion"

# The spell and the unlock through skill research
cast = read("source/spells/SpellSummonChampion.cpp")
assert "hasChampion(gameMap, player->getSeat())" in cast, "one champion per seat"
assert "takeMana(price)" in cast
assert "summonChampion" in read("source/spells/SpellType.h")
assert "spellSummonChampion" in read("source/game/SkillType.cpp")
manager = read("source/game/SkillManager.cpp")
assert "isRewardSkill" not in manager and "isRewardSkill" not in read("source/game/Seat.cpp"), "no reward-only skill"
assert "new Skill(resType, points, summonChampionDepends)" in manager, "the champion has a research prerequisite"
assert re.search(r"^    spellSummonChampion\s+\d+\s*$", read("config/skills.cfg"), re.M), "research cost entry"
assert "Heartstone" not in manager and "Heartstone" not in read("gui/WindowTabSpells.layout")

# The buttons exist in both windows, the tree node is visible so the spell can be researched
spells_tab = ET.parse(repo / "gui/WindowTabSpells.layout").find('.//Window[@name="SummonChampionButton"]')
assert spells_tab is not None
tree = ET.parse(repo / "gui/WindowSkillTree.layout").find('.//Window[@name="SummonChampionButton"]')
assert tree.find('Property[@name="Visible"]') is None, "the tree node is shown"
assert '"SummonChampionButton"' in read("source/render/Gui.cpp")
print("summon champion checks passed")
