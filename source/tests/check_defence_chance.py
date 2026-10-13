"""Dodging and parrying of melee blows: chances (Python mirror of source/entities/DefenceChance.h with a table),
order of the dice, config keys, server wiring, hitResult results and the client reactions, compatibility.
Pure Python wiring check, compiles nothing. With --probe it compiles source/entities/DefenceChance.h with cl and
checks the same table in C++ (not part of the normal run)."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


# ---------------------------------------------------------------------------------------------------------------
# The rules, mirrored from DefenceChance.h (percent)
def dodge_chance(level, base, per_level, maximum):
    return max(0.0, min(maximum, base + per_level * level))


def parry_chance(level, has_weapon, has_shield, base, per_level, maximum, shield_base, shield_per_level, shield_max):
    if not has_weapon:
        return 0.0
    if has_shield:
        return max(0.0, min(shield_max, shield_base + shield_per_level * level))
    return max(0.0, min(maximum, base + per_level * level))


def decide(dodge, parry, dodge_roll, parry_roll):
    if dodge_roll < dodge:
        return 'dodged'
    if parry_roll < parry:
        return 'parried'
    return 'none'


DODGE = (3.0, 0.5, 15.0)
PARRY = (3.0, 0.5, 15.0)
SHIELD = (6.0, 1.0, 25.0)

# level: (dodge, parry with weapon, parry with weapon and shield)
TABLE = {
    1: (3.5, 3.5, 7.0),
    10: (8.0, 8.0, 16.0),
    24: (15.0, 15.0, 25.0),
    100: (15.0, 15.0, 25.0),
}

for level, (dodge, parry, shield) in TABLE.items():
    assert abs(dodge_chance(level, *DODGE) - dodge) < 1e-9, (level, 'dodge')
    assert abs(parry_chance(level, True, False, *PARRY, *SHIELD) - parry) < 1e-9, (level, 'parry')
    assert abs(parry_chance(level, True, True, *PARRY, *SHIELD) - shield) < 1e-9, (level, 'parry shield')
    assert parry_chance(level, False, False, *PARRY, *SHIELD) == 0.0, (level, 'no weapon')
    assert parry_chance(level, False, True, *PARRY, *SHIELD) == 0.0, (level, 'shield alone does not parry')

# Order of the dice: dodging first; parrying only when the creature did not dodge
assert decide(10.0, 10.0, 5.0, 5.0) == 'dodged'
assert decide(10.0, 10.0, 15.0, 5.0) == 'parried'
assert decide(10.0, 10.0, 15.0, 15.0) == 'none'
assert decide(0.0, 0.0, 0.0, 0.0) == 'none'
assert decide(3.5, 0.0, 99.0, 0.0) == 'none'

# ---------------------------------------------------------------------------------------------------------------
# DefenceChance.h: pure (no game includes), the same formulas, the same order
header = read('source/entities/DefenceChance.h')
includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', header)
assert sorted(includes) == ['algorithm', 'cstdint', 'string'], includes
assert 'inline double dodgeChance(uint32_t level, double base, double perLevel, double max)' in header
assert 'std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)))' in header
assert 'std::max(0.0, std::min(shieldMax, shieldBase + shieldPerLevel * static_cast<double>(level)))' in header
assert 'std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)))' in header
parry_body = function_body(header, 'inline double parryChance(')
assert parry_body.index('if(!hasWeapon)') < parry_body.index('if(hasShield)'), 'a shield alone gives no parry'
decide_body = function_body(header, 'inline Outcome decide(')
assert decide_body.index('return dodged;') < decide_body.index('return parried;') < decide_body.index('return none;')
assert decide_body.index('dodgeRoll < dodgePercent') < decide_body.index('parryRoll < parryPercent')

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
KEYS = (('DodgeBase', '3', 'mDodgeBase(3.0)', 100), ('DodgePerLevel', '0.5', 'mDodgePerLevel(0.5)', 10),
        ('DodgeMax', '15', 'mDodgeMax(15.0)', 100), ('ParryBase', '3', 'mParryBase(3.0)', 100),
        ('ParryPerLevel', '0.5', 'mParryPerLevel(0.5)', 10), ('ParryMax', '15', 'mParryMax(15.0)', 100),
        ('ParryShieldBase', '6', 'mParryShieldBase(6.0)', 100), ('ParryShieldPerLevel', '1', 'mParryShieldPerLevel(1.0)', 10),
        ('ParryShieldMax', '25', 'mParryShieldMax(25.0)', 100))
assert re.search(r'^# .*\n(?:# .*\n)?\s+MeleeDodgeParry\t1\s*$', global_cfg, re.M), 'MeleeDodgeParry'
assert 'nextParam == "MeleeDodgeParry"' in config_cpp and 'mMeleeDodgeParry(true)' in config_cpp
assert 'inline bool getMeleeDodgeParry() const' in config_h
for key, value, member, limit in KEYS:
    assert re.search(r'^\s+' + key + r'\t' + re.escape(value) + r'\s*$', global_cfg, re.M), key
    assert 'nextParam == "' + key + '"' in config_cpp and member in config_cpp, key
    assert 'std::max(0.0, std::min(%d.0, m%s))' % (limit, key) in config_h, key
    assert 'inline double get' + key + '() const' in config_h, key

# ---------------------------------------------------------------------------------------------------------------
# Server: decided by the server before the damage, melee only, nothing about timing changes
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
melee = read('source/creatureskill/CreatureSkillMeleeFight.cpp')
launch = read('source/creatureskill/CreatureSkillMissileLaunch.cpp')
one_hit = read('source/entities/MissileOneHit.cpp')
roll = function_body(creature, 'DefenceChance::Outcome Creature::rollMeleeDefence() const')
for needle in ('getMeleeDodgeParry()', 'getIsOnServerMap()', 'isAlive()', 'isKo()',
               'DefenceChance::isShieldMesh(mesh)', 'DefenceChance::isParryWeaponMesh(mesh)',
               'DefenceChance::dodgeChance(', 'DefenceChance::parryChance(', 'DefenceChance::decide(',
               'Random::Double(0.0, 100.0)', 'getDodgeBase()', 'getDodgePerLevel()', 'getDodgeMax()', 'getParryBase()',
               'getParryPerLevel()', 'getParryMax()', 'getParryShieldBase()', 'getParryShieldPerLevel()',
               'getParryShieldMax()', 'getLevel()'):
    assert needle in roll, needle
# The dice are rolled in this order (dodge first), the creature must be a fighter that can move
assert roll.index('dodgeRoll = ') < roll.index('parryRoll = ') < roll.index('DefenceChance::decide(')
assert roll.index('getMeleeDodgeParry()') < roll.index('Random::Double')
for forbidden in ('takeDamage', 'mHp', 'setAnimation', 'mCooldown', 'mWarmup'):
    assert forbidden not in roll, forbidden
assert 'DefenceChance::Outcome rollMeleeDefence() const;' in creature_h
assert '#include "entities/DefenceChance.h"' in creature_h

body = function_body(melee, 'bool CreatureSkillMeleeFight::tryUseFight(')
assert 'rollMeleeDefence()' in body
# Before the damage is calculated; with a defence the blow has no damage but takeDamage still runs
assert body.index('rollMeleeDefence()') < body.index('takeDamage(')
assert 'phyAtk = 0.0;' in body and 'magAtk = 0.0;' in body and 'eleAtk = 0.0;' in body
assert body.index('phyAtk = 0.0;') < body.index('takeDamage(')
assert body.index('takeDamage(') < body.index('fireHitDefended(') < body.index('CosmeticEventType::meleeResult')
# Only creatures can defend (the check is inside the creature branch), the attack timing is outside this function
assert 'getObjectType() == GameEntityType::creature)\n        defence = static_cast<Creature*>(attackedObject)->rollMeleeDefence();' in body
use_attack = function_body(creature, 'void Creature::useAttack(')
assert 'skillData.mWarmup = skillData.mSkill->getWarmupNbTurns();' in use_attack
assert 'skillData.mCooldown = skillData.mSkill->getCooldownNbTurns();' in use_attack
# The older kind still follows for old clients: no damage means "blocked" for them
assert 'event.mValue = MELEE_RESULT_NO_DAMAGE;' in body
# Missiles are untouched
for text in (launch, one_hit):
    assert 'rollMeleeDefence' not in text and 'DefenceChance' not in text
for other in ('MissileObject.cpp', 'MissileOneHit.cpp'):
    assert 'rollMeleeDefence' not in read('source/entities/' + other)

fire = function_body(creature, 'void Creature::fireHitDefended(')
for needle in ('getHitEvents()', 'CosmeticEventType::hitResult', 'CosmeticHitResult::parried', 'CosmeticHitResult::dodged',
               'event.mText = "melee";', 'event.mValue2 = 0;', 'fireCosmeticEvent(event, false)'):
    assert needle in fire, needle
for forbidden in ('takeDamage', 'mHp', 'setAnimation'):
    assert forbidden not in fire, forbidden

# ---------------------------------------------------------------------------------------------------------------
# Network: the kind stays hitResult = 17, two new results behind the old ones, nothing is saved
event_h = read('source/network/CosmeticEvent.h')
event_cpp = read('source/network/CosmeticEvent.cpp')
results = re.findall(r'^\s+(\w+) = (\d+),?\s*$', event_h.split('enum class CosmeticHitResult')[1].split('};')[0], re.M)
assert results == [('hit', '0'), ('glanced', '1'), ('blocked', '2'), ('missed', '3'), ('dodged', '4'), ('parried', '5')], results
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
for result_name, defender, attacker in (('dodged', 'BlowDodged', 'BlowMissed'), ('parried', 'BlowParried', 'BlowDeflected')):
    case = handler[handler.index('case static_cast<int32_t>(CosmeticHitResult::' + result_name + ')'):]
    case = case[:case.index('break;')]
    assert 'queueReaction(reactions, target->getName(), "' + defender + '"' in case, result_name
    assert 'queueReaction(reactions, attacker->getName(), "' + attacker + '"' in case, result_name
    assert 'sSoftened[event.mObject] = now;' in case, result_name
reactions_cfg = read('config/creatureReactions.cfg')
for name in ('BlowDodged', 'BlowMissed', 'BlowParried', 'BlowDeflected'):
    match = re.search(r'\[Event\]\s*\n\s+Name\s+' + name + r'\s*\n(.*?)\[/Event\]', reactions_cfg, re.S)
    assert match, name
    variants = match.group(1).count('[Variant]')
    assert variants >= (3 if name == 'BlowDodged' else 2), (name, variants)
assert 'ReactionSparks' in re.search(r'Name\s+BlowParried\s*\n(.*?)\[/Event\]', reactions_cfg, re.S).group(1)


def probe_source():
    cases = []
    for level, (dodge, parry, shield) in TABLE.items():
        cases.append('    check(near(DefenceChance::dodgeChance(%d, 3.0, 0.5, 15.0), %r), "dodge level %d");' % (level, dodge, level))
        cases.append('    check(near(DefenceChance::parryChance(%d, true, false, 3.0, 0.5, 15.0, 6.0, 1.0, 25.0), %r), "parry level %d");'
                     % (level, parry, level))
        cases.append('    check(near(DefenceChance::parryChance(%d, true, true, 3.0, 0.5, 15.0, 6.0, 1.0, 25.0), %r), "shield level %d");'
                     % (level, shield, level))
        cases.append('    check(DefenceChance::parryChance(%d, false, true, 3.0, 0.5, 15.0, 6.0, 1.0, 25.0) == 0.0, "no weapon level %d");'
                     % (level, level))
    return '''#include "entities/DefenceChance.h"
#include <cmath>
#include <iostream>

static int checks = 0;
static int failures = 0;

static void check(bool ok, const char* what)
{
    ++checks;
    if(!ok)
    {
        ++failures;
        std::cout << "FAIL: " << what << '\\n';
    }
}

static bool near(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

int main()
{
''' + '\n'.join(cases) + '''
    check(DefenceChance::decide(10.0, 10.0, 5.0, 5.0) == DefenceChance::dodged, "dodge first");
    check(DefenceChance::decide(10.0, 10.0, 15.0, 5.0) == DefenceChance::parried, "then parry");
    check(DefenceChance::decide(10.0, 10.0, 15.0, 15.0) == DefenceChance::none, "else the blow lands");
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
