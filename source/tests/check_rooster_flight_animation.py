#!/usr/bin/env python3
"""Non-compiling flight wiring, continuous trajectory and authored skeleton checks."""
import math
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
chicken = (root / 'source/entities/ChickenEntity.cpp').read_text()
pose = (root / 'source/entities/ChickenPose.h').read_text()
client = (root / 'source/network/ODClient.cpp').read_text()
for token in ('(progress - ROOF_FLIGHT_CROUCH_PART) / ROOF_FLIGHT_TRAVEL_PART',
              'lateral * lateral * (3.0f - 2.0f * lateral)', 'position.z = high;',
              'ServerNotificationType::chickenRoofFlight', 'mHopFrom << mHopTo << turns << mHopElapsed',
              'ODApplication::turnsPerSecond * timeSinceLastFrame', 'ROOF_FLIGHT_CLIP_SECONDS * mHopElapsed / mHopTurns'):
    assert token in chicken, token
assert 'startRoofFlightFromServer(from, to, turns, elapsed)' in client
assert 'return "RoofFlight";' in pose
# No walk/Idle fallback and no short one-shot Flutter in the roof-flight dispatch.
start = chicken[chicken.index('void ChickenEntity::startHop('):chicken.index('void ChickenEntity::hopToRoof(')]
assert 'ChickenPose::flutter' not in start and 'moveTo(' not in start
# Late arrivals carry the same origin, target and elapsed flight time; save text stays unchanged.
export = chicken[chicken.index('void ChickenEntity::exportToPacket('):chicken.index('void ChickenEntity::exportToStream(')]
assert 'os << mHopFrom << mHopTo << mHopTurns << mHopElapsed' in export
assert 'is >> mHopFrom >> mHopTo >> mHopTurns >> mHopElapsed' in export


def travel(u):
    v = max(0.0, min(1.0, (u - 0.12) / 0.73))
    return v * v * (3.0 - 2.0 * v)


# Both directions are continuous, reach exactly their target, and remain still during crouch/settle.
assert travel(0.12) == 0 and travel(0.85) == 1 and travel(1) == 1
for turns in (1, 4, 8):
    samples = [travel(i / (turns * 60.0)) for i in range(turns * 60 + 1)]
    assert samples[0] == 0 and samples[-1] == 1
    assert all(0 <= b - a < 0.035 for a, b in zip(samples, samples[1:]))
    for tick in range(turns + 1):
        assert samples[tick * 60] == travel(tick / turns)
    down = [1 - x for x in samples]
    assert down[0] == 1 and down[-1] == 0

out = root / 'out/rooster-flight'
out.mkdir(parents=True, exist_ok=True)
converter = shutil.which('OgreXMLConverter')
if converter is None:
    print('OgreXMLConverter not on the PATH: mesh and skeleton checks skipped')
    sys.exit(0)
subprocess.run([converter, '-q', '-log', str(out / 'skeleton-check.log'),
                str(root / 'models/Chicken.skeleton'), str(out / 'Chicken.skeleton.xml')],
               check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, timeout=60)
skel = ET.parse(out / 'Chicken.skeleton.xml').getroot()
anims = {a.get('name'): a for a in skel.find('animations')}
clip = anims['RoofFlight']
assert float(clip.get('length')) == 4.0
tracks = {t.get('bone'): t.find('keyframes').findall('keyframe') for t in clip.find('tracks')}
assert len(tracks) == 22
for name, keys in tracks.items():
    assert len(keys) == 193, (name, len(keys))
    assert float(keys[0].get('time')) == 0 and float(keys[-1].get('time')) == 4
    # Flight clip has no root displacement: all world motion comes from the authoritative trajectory.
    if name == 'Root':
        translations = [tuple(float(k.find('translate').get(a)) for a in ('x', 'y', 'z')) for k in keys]
        assert max(max(abs(x - y) for x, y in zip(v, translations[0])) for v in translations) < 1e-5
    assert ET.tostring(keys[0].find('translate')) == ET.tostring(keys[-1].find('translate')), name
    assert ET.tostring(keys[0].find('rotate')) == ET.tostring(keys[-1].find('rotate')), name
# Wings keep changing across the full travel window, including after the old Flutter would have ended.
for name in ('Shoulder_L', 'Shoulder_R'):
    keys = tracks[name]
    rotations = [float(k.find('rotate').get('angle')) for k in keys]
    for a, b in ((0.15, 0.35), (0.35, 0.55), (0.55, 0.75), (0.75, 0.85)):
        values = [v for k, v in zip(keys, rotations) if a <= float(k.get('time')) / 4 <= b]
        assert max(values) - min(values) > 0.1, (name, a, b)
# All original clips remain available to hens, chicks and roosters.
assert {'Idle', 'Walk', 'Flutter', 'Lay', 'Crow', 'Mount', 'Tread', 'Dismount', 'Duck'} <= anims.keys()
print('rooster flight animation checks passed')
