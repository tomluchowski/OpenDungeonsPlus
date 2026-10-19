"""Weapon cooldown: every weapon in equipments.cfg names the melee cooldown, non-weapons have none.

Static source check (no compiler, no game): the config field, its loader/writer/packet, the choice of the
weapon value for melee (larger of two, definition value without a weapon) and the recovery clock."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


# Equipment blocks: name -> list of (key, value) stat lines. Shields (and other pure defence gear) are the
# non-weapons; an item counts as a shield when its name contains "shield" in any letter case.
text = read('config/equipments.cfg')
# the header comment mentions the tags too, so only the part after the opening tag is parsed
definitions_start = re.search(r'^\[EquipmentDefinitions\]', text, re.M).start()
body = text[definitions_start:]
blocks = re.findall(r'\[Equipment\](.*?)\[/Equipment\]', body, re.S)
assert blocks, 'no equipment found'
weapon_names = []
for block in blocks:
    name = re.search(r'^\s+Name\s+(\S+)', block, re.M).group(1)
    stats = dict(re.findall(r'^\s+(\w+)\s+(\S+)\s*$', block, re.M))
    is_shield = 'shield' in name.lower()
    if is_shield:
        assert 'CooldownNbTurns' not in stats, name + ' is not a weapon and must have no cooldown field'
        assert float(stats['PhysicalDamage']) == 0 and float(stats['MagicalDamage']) == 0 \
            and float(stats['ElementDamage']) == 0, name + ' deals damage, so it is not a plain shield'
        continue
    assert 'CooldownNbTurns' in stats, name + ' is a weapon and needs CooldownNbTurns'
    assert re.fullmatch(r'[1-9][0-9]*', stats['CooldownNbTurns']), name + ' needs a positive integer cooldown'
    weapon_names.append(name)
assert len(weapon_names) >= 10, weapon_names
header = text[:definitions_start]
assert 'CooldownNbTurns' in header and 'larger value' in header and 'non-weapons' in header

# Loader, writer and network packet of the weapon definition keep the field in step
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

# The melee cooldown comes from the weapons: larger of the two hands, definition value without any
creature = read('source/entities/Creature.cpp')
applied = function_body(creature, 'uint32_t Creature::getAppliedCooldownNbTurns(')
assert 'dynamic_cast<const CreatureSkillMeleeFight*>(&skill) == nullptr' in applied, 'only melee uses weapons'
assert 'const Weapon* weapons[2] = {mWeaponL, mWeaponR};' in applied
assert 'std::max(weaponCooldown, weapon->getCooldownNbTurns())' in applied, 'larger of two weapons'
assert 'if(weapon == nullptr)' in applied
assert 'if(weaponCooldown == 0)' in applied and applied.count('return definitionCooldown;') == 2, \
    'no weapon cooldown means the definition cooldown applies'

# Both the stored cooldown and the recovery clock use the applied value
use_attack = function_body(creature, 'void Creature::useAttack(')
assert 'skillData.mCooldown = getAppliedCooldownNbTurns(*skillData.mSkill);' in use_attack
assert 'mAttackRecoveryDuration = std::max(skillData.mWarmup, skillData.mCooldown);' in use_attack
assert 'mAttackRecoveryTurns = mAttackRecoveryDuration;' in use_attack
assert 'CreatureSkillData skillData(skill, getAppliedCooldownNbTurns(*skill), 0);' in creature
# Support skills (not melee, no weapon cooldown) keep their definition cooldown
assert creature.count('skillData.mCooldown = skillData.mSkill->getCooldownNbTurns();') == 2
# The clock reads the stored duration, nothing else
overlay = read('source/render/CreatureOverlayStatus.cpp')
assert 'mCreature->getAttackRecoveryDuration()' in overlay
print('check_weapon_cooldown: ok')
