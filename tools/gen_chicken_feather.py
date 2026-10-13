#!/usr/bin/env python3
"""Draws the feather of the chicken feather particles (grey, so the particle colour gives the plumage).

    python tools/gen_chicken_feather.py

Output: materials/textures/ChickenFeather.png (128x128 RGBA, light grey feather with a stem and slanted barbs on
transparent ground; the tip points up). The particle systems ChickenFeathers (hen) and ChickenFeathersRooster tint it
with the colours of the plumage. Everything is drawn from curves with a fixed random seed.

Needs Pillow and numpy.
"""

import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "materials", "textures", "ChickenFeather.png")
SIZE = 128
SUPER = 4


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def draw():
    rng = np.random.RandomState(7)
    size = SIZE * SUPER
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float64)
    u = (xs + 0.5) / size
    v = (ys + 0.5) / size

    # The stem bends a little to one side, the feather is a slim leaf that is widest at 40 % of its length
    stem = 0.5 + 0.09 * np.sin(v * np.pi) - 0.03
    distance = np.abs(u - stem)
    side = np.sign(u - stem)

    along = np.clip((v - 0.04) / 0.84, 0.0, 1.0)
    width = 0.25 * np.power(np.sin(np.pi * np.power(along, 0.72)), 0.85)
    # Ragged edge: the barbs end at slightly different places
    rows = rng.rand(size // 4 + 2)
    ragged_rows = np.interp(np.arange(size) / 4.0, np.arange(len(rows)), rows)
    ragged = 1.0 + 0.2 * (ragged_rows[(v * (size - 1)).astype(int)] - 0.5)
    width = width * ragged
    in_vane = (v > 0.05) & (v < 0.9)

    edge = 2.0 / size
    vane_alpha = (1.0 - smoothstep(width - edge, width + edge, distance)) * in_vane
    quill_alpha = (1.0 - smoothstep(0.012, 0.012 + edge, distance)) * ((v > 0.05) & (v < 0.98))
    alpha = np.maximum(vane_alpha, quill_alpha)

    # Slanted barbs: fine stripes running from the stem to the edge and towards the tip
    phase = v * 90.0 - side * distance * 40.0
    barbs = 0.5 + 0.5 * np.sin(phase * np.pi)
    shade = 0.86 + 0.09 * barbs
    # The edge is a little darker, the middle along the stem lighter
    shade = shade * (0.88 + 0.12 * (1.0 - np.clip(distance / np.maximum(width, 0.01), 0.0, 1.0)))
    quill = (1.0 - smoothstep(0.012, 0.03, distance)) * (v > 0.05)
    shade = np.where(quill > 0.5, 1.0, shade)
    shade = np.clip(shade, 0.0, 1.0)

    rgba = np.zeros((size, size, 4), dtype=np.float64)
    rgba[..., 0] = shade
    rgba[..., 1] = shade
    rgba[..., 2] = shade
    rgba[..., 3] = alpha
    # Premultiply for the downsampling so the edge does not get a dark halo, then undo it
    rgba[..., :3] *= rgba[..., 3:4]
    image = Image.fromarray((rgba * 255.0 + 0.5).astype(np.uint8), "RGBA").resize((SIZE, SIZE), Image.LANCZOS)
    small = np.asarray(image).astype(np.float64) / 255.0
    safe = np.maximum(small[..., 3:4], 1e-4)
    small[..., :3] = np.clip(small[..., :3] / safe, 0.0, 1.0)
    return Image.fromarray((small * 255.0 + 0.5).astype(np.uint8), "RGBA")


if __name__ == "__main__":
    draw().save(OUT)
    print("written", OUT)
