#!/usr/bin/env python3
"""Draws the ring textures of the spell effects (config/roomAmbienceSpellsFx.cfg).

Everything is drawn from gradients, lines and noise by this script with a fixed random seed, nothing is
taken from another work. Run it from the repository root to write the images again:

    python tools/gen_spell_marks.py

Output (all 128x128 RGBA, in materials/textures):
    RoomAmbRing.png         thin glowing ring (area markers, white, added to the picture)
    RoomAmbRingRunes.png    double ring with tick marks and small marks between (possession, defector)

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


TEXTURES = (
    ("RoomAmbRing.png", ring_plain),
    ("RoomAmbRingRunes.png", ring_runes),
)


def main():
    """Writes the ring textures into materials/textures."""
    for name, function in TEXTURES:
        function().save(os.path.join(OUT, name))
        print("wrote", name)


if __name__ == "__main__":
    main()
