"""The cosmetic event hitResult: what a blow or shot really did, shown by the clients only from the event.

Pure source wiring checks (no compiler, no game): kind list and order, the emitters on the server, the
negotiation and the old clients and servers, the client handlers, the configuration keys."""
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
global_cfg = read('config/global.cfg')
config_cpp = read('source/utils/ConfigManager.cpp')
config_h = read('source/utils/ConfigManager.h')
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
melee = read('source/creatureskill/CreatureSkillMeleeFight.cpp')
launch = read('source/creatureskill/CreatureSkillMissileLaunch.cpp')
one_hit = read('source/entities/MissileOneHit.cpp')
one_hit_h = read('source/entities/MissileOneHit.h')
missile = read('source/entities/MissileObject.cpp')
missile_h = read('source/entities/MissileObject.h')
visuals = read('source/render/CreatureWeaponVisuals.cpp')
visuals_h = read('source/render/CreatureWeaponVisuals.h')
combat = read('source/render/CreatureCombatReactions.cpp')
combat_h = read('source/render/CreatureCombatReactions.h')
reactions_cfg = read('config/creatureReactions.cfg')
server = read('source/network/ODServer.cpp')
client = read('source/network/ODClient.cpp')

# Kind list: appended after the last kind, 9 to 16 stay free for the kinds of other features, the old numbers never change
enum_body = event_h.split('enum class CosmeticEventType')[1].split('};')[0]
kinds = re.findall(r'^\s+(\w+) = (\d+),?\s*$', enum_body, re.M)
numbers = [int(number) for _, number in kinds]
names = [name for name, _ in kinds]
assert names[-3:] == ['portalArrival', 'hitResult', 'attackTurn'], names[-3:]
assert numbers[:9] == list(range(9)) and numbers[9] == 17 and numbers[10] == 18 and len(numbers) == 11, numbers
assert dict(kinds)['hitResult'] == '17' and dict(kinds)['attackTurn'] == '18' and dict(kinds)['portalArrival'] == '8'
assert dict(kinds)['meleeResult'] == '5'
assert 'return "hitResult";' in event_cpp
known = function_body(event_cpp, 'bool CosmeticEvent::isKnownType() const')
assert 'CosmeticEventType::portalArrival' in known and 'CosmeticEventType::hitResult' in known
# The old result numbers remain stable; shield blocking follows the existing avoidance results.
results = re.findall(r'^\s+(\w+) = (\d+),?\s*$', event_h.split('enum class CosmeticHitResult')[1].split('};')[0], re.M)
assert results == [('hit', '0'), ('glanced', '1'), ('blocked', '2'), ('missed', '3'), ('dodged', '4'), ('parried', '5'), ('shieldBlocked', '6')], results
assert '9 to 16 are kept free' in event_h

# Server: emitters in the damage places, serverauthoritative, no change of damage or timing
fire = function_body(creature, 'void Creature::fireHitResult(')
for needle in ('getHitEvents()', 'getHitGlanceShare()', 'getMaxHp()', 'CosmeticEventType::hitResult',
               'CosmeticHitResult::blocked', 'CosmeticHitResult::glanced', 'CosmeticHitResult::hit', 'fireCosmeticEvent(event, false)',
               'event.mPosition = getPosition()'):
    assert needle in fire, needle
for forbidden in ('takeDamage', 'mHp', 'mActivity', 'setAnimation'):
    assert forbidden not in fire, forbidden
