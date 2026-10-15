#!/usr/bin/env python3
"""Draws the texture of the lightning bolt mesh (models/SpellBolt.mesh, material SpellBolt).

    python tools/gen_spell_bolt.py

The strip is 64x128 pixels: across (u) a white hot core inside a blue glow, along (v) a short fade at both
ends. Colour is white and pale blue, the strength is in the alpha channel (the material blends it as added
light). Everything is computed by this script, nothing is taken from another work. Needs Pillow.
"""

import math
import os

from PIL import Image

WIDTH = 64
HEIGHT = 128
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "materials", "textures", "SpellBolt.png")


def smooth(edge0, edge1, x):
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def main():
    image = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    pixels = image.load()
    for py in range(HEIGHT):
        v = (py + 0.5) / HEIGHT
        ends = smooth(0.0, 0.06, v) * (1.0 - smooth(0.94, 1.0, v))
        for px in range(WIDTH):
            d = abs((px + 0.5) / WIDTH * 2.0 - 1.0)
            core = math.exp(-(d / 0.14) ** 2)
            glow = 0.55 * math.exp(-(d / 0.5) ** 2)
            edge = 1.0 - smooth(0.85, 1.0, d)
            strength = min(1.0, core + glow) * ends * edge
            # White in the core, pale blue in the glow
            mix = min(1.0, core * 1.2)
            red = int(round(255 * (0.55 + 0.45 * mix)))
            green = int(round(255 * (0.7 + 0.3 * mix)))
            pixels[px, py] = (red, green, 255, int(round(strength * 255)))
    image.save(OUT)
    print("wrote", os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
