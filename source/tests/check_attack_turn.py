"""The cosmetic event attackTurn: a creature turns smoothly to its target before a blow, the strike clip waits for it.

Pure source wiring checks (no compiler, no game): kind and number, server emitter, old clients and servers,
client handler, delayed clip start, configuration keys. The damage and its timing on the server stay untouched."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


event_h = read('source/network/CosmeticEvent.h')
event_cpp = read('source/network/CosmeticEvent.cpp')
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
movable = read('source/entities/MovableGameEntity.cpp')
movable_h = read('source/entities/MovableGameEntity.h')
render = read('source/render/RenderManager.cpp')
render_h = read('source/render/RenderManager.h')
client = read('source/network/ODClient.cpp')
config_cpp = read('source/utils/ConfigManager.cpp')
config_h = read('source/utils/ConfigManager.h')
global_cfg = read('config/global.cfg')

# Kind: appended behind hitResult (17), 9 to 16 stay free for the kinds of other features
enum_body = event_h.split('enum class CosmeticEventType')[1].split('};')[0]
kinds = dict(re.findall(r'^\s+(\w+) = (\d+),?\s*$', enum_body, re.M))
assert kinds['hitResult'] == '17' and kinds['attackTurn'] == '18', kinds
assert 'return "attackTurn";' in event_cpp
known = function_body(event_cpp, 'bool CosmeticEvent::isKnownType() const')
assert 'CosmeticEventType::attackTurn' in known and 'CosmeticEventType::hitResult' in known

# Server: sent from the start of a blow, before the animation, only reports (no damage, no timing, no state)
fire = function_body(creature, 'void Creature::fireAttackTurn(')
for needle in ('getAttackTurnEvents()', 'CosmeticEventType::attackTurn', 'event.mSubject = getName()',
               'event.mObject = targetName', 'event.mPosition = direction', 'fireCosmeticEvent(event, false)'):
    assert needle in fire, needle
for forbidden in ('takeDamage', 'mActivity', 'mNeedFireRefresh', 'setAnimationState', 'mWarmup', 'mCooldown'):
    assert forbidden not in fire, forbidden
assert 'void fireAttackTurn(' in creature_h
attack = function_body(creature, 'void Creature::useAttack(')
assert attack.index('fireAttackTurn(entityAttack.getName(), walkDirection);') < attack.index('setAnimationState(ranged')
assert attack.index('walkDirection.normalise();') < attack.index('fireAttackTurn(')
assert attack.index('fireAttackTurn(') < attack.index('tryUseFight('), 'announced before the damage is calculated'
assert 'mWarmup = skillData.mSkill->getWarmupNbTurns();' in attack, 'timing of the server unchanged'
# Only through the common path: negotiated clients that see the creature; old servers send nothing, old clients skip the kind
assert 'fireCosmeticEvent(event, false)' in fire
assert '!supportsCosmeticEvents() || !event.isKnownType()' in client

# Client: the event announces the turn, the animation message of the blow uses it
handler = client[client.index('event.is(CosmeticEventType::attackTurn)'):]
handler = handler[:handler.index('break;')]
assert 'rrNoteAttackTurn(event.mSubject, event.mPosition)' in handler
note = function_body(render, 'void RenderManager::rrNoteAttackTurn(')
assert 'mAttackTurnNotes[creatureName] = direction;' in note
take = function_body(render, 'bool RenderManager::rrTakeAttackTurn(')
assert 'mAttackTurnNotes.erase(it);' in take and 'return false;' in take
turn = function_body(render, 'Ogre::Real RenderManager::rrOrientEntityTowardSmoothly(')
for needle in ('rrTakeAttackTurn(gameEntity->getName(), aim)', 'getAttackTurnSpeed()', 'getAttackTurnMaxDelay()',
               'CREATURE_TURN_BASE_DURATION + CREATURE_TURN_HALF_TURN_DURATION * turn / Ogre::Math::PI', 'return 0.0f;'):
    assert needle in turn, needle
assert turn.index('CREATURE_TURN_BASE_DURATION + ') < turn.index('getAttackTurnSpeed()'), 'without the announcement the old fixed turn stays'
assert turn.index('if(!announced)\n        return 0.0f;') < turn.index('getAttackTurnMaxDelay()'), 'no waiting without the announcement'
assert 'std::min(duration,' in turn, 'a long turn runs over the wind-up'
assert 'Ogre::Real rrOrientEntityTowardSmoothly(' in render_h and 'void rrNoteAttackTurn(' in render_h
assert 'std::map<std::string, Ogre::Vector3> mAttackTurnNotes;' in render_h

# The clip of the blow starts after the turn; a new state, a walk or the end of the delay settle it; nothing is saved
state = function_body(movable, 'void MovableGameEntity::setAnimationState(')
assert 'blowDelay = RenderManager::getSingleton().rrOrientEntityTowardSmoothly(this, direction);' in state
assert 'mBlowStartDelay = 0.0;' in state and state.index('mBlowStartDelay = 0.0;') < state.index('blowDelay = RenderManager')
assert state.index('mBlowStartDelay = blowDelay;') < state.index('startAnimationClip();')
update = function_body(movable, 'void MovableGameEntity::update(')
assert 'mBlowStartDelay -= static_cast<double>(timeSinceLastFrame);' in update and 'startAnimationClip();' in update
assert 'if(mWalkQueue.empty())' in update.split('startAnimationClip();')[0].split('mBlowStartDelay = 0.0;')[1]
start = function_body(movable, 'void MovableGameEntity::startAnimationClip()')
assert 'rrSetObjectAnimationState(this, mPrevAnimationState, mPrevAnimationStateLoop)' in start and 'noteAnimation(this, mPrevAnimationState)' in start
assert 'mBlowStartDelay(0.0)' in movable and 'double mBlowStartDelay;' in movable_h
for stream_function in ('exportToStream', 'importFromStream', 'exportToPacket', 'importFromPacket'):
    body = function_body(movable, 'MovableGameEntity::' + stream_function + '(')
    assert 'mBlowStartDelay' not in body, stream_function
# The server never uses it: the delay is only counted on the client (rrSetObjectAnimationState is client code)
assert 'getIsOnServerMap()' in state.split('mBlowStartDelay = 0.0;')[0]

# Configuration: documented in global.cfg, read with a default, limited
for key, default, member in (('AttackTurnEvents', '1', 'mAttackTurnEvents(true)'), ('AttackTurnSpeed', '540', 'mAttackTurnSpeed(540.0)'),
                             ('AttackTurnMaxDelay', '0.25', 'mAttackTurnMaxDelay(0.25)')):
    assert re.search(r'^# .*\n(?:# .*\n)?\s+' + key + r'\t' + re.escape(default) + r'\s*$', global_cfg, re.M), key
    assert 'nextParam == "' + key + '"' in config_cpp, key
    assert member in config_cpp, key
assert 'std::max(90.0, std::min(1440.0, mAttackTurnSpeed))' in config_h
assert 'std::max(0.0, std::min(0.6, mAttackTurnMaxDelay))' in config_h
print('check_attack_turn: ok')
