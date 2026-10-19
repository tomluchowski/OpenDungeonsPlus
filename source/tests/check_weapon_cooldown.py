"""Weapon cooldown: every weapon in equipments.cfg names the melee cooldown, the cooldown values are the decided ones,
building damage keeps the base columns, and the level formula is in place.

Static source check (no compiler, no game): the config field, its loader/writer/packet, the choice of the
weapon value for melee (larger of two, definition value without a weapon), the level formula and the recovery clock.
Run with --table to print the per creature table (weapons, damage, cooldown before/after, level 30)."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
# State the damage and cooldown values are compared with (this branch before the weapon cooldown work)
BASE_COMMIT = 'b67c575cf'


def read(path):
    return (root / path).read_text(encoding='utf-8')


def read_base(path):
    return subprocess.check_output(['git', 'show', BASE_COMMIT + ':' + path], cwd=root).decode('utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


# ---------------------------------------------------------------- config parsing

def parse_equipments(text):
    """Returns name -> dict(mesh, damage=(phys, mag, ele), cooldown or None, parry=(base, per level, max) or None)."""
    # the header comment mentions the tags too, so only the part after the opening tag is parsed
    body = text[text.rindex('[EquipmentDefinitions]'):]
    result = {}
    for block in re.findall(r'\[Equipment\](.*?)\[/Equipment\]', body, re.S):
        name = re.search(r'^\s+Name\s+(\S+)', block, re.M).group(1)
        stats = dict(re.findall(r'^\s+(\w+)\s+(\S+)\s*$', block, re.M))
        cooldown = stats.get('CooldownNbTurns')
        result[name] = {
            'mesh': stats['MeshName'],
            'damage': (float(stats['PhysicalDamage']), float(stats['MagicalDamage']), float(stats['ElementDamage'])),
            'cooldown': None if cooldown is None else int(cooldown),
            'parry': None if 'ParryBase' not in stats else
                     (float(stats['ParryBase']), float(stats['ParryPerLevel']), float(stats['ParryMax'])),
        }
    return result


def parse_creatures(text):
    """Return held weapons, the configured avoidance and the skill fields."""
    body = text[text.rindex('[CreatureDefinitions]'):]
    result = {}
    for block in re.findall(r'\[Creature\](.*?)\[/Creature\]', body, re.S):
        name = re.search(r'^\s+Name\s+(\S+)', block, re.M).group(1)
        weapons = [w for w in re.findall(r'^\s+WeaponSpawn[LR]\s+(\S+)', block, re.M) if w != 'none']
        skills = {}
        section = re.search(r'\[CreatureSkills\](.*?)\[/CreatureSkills\]', block, re.S)
        for line in section.group(1).splitlines() if section else []:
            line = line.strip()
            if line and not line.startswith('#'):
                fields = line.split()
                skills[fields[0]] = fields[1:]
        avoidance = re.search(r'^\s+Avoidance\s+(\S+)', block, re.M)
        result[name] = {'weapons': weapons, 'skills': skills,
                        'avoidance': avoidance.group(1) if avoidance else 'none'}
    return result


ANCHORS = [1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 3.0, 4.0, 6.0]


def power_multiplier(level):
    """Mirror of CreatureProgression::powerMultiplier."""
    position = (max(1, min(30, level)) - 1) * (len(ANCHORS) - 1) / 29.0
    index = int(position)
    if index == len(ANCHORS) - 1:
        return ANCHORS[-1]
    return ANCHORS[index] + (ANCHORS[index + 1] - ANCHORS[index]) * (position - index)


def stat(base, per_level, level):
    """Mirror of CreatureProgression::stat."""
    return max(base + (max(1, level) - 1) * per_level, base * power_multiplier(level))


def cooldown_at_level(base_turns, level):
    """Mirror of CreatureProgression::cooldownAtLevel."""
    if base_turns == 0:
        return 0.0
    return max(1.0, base_turns * (1.0 - 0.0075 * (max(1, level) - 1)))


# Skills whose damage is built from the creature skill row and the carried weapons
DAMAGE_SKILLS = ('Melee', 'MissileLaunch', 'GuidedBolt', 'Grenade')


def damage_values(creature, skill):
    """Read the six retained building-damage columns before the appended creature range."""
    fields = creature['skills'][skill]
    # Their original offsets stay fixed when creature ranges are appended after the old columns.
    start = 3 if skill == 'Melee' else 8
    tail = fields[start:start + 6]
    return [float(v) for v in tail]


def per_damage_type(creature, equipments, skill, level):
    """Damage split by type (physical, magical, elemental) at a level: creature part plus the carried weapons."""
    values = damage_values(creature, skill)
    result = []
    for i in range(3):
        result.append(stat(values[2 * i] + values[2 * i + 1], values[2 * i + 1], level))
    for weapon in creature['weapons']:
        for i in range(3):
            result[i] += equipments[weapon]['damage'][i]
    return result


def cooldown_of(creature, equipments, skill):
    """Cooldown in turns at level 1 that the game uses for the skill (weapon value for melee)."""
    base = int(creature['skills'][skill][0])
    if skill == 'Melee':
        values = [equipments[w]['cooldown'] for w in creature['weapons'] if equipments[w]['cooldown']]
        if values:
            return max(values)
    return base


equipments = parse_equipments(read('config/equipments.cfg'))
creatures = parse_creatures(read('config/creatures.cfg'))
base_equipments = parse_equipments(read_base('config/equipments.cfg'))
base_creatures = parse_creatures(read_base('config/creatures.cfg'))

# ---------------------------------------------------------------- the decided values

# Weapons: cooldown in turns (shields and other non-weapons have none)
EXPECTED_WEAPON_COOLDOWN = {
    'ShortSword': 1, 'BasicHammer': 1, 'RustySword': 2, 'BasicSword': 2, 'Sabre': 2, 'Longsword': 3, 'BasicAxe': 3,
    'Staff': 3, 'Bow': 2, 'Crossbow': 2, 'T2SwordGood': 2, 'T3SwordGood': 3, 'T4SwordGood': 3,
    'T2HammerGood': 4, 'T3HammerGood': 5, 'T4HammerGood': 6,
    # weapons that belong to the model: knife and pick count as tools (1 turn), the Defender weapon follows its damage
    'Knife': 1, 'KoboldPick': 1, 'Pickaxe': 1, 'DefenderWeapon': 4,
}
# Weapons that are part of the creature model and attach no mesh of their own
EXPECTED_MODEL_WEAPONS = {'Knife': 'Goblin', 'KoboldPick': 'Kobold', 'DefenderWeapon': 'Defender'}
# Cooldown in turns at level 1 per creature skill (melee: from the weapon if one is carried)
EXPECTED_COOLDOWN = {
    ('Kobold', 'Melee'): 1, ('DwarfWorker', 'Melee'): 1, ('Rat', 'Melee'): 2, ('Orc', 'Melee'): 4,
    ('LavaSpawn', 'Melee'): 2, ('Goblin', 'Melee'): 1, ('PitDemon', 'Melee'): 4, ('Roach', 'Melee'): 2,
    ('Spider', 'Melee'): 2, ('Troll', 'Melee'): 4, ('Cultist', 'Melee'): 2, ('CaveHornet', 'Melee'): 2,
    ('Slime', 'Melee'): 2, ('Skeleton', 'Melee'): 2, ('Scarab', 'Melee'): 4, ('Adventurer', 'Melee'): 2,
    ('Monk', 'Melee'): 2, ('Dwarf1', 'Melee'): 2, ('Dwarf2', 'Melee'): 2, ('Knight', 'Melee'): 2,
    ('RunelordDwarf', 'Melee'): 2, ('NatureMonster', 'Melee'): 2, ('Defender', 'Melee'): 4, ('Wyvern', 'Melee'): 2,
    ('Gnome', 'Melee'): 2, ('TentacleAlbine', 'Melee'): 2, ('TentacleGreen', 'Melee'): 2, ('Dragon', 'Melee'): 4,
    ('LizardMan', 'MissileLaunch'): 3, ('Lich', 'MissileLaunch'): 2, ('Wizard', 'MissileLaunch'): 3,
    ('DarkElf', 'MissileLaunch'): 3, ('Elf', 'MissileLaunch'): 3, ('Kreatur', 'MissileLaunch'): 3,
}


# Parry values (percent) per weapon: the three weapons of the unskilled model carriers parry badly, all others keep the
# values global.cfg had (3 / 0.5 / 15); shields have no parry fields
WEAK_PARRY = (1.5, 0.25, 7.5)
GLOBAL_PARRY = (3.0, 0.5, 15.0)
SHIELD_BLOCK = (6.0, 1.0, 25.0)
WEAK_PARRY_WEAPONS = {'KoboldPick': 'Kobold', 'Pickaxe': 'DwarfWorker', 'DefenderWeapon': 'Defender'}


def parry_of(creature, equipments, level=1):
    """Only a parry creature uses the largest eligible held-weapon parry chance."""
    if creature.get('avoidance') != 'parry':
        return 0.0
    chances = []
    for w in creature['weapons']:
        mesh = equipments[w]['mesh'].lower()
        if 'shield' in mesh or 'bow' in mesh or 'staff' in mesh or 'wand' in mesh:
            continue
        base, per_level, maximum = equipments[w].get('parry') or GLOBAL_PARRY
        chances.append(max(0.0, min(maximum, base + per_level * level)))
    return max(chances) if chances else 0.0


def parry_text(creature, equipments):
    """Describe weapon parry only for creatures configured to parry."""
    if parry_of(creature, equipments) == 0.0:
        return '-'
    best = max(creature['weapons'], key=lambda w: parry_of({'weapons': [w], 'avoidance': 'parry'}, equipments, 10))
    return '%g/%g/%g' % (equipments[best].get('parry') or GLOBAL_PARRY)


def print_table():
    """Prints the per creature table and the weapons table as markdown."""
    print('| Creature skill | Weapons before -> after | Weapon damage | Skill base damage | Total damage before |'
          ' Total damage after | Cooldown before | Cooldown after | Cooldown after at level 30 |'
          ' Parry base/per level/max before -> after |')
    print('|---|---|---|---|---|---|---|---|---|---|')
    for (name, skill), after in sorted(EXPECTED_COOLDOWN.items()):
        old = base_creatures[name]
        new = creatures[name]
        weapon_damage = sum(sum(equipments[w]['damage']) for w in new['weapons'])
        base_damage = sum(damage_values(new, skill)[0::2])
        before_total = sum(per_damage_type(old, base_equipments, skill, 1))
        after_total = sum(per_damage_type(new, equipments, skill, 1))
        parry = parry_text(old, base_equipments) + ' -> ' + parry_text(new, equipments) if skill == 'Melee' else 'n/a'
        print('| %s %s | %s -> %s | %.4g | %.4g | %.4g | %.4g | %g | %g | %.4g | %s |' % (
            name, skill, '+'.join(old['weapons']) or '-', '+'.join(new['weapons']) or '-', weapon_damage, base_damage,
            before_total, after_total, cooldown_of(old, base_equipments, skill), after, cooldown_at_level(after, 30),
            parry))
    print()
    print('| Weapon | Physical | Magical | Element | Cooldown | Parry base | Parry per level | Parry max |')
    print('|---|---|---|---|---|---|---|---|')
    for name in sorted(equipments):
        item = equipments[name]
        parry = item['parry'] or ('-', '-', '-')
        print('| %s | %g | %g | %g | %s | %s | %s | %s |' % (name, item['damage'][0], item['damage'][1],
              item['damage'][2], item['cooldown'] or '-', parry[0], parry[1], parry[2]))


if '--table' in sys.argv:
    print_table()
    sys.exit(0)

# ---------------------------------------------------------------- weapon cooldown field

weapon_names = []
for name, item in equipments.items():
    # an item counts as a shield when its name contains "shield" in any letter case
    if 'shield' in name.lower():
        assert item['cooldown'] is None, name + ' is not a weapon and must have no cooldown field'
        assert item['damage'] == (0.0, 0.0, 0.0), name + ' deals damage, so it is not a plain shield'
        continue
    assert item['cooldown'] is not None, name + ' is a weapon and needs CooldownNbTurns'
    assert item['cooldown'] > 0, name + ' needs a positive cooldown'
    weapon_names.append(name)
assert set(weapon_names) == set(EXPECTED_WEAPON_COOLDOWN), set(weapon_names) ^ set(EXPECTED_WEAPON_COOLDOWN)
for name, expected in EXPECTED_WEAPON_COOLDOWN.items():
    assert equipments[name]['cooldown'] == expected, (name, equipments[name]['cooldown'], expected)

text = read('config/equipments.cfg')
header = text[:text.rindex('[EquipmentDefinitions]')]
assert 'CooldownNbTurns' in header and 'larger value' in header and 'non-weapons' in header
assert '0.75 percent' in header and '"none"' in header

# Every creature carries what its model shows: the model weapons attach nothing, the pickaxe is the dwarf tool
for name, carrier in EXPECTED_MODEL_WEAPONS.items():
    assert equipments[name]['mesh'] == 'none', name
    assert creatures[carrier]['weapons'] == [name], (carrier, creatures[carrier]['weapons'])
assert equipments['Pickaxe']['mesh'] == 'DwarfPick.mesh'
assert (root / 'models' / 'DwarfPick.mesh').is_file()
assert creatures['DwarfWorker']['weapons'] == ['Pickaxe']
# Evidence from the models: the knife and the Defender weapon are parts of their meshes, the kobold has a pick bone
assert b'GoblinKnife' in (root / 'models' / 'Goblin.mesh').read_bytes()
assert b'DefenderWeapon' in (root / 'models' / 'Defender.mesh').read_bytes()
assert b'Pick' in (root / 'models' / 'Kobold.skeleton').read_bytes()
for name, item in equipments.items():
    if item['mesh'] == 'none':
        assert name in EXPECTED_MODEL_WEAPONS, name + ' has no mesh but is not a model weapon'
# The goblin no longer carries the short sword (the knife replaces it)
assert 'ShortSword' not in creatures['Goblin']['weapons']

# ---------------------------------------------------------------- parry values per weapon

for name, item in equipments.items():
    if 'shield' in name.lower():
        assert item['parry'] is None, name + ' is a shield and has no parry fields'
        continue
    expected = WEAK_PARRY if name in WEAK_PARRY_WEAPONS else GLOBAL_PARRY
    assert item['parry'] == expected, (name, item['parry'], expected)
# The weak values belong to weapons that only one creature carries (and no level file names them)
for weapon_name, carrier in WEAK_PARRY_WEAPONS.items():
    carriers = [n for n, c in creatures.items() if weapon_name in c['weapons']]
    assert carriers == [carrier], (weapon_name, carriers)
    for level_file in (root / 'levels').rglob('*'):
        if level_file.is_file() and level_file.suffix in ('.level', '.cfg', '.txt'):
            assert weapon_name not in level_file.read_text(encoding='utf-8', errors='ignore'), (weapon_name, level_file)
# The shield values and the fallback values stay in global.cfg unchanged
global_cfg_text = read('config/global.cfg')
for key, value in (('ParryBase', '3'), ('ParryPerLevel', '0.5'), ('ParryMax', '15'), ('ShieldBlockBase', '6'),
                   ('ShieldBlockPerLevel', '1'), ('ShieldBlockMax', '25')):
    assert re.search(r'^\s+' + key + r'\t' + re.escape(value) + r'\s*$', global_cfg_text, re.M), key
assert 'ParryBase' in header and 'ParryPerLevel' in header and 'ParryMax' in header and 'Shields have no parry' in header
# Only the two configured parry types use weapon parry; other types have no weapon parry.
for name, creature in creatures.items():
    for level in (1, 10, 30):
        now = parry_of(creature, equipments, level)
        if creature['avoidance'] != 'parry':
            expected = 0.0
        elif any(w in WEAK_PARRY_WEAPONS for w in creature['weapons']):
            expected = max(0.0, min(7.5, 1.5 + 0.25 * level))
        elif creature['weapons'] == base_creatures[name]['weapons']:
            expected = max(0.0, min(15.0, 3.0 + 0.5 * level))
        else:
            continue
        assert abs(now - expected) < 1e-9, (name, level, now, expected)
assert abs(parry_of(creatures['Defender'], equipments, 1) - 1.75) < 1e-9
assert abs(parry_of(creatures['Defender'], equipments, 30) - 7.5) < 1e-9
assert abs(parry_of(creatures['Goblin'], equipments, 1) - 3.5) < 1e-9

# ---------------------------------------------------------------- cooldown values per creature skill

for (name, skill), expected in EXPECTED_COOLDOWN.items():
    assert name in creatures and skill in creatures[name]['skills'], (name, skill)
    assert cooldown_of(creatures[name], equipments, skill) == expected, \
        (name, skill, cooldown_of(creatures[name], equipments, skill), expected)
# A creature with weapons has the weapon cooldown in its melee row too, so both sources agree
for name, creature in creatures.items():
    if creature['weapons'] and 'Melee' in creature['skills']:
        assert int(creature['skills']['Melee'][0]) == cooldown_of(creature, equipments, 'Melee'), name
# Non-damage skills retain timing; approved healing and area ranges replace their old values.
for name, creature in creatures.items():
    for skill, fields in creature['skills'].items():
        if skill in DAMAGE_SKILLS:
            continue
        previous = base_creatures[name]['skills'][skill]
        if skill == 'HealSelf':
            assert fields[:-1] == previous[:-1], (name, skill)
            assert abs(float(fields[-1]) - float(previous[-1]) * 2.05) < 1e-9, (name, skill)
        elif skill in ('Explosion', 'GasCloud', 'HailStorm'):
            assert fields[:4] == previous[:4], (name, skill)
            assert len(fields) == len(previous) + 1, (name, skill)
        else:
            assert fields == previous, (name, skill)

# ---------------------------------------------------------------- total damage unchanged against the base state

assert set(creatures) == set(base_creatures)
checked = 0
for name, creature in creatures.items():
    for skill in creature['skills']:
        if skill not in DAMAGE_SKILLS:
            continue
        for level in range(1, 31):
            before = per_damage_type(base_creatures[name], base_equipments, skill, level)
            after = per_damage_type(creature, equipments, skill, level)
            for b, a in zip(before, after):
                assert abs(a - b) < 1e-9, (name, skill, level, before, after)
            checked += 1
assert checked > 1000, checked

# Weapons with damage 0 are listed: as decided on 2026-10-19 their damage stays 0 and the creature base damage
# stays unchanged; balancing the weapon damage follows later in a separate change
zero_damage = sorted(n for n in weapon_names if sum(equipments[n]['damage']) == 0)
if zero_damage:
    print('note: weapon damage stays 0 as decided on 2026-10-19, balancing follows later in a separate change: '
          + ', '.join(zero_damage))

# ---------------------------------------------------------------- level formula

progression = read('source/entities/CreatureProgression.h')
assert 'const double COOLDOWN_REDUCTION_PER_LEVEL = 0.0075;' in progression
body = function_body(progression, 'inline double cooldownAtLevel(uint32_t baseTurns, uint32_t level)')
assert '1.0 - COOLDOWN_REDUCTION_PER_LEVEL * (std::max(1u, level) - 1)' in body
assert 'std::max(1.0, baseTurns * factor)' in body
assert 'if(baseTurns == 0)' in body
assert cooldown_at_level(1, 30) == 1.0, 'never below one turn'
assert abs(cooldown_at_level(2, 30) - 2 * 0.7825) < 1e-9
assert abs(cooldown_at_level(6, 11) - 6 * 0.925) < 1e-9
assert cooldown_at_level(4, 1) == 4.0
assert abs(cooldown_at_level(22, 2) - 22 * 0.9925) < 1e-9, 'no rounding to whole turns'
assert cooldown_at_level(0, 30) == 0.0

# ---------------------------------------------------------------- loader, writer and packet

weapon_cpp = read('source/entities/Weapon.cpp')
weapon_h = read('source/entities/Weapon.h')
assert 'uint32_t getCooldownNbTurns() const' in weapon_h
assert weapon_h.count('mCooldownNbTurns(0)') == 2, 'both constructors initialise the cooldown'
assert 'if (nextParam == "CooldownNbTurns")' in weapon_cpp
assert 'weapon->mCooldownNbTurns = Helper::toUInt32(nextParam);' in weapon_cpp
write_body = function_body(weapon_cpp, 'void Weapon::writeWeaponDiff(')
assert '"    CooldownNbTurns\\t"' in write_body
out_body = function_body(weapon_cpp, 'ODPacket& operator <<(ODPacket& os, const Weapon *weapon)')
in_body = function_body(weapon_cpp, 'ODPacket& operator >>(ODPacket& is, Weapon *weapon)')
assert out_body.index('mElementDefense') < out_body.index('mCooldownNbTurns')
assert in_body.index('mElementDefense') < in_body.index('mCooldownNbTurns')

# Parry fields: loader, writer, packet, getters with the global fallback, and the roll that uses them
for field in ('mParryBase', 'mParryPerLevel', 'mParryMax'):
    assert weapon_h.count(field + '(-1.0)') == 2, 'both constructors mark the parry values as not set'
for field, key in (('mParryBase', 'ParryBase'), ('mParryPerLevel', 'ParryPerLevel'), ('mParryMax', 'ParryMax')):
    assert 'if (nextParam == "%s")' % key in weapon_cpp, key
    assert 'weapon->%s = Helper::toDouble(nextParam);' % field in weapon_cpp, key
    assert '"    %s\\t"' % key in write_body, key
    assert out_body.index('mCooldownNbTurns') < out_body.index(field), field
    assert in_body.index('mCooldownNbTurns') < in_body.index(field), field
for getter in ('getParryBase', 'getParryPerLevel', 'getParryMax'):
    body = function_body(weapon_cpp, 'double Weapon::%s() const' % getter)
    assert 'ConfigManager::getSingleton().%s()' % getter in body and '< 0.0' in body, getter
    assert 'double %s() const;' % getter in weapon_h, getter
parry_roll = function_body(read('source/entities/Creature.cpp'),
                           'DefenceChance::Outcome Creature::rollMeleeDefence(bool missile) const')
for needle in ('parryWeapons[i]->getParryBase()', 'parryWeapons[i]->getParryPerLevel()',
               'parryWeapons[i]->getParryMax()', 'config.getShieldBlockBase()', 'config.getShieldBlockPerLevel()',
               'config.getShieldBlockMax()', 'chance = std::max(chance, DefenceChance::parryChance(',
               'CreatureDefinition::Avoidance::parry', 'missile && avoidance == CreatureDefinition::Avoidance::parry'):
    assert needle in parry_roll, needle
assert 'config.getParryBase()' not in parry_roll, 'the weapon values replace the global ones in the roll'

# ---------------------------------------------------------------- choice of the applied cooldown

creature = read('source/entities/Creature.cpp')
applied = function_body(creature, 'double Creature::getAppliedCooldownNbTurns(')
assert 'dynamic_cast<const CreatureSkillMeleeFight*>(&skill) != nullptr' in applied, 'only melee uses weapons'
assert 'const Weapon* weapons[2] = {mWeaponL, mWeaponR};' in applied
assert 'std::max(weaponCooldown, weapon->getCooldownNbTurns())' in applied, 'larger of two weapons'
assert 'if(weapon == nullptr)' in applied
assert 'if(weaponCooldown != 0)' in applied and 'baseCooldown = weaponCooldown;' in applied, \
    'no weapon cooldown means the definition cooldown applies'
assert 'CreatureProgression::cooldownAtLevel(baseCooldown, getLevel())' in applied
scaled = function_body(creature, 'bool isLevelScaledAttack(const CreatureSkill& skill)')
for word in ('CreatureSkillMeleeFight', '"MissileLaunch"', '"GuidedBolt"', '"Grenade"'):
    assert word in scaled, word

# Both the stored cooldown and the recovery clock use the applied value, in fractions of turns
use_attack = function_body(creature, 'void Creature::useAttack(')
assert 'skillData.mCooldown = getAppliedCooldownNbTurns(*skillData.mSkill);' in use_attack
assert 'mAttackRecoveryDuration = std::max(static_cast<double>(skillData.mWarmup), skillData.mCooldown);' in use_attack
assert 'mAttackRecoveryTurns = mAttackRecoveryDuration;' in use_attack
assert 'CreatureSkillData skillData(skill, getAppliedCooldownNbTurns(*skill), 0);' in creature
creature_h = read('source/entities/Creature.h')
assert 'double mCooldown;' in creature_h and 'double mAttackRecoveryTurns = 0.0;' in creature_h
assert 'double mAttackRecoveryDuration = 0.0;' in creature_h
assert 'double getAttackRecoveryTurns() const' in creature_h and 'double getAttackRecoveryDuration() const' in creature_h
# the countdown never goes below zero and counts down in whole turns from a fractional start
assert creature.count('skillData.mCooldown = std::max(0.0, skillData.mCooldown - 1.0);') == 1
assert 'mAttackRecoveryTurns = std::max(0.0, mAttackRecoveryTurns - 1.0);' in creature
# The network carries the recovery as a real number
export_body = function_body(creature, 'void Creature::exportProgressToPacket(')
assert 'mAttackRecoveryTurns << mAttackRecoveryDuration' in export_body
import_body = function_body(creature, 'void Creature::importProgressFromPacket(')
assert 'double remaining = 0.0;' in import_body and 'double duration = 0.0;' in import_body
# Support skills (not melee, no weapon cooldown) keep their definition cooldown
assert creature.count('skillData.mCooldown = skillData.mSkill->getCooldownNbTurns();') == 1
# The clock reads the stored duration, nothing else, and does not run past a fractional remainder
overlay = read('source/render/CreatureOverlayStatus.cpp')
assert 'const double duration = mCreature->getAttackRecoveryDuration();' in overlay
assert 'remaining * 0.999' in overlay
# A model weapon attaches nothing
render = read('source/render/RenderManager.cpp')
assert 'if(curWeapon->getMeshName() == "none")' in function_body(render, 'void RenderManager::rrCreateWeapon(')
print('check_weapon_cooldown: ok')
