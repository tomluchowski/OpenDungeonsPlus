"""Weapon trail: a strong melee blow leaves a short bright streak along the blade tip that fades at once.

Pure source wiring check (no compiler, no game): client side and cosmetic only, started from the hit result the
server reported (never guessed), only in the option 'full', limited by a budget, values in
config/creatureReactions.cfg, removed with the creature."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


trail = read('source/render/WeaponTrail.cpp')
trail_h = read('source/render/WeaponTrail.h')
combat = read('source/render/CreatureCombatReactions.cpp')
render = read('source/render/RenderManager.cpp')
config_h = read('source/render/CreatureReactionConfig.h')
config_cpp = read('source/render/CreatureReactionConfig.cpp')
cfg = read('config/creatureReactions.cfg')
checker = read('tools/check_creature_reactions.py')

# New files in the build, no new asset (the existing additive glow material is used)
assert '${SRC}/render/WeaponTrail.cpp' in read('CMakeLists.txt')
for name in ('noteStrongBlow', 'update', 'removeCreature', 'stopAll'):
    assert re.search(r'\b' + name + r'\(', trail_h), name + ' not declared'
material = trail[trail.index('TRAIL_MATERIAL = "') + len('TRAIL_MATERIAL = "'):]
material = material[:material.index('"')]
assert re.search(r'^material ' + material + r'\s*$', read('materials/scripts/CreatureReactions.material'), re.M), material
glow = read('materials/scripts/CreatureReactions.material')
glow = glow[glow.index('material ' + material):]
glow = glow[:glow.index('\n}\n')]
assert 'scene_blend add' in glow and 'depth_write off' in glow and 'diffuse vertexcolour' in glow

# Config: documented, set, read with a default and limits, accepted by the format check
keys = {'WeaponTrail': 'mWeaponTrail(true)', 'WeaponTrailLife': 'mWeaponTrailLife(0.25)',
        'WeaponTrailWidth': 'mWeaponTrailWidth(0.12)', 'WeaponTrailLength': 'mWeaponTrailLength(2.0)',
        'WeaponTrailColour': 'mWeaponTrailRed(1.0)', 'WeaponTrailBrightness': 'mWeaponTrailBrightness(0.9)',
        'WeaponTrailMinShare': 'mWeaponTrailMinShare(0.0)', 'WeaponTrailMax': 'mWeaponTrailMax(4)'}
for key, default in keys.items():
    assert re.search(r'^#   ' + key + r'\s', cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\s+[\d.]', cfg, re.M), key + ' not set'
    assert '"' + key + '"' in config_cpp, key + ' not read'
    assert '"' + key + '"' in checker, key + ' not allowed by the format check'
    assert default in config_cpp, key + ' has no default'
for limit in ('std::max(0.05, std::min(2.0, mWeaponTrailLife))', 'std::max(0.01, std::min(1.0, mWeaponTrailWidth))',
              'std::max(0.1, std::min(10.0, mWeaponTrailLength))', 'std::min<uint32_t>(16, mWeaponTrailMax)'):
    assert limit in config_h, limit
assert 'HitStrongShare' in cfg, 'the strong threshold stays HitStrongShare'
assert 'getHitStrongShare' not in trail, 'the trail does not change the strong threshold'

# The streak is only started from a strong melee hit that the server reported
note = function_body(trail, 'void noteStrongBlow(')
assert 'CreatureWeaponVisuals::getLastHit(attacker->getName(), info)' in note
assert 'return;' in note[note.index('getLastHit'):note.index('getLastHit') + 120], 'no server event: no streak'
assert '!info.mStrong || info.mMissile' in note
assert 'getWeaponTrailMinShare()' in note and 'info.mHealthPermille' in note
hook = function_body(combat, 'void CreatureCombatReactions::noteHitEvent(')
assert 'if(strong && !missile)' in hook and 'WeaponTrail::noteStrongBlow(reactions, attacker, delay);' in hook
assert 'WeaponTrail::noteStrongBlow(' not in combat.replace(hook, ''), 'only the hit event starts a streak'
# Nothing starts from a guess: no flinch chance, no meleeResult, no health step
assert 'FLINCH_CHANCE' not in trail and 'meleeResult' not in trail and 'noteHealth' not in trail

# Only in the option 'full' (isActive = full and config loaded), the config switch and the budget
assert 'CreatureWeaponVisuals::isActive(reactions)' in note and 'config.getWeaponTrail()' in note
upd = function_body(trail, 'void update(')
assert '!config.getWeaponTrail() || !CreatureWeaponVisuals::isActive(reactions)' in upd and 'stopAll();' in upd
assert 'sTrails.size() >= config.getWeaponTrailMax()' in note, 'budget'
assert 'reactions.isCreatureNearCamera(attacker)' in note, 'only creatures on the screen and near the camera'
assert 'isVisible()' in trail, 'only a shown weapon'
assert 'for(std::vector<Trail>::iterator it = sTrails.begin(); it != sTrails.end(); ++it)' in note and \
    'it->mCreature == attacker->getName()' in note, 'one streak per creature'

# The weapon: no shield, bow or staff; the tip is derived from the bounding box of the weapon model
find = function_body(trail, 'std::string findStrikingWeapon(')
for word in ('shield', 'bow', 'staff', 'getWeaponR()', 'getWeaponL()'):
    assert word in find, word
tip = function_body(trail, 'bool getTipPosition(')
assert 'getBoundingBox()' in tip and 'getMaximum()' in tip and 'getMinimum()' in tip and '_getDerivedPosition()' in tip

# Fades at once: the age drives the colour and the end of life removes the points; the chain is destroyed
assert 'point.mAge / life' in trail and 'trail.mPoints.front().mAge >= life' in trail
assert 'getWeaponTrailLife()' in trail and 'getWeaponTrailWidth()' in trail and 'getWeaponTrailLength()' in trail
assert 'getWeaponTrailRed()' in trail and 'getWeaponTrailBrightness()' in trail
assert 'destroyBillboardChain(trail.mChainName)' in trail and 'destroySceneNode(trail.mNodeName)' in trail

# Cleanup: with the creature, when the reactions stop, when the option is not full
assert 'WeaponTrail::removeCreature((creature != nullptr) ? creature->getName() : std::string());' in \
    function_body(render, 'void RenderManager::clearCreatureCombatEffects(')
assert 'WeaponTrail::stopAll();' in function_body(combat, 'void CreatureCombatReactions::stopAll(')
assert 'WeaponTrail::update(reactions, timeSinceLastFrame);' in function_body(combat, 'void CreatureCombatReactions::update(')
assert render.index('void RenderManager::rrDestroyCreature(') < render.index('clearCreatureCombatEffects(curCreature);')

# Client side only: no server, timing, damage, network or save
for text in (trail, trail_h):
    for forbidden in ('ServerNotification', 'ClientNotification', 'ODPacket', 'fireCreatureRefreshIfNeeded',
                      'mActivity', 'setAnimationState', 'getIsOnServerMap', 'takeDamage', 'ODPacket'):
        assert forbidden not in text, forbidden
assert not re.search(r'\bauto\b', trail), 'explicit types only'

print('check_weapon_trail: OK')
