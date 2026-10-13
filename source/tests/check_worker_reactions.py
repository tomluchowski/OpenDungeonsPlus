"""Worker reactions: the hooks are wired and nothing touches the game.
Static checks only, never launches a game. The event and particle checks come with the worker reaction config."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


code = read('source/render/WorkerReactions.cpp')
reactions = read('source/render/CreatureReactions.cpp')
reactions_h = read('source/render/CreatureReactions.h')

# Overlay principle: no animation state, no activity, no refresh, no network in the worker reactions
for forbidden in ('setAnimationState', 'mActivity', 'fireCreatureRefreshIfNeeded', 'ODPacket', 'ServerNotification',
                  'sendCosmeticEvent'):
    assert forbidden not in code, forbidden

# The code names events; the config that defines them follows in the next change
used = set(re.findall(r'"((?:Dig|Claim|Reinforce|Pick|Gold|Treasury|Prisoner|Corpse|Trap|Worker)\w*)"', code))
assert used, 'no events found in the code'

# The hooks of the framework call the worker reactions
for call in ('WorkerReactions::update(', 'WorkerReactions::noteAnimation(', 'WorkerReactions::noteCarry(',
             'WorkerReactions::noteRelease(', 'WorkerReactions::noteHandled(', 'WorkerReactions::noteParticleEffect(',
             'WorkerReactions::noteCosmeticEvent(', 'WorkerReactions::stopAll('):
    assert call in reactions, call
assert 'friend class WorkerReactions;' in reactions_h
assert 'render/WorkerReactions.cpp' in read('CMakeLists.txt')

print('worker reactions: ok (%d events used by the code)' % len(used))
