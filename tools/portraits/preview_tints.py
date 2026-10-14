"""Offline preview of config/portrait-tints.cfg.

This mirrors the recolouring in source/render/PortraitTint.cpp (same maths, same quarter
resolution), so the config can be tuned without starting the game.

  preview_tints.py sheet OUT.png MESH [COUNT [X0,Y0,X1,Y1]]   original plus COUNT tinted variants (creature names MESH1..), optional zoomed crop
  preview_tints.py mask OUT.png MESH REGION       original with the selection weight of one region in red
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SCALE = 4
HUE_FEATHER = 10.0
SV_FEATHER = 0.08
BOX_FEATHER = 0.02
MASK64 = (1 << 64) - 1


def fnv1a64(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value ^= byte
        value = (value * 0x100000001b3) & MASK64
    return value


class Rng:
    def __init__(self, seed):
        self.state = seed & MASK64

    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & MASK64
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
        return z ^ (z >> 31)

    def below(self, count):
        value = self.next()
        return 0 if count == 0 else value % count


def field_rng(name, field):
    return Rng(fnv1a64(name) ^ fnv1a64('field:' + field))


def load_config(path):
    palettes = {}
    portraits = {}
    current = None
    for raw in Path(path).read_text().splitlines():
        line = raw.split('#', 1)[0].rstrip()
        if not line.strip():
            continue
        cols = line.split('\t')
        key = cols[0].strip()
        if key == '[Palette]':
            current = ('palette', None)
        elif key == '[Portrait]':
            current = ('portrait', None)
        elif key == 'Name':
            palettes[cols[1]] = []
            current = ('palette', cols[1])
        elif key == 'Colour':
            palettes[current[1]].append((cols[1], float(cols[2]), float(cols[3]), float(cols[4])))
        elif key == 'Mesh':
            portraits[cols[1]] = []
            current = ('portrait', cols[1])
        elif key == 'Region':
            fields = {}
            for col in cols[2:]:
                k, v = col.split('=')
                fields[k] = [float(x) for x in v.split(',')] if k != 'palette' else v
            portraits[current[1]].append((cols[1], fields))
    return palettes, portraits


def rgb_to_hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = np.max(rgb, axis=-1)
    mn = np.min(rgb, axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    safe = np.where(d > 0, d, 1.0)
    h = np.where(mx == r, ((g - b) / safe) % 6.0, h)
    h = np.where((mx == g) & (mx != r), (b - r) / safe + 2.0, h)
    h = np.where((mx == b) & (mx != r) & (mx != g), (r - g) / safe + 4.0, h)
    h = np.where(d > 0, h * 60.0, 0.0)
    s = np.where(mx > 0, d / np.where(mx > 0, mx, 1.0), 0.0)
    return h, s, mx


def hsv_to_rgb(h, s, v):
    c = v * s
    hp = (h % 360.0) / 60.0
    x = c * (1.0 - np.abs(hp % 2.0 - 1.0))
    m = v - c
    sector = np.floor(hp).astype(int) % 6
    r = np.choose(sector, [c, x, 0 * c, 0 * c, x, c])
    g = np.choose(sector, [x, c, c, x, 0 * c, 0 * c])
    b = np.choose(sector, [0 * c, 0 * c, x, c, c, x])
    return np.stack([r + m, g + m, b + m], axis=-1)


def ramp(value, lo, hi, feather):
    below = np.clip(1.0 - (lo - value) / feather, 0.0, 1.0)
    above = np.clip(1.0 - (value - hi) / feather, 0.0, 1.0)
    return np.minimum(below, above)


def circ(a, b):
    d = np.abs(a - b)
    return np.minimum(d, 360.0 - d)


def hue_weight(h, lo, hi):
    if lo <= hi:
        inside = (h >= lo) & (h <= hi)
    else:
        inside = (h >= lo) | (h <= hi)
    dist = np.minimum(circ(h, lo), circ(h, hi))
    return np.where(inside, 1.0, np.clip(1.0 - dist / HUE_FEATHER, 0.0, 1.0))


def region_weight(region, h, s, v):
    height, width = h.shape
    xs = (np.arange(width) + 0.5) / width
    ys = (np.arange(height) + 0.5) / height
    x0, y0, x1, y1 = region['box']
    box = np.outer(ramp(ys, y0, y1, BOX_FEATHER), ramp(xs, x0, x1, BOX_FEATHER))
    weight = (box * hue_weight(h, *region['hue']) * ramp(s, *region['sat'], SV_FEATHER) *
              ramp(v, *region['val'], SV_FEATHER))
    if 'not' in region:
        cut = region['not']
        for i in range(0, len(cut) - 3, 4):
            weight = weight * (1.0 - np.outer(ramp(ys, cut[i + 1], cut[i + 3], BOX_FEATHER),
                                              ramp(xs, cut[i], cut[i + 2], BOX_FEATHER)))
    return weight


def tint(base, name, palettes, regions):
    """base: float RGB array (quarter resolution). Returns the tinted array."""
    h, s, v = rgb_to_hsv(base)
    out = base.copy()
    for region_name, region in regions:
        w = region_weight(region, h, s, v)
        total = float(w.sum())
        if total < 1.0:
            continue
        rng = field_rng(name, 'portrait:' + region_name)
        if 'palette' in region:
            colours = palettes[region['palette']]
            _, th, ts, tv = colours[rng.below(len(colours))]
            mean_v = max(0.05, float((w * v).sum()) / total)
            nv = np.minimum(1.0, tv * np.power(np.maximum(v, 0.01) / mean_v, 0.85))
            nh = np.full_like(h, th)
            ns = np.full_like(s, ts)
        else:
            dh, ds, dv = region['shift']
            sh = (rng.below(2001) / 1000.0 - 1.0) * dh
            ss = 1.0 + (rng.below(2001) / 1000.0 - 1.0) * ds
            sv = 1.0 + (rng.below(2001) / 1000.0 - 1.0) * dv
            nh = h + sh
            ns = np.clip(s * ss, 0.0, 1.0)
            nv = np.clip(v * sv, 0.0, 1.0)
        new = hsv_to_rgb(nh, ns, nv)
        cur_h, cur_s, cur_v = rgb_to_hsv(out)
        out = out * (1.0 - w[..., None]) + new * w[..., None]
    return np.clip(out, 0.0, 1.0)


def load_base(mesh):
    image = Image.open(REPO / ('materials/textures/portrait-%s.mesh.png' % mesh)).convert('RGB')
    width, height = image.size[0] // SCALE, image.size[1] // SCALE
    array = np.asarray(image, dtype=np.float32) / 255.0
    array = array[:height * SCALE, :width * SCALE]
    return array.reshape(height, SCALE, width, SCALE, 3).mean(axis=(1, 3))


def to_image(array):
    return Image.fromarray((array * 255.0 + 0.5).astype(np.uint8))


def main():
    cmd, out, mesh = sys.argv[1], sys.argv[2], sys.argv[3]
    palettes, portraits = load_config(REPO / 'config/portrait-tints.cfg')
    base = load_base(mesh)
    regions = portraits[mesh]
    if cmd == 'mask':
        wanted = sys.argv[4]
        h, s, v = rgb_to_hsv(base)
        for region_name, region in regions:
            if region_name == wanted:
                w = region_weight(region, h, s, v)
                shown = base * (1 - 0.7 * w[..., None]) + np.array([1.0, 0.0, 0.0]) * 0.7 * w[..., None]
                to_image(shown).resize((base.shape[1] * 2, base.shape[0] * 2)).save(out)
                print('weight sum', w.sum())
        return
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 6
    tiles = [base] + [tint(base, '%s%d' % (mesh, i + 1), palettes, regions) for i in range(count)]
    height, width = base.shape[:2]
    box = (0, 0, width, height)
    if len(sys.argv) > 5:
        x0, y0, x1, y1 = [float(x) for x in sys.argv[5].split(',')]
        box = (int(x0 * width), int(y0 * height), int(x1 * width), int(y1 * height))
    cw, ch = box[2] - box[0], box[3] - box[1]
    zoom = 2 if len(sys.argv) > 5 else 1
    sheet = Image.new('RGB', (cw * zoom * len(tiles), ch * zoom))
    for i, tile in enumerate(tiles):
        crop = to_image(tile).crop(box).resize((cw * zoom, ch * zoom), Image.LANCZOS)
        sheet.paste(crop, (i * cw * zoom, 0))
    sheet.save(out)


if __name__ == '__main__':
    main()
