"""Two attack weapons: a creature that carries one in each hand strikes with the arms in turn.

Pure source wiring check (no compiler, no game): client side only, no server, timing, damage or network
involvement, values in config/creatureReactions.cfg, creatures with one weapon (or a shield) unchanged.
The pure rules in TwoWeaponStrike.h are compiled and run only with --probe (needs the MSVC compiler on the path)."""
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


def parse_blocks(text, tag):
    """The blocks [tag] ... [/tag] as lists of word lists (the same line format as the game files)."""
    blocks = []
    current = None
    for line in text.splitlines():
        words = line.split()
        if not words or words[0].startswith('#'):
            continue
        if words[0] == '[' + tag + ']':
            current = []
        elif words[0] == '[/' + tag + ']':
            blocks.append(current)
            current = None
        elif current is not None:
            current.append(words)
    return blocks


def field(block, key):
    for words in block:
        if words[0] == key and len(words) > 1:
            return words[1]
    return None


def weapon_kind(mesh):
    """The same order as weaponKind in CreatureCombatReactions.cpp."""
    mesh = mesh.lower()
    for kind, parts in (('Shield', ('shield',)), ('Bow', ('bow',)), ('Axe', ('axe',)), ('Hammer', ('hammer', 'mace')),
                        ('Spear', ('spear', 'lance', 'pike')), ('Dagger', ('dagger', 'knife')),
                        ('Staff', ('staff', 'wand'))):
        if any(part in mesh for part in parts):
            return kind
    return 'Sword'


STRIKE_KINDS = ('Sword', 'Axe', 'Hammer', 'Spear', 'Dagger')

equipment_mesh = {}
for block in parse_blocks(read('config/equipments.cfg'), 'Equipment'):
    equipment_mesh[field(block, 'Name')] = [w[1] for w in block if w[0] == 'MeshName'][0]


def two_attack_weapons(left, right):
    if left in (None, 'none') or right in (None, 'none'):
        return False
    return weapon_kind(equipment_mesh[left]) in STRIKE_KINDS and weapon_kind(equipment_mesh[right]) in STRIKE_KINDS


# The rule of the game: the pure header, mirrored here (mode constants and the order of the irregular mode)
header = read('source/render/TwoWeaponStrike.h')
modes = dict((m.group(1), int(m.group(2))) for m in re.finditer(r'const uint32_t (MODE_\w+) = (\d);', header))
assert modes == {'MODE_OFF': 0, 'MODE_ALTERNATE': 1, 'MODE_RANDOM': 2}, modes
pattern = [w == 'true' for w in re.search(r'PATTERN\[8\] = \{([^}]*)\}', header).group(1).replace(' ', '').split(',')]
assert len(pattern) == 8
assert '(blowCounter % 2) != 0' in header and 'PATTERN[blowCounter % 8]' in header


def is_left_blow(mode, counter):
    if mode == modes['MODE_ALTERNATE']:
        return counter % 2 != 0
    if mode == modes['MODE_RANDOM']:
        return pattern[counter % 8]
    return False


# Test creature definitions (not part of any game or level list)
fixture_path = 'source/tests/fixtures/two_weapons_test_creatures.cfg'
fixture = parse_blocks(read(fixture_path), 'Creature')
creatures = {}
for block in fixture:
    creatures[field(block, 'Name')] = (field(block, 'WeaponSpawnL'), field(block, 'WeaponSpawnR'))
assert creatures['TestTwoSwords'] == ('Longsword', 'Longsword')
assert two_attack_weapons(*creatures['TestTwoSwords'])
assert not two_attack_weapons(*creatures['TestSwordShield']), 'sword and shield is no two weapon creature'
assert not two_attack_weapons(*creatures['TestBowOnly']), 'a bow is no strike weapon'
assert not two_attack_weapons(*creatures['TestOneSword']), 'one sword is one weapon'
for name in creatures:
    for path in ('config', 'levels', 'source'):
        for file in (root / path).rglob('*'):
            if file.is_file() and file.suffix in ('.cfg', '.level', '.cpp', '.h') and file.name != Path(fixture_path).name:
                assert name not in file.read_text(encoding='utf-8', errors='ignore'), name + ' is used in ' + str(file)