miss = function_body(creature, 'void Creature::fireHitMissed(')
assert 'CosmeticHitResult::missed' in miss and 'getHitEvents()' in miss and 'takeDamage' not in miss
assert 'event.mText = missile ? "missile" : "melee";' in miss
assert 'void fireHitResult(' in creature_h and 'void fireHitMissed(' in creature_h
melee_body = function_body(melee, 'bool CreatureSkillMeleeFight::tryUseFight(')
assert 'target->fireHitResult(creature->getName(), damageDone, rawDamage, false);' in melee_body
assert 'target->fireHitDefended(creature->getName(), defence);' in melee_body
# After the damage was calculated, before the older kind; the older kind is still sent for clients without hitResult
assert melee_body.index('takeDamage(') < melee_body.index('fireHitResult(') < melee_body.index('CosmeticEventType::meleeResult')
hurt = function_body(one_hit, 'void MissileOneHit::hurt(')
assert hurt.index('takeDamage(') < hurt.index('fireHitResult(') and 'mHasHit = true;' in hurt
assert 'fireHitResult(mShooterName, damageDone, rawDamage, true)' in hurt
assert hurt.index('rollAttackHit(') < hurt.index('rollMeleeDefence(true)') < hurt.index('CombatDamage::roll(')
assert 'fireHitMissed(mShooterName, true)' in hurt and 'fireHitDefended(mShooterName, defence, true)' in hurt
assert 'hurt(tile, entity);' in function_body(one_hit, 'bool MissileOneHit::hitCreature(')
assert 'hurt(tile, entityTarget);' in function_body(one_hit, 'void MissileOneHit::hitTargetEntity(')
stopped = function_body(one_hit, 'void MissileOneHit::missileStopped()')
assert 'mHasHit' in stopped and 'fireHitMissed(mShooterName)' in stopped and 'isAlive()' in stopped
assert 'virtual void missileStopped()' in missile_h and 'missileStopped();' in missile
assert missile.index('setWalkPath(EntityAnimation::idle_anim') < missile.index('missileStopped();')
assert 'setShooter(creature->getName(), targetName)' in launch
# Saved creature shots retain their launch accuracy and cosmetic names; old rows keep loading.
export = function_body(one_hit, 'void MissileOneHit::exportToStream')
load = function_body(one_hit, 'bool MissileOneHit::importFromStream')
assert 'if(isCreatureShot() && mShotAttackerLevel != 0)' in export
assert 'ShotAccuracy' in export and 'ShotAccuracy' in load
for member in ('mShotAttackRating', 'mShotAttackerLevel', 'mShooterName', 'mTargetName'):
    assert member in export and member in load, member
assert 'is.seekg(accuracyPosition)' in load and 'is.clear();' in load
assert 'mHasHit' not in export and 'mHasHit' not in load
# Only through the common send path: human clients that negotiated cosmetic events and see the target
send = function_body(server, 'void ODServer::sendCosmeticEvent(')
assert 'supportsCosmeticEvents(player)' in send and 'getIsHuman()' in send
fire_cosmetic = function_body(creature, 'void Creature::fireCosmeticEvent(const CosmeticEvent& event')
assert 'mSeatsWithVisionNotified' in fire_cosmetic and 'getIsHuman()' in fire_cosmetic
# Old client: unknown kind is skipped by the same layout; the client only reads what it knows
assert '!supportsCosmeticEvents() || !event.isKnownType()' in client
assert 'CosmeticEventType::hitResult' not in client, 'the generic handler takes it'

# Configuration: documented in global.cfg, read with a default, limited
for key, default, member in (('HitEvents', '1', 'mHitEvents(true)'), ('HitGlanceShare', '0.34', 'mHitGlanceShare(DEFAULT_HIT_GLANCE_SHARE)'),
                             ('HitStrongShare', '0.15', 'mHitStrongShare(DEFAULT_HIT_STRONG_SHARE)')):
    assert re.search(r'^# .*\n(?:# .*\n)?\s+' + key + r'\t' + re.escape(default) + r'\s*$', global_cfg, re.M), key
    assert 'nextParam == "' + key + '"' in config_cpp, key
    assert member in config_cpp, key
