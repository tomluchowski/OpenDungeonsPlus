"""Inspect shipped coop geometry, opening, dark interior and ridge UVs; no compilation or game start."""
from pathlib import Path
from collections import Counter
import importlib.util
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'out' / 'coop-depth-check'
OUT.mkdir(parents=True, exist_ok=True)
# Converter executable: set OGRE_XML_CONVERTER to its full path, otherwise it is looked up on the PATH.
CONVERTER = Path(os.environ.get('OGRE_XML_CONVERTER', 'OgreXMLConverter'))


def convert(source, target):
    subprocess.run([str(CONVERTER), '-q', '-log', str(target) + '.log', str(source), str(target)],
                   check=True, timeout=60, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return ET.parse(target).getroot()


def geometry(root):
    buffers = root.findall('sharedgeometry/vertexbuffer')
    points = [tuple(float(p.get(c)) for c in ('x', 'y', 'z')) for p in buffers[0].findall('vertex/position')]
    uv = [tuple(float(p.get(c)) for c in ('u', 'v')) for p in buffers[1].findall('vertex/texcoord')]
    weights = {int(w.get('vertexindex')): int(w.get('boneindex'))
               for w in root.findall('boneassignments/vertexboneassignment')}
    faces = {}
    for sm in root.findall('submeshes/submesh'):
        faces[sm.get('material')] = [tuple(int(f.get(k)) for k in ('v1', 'v2', 'v3')) for f in sm.findall('faces/face')]
    return points, uv, weights, faces


spec = importlib.util.spec_from_file_location('coop_depth', ROOT / 'tools/blender-assets/coop_depth.py')
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)
BASELINE_REVISION = 'a4ac984c8^'  # the state before the doorway/ridge change was committed
baseline = subprocess.run(['git', 'show', BASELINE_REVISION + ':models/ChickenCoopHouse.mesh'], cwd=ROOT,
                          capture_output=True, check=True, timeout=60, stdin=subprocess.DEVNULL).stdout
(OUT / 'baseline.mesh').write_bytes(baseline)
before = convert(OUT / 'baseline.mesh', OUT / 'baseline.xml')
after = convert(ROOT / 'models/ChickenCoopHouse.mesh', OUT / 'current.xml')
a, auv, aw, afaces = geometry(before)
b, buv, bw, bfaces = geometry(after)
assert a == b and aw == bw, 'positions or bone weights changed outside the targeted face/UV changes'
assert (ROOT / 'models/ChickenCoopHouse.skeleton').read_bytes() == subprocess.run(
    ['git', 'show', BASELINE_REVISION + ':models/ChickenCoopHouse.skeleton'], cwd=ROOT,
    capture_output=True, check=True, timeout=60, stdin=subprocess.DEVNULL).stdout
# Door faces stay exactly as shipped, including their material and UVs.
def door(faces, weights):
    return Counter((mat, face) for mat, group in faces.items() for face in group if any(weights[i] == 1 for i in face))
assert door(afaces, aw) == door(bfaces, bw)
assert all(auv[i] == buv[i] for i in aw if aw[i] == 1)
removed = [f for f in afaces['ChickenCoop'] if all(aw[i] == 0 for i in f) and patch.doorway([a[i] for i in f])]
assert len(removed) == 136
assert not any(all(bw[i] == 0 for i in f) and patch.doorway([b[i] for i in f]) for group in bfaces.values() for f in group)
# All other triangles survive exactly; only existing lining changes material.
all_before = Counter(f for group in afaces.values() for f in group)
all_after = Counter(f for group in bfaces.values() for f in group)
assert all_before - all_after == Counter(removed)
assert not all_after - all_before
assert len(bfaces['ChickenCoopInterior']) == 300
assert all(patch.interior([b[i] for i in f]) for f in bfaces['ChickenCoopInterior'])
assert afaces['ChickenCoopStraw'] == bfaces['ChickenCoopStraw'], 'decorative straw/nests changed'
assert len(bfaces['ChickenCoopStraw']) == 1452
# Lining includes both side/back walls; floor straw has depth below the doorway's top.
lining = [b[i] for f in bfaces['ChickenCoopInterior'] for i in f]
assert min(p[0] for p in lining) < 0.07 and max(p[0] for p in lining) > 0.5
assert min(p[1] for p in lining) < -0.27 and max(p[1] for p in lining) > 0.27
straw = [b[i] for f in bfaces['ChickenCoopStraw'] for i in f]
assert min(p[2] for p in straw) < 0.22 and max(p[2] for p in straw) > 0.235
ridge_indices = [i for i, p in enumerate(b) if bw[i] != 1 and patch.ridge(p)]
assert len(ridge_indices) == 60 and all(auv[i] != buv[i] for i in ridge_indices)
assert all(auv[i] == buv[i] for i in range(len(a)) if i not in ridge_indices)
image = Image.open(ROOT / 'materials/textures/ChickenCoop.png').convert('RGB')
def brightness(coords):
    values = [sum(image.getpixel((int(u * image.width) % image.width, int(v * image.height) % image.height))) / 3 for u, v in coords]
    return sum(values) / len(values)
old_brightness = brightness([auv[i] for i in ridge_indices])
new_brightness = brightness([buv[i] for i in ridge_indices])
assert new_brightness < old_brightness * 0.9, (old_brightness, new_brightness)
material = (ROOT / 'materials/scripts/ChickenCoop.material').read_text()
interior_material = material[material.index('material ChickenCoopInterior'):]
assert 'texture ChickenCoop.png' in interior_material
assert 'ambient 0.22 0.18 0.14' in interior_material and 'specular 0.0 0.0 0.0' in interior_material
assert 'emissive' not in interior_material
credits = (ROOT / 'CREDITS').read_text()
assert 'coop_depth.py' in credits and 'ChickenCoopInterior' in credits
print('coop depth OK: 136 black plug triangles removed, door/skeleton unchanged, 300 dark lining faces, existing straw nests retained; ridge atlas brightness %.1f -> %.1f' % (old_brightness, new_brightness))
