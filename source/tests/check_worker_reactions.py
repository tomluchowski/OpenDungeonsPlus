"""Worker reactions: every event the code shows exists in the config, the hooks are wired, nothing touches the game.
Static checks only, never launches a game."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


code = read('source/render/WorkerReactions.cpp')
reactions = read('source/render/CreatureReactions.cpp')
reactions_h = read('source/render/CreatureReactions.h')
cfg = read('config/creatureReactions.cfg')

# Overlay principle: no animation state, no activity, no refresh, no network in the worker reactions
for forbidden in ('setAnimationState', 'mActivity', 'fireCreatureRefreshIfNeeded', 'ODPacket', 'ServerNotification',
                  'sendCosmeticEvent'):
    assert forbidden not in code, forbidden

# Every event named in the code is a [Event] of the config
events = set(re.findall(r'^\s*Name\s+(\w+)\s*$', cfg, re.MULTILINE))
used = set(re.findall(r'"((?:Dig|Claim|Reinforce|Pick|Gold|Treasury|Prisoner|Corpse|Trap|Worker)\w*)"', code))
assert used, 'no events found in the code'
# Clip, room, mesh and entity names that look like events
not_events = {'Claim', 'Dig', 'Treasury', 'GoldstackLv', 'WorkerGoldBody_'}
missing = sorted(name for name in used if name not in events and name not in not_events)
assert not missing, 'events missing in config/creatureReactions.cfg: %s' % missing

# The hooks of the framework call the worker reactions
for call in ('WorkerReactions::update(', 'WorkerReactions::noteAnimation(', 'WorkerReactions::noteCarry(',
             'WorkerReactions::noteRelease(', 'WorkerReactions::noteHandled(', 'WorkerReactions::noteParticleEffect(',
             'WorkerReactions::noteCosmeticEvent(', 'WorkerReactions::stopAll('):
    assert call in reactions, call
assert 'friend class WorkerReactions;' in reactions_h
assert 'render/WorkerReactions.cpp' in read('CMakeLists.txt')

# The particle systems used by the new events exist
particles = read('particles/WorkerReactions.particle')
for name in re.findall(r'\b(Worker(?:Dirt|RockSparks|GoldSplinters|GemGlint|CoinCloud|Rubble))\b', cfg):
    assert re.search(r'^particle_system\s+%s\s*$' % name, particles, re.MULTILINE), name

print('worker reactions: ok (%d events used by the code)' % len(used))
