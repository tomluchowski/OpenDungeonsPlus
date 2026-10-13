#!/usr/bin/env python3
"""Recolours the hen texture into the chick and rooster textures.

Usage: gen_chicken_variants.py materials/textures
Reads Chicken.png (unchanged) and writes ChickenChick.png (soft yellow down) and
ChickenRooster.png (copper brown body, black tail, deep red head).
The shading of the hen texture is kept, only the colours change.
"""
import os
import sys
import numpy as np
from PIL import Image

folder = sys.argv[1]
src = Image.open(os.path.join(folder, "Chicken.png"))
rgba = np.asarray(src.convert("RGBA")).astype(float)
rgb = rgba[..., :3]
r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
lum = 0.299 * r + 0.587 * g + 0.114 * b
red = (r > g * 1.5) & (r > b * 1.5)
white = np.clip((lum - 90.0) / 120.0, 0.0, 1.0)


def make(white_color, dark_color, red_color, name):
    shade = (lum / 255.0)[..., None]
    light = np.array(white_color, dtype=float) * np.clip(shade * 1.1, 0.0, 1.2)
    dark = np.array(dark_color, dtype=float) * (0.6 + shade)
    mixed = light * white[..., None] + dark * (1.0 - white[..., None])
    redpart = np.array(red_color, dtype=float) * np.clip(shade * 1.6, 0.3, 1.2)
    out = np.where(red[..., None], redpart, mixed)
    result = np.concatenate([np.clip(out, 0, 255), rgba[..., 3:]], axis=-1).astype(np.uint8)
    Image.fromarray(result, "RGBA").save(os.path.join(folder, name))


make((255, 226, 112), (96, 70, 30), (255, 150, 40), "ChickenChick.png")
make((176, 92, 44), (28, 22, 20), (190, 24, 24), "ChickenRooster.png")
