#!/usr/bin/env python3
"""Generates materials/textures/TreasuryGoldPile.png, the coin texture of the treasury gold layer.

Original work of the project, licence CC0. Everything is procedural and seeded; running the script again gives
byte-identical files. The texture is periodic (every coin is drawn at all nine tile offsets), so it repeats
without a seam. Needs numpy and Pillow.

Usage: python generate_gold_pile.py [output file]
"""

import os
import sys

import numpy as np
from PIL import Image, ImageDraw

SIZE = 256
SUPER = 2
COINS = 320


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', '..', 'materials', 'textures', 'TreasuryGoldPile.png')
    rng = np.random.RandomState(20261004)
    big = SIZE * SUPER
    image = Image.new('RGB', (big, big), (120, 78, 18))
    draw = ImageDraw.Draw(image)

    # Coins lie in a heap: draw them in random order, each one a little lighter or darker
    for _ in range(COINS):
        cx = rng.uniform(0, big)
        cy = rng.uniform(0, big)
        radius = rng.uniform(0.032, 0.05) * big
        squash = rng.uniform(0.55, 1.0)
        tilt = rng.uniform(0.0, 1.0)
        shade = rng.uniform(0.72, 1.12)
        rim = tuple(int(min(255, c * shade)) for c in (232, 178, 58))
        face = tuple(int(min(255, c * shade)) for c in (196, 142, 38))
        shine = tuple(int(min(255, c * shade)) for c in (255, 226, 130))
        edge = tuple(int(c * shade) for c in (120, 78, 16))
        for ox in (-big, 0, big):
            for oy in (-big, 0, big):
                x = cx + ox
                y = cy + oy
                rx = radius
                ry = radius * squash
                draw.ellipse((x - rx - 2, y - ry - 2, x + rx + 2, y + ry + 2), fill=edge)
                draw.ellipse((x - rx, y - ry, x + rx, y + ry), fill=rim)
                draw.ellipse((x - rx * 0.78, y - ry * 0.78, x + rx * 0.78, y + ry * 0.78), fill=face)
                # a glint on the rim, toward the light
                gx = x - rx * (0.35 + 0.2 * tilt)
                gy = y - ry * 0.45
                draw.ellipse((gx - rx * 0.28, gy - ry * 0.18, gx + rx * 0.28, gy + ry * 0.18), fill=shine)

    image = image.resize((SIZE, SIZE), Image.LANCZOS)
    pixels = np.asarray(image).astype(np.float32)
    # Fine grain so the surface does not look flat when close up
    grain = rng.normal(0.0, 5.0, (SIZE, SIZE, 1))
    pixels = np.clip(pixels + grain, 0, 255).astype(np.uint8)
    Image.fromarray(pixels, 'RGB').save(out)
    print('wrote', os.path.normpath(out))


if __name__ == '__main__':
    main()
