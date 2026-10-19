#!/usr/bin/env python3
"""Turn-based combat comparisons against the actual creature and item configuration."""

import argparse
from dataclasses import dataclass, replace
import json
from pathlib import Path
import random
import re
import statistics

CHANNELS = ("Physical", "Magical", "Element")
MAX_LEVEL = 30
DUEL_LIMIT = 5000
GROUP_LIMIT = 20000
HP_SCALE = 2.05


# Read named configuration blocks, ignoring comments and section delimiters.
def definitions(path, section, tag):
    text = path.read_text(encoding="utf-8")
    text = text[text.rindex("[" + section + "]"):]
    result = {}
    for block in re.findall(r"\[" + tag + r"\](.*?)\[/" + tag + r"\]", text, re.S):
        fields = {}
        for line in block.splitlines():
            words = line.split()
            if len(words) >= 2 and not words[0].startswith("#"):
                fields.setdefault(words[0], words[1:])
        result[fields["Name"][0]] = (fields, block)
    return result


# Evaluate the separate level curves within the server's level bounds.
def multiplier(level, kind):
    level = max(1, min(MAX_LEVEL, level))
    if kind == "hp":
        return 1 + 0.173 * (level - 1) ** 1.37
    if kind == "damage":
        return 1 + 0.278 * (level - 1) ** 1.02
    if kind == "attack":
        return 1 + 0.847 * (level - 1) ** 1.13
    if kind == "defense":
        return float(level)
    raise ValueError(kind)


# Retain configured linear growth whenever it exceeds the selected curve.
def combat_stat(base, per_level, level, kind):
    level = max(1, min(MAX_LEVEL, level))
    return max(base + (level - 1) * per_level, base * multiplier(level, kind))


# Preserve the former interpolated power curve for damage against buildings.
def building_stat(base, per_level, level):
    anchors = (1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 3, 4, 6)
    position = (max(1, min(MAX_LEVEL, level)) - 1) * 9 / 29
    index = int(position)
    power = anchors[-1] if index == 9 else anchors[index] + (anchors[index + 1] - anchors[index]) * (position - index)
    return max(base + (max(1, level) - 1) * per_level, base * power)


# Hold a combatant's statistics and mutable remaining health.
@dataclass
class Unit:
    name: str
    level: int
    hp: float
    minimum: tuple
    maximum: tuple
    defense: tuple
    attack_rating: float
    defense_rating: float
    cooldown: float
    melee: bool
    avoidance: str
    shield: bool
    parry: float