# Alternation: the arms change with every blow
alternate = [is_left_blow(modes['MODE_ALTERNATE'], i) for i in range(8)]
assert alternate == [False, True] * 4, alternate
assert all(not is_left_blow(modes['MODE_OFF'], i) for i in range(16)), 'mode 0 is always the right arm'
irregular = [is_left_blow(modes['MODE_RANDOM'], i) for i in range(32)]
assert any(irregular) and not all(irregular)
run = longest = 1
for a, b in zip(irregular, irregular[1:]):
    run = run + 1 if a == b else 1
    longest = max(longest, run)
assert longest <= 2, 'never more than two on a side in a row'
assert irregular != [i % 2 == 1 for i in range(32)], 'the irregular mode is not the plain alternation'

# Existing creatures: nobody changes (no creature of the game carries two attack weapons today, so nobody
# takes the new path; sword and shield, bow and crossbow creatures stay as they were)
game = parse_blocks(read('config/creatures.cfg'), 'Creature')
dual = [field(b, 'Name') for b in game if two_attack_weapons(field(b, 'WeaponSpawnL'), field(b, 'WeaponSpawnR'))]
assert dual == [], 'a creature of the game has two attack weapons now: ' + str(dual)

# Wiring: combat reactions, render manager, config
reactions = read('source/render/CreatureCombatReactions.cpp')
carries = function_body(reactions, 'bool CreatureCombatReactions::carriesTwoAttackWeapons(')
assert 'getWeaponL()' in carries and 'getWeaponR()' in carries and 'TwoWeaponStrike::isStrikeKind(' in carries
assert 'static bool carriesTwoAttackWeapons(const Creature* creature);' in read('source/render/CreatureCombatReactions.h')

render = read('source/render/RenderManager.cpp')
render_h = read('source/render/RenderManager.h')
assert 'mCreatureAttackSides;' in render_h
assert render.count('mCreatureAttackSides.erase(curCreature);') == 1 and render.count('mCreatureAttackSides.clear();') == 1
variants = render[render.index('combat_attack_anim && dropCreature != nullptr'):]
variants = variants[:variants.index('if(anim == EntityAnimation::die_anim')]
assert 'carriesTwoAttackWeapons(dropCreature)' in variants and 'TwoWeaponStrike::isLeftBlow(twoWeaponMode, nextSide)' in variants
assert 'getTwoWeaponMode()' in variants and 'getTwoWeaponArmStrength()' in variants
assert 'humanoid && (twoWeaponMode != TwoWeaponStrike::MODE_OFF)' in variants
# One weapon: the old condition stays (carriesSword), the side stays the right arm with the scale 1
assert 'CreatureCombatReactions::carriesSword(dropCreature)' in variants
assert 'twoWeapons ? twoWeaponStrength : 1.0f' in variants
blow_bones = function_body(render, 'void addCombatSwordBlow(')
assert 'leftHand ? isLeftArmBone(boneName) : isRightArmBone(boneName)' in blow_bones
assert 'leftHand ? isLeftForearmBone(boneName) : isRightForearmBone(boneName)' in blow_bones
attack = function_body(render, 'std::string createCreatureCombatAttack(')
assert 'name += "L";' in attack and 'addCombatSwordBlow(blow, boneName, leftHand, armScale,' in attack
# A mirrored clip (source name + "Left") is the source of a left-hand blow, the arm deformation is only the fallback
assert 'const std::string MIRRORED_CLIP_SUFFIX = "Left";' in render
assert 'leftHand && blow != COMBAT_BLOW_PLAIN && skeleton->hasAnimation(original + MIRRORED_CLIP_SUFFIX)' in attack
assert 'sourceName += MIRRORED_CLIP_SUFFIX;' in attack and 'getAnimation(sourceName)' in attack, 'the clip is preferred'
assert 'if(mirroredClip)' in attack and 'armScale = 1.0f;' in attack, 'TwoWeaponArmStrength is only for the fallback'
assert 'name += "M";' in attack, 'own cache name for the mirrored clip'
assert attack.index('mirroredClip') < attack.index('addCombatSwordBlow(blow, boneName'), 'the fallback deformation stays'

# The side of the last blow is available for the weapon trail
assert 'bool isLastBlowLeftHanded(const Creature* creature) const;' in render_h
assert 'std::map<const Creature*, bool> mCreatureLastBlowLeft;' in render_h
assert 'mCreatureLastBlowLeft[dropCreature] = leftHand;' in variants
assert render.count('mCreatureLastBlowLeft.erase(curCreature);') == 1 and render.count('mCreatureLastBlowLeft.clear();') == 1

