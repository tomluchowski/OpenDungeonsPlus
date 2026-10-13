#!/usr/bin/env python3
"""Writes the small texture shared by the room decorations (weapon rack, wall tools, arena banner, grave mound).

A 32x32 image made from random numbers only, in four fields: dark iron (upper left), brown wood grain
(upper right), red cloth (lower left) and dark soil (lower right). Run it from the repository root:

    python tools/gen_room_decor_texture.py
"""
import os
import random

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    random.seed(41)
    image = Image.new("RGB", (32, 32))
    for y in range(32):
        for x in range(32):
            noise = random.randint(-8, 8)
            if y < 16 and x < 16:
                grey = int(92 + 18 * (y / 15.0) + noise)
                image.putpixel((x, y), (grey, grey + 2, grey + 6))
            elif y < 16:
                streak = 10 if (x // 3) % 2 == 0 else 0
                base = 118 + streak + noise
                image.putpixel((x, y), (base, int(base * 0.68), int(base * 0.38)))
            elif x < 16:
                weave = 8 if ((x + y) // 2) % 2 == 0 else 0
                base = 150 + weave + noise
                image.putpixel((x, y), (base, int(base * 0.22), int(base * 0.2)))
            else:
                clod = 10 if ((x * 7 + y * 13) % 5) == 0 else 0
                base = 64 + clod + noise
                image.putpixel((x, y), (base, int(base * 0.75), int(base * 0.55)))
    image.save(os.path.join(ROOT, "materials", "textures", "RoomDecorSurface.png"))


if __name__ == "__main__":
    main()