# Load combat statistics directly from the supplied repository's configuration.
class Roster:
    # Cache current definitions without using an external statistics snapshot.
    def __init__(self, root):
        self.root = Path(root)
        self.creatures = definitions(self.root / "config/creatures.cfg", "CreatureDefinitions", "Creature")
        self.equipment = definitions(self.root / "config/equipments.cfg", "EquipmentDefinitions", "Equipment")
        self.global_values = {}
        for line in (self.root / "config/global.cfg").read_text(encoding="utf-8").splitlines():
            words = line.split()
            if len(words) == 2 and not words[0].startswith("#"):
                self.global_values[words[0]] = words[1]

    # Read a numeric field using the optional-field default when absent.
    @staticmethod
    def number(fields, key, default=0):
        return float(fields.get(key, [default])[0])

    # Select the first level-one melee or single-target attack in definition order.
    def attack(self, name):
        section = re.search(r"\[CreatureSkills\](.*?)\[/CreatureSkills\]", self.creatures[name][1], re.S)
        if section is not None:
            for line in section[1].splitlines():
                words = line.split()
                if not words or words[0].startswith("#"):
                    continue
                if words[0] == "Melee" and int(words[3]) <= 1:
                    return words, 4, 10
                if words[0] == "MissileLaunch" and int(words[5]) <= 1:
                    return words, 9, 15
        raise ValueError("No level-one attack: " + name)

    # Enumerate ordinary fighters, excluding the invulnerable summoned creature.
    def fighters(self):
        result = []
        for name, (fields, _) in self.creatures.items():
            if fields.get("CreatureJob", [""])[0] == "Fighter" and self.number(fields, "Champion") != 1:
                self.attack(name)
                result.append(name)
        return result

    # Apply level curves, item bonuses, recovery and the configured avoidance type.
    def unit(self, name, level=1):
        level = max(1, min(MAX_LEVEL, level))
        fields = self.creatures[name][0]
        attack, old_start, range_start = self.attack(name)
        items = []
        for hand in ("L", "R"):
            item = fields.get("WeaponSpawn" + hand, ["none"])[0]
            if item != "none":
                items.append(self.equipment[item][0])
        minimum, maximum, defense = [], [], []
        for i, channel in enumerate(CHANNELS):
            old = float(attack[old_start + 2 * i]) + float(attack[old_start + 2 * i + 1])
            lo = float(attack[range_start + 2 * i]) if len(attack) >= range_start + 6 else old
            hi = float(attack[range_start + 2 * i + 1]) if len(attack) >= range_start + 6 else old
            minimum.append(lo * multiplier(level, "damage") + sum(self.number(item, channel + "DamageMin", self.number(item, channel + "Damage")) for item in items))
            maximum.append(hi * multiplier(level, "damage") + sum(self.number(item, channel + "Damage") for item in items))
            defense.append(combat_stat(self.number(fields, channel + "Defense"), self.number(fields, channel + "Def/Level"), level, "damage") + sum(self.number(item, channel + "Defense") for item in items))
        melee = attack[0] == "Melee"
        weapon_cooldowns = [self.number(item, "CooldownNbTurns") for item in items if self.number(item, "CooldownNbTurns") > 0]
        cooldown = max(weapon_cooldowns) if melee and weapon_cooldowns else float(attack[1])
        cooldown = max(1, cooldown * (1 - 0.0075 * (level - 1))) if cooldown else 0
        parry = 0
        shield = False
        for item in items:
            mesh = item.get("MeshName", [""])[0].lower()
            if "shield" in mesh:
                shield = True
            elif not any(word in mesh for word in ("bow", "staff", "wand")):
                base = self.number(item, "ParryBase", float(self.global_values["ParryBase"]))
                step = self.number(item, "ParryPerLevel", float(self.global_values["ParryPerLevel"]))
                cap = self.number(item, "ParryMax", float(self.global_values["ParryMax"]))
                parry = max(parry, max(0, min(cap, base + step * level)))
        return Unit(name, level, combat_stat(self.number(fields, "MinHP"), self.number(fields, "HP/Level"), level, "hp"),
                    tuple(minimum), tuple(maximum), tuple(defense),
                    combat_stat(self.number(fields, "AttackRating", 300), 0, level, "attack") + sum(self.number(item, "AttackRating") for item in items),
                    combat_stat(self.number(fields, "DefenseRating", 100), 0, level, "defense") + sum(self.number(item, "DefenseRating") for item in items),
                    cooldown, melee, fields.get("Avoidance", ["none"])[0], shield, parry)

    # Evaluate exactly one configured avoidance type after a landed hit.
    def avoidance_chance(self, target, melee):
        if int(self.global_values.get("MeleeDodgeParry", "1")) == 0:
            return 0
        prefix = None
        if target.avoidance == "dodge":
            prefix = "Dodge"
        elif target.avoidance == "block" and target.shield:
            prefix = "ShieldBlock"
        elif target.avoidance == "parry" and melee:
            return target.parry
        if prefix is None:
            return 0
        return max(0, min(float(self.global_values[prefix + "Max"]), float(self.global_values[prefix + "Base"]) + target.level * float(self.global_values[prefix + "PerLevel"])))


# Compute the bounded hit percentage independently of avoidance.
def hit_chance(attacker, target):
    return max(5, min(95, 200 * attacker.attack_rating / (attacker.attack_rating + target.defense_rating) * attacker.level / (attacker.level + target.level)))


# Subtract defense separately in each channel before summing the range endpoints.
def damage_bounds(attacker, target):
    return (sum(max(value - defense, 0) for value, defense in zip(attacker.minimum, target.defense)),
            sum(max(value - defense, 0) for value, defense in zip(attacker.maximum, target.defense)))


