#!/usr/bin/env python3
"""Pure check (nothing compiled, nothing started): a WhileWorking reaction can lay a named clip over the running work
clip, the work clip keeps running with weight 0 (restored afterwards), and the exhaustion event is hooked up to its
clips."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
cpp = (root / 'source' / 'render' / 'CreatureReactions.cpp').read_text(encoding='utf-8')
cfg = (root / 'config' / 'creatureReactions.cfg').read_text(encoding='utf-8')

# The gate in front of startClip: dying events never, WhileWorking events only when the variant names a clip
assert re.search(r'clipAllowed\s*=\s*!event\.mDying\s*&&\s*\(!event\.mWhileWorking\s*\|\|\s*!variant\.mClip\.empty\(\)\)', cpp)
assert 'clipAllowed && startClip(reaction, creature, variant)' in cpp
# No mActivity change, no refresh in the clip code
start = cpp.index('bool CreatureReactions::startClip')
end = cpp.index('bool CreatureReactions::addParticles')
clip_code = cpp[start:end]
assert 'mActivity' not in clip_code and 'fireCreatureRefreshIfNeeded' not in clip_code
# The work clip keeps its time and gets weight 0, and is restored when the overlay ends
assert 'base->setWeight(0.0f);' in clip_code and 'base->setWeight(1.0f);' in clip_code
# Priority rule for a WhileWorking event stays
assert 'event->mWhileWorking &&' in cpp and 'ReactionPriority::none' in cpp

events = {}
for match in re.finditer(r'\[Event\](.*?)\[/Event\]', cfg, re.S):
    body = match.group(1)
    name = re.search(r'Name\s+(\S+)', body).group(1)
    events[name] = body

def variant_clips(name):
    return re.findall(r'^\s*Clip\s+(\S+)', events[name], re.M)

for name, clips in (('DigExhausted', ('WipeBrow', 'Pant')),):
    assert re.search(r'^\s*WhileWorking\s+yes', events[name], re.M), name
    assert variant_clips(name) == list(clips), (name, variant_clips(name))
    # The old effect and emote parts stay
    assert re.search(r'^\s*(Emote|Effect)\s', events[name], re.M), name

print('work clip overlay: ok')