assert 'std::max(HitShareMin, std::min(HitGlanceShareMax, mHitGlanceShare))' in config_h
assert 'std::max(HitShareMin, std::min(HitStrongShareMax, mHitStrongShare))' in config_h
# The named defaults and limits keep the numbers they had before they got names
for name, value in (('DEFAULT_HIT_GLANCE_SHARE', '0.34'), ('DEFAULT_HIT_STRONG_SHARE', '0.15'),
                    ('ConfigManager::HitShareMin', '0.01'), ('ConfigManager::HitGlanceShareMax', '0.9'),
                    ('ConfigManager::HitStrongShareMax', '1.0')):
    assert re.search(re.escape(name) + r' = ' + re.escape(value) + ';', config_cpp), name

# Client: the hits, dodges, glancing blows and misses show only from the event once the server sends it
handler = function_body(visuals, 'bool CreatureWeaponVisuals::noteCosmeticEvent(')
assert 'CosmeticEventType::hitResult' in handler and 'sHitEvents = true;' in handler
assert handler.index('sHitEvents = true;') < handler.index('isActive(reactions)')
result = function_body(visuals, 'void noteHitResult(')
for needle in ('CosmeticHitResult::hit', 'CosmeticHitResult::glanced', 'CosmeticHitResult::blocked', 'CosmeticHitResult::missed',
               '"BlowGlanced"', '"BlowDodged"', '"BlowMissed"', 'getHitStrongShare()', 'sLastHits[event.mSubject] = info;',
               'CreatureCombatReactions::noteHitEvent('):
    assert needle in result, needle
blow = function_body(visuals, 'void noteBlow(')
assert blow.index('if(sHitEvents)') < blow.index('"BlowDodged"'), 'the older kind drives the reactions only without hitResult'
assert 'startTrail(reactions, attacker)' in blow, 'the existing trail trigger is untouched'
assert 'static bool getLastHit(' in visuals_h and 'static bool hasHitEvents();' in visuals_h
for field in ('mResult', 'mHealthPermille', 'mStrong', 'mMissile', 'mTarget', 'mTime'):
    assert field in visuals_h, field
assert 'sHitEvents = false;' in function_body(visuals, 'void CreatureWeaponVisuals::stopAll(')
# No trail is built here: the readable state is only offered
assert 'startTrail' not in result
missed_result = result.split('case static_cast<int32_t>(CosmeticHitResult::missed):', 1)[1].split('break;', 1)[0]
assert 'attacker->getName(), "BlowMissed"' in missed_result and 'target->' not in missed_result
shield_result = result.split('case static_cast<int32_t>(CosmeticHitResult::shieldBlocked):', 1)[1].split('break;', 1)[0]
assert 'target->getName(), "BlowParried"' in shield_result
assert 'attacker->getName(), "BlowDeflected"' in shield_result
# The guess is gone with the event: no scheduled flinch from the attack animation, no random flinch chance
attacks = function_body(combat, 'void CreatureCombatReactions::processAttacks(')
assert 'if(!CreatureWeaponVisuals::hasHitEvents())\n            scheduleHit(' in attacks
hits = function_body(combat, 'void CreatureCombatReactions::processHits(')
assert '!CreatureWeaponVisuals::hasHitEvents() && (combatRandom(0.0, 1.0) >= FLINCH_CHANCE)' in hits
assert '!CreatureWeaponVisuals::hasHitEvents() && CreatureWeaponVisuals::wasBlowSoftened(' in hits
note = function_body(combat, 'void CreatureCombatReactions::noteHitEvent(')
assert 'strong ? 1 : 0' in note and 'scheduleHit(' in note and 'static void noteHitEvent(' in combat_h
# The health steps still drive the stagger without an event (spells, traps and older servers)
assert 'void CreatureCombatReactions::noteHealth(' in combat
for event_name in ('BlowDodged', 'BlowMissed', 'BlowGlanced'):
    assert 'Name        ' + event_name in reactions_cfg, event_name
assert 'HitStrongShare' in reactions_cfg

print('check_hit_event: ok')
