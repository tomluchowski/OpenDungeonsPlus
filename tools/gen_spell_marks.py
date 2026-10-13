#!/usr/bin/env python3
"""Draws the ring and ground mark textures of the spell effects (config/roomAmbienceSpells.cfg).

Everything is drawn from gradients, lines and noise by this script with a fixed random seed, nothing is
taken from another work. Run it from the repository root to write the images again:

    python tools/gen_spell_marks.py

Output (all 128x128 RGBA, in materials/textures):
    RoomAmbRing.png         thin glowing ring (area markers, white, added to the picture)
    RoomAmbRingRunes.png    double ring with tick marks and small marks between (summoning, possession)
    RoomAmbRingDust.png     wide uneven ring of dust (tremor wave)
    RoomAmbMarkScorch.png   dark scorched patch with a lighter singed edge (explosion, inferno)
    RoomAmbMarkCracks.png   cracks running out from the middle (tremor)
    RoomAmbMarkBolt.png     branching burn lines (lightning)

Needs Pillow.
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

SIZE = 128
SUPER = 4
CANVAS = SIZE * SUPER
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "materials", "textures")


def smooth_edge(r, start=0.9, end=1.0):
    """1 inside, falling to 0 at the border, so nothing is cut off at the texture edge."""
    if r <= start:
        return 1.0
    if r >= end:
        return 0.0
    t = (r - start) / (end - start)
    return 1.0 - t * t * (3.0 - 2.0 * t)


def per_pixel(function):
    """Builds an image from function(x, y, r, angle) -> (red, green, blue, alpha 0..1); x and y run -1..1."""
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    pixels = image.load()
    for py in range(SIZE):
        for px in range(SIZE):
            x = (px + 0.5) / SIZE * 2.0 - 1.0
            y = (py + 0.5) / SIZE * 2.0 - 1.0
            r = math.hypot(x, y)
            red, green, blue, alpha = function(x, y, r, math.atan2(y, x))
            alpha = max(0.0, min(1.0, alpha)) * smooth_edge(r)
            pixels[px, py] = (red, green, blue, int(round(alpha * 255)))
    return image


def band(r, center, width):
    return math.exp(-((r - center) / width) ** 2)


def ring_plain():
    def function(x, y, r, angle):
        alpha = band(r, 0.8, 0.055) + 0.35 * band(r, 0.8, 0.14) + 0.08 * band(r, 0.8, 0.3)
        return 255, 255, 255, alpha
    return per_pixel(function)


def ring_runes():
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    c = CANVAS / 2.0

    def polar(radius, angle):
        return c + math.cos(angle) * radius * c, c + math.sin(angle) * radius * c

    for radius, width in ((0.92, 9), (0.7, 7)):
        box = (c - radius * c, c - radius * c, c + radius * c, c + radius * c)
        draw.ellipse(box, outline=(255, 255, 255, 255), width=width)

    rng = random.Random(21)
    for i in range(24):
        angle = i * math.tau / 24.0
        inner = 0.74
        outer = 0.74 + rng.choice((0.08, 0.12, 0.16))
        draw.line([polar(inner, angle), polar(outer, angle)], fill=(255, 255, 255, 255), width=8)
    for i in range(8):
        angle = (i + 0.5) * math.tau / 8.0
        tip = polar(0.97, angle)
        left = polar(0.88, angle - 0.07)
        right = polar(0.88, angle + 0.07)
        draw.polygon([tip, left, right], fill=(255, 255, 255, 255))
    image = image.filter(ImageFilter.GaussianBlur(2.2))
    # A faint glow along the rings
    glow = per_pixel(lambda x, y, r, a: (255, 255, 255, 0.12 * band(r, 0.81, 0.17))).resize(
        (CANVAS, CANVAS), Image.BILINEAR)
    image = Image.alpha_composite(glow, image)
    return image.resize((SIZE, SIZE), Image.LANCZOS)


def noise_field(seed, cells=9):
    """Smooth random values 0..1 on a grid of cells x cells, read with bilinear interpolation."""
    rng = random.Random(seed)
    grid = [[rng.random() for _ in range(cells + 1)] for _ in range(cells + 1)]

    def sample(x, y):
        u = (x * 0.5 + 0.5) * cells
        v = (y * 0.5 + 0.5) * cells
        iu = min(cells - 1, max(0, int(u)))
        iv = min(cells - 1, max(0, int(v)))
        fu = u - iu
        fv = v - iv
        top = grid[iv][iu] * (1 - fu) + grid[iv][iu + 1] * fu
        bottom = grid[iv + 1][iu] * (1 - fu) + grid[iv + 1][iu + 1] * fu
        return top * (1 - fv) + bottom * fv
    return sample


def ring_dust():
    coarse = noise_field(5, 7)
    fine = noise_field(9, 17)

    def function(x, y, r, angle):
        grain = 0.55 * coarse(x, y) + 0.45 * fine(x, y)
        wobble = (coarse(y, x) - 0.5) * 0.16
        alpha = band(r, 0.74 + wobble, 0.15) * (0.35 + 0.9 * grain)
        return 255, 255, 255, alpha
    return per_pixel(function)


def mark_scorch():
    rng = random.Random(33)
    harmonics = [(k, rng.uniform(0.04, 0.11) / (k * 0.6), rng.uniform(0, math.tau)) for k in (2, 3, 4, 5, 7, 9)]
    flecks = noise_field(14, 13)

    def edge(angle):
        return 0.66 + sum(a * math.sin(k * angle + p) for k, a, p in harmonics)

    def function(x, y, r, angle):
        limit = edge(angle)
        d = r / limit
        if d >= 1.25:
            return 0, 0, 0, 0
        core = 1.0 - max(0.0, min(1.0, (d - 0.55) / 0.5))
        alpha = (1.0 - max(0.0, min(1.0, (d - 0.7) / 0.55))) * (0.78 + 0.22 * flecks(x, y))
        # Dark in the middle, brown and thinner at the rim
        mix = max(0.0, min(1.0, d))
        red = int(24 + 52 * mix * mix)
        green = int(20 + 36 * mix * mix)
        blue = int(18 + 22 * mix * mix)
        return red, green, blue, alpha * (0.55 + 0.45 * core) if d < 1.0 else alpha * 0.8
    return per_pixel(function)


def jagged(draw, rng, c, angle, length, width, color, branches, depth=0):
    """A crack: a jagged line that gets thinner, with side branches."""
    steps = 9
    x, y = c
    heading = angle
    step_length = length / steps
    for i in range(steps):
        heading += rng.uniform(-0.38, 0.38)
        nx = x + math.cos(heading) * step_length
        ny = y + math.sin(heading) * step_length
        w = max(1, int(width * (1.0 - i / float(steps)) ** 0.8))
        draw.line([(x, y), (nx, ny)], fill=color, width=w)
        if branches > 0 and i in (3, 6) and rng.random() < 0.8:
            side = heading + rng.choice((-1, 1)) * rng.uniform(0.5, 0.9)
            jagged(draw, rng, (nx, ny), side, length * 0.38, max(2, width * 0.55), color, branches - 1, depth + 1)
        x, y = nx, ny


def with_edge_fade(image):
    """Fades the supersampled drawing towards the border of the square."""
    pixels = image.load()
    for py in range(CANVAS):
        for px in range(CANVAS):
            x = (px + 0.5) / CANVAS * 2.0 - 1.0
            y = (py + 0.5) / CANVAS * 2.0 - 1.0
            fade = smooth_edge(math.hypot(x, y), 0.8, 0.98)
            red, green, blue, alpha = pixels[px, py]
            if alpha and fade < 1.0:
                pixels[px, py] = (red, green, blue, int(alpha * fade))
    return image


def mark_cracks():
    rng = random.Random(8)
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    c = (CANVAS / 2.0, CANVAS / 2.0)
    draw.ellipse((c[0] - 34, c[1] - 34, c[0] + 34, c[1] + 34), fill=(18, 14, 12, 190))
    count = 9
    for i in range(count):
        angle = i * math.tau / count + rng.uniform(-0.2, 0.2)
        jagged(draw, rng, c, angle, rng.uniform(0.62, 0.9) * CANVAS / 2.0, 13, (16, 12, 10, 235), 2)
    image = image.filter(ImageFilter.GaussianBlur(1.6))
    image = with_edge_fade(image)
    return image.resize((SIZE, SIZE), Image.LANCZOS)


def mark_bolt():
    rng = random.Random(61)
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    glow = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    glow_draw = ImageDraw.Draw(glow)
    c = (CANVAS / 2.0, CANVAS / 2.0)
    glow_draw.ellipse((c[0] - 70, c[1] - 70, c[0] + 70, c[1] + 70), fill=(20, 22, 30, 120))
    glow = glow.filter(ImageFilter.GaussianBlur(26))
    count = 6
    for i in range(count):
        angle = i * math.tau / count + rng.uniform(-0.3, 0.3)
        jagged(draw, rng, c, angle, rng.uniform(0.55, 0.85) * CANVAS / 2.0, 8, (22, 24, 34, 240), 3)
    draw.ellipse((c[0] - 20, c[1] - 20, c[0] + 20, c[1] + 20), fill=(14, 15, 22, 235))
    image = image.filter(ImageFilter.GaussianBlur(1.2))
    image = Image.alpha_composite(glow, image)
    image = with_edge_fade(image)
    return image.resize((SIZE, SIZE), Image.LANCZOS)


TEXTURES = (
    ("RoomAmbRing.png", ring_plain),
    ("RoomAmbRingRunes.png", ring_runes),
    ("RoomAmbRingDust.png", ring_dust),
    ("RoomAmbMarkScorch.png", mark_scorch),
    ("RoomAmbMarkCracks.png", mark_cracks),
    ("RoomAmbMarkBolt.png", mark_bolt),
)


def main():
    for name, function in TEXTURES:
        function().save(os.path.join(OUT, name))
        print("wrote", name)


if __name__ == "__main__":
    main()
