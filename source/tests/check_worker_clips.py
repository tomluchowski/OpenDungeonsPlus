#!/usr/bin/env python3
"""Pure check (nothing compiled, nothing started): the clips that the worker events of
config/creatureReactions.cfg name are the known worker clips, and where the worker skeletons carry them they exist in
both skeletons and the older clips are still there. Events that decorate a running animation (WhileWorking) may name
a clip: the framework lays it over the work clip (see check_work_clip_overlay.py)."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
cfg = (root / 'config' / 'creatureReactions.cfg').read_text(encoding='utf-8')
WORKER_CLIPS = ('PickUp', 'PutDown', 'Toss', 'Pat', 'Hammer', 'Shove', 'Panic', 'Cheer', 'Sweep', 'Sharpen', 'Lean', 'Rest')
# Clips that only the events of later features name (the rooms and doors that put things in place)
LATER_CLIPS = ('PutDown',)
OLD_CLIPS = ('Idle', 'Walk', 'Attack1', 'Die', 'Dig', 'Claim', 'Drag')
skeletons = {name: (root / 'models' / (name + '.skeleton')).read_bytes() for name in ('Dwarf1', 'Kobold')}

used = set()
for match in re.finditer(r'\[Event\](.*?)\[/Event\]', cfg, re.S):
    body = match.group(1)
    name = re.search(r'Name\s+(\S+)', body).group(1)
    clips = re.findall(r'^\s*Clip\s+(\S+)', body, re.M)
    for clip in clips:
        if clip in WORKER_CLIPS:
            used.add(clip)
assert set(WORKER_CLIPS) - used <= set(LATER_CLIPS), sorted(set(WORKER_CLIPS) - used)

# The clips live in the worker skeletons of the worker animation changes; without them the model keeps the small
# movement and the effect, so the skeletons are only checked where they carry the clips
checked = 0
for skeleton, data in skeletons.items():
    if b'PickUp' not in data:
        continue

    checked += 1
    for clip in WORKER_CLIPS + ('Flee',) + OLD_CLIPS:
        assert clip.encode() in data, skeleton + ' has no ' + clip

print('worker clips: ok (%d skeletons checked)' % checked)
