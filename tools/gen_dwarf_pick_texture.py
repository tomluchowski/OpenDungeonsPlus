#!/usr/bin/env python3
"""Writes the small texture of the pickaxe model (models/DwarfPick.mesh).

A 32x32 image made from random numbers only: the upper half is dark iron (the pick head),
the lower half is brown wood grain (the handle). Run it from the repository root:

    python tools/gen_dwarf_pick_texture.py
"""
import os
import random

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    random.seed(23)
    image = Image.new("RGB", (32, 32))
    for y in range(32):
        for x in range(32):
            noise = random.randint(-8, 8)
            if y < 16:
                grey = int(92 + 18 * (y / 15.0) + noise)
                image.putpixel((x, y), (grey, grey + 2, grey + 6))
            else:
                streak = 10 if (x // 3) % 2 == 0 else 0
                base = 118 + streak + noise
                image.putpixel((x, y), (base, int(base * 0.68), int(base * 0.38)))
    image.save(os.path.join(ROOT, "materials", "textures", "DwarfPickSurface.png"))


if __name__ == "__main__":
    main()
