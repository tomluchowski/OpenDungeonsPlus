"""Reference for the golden hash test of composing plus tinting (test_GoldenComposeAndTint in
test_AppearanceCompose.cpp). Recomputes the expected pixels and hashes without the game and checks that the
test contains exactly these values.

The scene is built so that every step has an exactly determinable result:

* base 16x32 solid (120,100,90,255); hair part 8x8 at (4,4) solid (200,40,40,255); scar part 4x4 at (8,16)
  solid (255,255,255) with alpha 128. Composing is integer arithmetic (blendPixel), mirrored here with Python
  integers, so the compose hash is exact.
* flattenAndTint shrinks by 4 with the dark background behind it. All parts start and end on multiples of 4,
  so every 4x4 block is one colour. The sums are integers below 2^24 (exact in float32) and the division by
  the block size is correctly rounded in C++ and in numpy.float32 alike; the result times 255 plus 0.5 is
  nowhere near a rounding boundary, so the bytes are the integer colours.
* tint: one palette with ONE colour (hue 120, saturation 1, value 0.5) and one region whose hue/saturation/value
  limits select only the hair blocks (hue wraps 350..10, saturation 0.6..1, value 0.6..1) with the full box.
  The weight of a hair pixel is exactly 1 (the pixel lies inside every limit, so every ramp returns 1) and
  exactly 0 for all other blocks (their saturation is far below the limit, so the product is 0, the hue
  weight is irrelevant). The mean value of the selected pixels is the value of the hair colour itself, so
  value / mean is exactly 1, pow(1, 0.85) is exactly 1, and the new value is 0.5. HSV to RGB for hue 120
  gives (0, 0.5, 0): 0.5 * 255 + 0.5 = 128.0 exactly. A palette with one colour makes the random choice
  (Rng::below(1)) irrelevant.
numpy.float32 is used below for the steps that touch floats, and every one of them is asserted to be exact.
"""
from pathlib import Path
import re

import numpy as np

repo = Path(__file__).resolve().parents[2]
test_source = (repo / 'source/tests/test_AppearanceCompose.cpp').read_text()


def blend(dst, src, src_alpha):
    """AppearanceCompose blendPixel with Python integers."""
    if src_alpha == 0:
        return dst
    dst_alpha = dst[3]
    out_alpha = (src_alpha * 255 + dst_alpha * (255 - src_alpha) + 127) // 255
    if out_alpha == 0:
        return [0, 0, 0, 0]
    denominator = out_alpha * 255
    out = []
    for c in range(3):
        numerator = src[c] * src_alpha * 255 + dst[c] * dst_alpha * (255 - src_alpha)
        out.append((numerator + denominator // 2) // denominator)
    return out + [out_alpha]


def fnv(data):
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


W, H = 16, 32
image = [[[120, 100, 90, 255] for _ in range(W)] for _ in range(H)]


def draw(x0, y0, w, h, colour, alpha):
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            image[y][x] = blend(image[y][x], colour, alpha)


draw(4, 4, 8, 8, (200, 40, 40), 255)
draw(8, 16, 4, 4, (255, 255, 255), 128)
compose_bytes = bytes(c for row in image for px in row for c in px)
compose_hash = fnv(compose_bytes)

# flatten: 4x4 blocks, all uniform here
f32 = np.float32
bg = (f32(0.025) * f32(255), f32(0.018) * f32(255), f32(0.015) * f32(255))
rgb = np.zeros((H // 4, W // 4, 3), dtype=np.float32)
for by in range(H // 4):
    for bx in range(W // 4):
        block = [image[by * 4 + dy][bx * 4 + dx] for dy in range(4) for dx in range(4)]
        assert len({tuple(p) for p in block}) == 1, 'blocks must be uniform'
        for c in range(3):
            total = f32(0)
            for p in block:
                alpha = f32(p[3])
                inverse = f32(255) - alpha
                total = f32(total + f32(f32(p[c]) * alpha + bg[c] * inverse))
            rgb[by, bx, c] = f32(total / f32(f32(16) * f32(255) * f32(255)))

# tint: region weights
hair_blocks = {(by, bx) for by in (1, 2) for bx in (1, 2)}


def hsv(r, g, b):
    mx, mn = max(r, g, b), min(r, g, b)
    d = f32(mx - mn)
    h = f32(0)
    if d > 0:
        if mx == r:
            h = f32(np.fmod(f32((g - b) / d), f32(6)))
        elif mx == g:
            h = f32(f32((b - r) / d) + f32(2))
        else:
            h = f32(f32((r - g) / d) + f32(4))
        h = f32(h * f32(60))
        if h < 0:
            h = f32(h + f32(360))
    return h, (f32(d / mx) if mx > 0 else f32(0)), mx


def ramp(v, lo, hi, feather):
    below = min(1.0, max(0.0, 1.0 - (lo - float(v)) / feather))
    above = min(1.0, max(0.0, 1.0 - (float(v) - hi) / feather))
    return min(below, above)


weights_sum = 0
values = []
for by in range(H // 4):
    for bx in range(W // 4):
        h, s, v = hsv(*rgb[by, bx])
        sat_w = ramp(s, 0.6, 1.0, 0.08)
        val_w = ramp(v, 0.6, 1.0, 0.08)
        hue_inside = (h >= 350) or (h <= 10)
        if (by, bx) in hair_blocks:
            assert hue_inside and sat_w == 1.0 and val_w == 1.0, ('hair weight must be exactly 1', by, bx, h, s, v)
            weights_sum += 1
            values.append(v)
        else:
            assert sat_w == 0.0, ('other blocks must have weight exactly 0', by, bx, s)
assert weights_sum == 4 and len(set(values)) == 1, 'the mean value must be the value of the hair colour'

# new colour: value = min(1, 0.5 * pow(v / v, 0.85)) = 0.5, hue 120, saturation 1 -> (0, 0.5, 0), weight 1
assert f32(values[0] / values[0]) == 1 and f32(np.power(f32(1), f32(0.85))) == 1
new_rgb = (0.0, 0.5, 0.0)
out = []
for by in range(H // 4):
    for bx in range(W // 4):
        if (by, bx) in hair_blocks:
            px = [int(f32(f32(c) * f32(255) + f32(0.5))) for c in new_rgb]
            assert px == [0, 128, 0]
        else:
            px = [int(f32(rgb[by, bx, c] * f32(255) + f32(0.5))) for c in range(3)]
            assert px == [int(round(float(image[by * 4][bx * 4][c]))) for c in range(3)], 'untinted blocks keep their integer colour'
        out.extend(px + [255])
tint_hash = fnv(out)

print('compose hash 0x%08x, flatten and tint hash 0x%08x' % (compose_hash, tint_hash))
for name, value in (('GOLDEN_COMPOSE_HASH', compose_hash), ('GOLDEN_TINT_HASH', tint_hash)):
    match = re.search(name + r' = 0x([0-9a-fA-F]+)u', test_source)
    assert match, name + ' is missing in the test'
    assert int(match[1], 16) == value, '%s in the test is 0x%s, the reference says 0x%08x' % (name, match[1], value)
print('golden values in the test match the reference')