# The mirrored clips exist in the skeletons of the humanoid sword/axe bearers (new clip only, Attack1 stays)
MIRRORED = ('Adventurer', 'Cultist', 'Dwarf1', 'Dwarf2', 'Gnome', 'Goblin', 'Knight', 'Monk', 'Orc', 'RunelordDwarf')
for skeleton in MIRRORED:
    data = (root / 'models' / (skeleton + '.skeleton')).read_bytes()
    for needed in (b'Attack1Left', b'Attack1', b'Weapon_L', b'Weapon_R'):
        assert needed in data, skeleton + ' has no ' + needed.decode()
for bone in ('arm_l', 'upper_arm.l', 'upperarm_l', 'forearm_l', 'forearm.l'):
    assert '"' + bone + '"' in render, bone
# The weapon models stay on their own bones (the hand carries the weapon of its side)
mount = function_body(render, 'bool RenderManager::getWeaponMount(')
assert 'mount.mBoneName = "Weapon_" + hand;' in mount

# Client side only: no server, timing, damage or network in the touched code
for text in (header, carries, variants):
    for forbidden in ('ServerNotification', 'ClientNotification', 'ODPacket', 'fireCreatureRefreshIfNeeded',
                      'mActivity', 'setAnimationState', 'getIsOnServerMap'):
        assert forbidden not in text, forbidden

# Config: documented, set in the cfg, read with a default, accepted by the format check
cfg = read('config/creatureReactions.cfg')
for key in ('TwoWeaponMode', 'TwoWeaponArmStrength'):
    assert re.search(r'^#   ' + key + r'\s', cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\s+\d', cfg, re.M), key + ' not set'
    assert '"' + key + '"' in read('source/render/CreatureReactionConfig.cpp'), key + ' not read'
    assert '"' + key + '"' in read('tools/check_creature_reactions.py'), key + ' not allowed by the format check'
config_cpp = read('source/render/CreatureReactionConfig.cpp')
assert 'mTwoWeaponMode(1)' in config_cpp and 'mTwoWeaponArmStrength(1.0)' in config_cpp

if '--probe' in sys.argv:
    probe = r'''
#include "render/TwoWeaponStrike.h"
#include <iostream>
static int gFailures = 0;
static void check(bool ok, const char* msg)
{
    if(!ok)
    {
        ++gFailures;
        std::cout << "FAIL: " << msg << '\n';
    }
}
int main()
{
    check(TwoWeaponStrike::isStrikeKind("Sword") && TwoWeaponStrike::isStrikeKind("Axe"), "swords and axes strike");
    check(!TwoWeaponStrike::isStrikeKind("Shield") && !TwoWeaponStrike::isStrikeKind("Bow"), "shield and bow do not");
    check(!TwoWeaponStrike::isStrikeKind("Staff") && !TwoWeaponStrike::isStrikeKind(""), "staff and none do not");
    for(uint32_t i = 0; i < 16; ++i)
    {
        check(TwoWeaponStrike::isLeftBlow(TwoWeaponStrike::MODE_ALTERNATE, i) == ((i % 2) != 0), "alternate order");
        check(!TwoWeaponStrike::isLeftBlow(TwoWeaponStrike::MODE_OFF, i), "off is always right");
    }
    uint32_t run = 1, longest = 1;
    for(uint32_t i = 1; i < 64; ++i)
    {
        run = (TwoWeaponStrike::isLeftBlow(TwoWeaponStrike::MODE_RANDOM, i) ==
            TwoWeaponStrike::isLeftBlow(TwoWeaponStrike::MODE_RANDOM, i - 1)) ? run + 1 : 1;
        if(run > longest)
            longest = run;
    }
    check(longest <= 2, "irregular mode: at most two on a side in a row");
    std::cout << "FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='odp-two-weapons-') as directory:
        work = Path(directory)
        (work / 'check.cpp').write_text(probe)
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/I', str(root / 'source'), 'check.cpp', '/Fecheck.exe'],
                       cwd=work, check=True, stdout=subprocess.DEVNULL)
        result = subprocess.run([str(work / 'check.exe')], cwd=work, capture_output=True, text=True)
        sys.stdout.write(result.stdout)
        assert result.returncode == 0, 'probe failed'

print('two weapons: ok')
