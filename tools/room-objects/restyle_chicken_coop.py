#!/usr/bin/env python3
"""Restyle ChickenCoop.png: dark weathered slate roof, greyed rough wood.

Usage: restyle_chicken_coop.py ORIGINAL.png OUTPUT.png
ORIGINAL is the unmodified texture from the repository history (the script is
not idempotent on its own output).  Original work, CC0.
"""
import sys
import numpy as np
from PIL import Image

src = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(float)
r, g, b = src[..., 0], src[..., 1], src[..., 2]
roof = (r > g * 1.5) & (r > b * 1.5)
lum = (0.299 * r + 0.587 * g + 0.114 * b)
rng = np.random.RandomState(7)

# Roof: slate-grey shingles with a faint warm cast, keeping the shingle relief.
rl = np.clip(lum / lum[roof].mean(), 0.45, 1.6)
noise = rng.normal(0, 0.06, lum.shape)
slate = np.stack([104, 96, 90], axis=-1) * (rl + noise)[..., None]

# Wood (everything else that is not flat grey UV filler): darker, greyer, rougher.
wl = lum[..., None]
grey = np.repeat(wl, 3, axis=-1)
wood = (src * 0.62 + grey * 0.30) * np.array([1.0, 0.96, 0.92])
wood *= (1.0 + rng.normal(0, 0.07, lum.shape))[..., None]

out = np.where(roof[..., None], slate, wood)
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(sys.argv[2])
