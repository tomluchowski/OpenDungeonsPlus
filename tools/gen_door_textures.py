#!/usr/bin/env python3
"""Draws the textures of the door types (iron-bound, steel, barricade, secret, rune).

    python tools/gen_door_textures.py

Everything is drawn from gradients, lines and noise with a fixed random seed; the iron-bound door wood is the
wooden door texture of this project made darker. Output (256x256 RGB, in materials/textures):
    DoorIronboundWood.png   darkened planks of the wooden door
    DoorMetal.png           dark forged iron with a few scratches
    DoorSteel.png           brushed steel with a faint blue tint
    DoorBarricade.png       rough, grey, weathered planks
    DoorSecret.png          cut stone blocks
    DoorRune.png            dark carved stone
    DoorRuneGlow.png        glowing rune glyphs on black (added to the picture)

Needs Pillow and numpy.
"""

import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "materials", "textures")
SIZE = 256


def noise(seed, scale, amount):
    rng = np.random.RandomState(seed)
    small = rng.rand(SIZE // scale, SIZE // scale)
    image = Image.fromarray((small * 255).astype(np.uint8)).resize((SIZE, SIZE), Image.BICUBIC)
    return (np.asarray(image, dtype=np.float32) / 255.0 - 0.5) * amount


def save(name, array):
    Image.fromarray(np.clip(array, 0, 255).astype(np.uint8)).save(os.path.join(OUT, name))
    print("wrote", name)


def ironbound_wood():
    base = Image.open(os.path.join(OUT, "WoodenDoor.png")).convert("RGB").resize((SIZE, SIZE))
    array = np.asarray(base, dtype=np.float32) * np.array([0.55, 0.5, 0.45])
    save("DoorIronboundWood.png", array)


def metal():
    base = np.array([58.0, 58.0, 64.0])
    array = np.zeros((SIZE, SIZE, 3), np.float32) + base
    array += noise(1, 4, 40)[..., None] + noise(2, 32, 25)[..., None]
    image = Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))
    draw = ImageDraw.Draw(image)
    rng = random.Random(3)
    for _ in range(40):
        x, y = rng.randint(0, SIZE), rng.randint(0, SIZE)
        length = rng.randint(10, 40)
        shade = rng.randint(85, 120)
        draw.line((x, y, x + length, y + rng.randint(-3, 3)), fill=(shade, shade, shade + 6), width=1)
    save("DoorMetal.png", np.asarray(image, dtype=np.float32))


def steel():
    rng = np.random.RandomState(4)
    array = np.zeros((SIZE, SIZE, 3), np.float32) + np.array([138.0, 146.0, 158.0])
    array += rng.rand(1, SIZE, 1) * 36.0 - 18.0
    array += noise(5, 8, 22)[..., None]
    image = Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))
    draw = ImageDraw.Draw(image)
    for y in (0, SIZE // 2):
        draw.line((0, y, SIZE, y), fill=(70, 76, 86), width=3)
    save("DoorSteel.png", np.asarray(image, dtype=np.float32))


def barricade():
    array = np.zeros((SIZE, SIZE, 3), np.float32) + np.array([112.0, 100.0, 88.0])
    array += noise(6, 3, 38)[..., None] + noise(7, 24, 30)[..., None]
    image = Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))
    draw = ImageDraw.Draw(image)
    rng = random.Random(8)
    for y in range(0, SIZE, 64):
        draw.line((0, y, SIZE, y), fill=(40, 34, 28), width=3)
    for _ in range(30):
        x, y = rng.randint(0, SIZE), rng.randint(0, SIZE)
        draw.line((x, y, x + rng.randint(20, 70), y + rng.randint(-2, 2)), fill=(70, 62, 52), width=1)
    save("DoorBarricade.png", np.asarray(image, dtype=np.float32))


def secret():
    array = np.zeros((SIZE, SIZE, 3), np.float32) + np.array([120.0, 116.0, 110.0])
    array += noise(9, 4, 36)[..., None] + noise(10, 32, 26)[..., None]
    image = Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))
    draw = ImageDraw.Draw(image)
    for y in range(0, SIZE, 64):
        draw.line((0, y, SIZE, y), fill=(60, 58, 54), width=3)
        offset = 0 if (y // 64) % 2 == 0 else 64
        for x in range(offset, SIZE, 128):
            draw.line((x, y, x, y + 64), fill=(60, 58, 54), width=3)
    save("DoorSecret.png", np.asarray(image, dtype=np.float32))


def rune_base():
    array = np.zeros((SIZE, SIZE, 3), np.float32) + np.array([48.0, 46.0, 62.0])
    array += noise(11, 4, 30)[..., None] + noise(12, 32, 24)[..., None]
    save("DoorRune.png", array)


def rune_glow():
    image = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    rng = random.Random(13)
    colour = (120, 150, 255)
    for cell_y in range(2):
        for cell_x in range(2):
            cx, cy = cell_x * 128 + 64, cell_y * 128 + 64
            draw.ellipse((cx - 44, cy - 44, cx + 44, cy + 44), outline=colour, width=4)
            points = []
            for _ in range(5):
                angle = rng.uniform(0, 2 * math.pi)
                points.append((cx + 30 * math.cos(angle), cy + 30 * math.sin(angle)))
            for first, second in zip(points, points[1:] + points[:1]):
                draw.line((first, second), fill=colour, width=4)
            draw.line((cx, cy - 20, cx, cy + 20), fill=colour, width=4)
    image = image.filter(ImageFilter.GaussianBlur(1.2))
    image.save(os.path.join(OUT, "DoorRuneGlow.png"))
    print("wrote DoorRuneGlow.png")


def main():
    ironbound_wood()
    metal()
    steel()
    barricade()
    secret()
    rune_base()
    rune_glow()


if __name__ == "__main__":
    main()
