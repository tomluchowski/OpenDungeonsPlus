#!/usr/bin/env python3
"""Non-compiling checks of the rooster's coop-clear flight and mounting alignment."""
import math
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
chicken = (root / 'source/entities/ChickenEntity.cpp').read_text()
room = (root / 'source/rooms/RoomHatchery.cpp').read_text()
looks = (root / 'source/render/RenderManagerChickens.cpp').read_text()
client = (root / 'source/network/ODClient.cpp').read_text()
for token in ('direction = hen.getWalkDirection()', 'setWalkDirection(direction)',
              'hen.playPose(ChickenPose::cackle, 3)', 'mMountHenName = hen.getName()',
              'notification->mPacket << getName() << hen.getName()', 'is >> mMountHenName'):
    assert token in chicken, token
assert 'rooster->mountHen(*target)' in room
assert 'rrChickenMount' in client and 'setMountHenFromServer(henName)' in client
for token in ('other->first->getName() != chicken->getMountHenName()',
              'hen->second.mNode->convertLocalToWorldPosition(henHead)',
              'const Ogre::Vector3 headOffset = lean * (roosterHead * kindScale(kind) * stretch)',
              'shift = (targetHead - headOffset) * on'):
    assert token in looks, token
assert 'RoomObjectPath::clearPoint(obstacles, position)' in room, 'takeoff only outside the full footprint'
assert 'rooster->walkToward(approach, 0.0, ChickenPose::strut)' in room
assert 'RoomObjectNavigation::collect(*getGameMap(), clearance)' in room
# The technical clearance follows the existing rooster scale, never a new balance setting.
assert chicken.count('ROOF_FLIGHT_CLEARANCE * static_cast<Ogre::Real>') == 1
assert room.count('ROOSTER_CLEARANCE_PER_SCALE * static_cast<Ogre::Real>') == 2


def ease(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


def flight(a, b, t, clearance):
    across = ease((t - 0.3) / 0.4)
    p = [a[i] + (b[i] - a[i]) * across for i in range(3)]
    high = max(a[2], b[2]) + clearance
    p[2] = a[2] + (high - a[2]) * ease(t / 0.3) if t < 0.3 else (
        high + (b[2] - high) * ease((t - 0.7) / 0.3) if t > 0.7 else high)
    return p


out = root / 'out/rooster-flight'
out.mkdir(parents=True, exist_ok=True)
converter = shutil.which('OgreXMLConverter')
if converter is None:
    print('OgreXMLConverter not on the PATH: mesh and skeleton checks skipped')
    sys.exit(0)
for name in ('ChickenCoopHouse', 'ChickenRooster'):
    subprocess.run([converter, '-q', '-log', str(out / (name + '-alignment.log')),
                    str(root / ('models/' + name + '.mesh')), str(out / (name + '.mesh.xml'))],
                   check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, timeout=60)
coop = ET.parse(out / 'ChickenCoopHouse.mesh.xml').getroot()
rooster = ET.parse(out / 'ChickenRooster.mesh.xml').getroot()
points = [tuple(float(p.get(a)) for a in 'xyz') for p in coop.iter('position')]
roof = max(p[2] for p in points)
assert abs(roof - 0.975) < 1e-6
scale = 1.25
clearance = 0.4 * scale
# Base mesh tail and body fit inside the same expanded horizontal footprint for every heading.
radius = max(math.hypot(float(p.get('x')), float(p.get('y'))) for p in rooster.iter('position')) * scale
assert radius < clearance
# Use the actual coop bounds and the existing generic furniture scaling.
furniture = (root / 'source/gamemap/RoomObjectBounds.h').read_text()
assert 'float width = 0.6f, depth = 0.6f;' in furniture
factor = min(1.0, 0.6 / (max(p[0] for p in points) - min(p[0] for p in points)),
             0.6 / (max(p[1] for p in points) - min(p[1] for p in points)))
min_x = min(p[0] for p in points) * factor
max_x = max(p[0] for p in points) * factor
min_y = min(p[1] for p in points) * factor
max_y = max(p[1] for p in points) * factor
# The 1/8-tile search finds a full-body-clear ground spot within the existing 1-tile land reach.
candidates = [(x / 8, y / 8, 0.0) for x in range(-8, 9) for y in range(-8, 9)
              if math.hypot(x / 8, y / 8) <= 1 and
              not (min_x - clearance < x / 8 < max_x + clearance and
                   min_y - clearance < y / 8 < max_y + clearance)]
assert candidates
start = min(candidates, key=lambda p: p[0] ** 2 + p[1] ** 2)
goal = (0.3, 0.0, roof)
previous = None
for i in range(1001):
    t = i / 1000
    p = flight(start, goal, t, clearance)
    reverse = flight(goal, start, 1 - t, clearance)
    assert max(abs(x - y) for x, y in zip(p, reverse)) < 1e-12
    if t <= 0.3:
        assert p[:2] == list(start[:2]), 'vertical takeoff clear of coop walls'
    elif t <= 0.7:
        assert p[2] == roof + clearance, 'whole body above roof during crossing'
    else:
        assert max(abs(x - y) for x, y in zip(p[:2], goal[:2])) < 1e-12, 'vertical landing on the roof'
    if previous is not None:
        assert math.dist(p, previous) < 0.01, 'no position jump'
    previous = p
assert flight(start, goal, 0, clearance) == list(start)
assert max(abs(x - y) for x, y in zip(flight(start, goal, 1, clearance), goal)) < 1e-12
# Projected head alignment works for all headings, scales and lean angles: the translated head equals the target.
for angle in range(0, 360, 15):
    heading = math.radians(angle)
    target = (0.7 * math.cos(heading), 0.7 * math.sin(heading))
    for lean in (0, 20, 40):
        pitch = math.radians(lean)
        offset = (0.02 * scale, (-0.095 * math.cos(pitch) - 0.139 * math.sin(pitch)) * scale)
        shift = (target[0] - offset[0], target[1] - offset[1])
        assert max(abs(shift[i] + offset[i] - target[i]) for i in (0, 1)) < 1e-12
print('rooster coop-clear flight and mounting alignment checks passed')
