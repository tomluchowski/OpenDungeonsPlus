"""Creature accuracy and one configured avoidance: table values, one-roll ordering, shield requirements,
melee and missile wiring, hitResult results and existing client compatibility.
Pure Python wiring check, compiles nothing. With --probe it compiles source/entities/DefenceChance.h with cl and
checks the same table in C++ (not part of the normal run)."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]


# Read production inputs without modifying their config or source.
def read(path):
    return (root / path).read_text(encoding='utf-8')


# Extract a function for checks of the actual runtime path.
def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


# ---------------------------------------------------------------------------------------------------------------
# The rules, mirrored from DefenceChance.h (percent)
# Mirror the configured dodge or shield block growth.
def dodge_chance(level, base, per_level, maximum):
    return max(0.0, min(maximum, base + per_level * level))


# A parry-capable carried weapon supplies its own values.
def parry_chance(level, has_weapon, base, per_level, maximum):
    if not has_weapon:
        return 0.0
    return max(0.0, min(maximum, base + per_level * level))


# Exactly one configured kind is resolved by exactly one random draw.
def decide(kind, chance, roll):
    return kind if roll < chance else 'none'


# Mirror the real rating and level-ratio hit formula.
def hit_chance(attack, defense, attacker_level, defender_level):
    return max(5.0, min(95.0, 200.0 * attack / (attack + defense) * attacker_level / (attacker_level + defender_level)))


DODGE = (3.0, 0.5, 15.0)
PARRY = (3.0, 0.5, 15.0)
SHIELD = (6.0, 1.0, 25.0)

# level: (dodge, parry with weapon, independent shield block)
TABLE = {
    1: (3.5, 3.5, 7.0),
    10: (8.0, 8.0, 16.0),
    24: (15.0, 15.0, 25.0),
    100: (15.0, 15.0, 25.0),
}

for level, (dodge, parry, shield) in TABLE.items():
    assert abs(dodge_chance(level, *DODGE) - dodge) < 1e-9, (level, 'dodge')
    assert abs(parry_chance(level, True, *PARRY) - parry) < 1e-9, (level, 'parry')
    assert abs(dodge_chance(level, *SHIELD) - shield) < 1e-9, (level, 'shield block')
    assert parry_chance(level, False, *PARRY) == 0.0, (level, 'no weapon')

# One selected kind can avoid a landed hit; chance boundaries use a strict comparison.
for kind in ('dodged', 'parried', 'shieldBlocked'):
    assert decide(kind, 10.0, 5.0) == kind
    assert decide(kind, 10.0, 10.0) == 'none'
    assert decide(kind, 0.0, 0.0) == 'none'
assert hit_chance(300.0, 100.0, 1, 1) == 75.0
assert hit_chance(1.0, 100000.0, 1, 30) == 5.0
assert hit_chance(100000.0, 1.0, 30, 1) == 95.0

# ---------------------------------------------------------------------------------------------------------------
# DefenceChance.h: pure (no game includes), the same formulas and one-kind resolution
header = read('source/entities/DefenceChance.h')
includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', header)
assert sorted(includes) == ['algorithm', 'cstdint', 'string'], includes
assert 'inline double dodgeChance(uint32_t level, double base, double perLevel, double max)' in header
assert 'std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)))' in header
assert 'std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)))' in header
parry_body = function_body(header, 'inline double parryChance(')
assert 'if(!hasWeapon)' in parry_body and 'return 0.0;' in parry_body
assert 'hasShield' not in parry_body, 'shield block must not replace weapon parry'
decide_body = function_body(header, 'inline Outcome decide(')
assert 'return roll < chance ? kind : none;' in decide_body
hit = function_body(header, 'inline double hitChance(')
assert 'HIT_SCALE * attackRating / (attackRating + defenseRating)' in hit
assert 'attackerLevel / static_cast<double>(attackerLevel + defenderLevel)' in hit
assert 'std::max(MIN_HIT_PERCENT, std::min(MAX_HIT_PERCENT, chance))' in hit
for constant in ('HIT_SCALE = 200.0', 'MIN_HIT_PERCENT = 5.0', 'MAX_HIT_PERCENT = 95.0'):
    assert constant in header, constant

# The weapon keywords are the ones the client uses to show the weapon (CreatureCombatReactions.cpp weaponKind)
client_kinds = read('source/render/CreatureCombatReactions.cpp')
client_kinds = client_kinds[client_kinds.index('std::string weaponKind('):]
client_kinds = client_kinds[:client_kinds.index('\n}\n')]
for keyword in ('shield', 'bow', 'staff', 'wand'):
    assert '"' + keyword + '"' in client_kinds and '"' + keyword + '"' in header, keyword

# ---------------------------------------------------------------------------------------------------------------
# Configuration: global.cfg with the values of the order, ConfigManager reads them with limits
global_cfg = read('config/global.cfg')
config_cpp = read('source/utils/ConfigManager.cpp')
config_h = read('source/utils/ConfigManager.h')
KEYS = (('DodgeBase', '3', 'mDodgeBase(DEFAULT_DODGE_BASE)', 100), ('DodgePerLevel', '0.5', 'mDodgePerLevel(DEFAULT_DODGE_PER_LEVEL)', 10),
        ('DodgeMax', '15', 'mDodgeMax(DEFAULT_DODGE_MAX)', 100), ('ParryBase', '3', 'mParryBase(DEFAULT_PARRY_BASE)', 100),
        ('ParryPerLevel', '0.5', 'mParryPerLevel(DEFAULT_PARRY_PER_LEVEL)', 10), ('ParryMax', '15', 'mParryMax(DEFAULT_PARRY_MAX)', 100),
        ('ShieldBlockBase', '6', 'mShieldBlockBase(6.0)', 100), ('ShieldBlockPerLevel', '1', 'mShieldBlockPerLevel(1.0)', 10),
        ('ShieldBlockMax', '25', 'mShieldBlockMax(25.0)', 100))
# The named defaults and limits keep the numbers they had before they got names
for name, value in (('DEFAULT_DODGE_BASE', '3.0'), ('DEFAULT_DODGE_PER_LEVEL', '0.5'), ('DEFAULT_DODGE_MAX', '15.0'),
                    ('DEFAULT_PARRY_BASE', '3.0'), ('DEFAULT_PARRY_PER_LEVEL', '0.5'), ('DEFAULT_PARRY_MAX', '15.0'),
                    ('ConfigManager::ChanceMin', '0.0'),
                    ('ConfigManager::ChanceMax', '100.0'), ('ConfigManager::ChancePerLevelMax', '10.0')):
    assert re.search(re.escape(name) + r' = ' + re.escape(value) + ';', config_cpp), name
assert re.search(r'^# .*\n(?:# .*\n)?\s+MeleeDodgeParry\t1\s*$', global_cfg, re.M), 'MeleeDodgeParry'
assert 'nextParam == "MeleeDodgeParry"' in config_cpp and 'mMeleeDodgeParry(true)' in config_cpp
assert 'inline bool getMeleeDodgeParry() const' in config_h
for key, value, member, limit in KEYS:
    assert re.search(r'^\s+' + key + r'\t' + re.escape(value) + r'\s*$', global_cfg, re.M), key
    assert 'nextParam == "' + key + '"' in config_cpp and member in config_cpp, key
    if key.startswith('ShieldBlock'):
        assert 'std::max(0.0, std::min(%d.0, m%s))' % (limit, key) in config_h, key
    else:
        limit_name = 'ChanceMax' if limit == 100 else 'ChancePerLevelMax'
        assert 'std::max(ChanceMin, std::min(%s, m%s))' % (limit_name, key) in config_h, key
    assert 'inline double get' + key + '() const' in config_h, key

# ---------------------------------------------------------------------------------------------------------------
# Server: hit first, one configured avoidance second, then damage; attack timing stays outside these rolls.
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
melee = read('source/creatureskill/CreatureSkillMeleeFight.cpp')
launch = read('source/creatureskill/CreatureSkillMissileLaunch.cpp')
one_hit = read('source/entities/MissileOneHit.cpp')
roll = function_body(creature, 'DefenceChance::Outcome Creature::rollMeleeDefence(bool missile) const')
for needle in ('getMeleeDodgeParry()', 'getIsOnServerMap()', 'isAlive()', 'isKo()',
               'DefenceChance::isShieldMesh(mesh)', 'DefenceChance::isParryWeaponMesh(mesh)',
               'DefenceChance::dodgeChance(', 'DefenceChance::parryChance(', 'DefenceChance::decide(',
               'Random::Double(0.0, 100.0)', 'getDodgeBase()', 'getDodgePerLevel()', 'getDodgeMax()', 'getParryBase()',
               'getParryPerLevel()', 'getParryMax()', 'getShieldBlockBase()', 'getShieldBlockPerLevel()',
               'getShieldBlockMax()', 'getLevel()'):
    assert needle in roll, needle
# One roll only, and only the configured mechanism; a block also needs a real shield.
assert roll.count('Random::Double(0.0, 100.0)') == 1
for kind in ('none', 'dodge', 'parry', 'block'):
    assert 'CreatureDefinition::Avoidance::' + kind in roll
assert '(missile && avoidance == CreatureDefinition::Avoidance::parry)' in roll
assert 'CreatureDefinition::Avoidance::block && hasShield' in roll
assert roll.index('getMeleeDodgeParry()') < roll.index('Random::Double')
for forbidden in ('takeDamage', 'mHp', 'setAnimation', 'mCooldown', 'mWarmup'):
    assert forbidden not in roll, forbidden
assert 'DefenceChance::Outcome rollMeleeDefence(bool missile = false) const;' in creature_h
assert '#include "entities/DefenceChance.h"' in creature_h

body = function_body(melee, 'bool CreatureSkillMeleeFight::tryUseFight(')
assert 'rollMeleeDefence(false)' in body
# Before the damage is calculated; with a defence the blow has no damage but takeDamage still runs
assert body.index('rollAttackHit(') < body.index('rollMeleeDefence(false)') < body.index('takeDamage(')
assert 'if(!missed)' in body and 'if(missed || defence != DefenceChance::none)' in body
assert 'phyAtk = 0.0;' in body and 'magAtk = 0.0;' in body and 'eleAtk = 0.0;' in body
assert body.index('phyAtk = 0.0;') < body.index('takeDamage(')
assert body.index('takeDamage(') < body.index('fireHitDefended(') < body.index('CosmeticEventType::meleeResult')
# Only creatures can defend (the check is inside the creature branch), the attack timing is outside this function
assert body.index('getObjectType() == GameEntityType::creature') < body.index('rollAttackHit(')
accuracy = function_body(creature, 'bool Creature::rollAttackHit(')
assert accuracy.index('if(possessed)') < accuracy.index('Random::Double(0.0, 100.0)')
assert 'DefenceChance::hitChance(attackRating, getDefenseRating(),' in accuracy
use_attack = function_body(creature, 'void Creature::useAttack(')
assert 'skillData.mWarmup = skillData.mSkill->getWarmupNbTurns();' in use_attack
assert 'skillData.mCooldown = getAppliedCooldownNbTurns(*skillData.mSkill);' in use_attack
# The older kind still follows for old clients: no damage means "blocked" for them
assert 'event.mValue = MELEE_RESULT_NO_DAMAGE;' in body
# Only single-target creature shots roll; traps and area blasts bypass accuracy and avoidance.
missile_hurt = function_body(one_hit, 'void MissileOneHit::hurt(')
assert 'isCreatureShot() && usesCreatureHitRoll()' in missile_hurt
assert missile_hurt.index('rollAttackHit(') < missile_hurt.index('rollMeleeDefence(true)') < missile_hurt.index('takeDamage(')
assert 'isPossessedShot()' in missile_hurt and 'mShotAttackRating' in missile_hurt and 'mShotAttackerLevel' in missile_hurt

fire = function_body(creature, 'void Creature::fireHitDefended(')
for needle in ('getHitEvents()', 'CosmeticEventType::hitResult', 'CosmeticHitResult::parried', 'CosmeticHitResult::dodged',
               'CosmeticHitResult::shieldBlocked', 'event.mText = missile ? "missile" : "melee";',
               'event.mValue2 = 0;', 'fireCosmeticEvent(event, false)'):
    assert needle in fire, needle
for forbidden in ('takeDamage', 'mHp', 'setAnimation'):
    assert forbidden not in fire, forbidden

# ---------------------------------------------------------------------------------------------------------------
# Network: the kind stays hitResult = 17; shield block appends a result behind all existing results.
event_h = read('source/network/CosmeticEvent.h')
event_cpp = read('source/network/CosmeticEvent.cpp')
results = re.findall(r'^\s+(\w+) = (\d+),?\s*$', event_h.split('enum class CosmeticHitResult')[1].split('};')[0], re.M)
assert results == [('hit', '0'), ('glanced', '1'), ('blocked', '2'), ('missed', '3'), ('dodged', '4'), ('parried', '5'),
                   ('shieldBlocked', '6')], results
assert 'hitResult = 17' in event_h and 'casinoResult' not in event_h
assert '9 to 16 are kept free' in event_h
known = function_body(event_cpp, 'bool CosmeticEvent::isKnownType() const')
assert 'CosmeticEventType::hitResult' in known
# An old client skips the unknown result values (default branch of noteHitResult) and reads the older kind
visuals = read('source/render/CreatureWeaponVisuals.cpp')
handler = function_body(visuals, 'void noteHitResult(')
assert 'default:\n            // A result this client does not know' in handler
# Nothing about it in the saved game
for path in ('source/entities/Creature.cpp',):
    text = read(path)
    for stream_function in ('void Creature::exportToStream', 'bool Creature::importFromStream'):
        stream_body = function_body(text, stream_function)
        assert 'Defence' not in stream_body and 'dodge' not in stream_body.lower() and 'parr' not in stream_body.lower()

# ---------------------------------------------------------------------------------------------------------------
# Client: only from the hitResult, nothing is guessed
for result_name, defender, attacker in (('dodged', 'BlowDodged', 'BlowMissed'), ('parried', 'BlowParried', 'BlowDeflected'),
                                        ('shieldBlocked', 'BlowParried', 'BlowDeflected')):
    case = handler[handler.index('case static_cast<int32_t>(CosmeticHitResult::' + result_name + ')'):]
    case = case[:case.index('break;')]
    assert 'queueReaction(reactions, target->getName(), "' + defender + '"' in case, result_name
    assert 'queueReaction(reactions, attacker->getName(), "' + attacker + '"' in case, result_name
    assert 'sSoftened[event.mObject] = now;' in case, result_name
missed_case = handler[handler.index('case static_cast<int32_t>(CosmeticHitResult::missed)'):]
missed_case = missed_case[:missed_case.index('break;')]
assert 'attacker->getName(), "BlowMissed"' in missed_case and 'target->getName()' not in missed_case
reactions_cfg = read('config/creatureReactions.cfg')
for name in ('BlowDodged', 'BlowMissed', 'BlowParried', 'BlowDeflected'):
    match = re.search(r'\[Event\]\s*\n\s+Name\s+' + name + r'\s*\n(.*?)\[/Event\]', reactions_cfg, re.S)
    assert match, name
    variants = match.group(1).count('[Variant]')
    assert variants >= (3 if name == 'BlowDodged' else 2), (name, variants)
assert 'ReactionSparks' in re.search(r'Name\s+BlowParried\s*\n(.*?)\[/Event\]', reactions_cfg, re.S).group(1)

# The parrying defender raises its weapon with an own clip where the skeleton has one; the sparks and the small movement stay as
# the fall back for the creatures without it
parried = re.search(r'Name\s+BlowParried\s*\n(.*?)\[/Event\]', reactions_cfg, re.S).group(1)
assert parried.count('Clip    WeaponParry') == parried.count('[Variant]') >= 2
assert 'Motion' in parried and 'Effect  ReactionSparks' in parried
for skeleton in ('Adventurer', 'Cultist', 'Dwarf2', 'Gnome', 'Goblin', 'Knight', 'LavaSpawn', 'Monk', 'NatureMonster', 'Orc', 'RunelordDwarf'):
    data = (root / 'models' / (skeleton + '.skeleton')).read_bytes()
    assert b'WeaponParry' in data and b'Idle' in data and b'Attack1' in data, skeleton

# The Goblin carries its knife, which is part of the model (visual and parry only: the knife adds no damage and no defence yet).
# Its mesh name is classified like the code does: client weaponKind and server isShieldMesh / isParryWeaponMesh
# Mirror the existing client mesh classification for compatibility checks.
def mirror_weapon_kind(mesh):
    mesh = mesh.lower()
    for kind, words in (('Shield', ('shield',)), ('Bow', ('bow',)), ('Axe', ('axe',)), ('Hammer', ('hammer', 'mace')),
                        ('Spear', ('spear', 'lance', 'pike')), ('Dagger', ('dagger', 'knife')), ('Staff', ('staff', 'wand'))):
        if any(word in mesh for word in words):
            return kind
    return 'Sword'


# Mirror the server's carried weapon filter for parry-configured creature types.
def mirror_is_parry_weapon(mesh):
    mesh = mesh.lower()
    return not ('shield' in mesh or 'bow' in mesh or 'staff' in mesh or 'wand' in mesh)


combat = read('source/render/CreatureCombatReactions.cpp')
kind_body = function_body(combat, 'std::string weaponKind(const Weapon* weapon)')
for word in ('shield', 'bow', 'axe', 'hammer', 'mace', 'spear', 'lance', 'pike', 'dagger', 'knife', 'staff', 'wand'):
    assert 'contains(mesh, "' + word + '")' in kind_body, word
creatures_cfg = read('config/creatures.cfg')
goblin = re.search(r'\[Creature\]\s*\n\s+Name\s+Goblin\s*\n(.*?)\[/Creature\]', creatures_cfg, re.S).group(1)
assert re.search(r'^\s+WeaponSpawnR\s+Knife\s*$', goblin, re.M)
assert not re.search(r'^\s+WeaponSpawnL\s+(?!none)\S+', goblin, re.M)
equipment = re.search(r'\[Equipment\]\s*\n\s+Name\s+Knife\s*\n(.*?)\[/Equipment\]', read('config/equipments.cfg'), re.S).group(1)
knife_mesh = re.search(r'MeshName\s+(\S+)', equipment).group(1)
# the knife is a submesh of the Goblin model, so the weapon attaches no mesh of its own
assert knife_mesh == 'none'
assert b'GoblinKnife' in (root / 'models' / 'Goblin.mesh').read_bytes()
assert mirror_is_parry_weapon(knife_mesh)
# no shield, so the parry chance is the weapon one: level 1 and level 10 as in the table above
assert abs(parry_chance(1, mirror_is_parry_weapon(knife_mesh), *PARRY) - 3.5) < 1e-9
assert abs(parry_chance(10, mirror_is_parry_weapon(knife_mesh), *PARRY) - 8.0) < 1e-9
for stat in ('PhysicalDefense', 'MagicalDefense', 'ElementDefense'):
    assert re.search(r'^\s+' + stat + r'\s+0\s*$', equipment, re.M), stat


# Generate the optional integrator-only production-header probe with the same typed avoidance table.
def probe_source():
    cases = []
    for level, (dodge, parry, shield) in TABLE.items():
        cases.append('    check(near(DefenceChance::dodgeChance(%d, 3.0, 0.5, 15.0), %r), "dodge level %d");' % (level, dodge, level))
        cases.append('    check(near(DefenceChance::parryChance(%d, true, 3.0, 0.5, 15.0), %r), "parry level %d");'
                     % (level, parry, level))
        cases.append('    check(near(DefenceChance::dodgeChance(%d, 6.0, 1.0, 25.0), %r), "shield level %d");'
                     % (level, shield, level))
        cases.append('    check(DefenceChance::parryChance(%d, false, 3.0, 0.5, 15.0) == 0.0, "no weapon level %d");'
                     % (level, level))
    return '''#include "entities/DefenceChance.h"
#include <cmath>
#include <iostream>

static int checks = 0;
static int failures = 0;

// Check one production-header result and count failures.
static void check(bool ok, const char* what)
{
    ++checks;
    if(!ok)
    {
        ++failures;
        std::cout << "FAIL: " << what << '\\n';
    }
}

// Compare computed chances with their expected values.
static bool near(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

// Run the production-header chance and equipment cases.
int main()
{
''' + '\n'.join(cases) + '''
    check(DefenceChance::decide(DefenceChance::dodged, 10.0, 5.0) == DefenceChance::dodged, "selected dodge");
    check(DefenceChance::decide(DefenceChance::parried, 10.0, 5.0) == DefenceChance::parried, "selected parry");
    check(DefenceChance::decide(DefenceChance::shieldBlocked, 10.0, 5.0) == DefenceChance::shieldBlocked, "selected block");
    check(DefenceChance::decide(DefenceChance::dodged, 10.0, 10.0) == DefenceChance::none, "boundary lands");
    check(near(DefenceChance::hitChance(300.0, 100.0, 1, 1), 75.0), "same-level hit chance");
    check(near(DefenceChance::hitChance(1.0, 100000.0, 1, 30), 5.0), "minimum hit chance");
    check(near(DefenceChance::hitChance(100000.0, 1.0, 30, 1), 95.0), "maximum hit chance");
    check(DefenceChance::isShieldMesh("woodenshield.mesh") && !DefenceChance::isParryWeaponMesh("woodenshield.mesh"), "shield");
    check(!DefenceChance::isParryWeaponMesh("longbow.mesh") && !DefenceChance::isParryWeaponMesh("staff.mesh"), "bow and staff");
    check(DefenceChance::isParryWeaponMesh("sword.mesh") && DefenceChance::isParryWeaponMesh("axe.mesh"), "sword and axe");
    std::cout << "CHECKS=" << checks << " FAILURES=" << failures << '\\n';
    return failures ? 1 : 0;
}
'''


if '--probe' in sys.argv:
    with tempfile.TemporaryDirectory(prefix='odp-defence-') as directory:
        work = Path(directory)
        (work / 'probe.cpp').write_text(probe_source())
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{root / "source"}', 'probe.cpp', '/Feprobe.exe'],
                       cwd=work, check=True)
        subprocess.run([str(work / 'probe.exe')], cwd=work, check=True)

print('check_defence_chance: ok')
