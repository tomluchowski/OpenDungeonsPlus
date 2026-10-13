#!/usr/bin/env python3
"""Generates materials/textures/DungeonHeartPedestal.png, the colour texture of the dungeon heart's pedestal.

The pedestal keeps the texture layout (UV islands) of the old temple texture DungeonTemple.png, whose
stone grain is high contrast and carries baked-in white highlight blobs. Lit and blended with an environment
map, those blobs show up as white speckles. This script takes only the grain of the old texture, removes the
highlights, softens the contrast and paints it onto a dark obsidian-grey base, so that the pedestal reads as
dark weathered stone. The result is opaque and deterministic.

Usage: python generate_pedestal_texture.py [input DungeonTemple.png] [output DungeonHeartPedestal.png]
Needs numpy and Pillow.
"""

import os
import sys

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
TEXTURES = os.path.join(HERE, '..', '..', 'materials', 'textures')

BASE = np.array([24.0, 24.0, 28.0])   # dark cool grey
GRAIN = 0.55                          # how much of the old grain is kept (per luminance level)
CLIP = 60.0                           # luminance above this is a baked highlight and is cut


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(TEXTURES, 'DungeonTemple.png')
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(TEXTURES, 'DungeonHeartPedestal.png')
    img = Image.open(src).convert('RGB')
    lum = np.asarray(img.convert('L')).astype(np.float64)
    lum = np.minimum(lum, CLIP)
    smooth = Image.fromarray(lum.astype(np.uint8), 'L').filter(ImageFilter.GaussianBlur(1.2))
    lum = np.asarray(smooth).astype(np.float64)
    out = BASE[None, None, :] + GRAIN * lum[..., None] * np.array([0.95, 0.95, 1.0])[None, None, :]
    Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), 'RGB').save(dst)


if __name__ == '__main__':
    main()
