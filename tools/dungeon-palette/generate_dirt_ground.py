#!/usr/bin/env python3
"""Regrades Dirt.png, the ground of unclaimed dug-out tiles, to the dungeon palette (materials/textures/).

The painted structure (clods, pebbles, cracks) of the original texture is kept. Only the colour changes: the
luminance is scaled down around a new, cooler trodden-earth mean so that the claimed floor is about 1.9x brighter
than the unclaimed ground in the same view (style guide: at least 1.8x), with a slightly reduced contrast. The
normal map (DirtNormal.png, also used by GoldGround and GemGround) is not touched; GoldGround.png is not touched.

The source is read from the git history (commit ORIGINAL_COMMIT) so the script can be run repeatedly and gives
byte-identical output. Original work of the project, licence CC0. Needs numpy, Pillow and git.

Usage: python generate_dirt_ground.py <materials/textures folder> [output folder]
"""

import io
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ORIGINAL_COMMIT = os.environ.get('DIRT_COMMIT', '21320e9d6')

LUMA = np.array([0.299, 0.587, 0.114])
MEAN_RGB = np.array([41.0, 35.0, 29.0])   # cool grey-brown, hue about 30 degrees, sat about 0.29
CONTRAST = 0.75                            # keep the relief readable but less noisy


def main():
    texdir = sys.argv[1]
    outdir = sys.argv[2] if len(sys.argv) > 2 else texdir
    data = subprocess.check_output(['git', 'show', '%s:materials/textures/Dirt.png' % ORIGINAL_COMMIT],
                                   cwd=texdir)
    old = np.array(Image.open(io.BytesIO(data)).convert('RGB')).astype(float)
    lum = old @ LUMA
    rel = (lum - lum.mean()) / lum.mean() * CONTRAST          # relative luminance deviation
    new = MEAN_RGB[None, None, :] * (1.0 + rel[:, :, None])
    Image.fromarray(np.clip(new + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(outdir, 'Dirt.png'), optimize=True)


if __name__ == '__main__':
    main()
