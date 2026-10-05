"""Check the iris regions of the eye tints (needs the delivered artwork in materials/portraits/variants, numpy and
Pillow; skipped with a note otherwise).

For every eye option with a block in config/dungeonbook-part-tints.cfg the pixels the tint touches (weight above
0.25, mirror of PortraitTint's ellipse and colour limits) are computed. The share that does not belong to the
one connected iris area (fringes on the brow, specks on lids or skin, detached islands; the area is joined over
gaps of two pixels) must stay below SPILL_LIMIT, and at least MIN_PIXELS pixels must be touched.
"""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[2]
root = repo / 'materials' / 'portraits' / 'variants'
if not root.is_dir():
    print('dungeonbook eyes skipped: no artwork in materials/portraits/variants')
    sys.exit(0)
try:
    import numpy as np
    from PIL import Image
except ImportError:
    print('dungeonbook eyes skipped: numpy or Pillow missing')
    sys.exit(0)

SPILL_LIMIT = 0.08
MIN_PIXELS = 12


def hsv(rgb):
    mx = rgb.max(-1)
    mn = rgb.min(-1)
    d = mx - mn
    s = np.where(mx > 0, d / np.maximum(mx, 1e-9), 0)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    dd = np.where(d > 0, d, 1)
    h = np.where(mx == r, ((g - b) / dd) % 6, np.where(mx == g, (b - r) / dd + 2, (r - g) / dd + 4)) * 60
    return np.where(d > 0, h, 0), s, mx


def ramp(v, lo, hi, f):
    return np.minimum(np.clip(1 - (lo - v) / f, 0, 1), np.clip(1 - (v - hi) / f, 0, 1))


def hue_weight(h, lo, hi):
    inside = (h >= lo) & (h <= hi) if lo <= hi else (h >= lo) | (h <= hi)
    dist = np.minimum(np.minimum(abs(h - lo), 360 - abs(h - lo)), np.minimum(abs(h - hi), 360 - abs(h - hi)))
    return np.where(inside, 1, np.clip(1 - dist / 10, 0, 1))


def dilate(m):
    p = np.pad(m, 1)
    o = np.zeros_like(m)
    for dy in range(3):
        for dx in range(3):
            o |= p[dy:dy + m.shape[0], dx:dx + m.shape[1]]
    return o


def label(mask):
    lab = np.zeros(mask.shape, int)
    sizes = [0]
    for sy, sx in zip(*np.nonzero(mask)):
        if lab[sy, sx]:
            continue
        n = len(sizes)
        stack = [(sy, sx)]
        lab[sy, sx] = n
        count = 0
        while stack:
            cy, cx = stack.pop()
            count += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    ny, nx = cy + dy, cx + dx
                    if 0 <= ny < mask.shape[0] and 0 <= nx < mask.shape[1] and mask[ny, nx] and not lab[ny, nx]:
                        lab[ny, nx] = n
                        stack.append((ny, nx))
        sizes.append(count)
    return lab, sizes


blocks = {}
current = None
for line in (repo / 'config' / 'dungeonbook-part-tints.cfg').read_text(encoding='utf-8').splitlines():
    c = line.split('\t')
    if c[0] == 'Mesh':
        current = c[1]
        blocks[current] = []
    elif c[0] == 'Region' and current and any(x.startswith('ellipse=') for x in c):
        blocks[current].append({x.split('=')[0]: x.split('=')[1] for x in c[2:]})

checked = 0
worst = 0.0
for key, regions in sorted(blocks.items()):
    if ':eyes:' not in key:
        continue
    catalog, _, option = key.split(':')
    manifest = (root / catalog / 'manifest.cfg').read_text(encoding='utf-8').splitlines()
    name = [l.split('\t')[4] for l in manifest if l.startswith('Option\teyes\t') and l.split('\t')[3] == option]
    assert name, 'no eye option %s in the manifest of %s' % (option, catalog)
    image = np.array(Image.open(root / catalog / name[0]).convert('RGBA')).astype(float) / 255
    height, width = image.shape[:2]
    yy, xx = np.mgrid[0:height, 0:width]
    h, s, v = hsv(image[..., :3])
    for region in regions:
        cx, cy, rx, ry = [float(t) for t in region['ellipse'].split(',')]
        lo, hi = [float(t) for t in region['hue'].split(',')]
        slo, shi = [float(t) for t in region['sat'].split(',')]
        vlo, vhi = [float(t) for t in region['val'].split(',')]
        rx, ry = rx * width, ry * width
        small = min(rx, ry)
        feather = max(1.0, 0.15 * small)
        dist = np.hypot((xx + .5 - cx * width) / rx, (yy + .5 - cy * height) / ry) * small
        weight = np.clip(1 - (dist - small) / feather, 0, 1) * hue_weight(h, lo, hi) * ramp(s, slo, shi, 0.08) * ramp(v, vlo, vhi, 0.08)
        mask = (weight > 0.25) & (image[..., 3] > 0)
        total = int(mask.sum())
        assert total >= MIN_PIXELS, '%s: only %d pixels are tinted' % (key, total)
        lab, sizes = label(dilate(dilate(mask)))
        big = 1 + int(np.argmax(sizes[1:]))
        spill = 1 - float((mask & (lab == big)).sum()) / total
        worst = max(worst, spill)
        assert spill <= SPILL_LIMIT, '%s: %.0f%% of the tinted pixels lie outside the iris area (limit %.0f%%)' % (key, spill * 100, SPILL_LIMIT * 100)
        checked += 1
print('dungeonbook eyes ok: %d iris regions, worst spill %.1f%%' % (checked, worst * 100))
