#!/usr/bin/env python3
"""Worker extras: struggling prisoners, coins of a dying worker and worker sounds.

Static checks only (no compiling, never launches a game):
- the extras code does not touch the game (no animation state, no activity, no refresh, no network)
- every event with a sound exists in the config and every sound family has at least one .ogg file
- every sound file is listed in CREDITS
- the death coins event exists, is made for the dying creature and its effect exists
- the hooks in the worker reactions are in place and the source is in the build file
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]


def read(path):
    return (root / path).read_text(encoding='utf-8')


extras = read('source/render/WorkerExtras.cpp')
reactions = read('source/render/WorkerReactions.cpp')
cfg = read('config/creatureReactions.cfg')
credits = read('CREDITS')

for forbidden in ('setAnimationState', 'mActivity', 'fireCreatureRefreshIfNeeded', 'ODPacket', 'ServerNotification',
                  'sendCosmeticEvent'):
    assert forbidden not in extras, forbidden

events = set(re.findall(r'^\s*Name\s+(\w+)\s*$', cfg, re.MULTILINE))

# Sound table: event -> family
table = re.findall(r'eventName == "(\w+)"\)', extras)
families = set(re.findall(r'PREFIX \+ "(\w+)"', extras))
assert table and families
for name in table:
    assert name in events, 'sound event missing in the config: %s' % name
for family in families:
    folder = root / 'sounds' / 'Spatial' / 'Creatures' / 'Worker' / family
    files = sorted(folder.glob('*.ogg'))
    assert files, 'no sound files for %s' % family
    assert ('Spatial/Creatures/Worker/%s/' % family) in credits, 'no CREDITS entry for %s' % family
    for item in files:
        assert item.stat().st_size > 1000, item
        assert item.read_bytes()[:4] == b'OggS', item

# Death coins: for the dying creature, with an effect that exists
match = re.search(r'Name\s+WorkerDeathCoins\s*\r?\n(.*?)\[/Event\]', cfg, re.DOTALL)
assert match, 'WorkerDeathCoins missing'
assert re.search(r'^\s*Dying\s+yes\s*$', match.group(1), re.MULTILINE)
particles = read('particles/WorkerReactions.particle') + read('particles/CreatureReactions.particle')
for effect in re.findall(r'^\s*(?:Late)?Effect\s+(\w+)', match.group(1), re.MULTILINE):
    assert re.search(r'^particle_system\s+%s\s*$' % effect, particles, re.MULTILINE), effect

# Hooks and build
for call in ('WorkerExtras::startStruggle(', 'WorkerExtras::endStruggle(', 'WorkerExtras::update(',
             'WorkerExtras::stopAll(', 'WorkerExtras::playSound('):
    assert call in reactions, call
assert '"WorkerDeathCoins"' in reactions
assert 'render/WorkerExtras.cpp' in read('CMakeLists.txt')

# Gold digging shows small splinters only: no big additive glow over the worker
for name in ('DigHitGold', 'DigFinishGold'):
    block = re.search(r'Name\s+%s\s*$(.*?)\[/Event\]' % name, cfg, re.MULTILINE | re.DOTALL)
    assert block, name
    assert 'ReactionGlow' not in block.group(1), '%s must not use the big glow' % name

print('worker extras: ok (%d sound events, %d families)' % (len(table), len(families)))
