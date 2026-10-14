#!/usr/bin/env python3
"""Generates materials/textures/GoldSack.png, the cloth texture of the gold sack carried by workers.

Original work of the project, licence CC0. Everything is procedural and seeded; running the script again gives
byte-identical files. The texture is a woven cloth with a few darker patches and repeats without a seam.
Needs numpy and Pillow.

Usage: python generate_gold_sack.py [output file]
"""

import os
import sys

import numpy as np
from PIL import Image

SIZE = 128
THREADS = 32


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', '..', 'materials', 'textures', 'GoldSack.png')
    rng = np.random.RandomState(20261005)
    cell = SIZE // THREADS
    yy, xx = np.mgrid[0:SIZE, 0:SIZE]
    # Over/under weave: a light thread on top in a checker pattern, a darker one below
    over = ((xx // cell) + (yy // cell)) % 2
    inside_x = (xx % cell) / float(cell)
    inside_y = (yy % cell) / float(cell)
    round_x = np.sin(np.pi * inside_x)
    round_y = np.sin(np.pi * inside_y)
    thread = np.where(over == 1, round_x, round_y)
    shade = 0.55 + 0.45 * thread
    # Slow patches, periodic so the texture tiles
    patches = np.zeros((SIZE, SIZE))
    for k in range(3):
        fx = rng.randint(1, 4)
        fy = rng.randint(1, 4)
        phase = rng.uniform(0, 2 * np.pi)
        patches += np.sin(2 * np.pi * (fx * xx + fy * yy) / SIZE + phase)
    shade *= 0.9 + 0.05 * patches
    base = np.array([168.0, 128.0, 78.0])
    pixels = shade[:, :, None] * base[None, None, :]
    pixels += rng.normal(0.0, 4.0, (SIZE, SIZE, 1))
    pixels = np.clip(pixels, 0, 255).astype(np.uint8)
    Image.fromarray(pixels, 'RGB').save(out)
    print('wrote', os.path.normpath(out))


if __name__ == '__main__':
    main()
