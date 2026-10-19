"""Static armament config, asset, skeleton and production-source checks; compiles nothing."""
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
ASSIGNMENTS = {
    'Orc': 'BasicAxeOrc', 'Troll': 'Club', 'PitDemon': 'Trident', 'Cultist': 'RitualDagger',
    'Gnome': 'Wand', 'Adventurer': 'BasicSword', 'Skeleton': 'BasicSword', 'Dwarf1': 'HandAxe',
    'Dwarf2': 'BasicHammerDwarf2', 'Wizard': 'Staff', 'Lich': 'StaffLich', 'LizardMan': 'Staff',
}
UNCHANGED = ('Kobold', 'DwarfWorker', 'LavaSpawn', 'Goblin', 'Monk', 'Knight', 'DarkElf', 'Elf',
             'RunelordDwarf', 'NatureMonster', 'Defender')
UNARMED = ('Rat', 'Spider', 'Roach', 'CaveHornet', 'Slime', 'Scarab', 'TentacleAlbine', 'TentacleGreen',
           'Wyvern', 'Dragon', 'Champion', 'Kreatur')
TYPES = {
    'sword': (('RustySword', 'BasicSword', 'ShortSword', 'Sabre', 'T2SwordGood'), 1),
    'dagger': (('Knife', 'RitualDagger'), 1),
    'axe': (('BasicAxe', 'BasicAxeOrc'), 2),
    'staff': (('Staff', 'StaffLich'), 2), 'bow': (('Bow',), 2), 'crossbow': (('Crossbow',), 2),
    'tool': (('KoboldPick', 'Pickaxe'), 1), 'club': (('Club',), 1), 'spear': (('Trident',), 2),
    'wand': (('Wand',), 1),
}
EXPECTED_TYPES = {name: (kind, hands) for kind, (names, hands) in TYPES.items() for name in names}
EXPECTED_TYPES.update({name: ('sword', 2) for name in ('Longsword', 'T3SwordGood', 'T4SwordGood', 'DefenderWeapon')})
EXPECTED_TYPES.update({name: ('hammer', 1) for name in ('BasicHammer', 'BasicHammerDwarf2')})
EXPECTED_TYPES.update({name: ('hammer', 2) for name in ('T2HammerGood', 'T3HammerGood', 'T4HammerGood')})
EXPECTED_TYPES['HandAxe'] = ('axe', 1)
EXPECTED_TYPES.update({name: ('shield', 1) for name in ('RustyShield', 'Roundshield', 'BasicShield')})
EXPECTED_TYPES.update({f'T{tier}Shield{side}': ('shield', 1) for tier in (2, 3, 4) for side in ('Good', 'Evil')})
PARENTS = {
    'Orc': ('Orc', 'hand_r'), 'Troll': ('Troll', 'arm.R'), 'Cultist': ('Cultist', 'hand.R'),
    'Gnome': ('Gnome', 'hand_r'), 'Adventurer': ('Adventurer', 'Hand_R'),
    'Dwarf1': ('Dwarf1', 'hand_r'), 'Dwarf2': ('Dwarf2', 'hand_r'), 'Wizard': ('Wizard', 'hand_r'),
    'LizardMan': ('Lizardman', 'Hand_R'), 'Skeleton': ('skeleton', 'itemRightHand'),
    'Lich': ('lich', 'itemRightHand'), 'PitDemon': ('PitDemon', 'Hand_R'),
}
NEW_WEAPONS = {'HandAxe': 2, 'Club': 2, 'Trident': 4, 'RitualDagger': 1, 'Wand': 2}


# Read a tracked production input without invoking any asset tools.
def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


# Parse named equipment or creature definitions, ignoring comments and nested skill rows.
def definitions(text, tag):
    text = text[text.rindex('[' + tag + 'Definitions]'):]
    result = {}
    for block in re.findall(r'\[' + tag + r'\](.*?)\[/' + tag + r'\]', text, re.S):
        fields = dict(re.findall(r'^\s+(\w+)\s+([^\s#]+)\s*$', block, re.M))
        assert fields['Name'] not in result, fields['Name']
        result[fields['Name']] = fields
    return result


# Extract a source function with balanced braces so nested blocks remain covered.
def body(source, signature):
    start = source.index('{', source.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start + 1:end - 1]