# Resolve hit, avoidance and one shared random value across all damage channels.
def blow(roster, attacker, target, rng):
    if rng.random() * 100 >= hit_chance(attacker, target):
        return 0
    if rng.random() * 100 < roster.avoidance_chance(target, attacker.melee):
        return 0
    low, high = damage_bounds(attacker, target)
    return low + rng.random() * (high - low)


# Repeat the reference duel scheduler; only a surviving defender answers.
def duel(roster, first, second, fights, rng):
    wins = 0
    for _ in range(fights):
        a, b = replace(first), replace(second)
        next_a = next_b = 0
        for turn in range(1, DUEL_LIMIT + 1):
            if turn >= next_a:
                b.hp -= blow(roster, a, b, rng)
                next_a = turn + a.cooldown
            if b.hp > 0 and turn >= next_b:
                a.hp -= blow(roster, b, a, rng)
                next_b = turn + b.cooldown
            if a.hp <= 0 or b.hp <= 0:
                break
        wins += a.hp > 0
    return wins / fights


# Repeat the reference group scheduler, including simultaneous reply ordering.
def fight_group(roster, first, opponent, count, fights, rng):
    wins = 0
    for _ in range(fights):
        a = replace(first)
        opponents = [replace(opponent) for _ in range(count)]
        next_a = 0
        clocks = [0] * count
        for turn in range(1, GROUP_LIMIT + 1):
            if turn >= next_a:
                opponents[0].hp -= blow(roster, a, opponents[0], rng)
                next_a = turn + a.cooldown
            for i, b in enumerate(opponents):
                if turn >= clocks[i]:
                    a.hp -= blow(roster, b, a, rng)
                    clocks[i] = turn + b.cooldown
            survivors = [i for i, b in enumerate(opponents) if b.hp > 0]
            opponents = [opponents[i] for i in survivors]
            clocks = [clocks[i] for i in survivors]
            if a.hp <= 0 or not opponents:
                break
        wins += a.hp > 0
    return wins / fights


# Run seeded cross-type and level comparisons and return every measured result.
def evaluate(roster, seed=1, fights=150, group_fights=60):
    rng = random.Random(seed)
    names = roster.fighters()
    units = {name: roster.unit(name) for name in names}
    scores = {name: [] for name in names}
    for index, name in enumerate(names):
        for other in names[index + 1:]:
            score = duel(roster, units[name], units[other], fights, rng)
            scores[name].append(score)
            scores[other].append(1 - score)
    averages = {name: statistics.mean(scores[name]) for name in names}
    tiers = {}
    for name in names:
        tier = round(roster.number(roster.creatures[name][0], "MinHP") / HP_SCALE, 8)
        tiers.setdefault(tier, []).append(name)
    tier_means = {tier: statistics.mean(averages[name] for name in members) for tier, members in sorted(tiers.items())}
    levels = {}
    for name in names:
        levels[name] = {
            "10_vs_3x5": fight_group(roster, roster.unit(name, 10), roster.unit(name, 5), 3, group_fights, rng),
            "30_vs_3x15": fight_group(roster, roster.unit(name, 30), roster.unit(name, 15), 3, group_fights, rng),
            "1_vs_10": duel(roster, roster.unit(name, 1), roster.unit(name, 10), group_fights, rng),
            "5_vs_10": duel(roster, roster.unit(name, 5), roster.unit(name, 10), group_fights, rng),
        }
    return {"seed": seed, "fights": fights, "group_fights": group_fights,
            "averages": averages, "tiers": tiers, "tier_means": tier_means, "levels": levels}


# Compare any checkout's configuration using reproducible command-line parameters.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--fights", type=int, default=150)
    parser.add_argument("--group-fights", type=int, default=60)
    args = parser.parse_args()
    print(json.dumps(evaluate(Roster(args.repo), args.seed, args.fights, args.group_fights), indent=2))


if __name__ == "__main__":
    main()
