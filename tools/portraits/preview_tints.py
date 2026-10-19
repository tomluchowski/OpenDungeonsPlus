"""Offline preview of config/portrait-tints.cfg.

This mirrors the recolouring in source/render/PortraitTint.cpp (same maths, same quarter
resolution), so the config can be tuned without starting the game.

KEY is the portrait image name without 'portrait-' and '.png', for example Orc.mesh or Orc.mesh-female.

  preview_tints.py sheet OUT.png KEY [COUNT [X0,Y0,X1,Y1]]   original plus COUNT tinted variants (creature names MESH1..), optional zoomed crop
  preview_tints.py mask OUT.png KEY REGION       original with the selection weight of one region in red
"""
import math
import re
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


# Kinds of block the config file is in while it is read (as in PortraitTint.cpp)
BLOCK_NONE = -1
BLOCK_PALETTE = 0
BLOCK_PORTRAIT = 1
NUMBER = re.compile(r'^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$')


def parse_floats(text):
    """Same rules as parseFloats() in PortraitTint.cpp: comma separated numbers, at least one."""
    values = []
    for item in text.split(','):
        item = item.strip()
        if not NUMBER.match(item):
            return None
        values.append(float(item))
    return values


def parse_region(cols, palettes):
    """Same rules as PortraitTint::parseRegion(). Returns (region, error)."""
    if len(cols) < 3:
        return None, 'Region needs a name and settings'
    region = {}
    for col in cols[2:]:
        if '=' not in col:
            return None, "expected key=value but found '%s'" % col
        key, text = col.split('=', 1)
        if key == 'palette':
            if text not in palettes:
                return None, "unknown palette '%s'" % text
            region['palette'] = text
            continue
        values = parse_floats(text)
        if values is None:
            return None, "bad numbers in '%s'" % col
        if key == 'shift' and len(values) == 3:
            region['shift'] = values
        elif key in ('box',) and len(values) == 4:
            region[key] = values
        elif key == 'not' and len(values) % 4 == 0:
            region[key] = values
        elif key in ('hue', 'sat', 'val') and len(values) == 2:
            region[key] = values
        else:
            return None, "unknown key or wrong number of values in '%s'" % col
    if not (('palette' in region or 'shift' in region) and all(k in region for k in ('box', 'hue', 'sat', 'val'))):
        return None, "Region '%s' needs palette= or shift=, box=, hue=, sat= and val=" % cols[1]
    return region, None


def load_config(path):
    """Parse the tint config with the same rules as PortraitTint::loadFromFile().

    Returns (palettes, portraits, errors); every line the C++ loader would reject is an entry in errors.
    """
    palettes = {}
    portraits = {}
    errors = []
    current = BLOCK_NONE
    palette = None
    portrait = None
    for number, raw in enumerate(Path(path).read_text().splitlines(), 1):
        line = raw.split('#', 1)[0]
        if not line.strip():
            continue
        cols = [c.strip() for c in line.split('\t')]
        key = cols[0]
        where = '%s:%d: ' % (Path(path).name, number)
        if key == '[Palette]':
            palette = []
            palettes['?unnamed%d' % len(palettes)] = palette
            current = BLOCK_PALETTE
        elif key == '[Portrait]':
            portrait = []
            portraits['?unnamed%d' % len(portraits)] = portrait
            current = BLOCK_PORTRAIT
        elif key == 'Name' and current == BLOCK_PALETTE and len(cols) >= 2:
            if cols[1] in palettes:
                errors.append(where + "duplicate palette '%s'" % cols[1])
            palettes = {(cols[1] if v is palette else k): v for k, v in palettes.items()}
        elif key == 'Colour' and current == BLOCK_PALETTE and len(cols) >= 5:
            numbers = [parse_floats(c) for c in cols[2:5]]
            if any(n is None or len(n) != 1 for n in numbers):
                errors.append(where + 'bad Colour numbers')
                continue
            palette.append((cols[1], numbers[0][0], numbers[1][0], numbers[2][0]))
        elif key == 'Mesh' and current == BLOCK_PORTRAIT and len(cols) >= 2:
            if cols[1] in portraits:
                errors.append(where + "duplicate portrait '%s'" % cols[1])
            portraits = {(cols[1] if v is portrait else k): v for k, v in portraits.items()}
        elif key == 'Region' and current == BLOCK_PORTRAIT:
            region, error = parse_region(cols, palettes)
            if error:
                errors.append(where + error)
            elif 'palette' in region and not palettes[region['palette']]:
                errors.append(where + "palette '%s' has no colours" % region['palette'])
            else:
                portrait.append((cols[1], region))
        else:
            errors.append(where + "unexpected line '%s'" % key)
    palettes = {k: v for k, v in palettes.items() if not k.startswith('?')}
    portraits = {k: v for k, v in portraits.items() if not k.startswith('?')}
    return palettes, portraits, errors


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
    image = Image.open(REPO / ('materials/textures/portrait-%s.png' % mesh)).convert('RGB')
    width, height = image.size[0] // SCALE, image.size[1] // SCALE
    array = np.asarray(image, dtype=np.float32) / 255.0
    array = array[:height * SCALE, :width * SCALE]
    return array.reshape(height, SCALE, width, SCALE, 3).mean(axis=(1, 3))


def to_image(array):
    return Image.fromarray((array * 255.0 + 0.5).astype(np.uint8))


def main():
    cmd, out, mesh = sys.argv[1], sys.argv[2], sys.argv[3]
    palettes, portraits, errors = load_config(REPO / 'config/portrait-tints.cfg')
    for error in errors:
        print('config error:', error)
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
