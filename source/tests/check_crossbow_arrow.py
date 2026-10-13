"""Arrow in the pulling hand and visible crossbow reload (client side only, no compiler, no game).

Pure source wiring check: the settings in config/creatureReactions.cfg, the choice of the pulling hand bone (also
against the real skeleton files of the Elf and the DarkElf), the release at the moment of the projectile start, a reload
that is never longer than the time between the shots, clean up and the fallback without a hand bone."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


cpp = read('source/render/CreatureWeaponVisuals.cpp')
config_h = read('source/render/CreatureReactionConfig.h')
config_cpp = read('source/render/CreatureReactionConfig.cpp')
cfg = read('config/creatureReactions.cfg')
tool = read('tools/check_creature_reactions.py')

# 1. Settings: cfg value, cfg header text, parser, default, getter, check script
KEYS = {
    'ArrowFollowsHand': ('mArrowFollowsHand(true)', 'getArrowFollowsHand'),
    'ArrowPullDistance': ('mArrowPullDistance(0.12)', 'getArrowPullDistance'),
    'ArrowDrawTime': ('mArrowDrawTime(1.6)', 'getArrowDrawTime'),
    'ArrowRetakeGap': ('mArrowRetakeGap(0.3)', 'getArrowRetakeGap'),
    'ArrowReloadTime': ('mArrowReloadTime(0.45)', 'getArrowReloadTime'),
    'ArrowHandOffset': ('mArrowHandOffset(Ogre::Vector3::ZERO)', 'getArrowHandOffset'),
    'CrossbowReloadTime': ('mCrossbowReloadTime(1.1)', 'getCrossbowReloadTime'),
    'CrossbowReloadJolt': ('mCrossbowReloadJolt(5.0)', 'getCrossbowReloadJolt'),
}
settings_start = cfg.rindex('[Settings]')
settings = cfg[settings_start:cfg.index('[/Settings]', settings_start)]
header = cfg[:settings_start]
for key, (default, getter) in KEYS.items():
    assert re.search(r'^\s+%s\s+\S' % key, settings, re.M), 'cfg value missing: ' + key
    assert re.search(r'^#\s+%s\s' % key, header, re.M), 'cfg header does not explain ' + key
    assert 'words[0] == "%s"' % key in config_cpp, 'parser misses ' + key
    assert default in config_cpp, 'default missing: ' + key
    assert getter in config_h, 'getter missing: ' + getter
    assert '"%s"' % key in tool, 'tools/check_creature_reactions.py does not know ' + key
    assert getter + '()' in cpp, 'the visuals do not read ' + key

# the hard coded times of the first version are gone: they come from the settings now
for old in ('RELOAD_GAP', 'RELOAD_TIME', 'DRAW_TIME', 'DRAW_DISTANCE'):
    assert old not in cpp, old

# 2. The pulling hand: the side that does not carry the weapon, read from the bone names
body = function_body(cpp, 'std::string findPullHandBone(')
assert 'Weapon_R' in body and 'RightHand' in body and 'Weapon_L' in body and 'LeftHand' in body
assert 'return std::string();' in body, 'no hand bone must give an empty name'


def find_pull_hand(bones, mount_bone):
    """Python twin of findPullHandBone."""
    lower = mount_bone.lower()
    left = 'left' in lower or lower.endswith('_l') or lower.endswith('.l')
    right = 'right' in lower or lower.endswith('_r') or lower.endswith('.r')
    if left == right:
        return ''
    candidates = (['Weapon_R', 'RightHand', 'Hand_R', 'hand.R', 'Hand.R', 'RightFinger', 'Finger_R'] if left else
                  ['Weapon_L', 'LeftHand', 'Hand_L', 'hand.L', 'Hand.L', 'LeftFinger', 'Finger_L'])
    for name in candidates:
        if name in bones:
            return name
    return ''


def skeleton_bones(name):
    data = (root / 'models' / (name + '.skeleton')).read_bytes()
    return set(m.decode() for m in re.findall(rb'Weapon_[LR]|LeftHand|RightHand', data))


elf = skeleton_bones('Elf')
dark_elf = skeleton_bones('DarkElf')
assert find_pull_hand(elf, 'Weapon_L') == 'Weapon_R', 'Elf: bow in the left hand, the right hand pulls'
assert find_pull_hand(dark_elf, 'LeftHand') in ('Weapon_R', 'RightHand'), 'DarkElf: crossbow on the left hand'
assert find_pull_hand({'Weapon_R'}, 'Weapon_R') == '', 'the weapon bone is never its own pulling hand'
assert find_pull_hand({'Body'}, 'Weapon_L') == '', 'no hand bone: fallback'
assert find_pull_hand({'Weapon_L', 'LeftHand'}, 'Spine') == '', 'unknown side: fallback'

# 3. The arrow is taken from the hand and follows it (position from the bone world positions, every frame)
place = function_body(cpp, 'void placeArrow(')
assert '_getDerivedPosition()' in function_body(cpp, 'bool getHandPosition(') and 'handPosition' in place
assert 'config.getArrowPullDistance()' in place and 'toHand' in place, 'drawn from the string toward the hand'
assert 'std::min(distance, config.getArrowPullDistance())' in place, 'the pull is limited'
assert 'followsHand' in place

# 4. Release: the held arrow is gone with the projectile (same event, no waiting for the next look)
shot = function_body(cpp, 'void noteShot(')
assert 'mReleasedAt = now;' in shot
assert 'setVisible(false)' in shot, 'the held arrow is hidden at once at the launch'
assert 'event.mText.empty()' in shot, 'magic missiles have no arrow'
assert 'sinceShot < config.getArrowRetakeGap()' in place and 'arrow->setVisible(false)' in place

# 5. The reload is never longer than the time between the shots
duration = function_body(cpp, 'double getReloadDuration(')
assert 'getCrossbowReloadTime()' in duration and 'shooter.mShotInterval - config.getArrowRetakeGap()' in duration
assert 'std::min(duration,' in duration
assert 'held.mShotInterval = (held.mReleasedAt > -999.0) ? (now - held.mReleasedAt) : 0.0;' in shot


def reload_duration(setting, interval, gap, minimum=0.05):
    return min(setting, max(minimum, interval - gap)) if interval > 0 else setting


for interval in (0.0, 0.5, 0.9, 1.4, 2.0, 5.0, 12.0):
    d = reload_duration(1.1, interval, 0.3)
    assert d <= 1.1
    if interval > 0.35:
        assert d + 0.3 <= interval + 1e-9, 'reload longer than the shot interval'
assert reload_duration(1.1, 3.0, 0.3) == 1.1

# 6. The crossbow reload has three visible parts and a jolt of the weapon that is set straight again
assert 'BOLT_TAKEN_SHARE' in place and 'BOLT_ON_RAIL_SHARE' in place and 'getCrossbowReloadJolt()' in place
assert 'weaponTag->setOrientation(shooter.mMountRotation * tilt)' in place
assert 'tagPoint->setOrientation(shooter.mMountRotation)' in function_body(cpp, 'void straightenWeapon(')
assert 'BOLT_TAKEN_SHARE = 0.3' in cpp and 'BOLT_ON_RAIL_SHARE = 0.7' in cpp

# 7. Clean up: removeArrow (arrow gone, weapon straight) on every way out
remove = function_body(cpp, 'void removeArrow(')
assert 'straightenWeapon(shooter)' in remove and 'destroyArrow(shooter.mArrowName)' in remove
tick = function_body(cpp, 'void tick(')
assert tick.count('removeArrow(') >= 3, 'not armed, dead or gone, creature removed'
assert 'removeArrow(it->second)' in function_body(cpp, 'void removeAllArrows(')
assert 'removeAllArrows();' in function_body(cpp, 'void CreatureWeaponVisuals::stopAll(')
assert 'stopAll(reactions)' in function_body(cpp, 'void CreatureWeaponVisuals::update(')
assert 'isActive' in function_body(cpp, 'void CreatureWeaponVisuals::update('), 'only the full mode'
assert 'isCreatureNearCamera' in tick, 'only creatures near the camera'
destroy = function_body(cpp, 'void destroyArrow(')
assert 'detachFromParent()' in destroy and 'destroyEntity' in destroy

# 8. Fallback without a hand bone: old behavior (arrow on the bow), logged once per mesh
assert 'sNoHandLogged' in cpp and 'OD_LOG_INF(' in cpp
assert 'if(followsHand)' in place and place.count('No hand known') == 2
assert 'getArrowFollowsHand()' in function_body(cpp, 'bool createArrow(')

# 9. Nothing on the server or the network was touched by this feature
for forbidden in ('ODServer', 'ServerNotification', 'ClientNotification', 'sendCosmeticEvent', 'fireCreatureRefreshIfNeeded'):
    assert forbidden not in cpp, forbidden
assert 'mActivity' not in cpp

print('check_crossbow_arrow: ok')
sys.exit(0)
