#!/usr/bin/env python3
"""Writes the small iron texture of the shackle model (models/Shackle.mesh).

A 32x32 blue-grey gradient with a little noise, made from nothing but random numbers.
Run it from the repository root to write the image again:

    python tools/gen_shackle_texture.py
"""
import os
import random

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    random.seed(11)
    image = Image.new("RGB", (32, 32))
    for y in range(32):
        for x in range(32):
            noise = random.randint(-9, 9)
            grey = int(86 + 14 * (y / 31.0) + noise)
            image.putpixel((x, y), (grey, grey + 3, grey + 8))
    image.save(os.path.join(ROOT, "materials", "textures", "ShackleIron.png"))


if __name__ == "__main__":
    main()