# Read Ogre bone and hierarchy chunks directly; string bytes are outside their recorded sizes.
def skeleton_bones(path):
    data = path.read_bytes()
    assert struct.unpack_from('<H', data)[0] == 0x1000, path
    offset = data.index(b'\n') + 1
    bones, parents = {}, {}
    while offset < len(data):
        kind, size = struct.unpack_from('<HI', data, offset)
        assert size >= 6, (path, offset)
        if kind == 0x2000:
            end = data.index(b'\n', offset + 6)
            name = data[offset + 6:end].decode('utf-8')
            handle = struct.unpack_from('<H', data, end + 1)[0]
            assert handle not in bones, (path, handle)
            bones[handle] = (name, struct.unpack_from('<7f', data, end + 3))
            offset += size + end - (offset + 6) + 1
        elif kind == 0x3000:
            child, parent = struct.unpack_from('<HH', data, offset + 6)
            assert child not in parents, (path, child)
            parents[child] = parent
            offset += size
        elif kind == 0x1010:
            offset += size
        else:
            assert kind in (0x4000, 0x5000), (path, hex(kind))
            break
    return bones, parents


# Check exact equipment metadata, neutral new damage, and unchanged values of scaled copies.
def check_equipment(equipment):
    assert set(equipment) == set(EXPECTED_TYPES), set(equipment) ^ set(EXPECTED_TYPES)
    for name, (kind, hands) in EXPECTED_TYPES.items():
        assert equipment[name].get('WeaponType') == kind, (name, 'WeaponType')
        assert equipment[name].get('Hands') == str(hands), (name, 'Hands')
    for name, cooldown in NEW_WEAPONS.items():
        item = equipment[name]
        assert item['MeshName'] == name + '.mesh', name
        assert int(item['CooldownNbTurns']) == cooldown, name
        for field in ('PhysicalDamage', 'MagicalDamage', 'ElementDamage', 'PhysicalDefense',
                      'MagicalDefense', 'ElementDefense', 'PhysicalDamageMin', 'MagicalDamageMin',
                      'ElementDamageMin', 'AttackRating', 'DefenseRating'):
            assert float(item.get(field, '0')) == 0, (name, field)
        assert tuple(float(item[key]) for key in ('ParryBase', 'ParryPerLevel', 'ParryMax')) == (3, .5, 15), name
    for name, original in (('BasicAxeOrc', 'BasicAxe'), ('BasicHammerDwarf2', 'BasicHammer'), ('StaffLich', 'Staff')):
        expected = dict(equipment[original], Name=name, MeshName=name + '.mesh')
        assert equipment[name] == expected, (name, equipment[name], expected)


# Check all requested assignments and reject any occupied second hand beside a two-handed item.
def check_creatures(creatures, equipment):
    baseline = subprocess.run(['git', 'show', 'feature/combat-rebalance:config/creatures.cfg'], cwd=ROOT,
                              stdin=subprocess.DEVNULL, capture_output=True, check=True, timeout=30).stdout.decode('utf-8')
    previous = definitions(baseline, 'Creature')
    for name, weapon in ASSIGNMENTS.items():
        assert creatures[name].get('WeaponSpawnR') == weapon, name
        assert creatures[name].get('WeaponSpawnL', 'none') == 'none', name
    for name in UNCHANGED:
        for hand in ('R', 'L'):
            assert creatures[name].get('WeaponSpawn' + hand, 'none') == previous[name].get('WeaponSpawn' + hand, 'none'), name
    for name in UNARMED:
        assert all(creatures[name].get('WeaponSpawn' + hand, 'none') == 'none' for hand in ('R', 'L')), name
    for name, creature in creatures.items():
        held = [creature.get('WeaponSpawn' + hand, 'none') for hand in ('R', 'L')]
        for weapon in held:
            if weapon != 'none':
                assert weapon in equipment, (name, weapon)
                assert equipment[weapon]['Hands'] != '2' or held.count('none') == 1, (name, held)
    for name, (filename, parent_name) in PARENTS.items():
        bones, parents = skeleton_bones(ROOT / 'models' / (filename + '.skeleton'))
        mounts = [handle for handle, (bone_name, _) in bones.items() if bone_name == 'Weapon_R']
        assert len(mounts) == 1, name
        assert bones[parents[mounts[0]]][0] == parent_name, (name, parent_name)


# Require every delivered asset/tool and its explicit credit row, plus the shared material binding.
def check_assets():
    paths = ['models/' + name + '.mesh' for name in NEW_WEAPONS]
    paths += ['models/' + name + '.mesh' for name in ('BasicAxeOrc', 'BasicHammerDwarf2', 'StaffLich')]
    paths += ['models/' + name + '.skeleton' for name in ('StaffLich', 'skeleton', 'lich', 'PitDemon')]
    paths += ['materials/scripts/ArmamentWeapons.material', 'materials/textures/ArmamentWeaponsSurface.png',
              'tools/gen_armament_weapons.py', 'tools/gen_scaled_weapons.py', 'tools/add_weapon_bones.py',
              'tools/blender-assets/armament_blend.py', 'tools/blender-assets/armament_preview.py',
              'source/tests/check_creature_armament.py']
    credits = read('CREDITS')
    for path in paths:
        assert (ROOT / path).is_file(), path
        assert re.search(r'^' + re.escape(path) + r'\s+', credits, re.M), 'Missing credit: ' + path
    material = read('materials/scripts/ArmamentWeapons.material')
    assert re.search(r'^material\s+ArmamentWeapons\s*$', material, re.M)
    assert re.search(r'\btexture\s+ArmamentWeaponsSurface\.png\b', material)
    texture = (ROOT / 'materials/textures/ArmamentWeaponsSurface.png').read_bytes()
    assert texture[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', texture[16:24]) == (64, 64)
    for name in NEW_WEAPONS:
        assert b'ArmamentWeapons' in (ROOT / 'models' / (name + '.mesh')).read_bytes(), name
    assert b'StaffLich.skeleton' in (ROOT / 'models/StaffLich.mesh').read_bytes()


# Check actual client/server type mappings, serialization and the creature-only timing compensation path.
def check_source():
    weapon = read('source/entities/Weapon.cpp')
    for field in ('WeaponType', 'Hands'):
        assert 'nextParam == "' + field + '"' in weapon
        assert 'file << "    ' + field + '\\t"' in weapon
        assert 'os << weapon->m' + field + ';' in weapon
        assert 'is >> weapon->m' + field in weapon
    fallback = body(weapon, 'std::string Weapon::getWeaponType() const')
    assert 'if(!mWeaponType.empty())' in fallback and 'return mWeaponType;' in fallback
    for keyword in ('shield', 'bow', 'axe', 'hammer', 'mace', 'spear', 'lance', 'pike', 'dagger', 'knife', 'staff', 'wand'):
        assert 'mesh.find("' + keyword + '")' in fallback, keyword
    reaction = body(read('source/render/CreatureCombatReactions.cpp'), 'std::string weaponKind(')
    assert 'weapon->getWeaponType()' in reaction and 'getMeshName' not in reaction
    actual = {}
    for condition, kind in re.findall(r'if\((.*?)\)\s*return "(\w+)";', reaction):
        for name in re.findall(r'type == "(\w+)"', condition):
            actual[name] = kind
    expected = {'axe': 'Axe', 'shield': 'Shield', 'bow': 'Bow', 'crossbow': 'Bow',
                'club': 'Hammer', 'mace': 'Hammer', 'hammer': 'Hammer', 'scepter': 'Hammer',
                'spear': 'Spear', 'polearm': 'Spear', 'javelin': 'Spear', 'dagger': 'Dagger',
                'throwing': 'Dagger', 'claw': 'Dagger', 'staff': 'Staff', 'wand': 'Staff', 'orb': 'Staff'}
    assert actual == expected, (actual, expected)
    assert reaction.rstrip().endswith('return "Sword";')
    defence = read('source/entities/DefenceChance.h')
    assert body(defence, 'inline bool isShieldType(').strip() == 'return type == "shield";'
    parry = body(defence, 'inline bool isParryWeaponType(')
    assert set(re.findall(r'type == "(\w+)"', parry)) == {
        'sword', 'axe', 'dagger', 'club', 'mace', 'hammer', 'scepter', 'spear', 'polearm'}
    creature = read('source/entities/Creature.cpp')
    assert 'const std::string type = weapons[i]->getWeaponType();' in creature
    assert 'DefenceChance::isShieldType(type)' in creature
    assert 'DefenceChance::isParryWeaponType(type)' in creature
    melee = read('source/creatureskill/CreatureSkillMeleeFight.cpp')
    timing = melee[melee.index('uint32_t weaponCooldown = 0;'):melee.index('creatureDamage = CombatDamage::roll')]
    assert 'std::max(weaponCooldown, weapon->getCooldownNbTurns())' in timing
    assert 'weaponCooldown > getCooldownNbTurns() && getCooldownNbTurns() > 0' in timing
    assert 'static_cast<double>(weaponCooldown) / getCooldownNbTurns() : 1.0' in timing
    for channel in ('phyMin', 'phyMax', 'magMin', 'magMax', 'eleMin', 'eleMax'):
        assert channel + ' *= timingFactor;' in timing, channel
    assert melee.rfind('if(attackedObject->getObjectType() == GameEntityType::creature)', 0,
                       melee.index('uint32_t weaponCooldown = 0;')) >= 0
    cooldown = body(creature, 'double Creature::getAppliedCooldownNbTurns(')
    assert 'baseCooldown = std::max(baseCooldown, weaponCooldown);' in cooldown


# Run all static checks with clear failures while leaving build and runtime verification separate.
def main():
    equipment = definitions(read('config/equipments.cfg'), 'Equipment')
    creatures = definitions(read('config/creatures.cfg'), 'Creature')
    check_equipment(equipment)
    check_creatures(creatures, equipment)
    check_assets()
    check_source()
    print('PASS: creature armament assignments, types, hands, skeleton mounts, assets, credits and source wiring')


if __name__ == '__main__':
    main()
