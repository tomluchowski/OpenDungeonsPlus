#!/usr/bin/env python3
"""Draws the particle sprites of the extra room effects (config/roomAmbienceDeferred.cfg).

Everything is drawn from simple shapes and soft gradients by this script, nothing is taken from
another work. Run it from the repository root to write the images again:

    python tools/gen_room_effects_art.py

Output (all 64x64 RGBA, in materials/textures): RoomAmbFlame, RoomAmbBracket, RoomAmbGear, RoomAmbRat,
RoomAmbRatL, RoomAmbRumple and RoomAmbSoftRing, used by particles/RoomAmbienceDeferred.particle.

Needs Pillow. The helpers are the ones of tools/gen_room_ambience_art.py.
"""

import math
import os
import random
import sys

from PIL import ImageDraw, ImageFilter, Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_room_ambience_art import CANVAS, finish, new_canvas, radial  # noqa: E402


def sprite_flame(path):
    """A teardrop flame with a pointed tip: orange outside, yellow inside, a hot white core at the base."""
    image, draw = new_canvas()
    c = CANVAS / 2
    layers = ((1.0, (255, 110, 15, 255)), (0.78, (255, 175, 45, 255)), (0.52, (255, 235, 150, 255)))
    for scale, color in layers:
        top = c - 118 * scale
        base = c + 96 * scale
        half = 64 * scale
        left = []
        right = []
        for i in range(61):
            t = i / 60.0
            y = top + (base - top) * t
            # pointed tip, widest at about two thirds, round base
            width = half * 1.9 * (t ** 0.85) * math.sqrt(1.0 - t)
            left.append((c - width, y))
            right.append((c + width, y))
        draw.polygon(left + right[::-1], fill=color)
    image = image.filter(ImageFilter.GaussianBlur(6))
    finish(image, path)


def sprite_bracket(path):
    """An iron wall bracket with an unlit torch head, seen from the front (alpha blended)."""
    image, draw = new_canvas()
    c = CANVAS // 2
    # wooden stick
    draw.polygon([(c - 15, 90), (c + 15, 90), (c + 11, 238), (c - 11, 238)], fill=(92, 62, 36, 255))
    draw.line((c - 6, 96, c - 4, 230), fill=(132, 94, 58, 255), width=5)
    # torch head wrapped in rags
    draw.ellipse((c - 30, 40, c + 30, 110), fill=(60, 44, 32, 255))
    draw.ellipse((c - 22, 48, c + 6, 84), fill=(88, 66, 48, 255))
    # iron band and bracket
    draw.rectangle((c - 22, 150, c + 22, 172), fill=(58, 60, 66, 255))
    draw.line((c - 22, 152, c + 22, 152), fill=(130, 134, 142, 255), width=3)
    draw.rectangle((c - 40, 196, c + 40, 214), fill=(58, 60, 66, 255))
    draw.line((c - 40, 198, c + 40, 198), fill=(130, 134, 142, 255), width=3)
    image = image.filter(ImageFilter.GaussianBlur(1.2))
    finish(image, path)


def sprite_gear(path):
    """A cog with eight teeth and a hub (alpha blended)."""
    image, draw = new_canvas()
    c = CANVAS / 2
    teeth = 8
    outer = 118
    inner = 92
    points = []
    for i in range(teeth):
        base = i * 2 * math.pi / teeth
        step = 2 * math.pi / teeth
        for angle, radius in ((base - 0.22 * step, inner), (base - 0.14 * step, outer), (base + 0.14 * step, outer),
                              (base + 0.22 * step, inner)):
            points.append((c + radius * math.cos(angle), c + radius * math.sin(angle)))
    draw.polygon(points, fill=(86, 74, 62, 255), outline=(40, 34, 30, 255))
    draw.ellipse((c - 76, c - 76, c + 76, c + 76), outline=(130, 112, 92, 255), width=8)
    for i in range(4):
        angle = i * math.pi / 2 + math.pi / 4
        draw.line((c, c, c + 74 * math.cos(angle), c + 74 * math.sin(angle)), fill=(60, 52, 44, 255), width=14)
    draw.ellipse((c - 28, c - 28, c + 28, c + 28), fill=(54, 46, 40, 255), outline=(150, 130, 108, 255), width=5)
    draw.ellipse((c - 10, c - 10, c + 10, c + 10), fill=(20, 18, 16, 255))
    image = image.filter(ImageFilter.GaussianBlur(1.0))
    finish(image, path)


def draw_rat(mirror):
    """A small rat seen from above, head to the right (or to the left when mirrored)."""
    image, draw = new_canvas()
    cy = CANVAS // 2
    body = (70, 56, 48, 255)
    dark = (44, 34, 30, 255)
    # tail
    tail = []
    for i in range(30):
        t = i / 29.0
        tail.append((70 - 54 * t, cy + 16 * math.sin(t * 5.0) * (0.4 + t)))
    for i in range(len(tail) - 1):
        draw.line((tail[i], tail[i + 1]), fill=(150, 112, 104, 255), width=max(3, 9 - int(i / 4)))
    # body, head and ears
    draw.ellipse((60, cy - 40, 170, cy + 40), fill=body)
    draw.polygon([(150, cy - 28), (232, cy), (150, cy + 28)], fill=body)
    draw.ellipse((140, cy - 40, 184, cy - 8), fill=dark)
    draw.ellipse((140, cy + 8, 184, cy + 40), fill=dark)
    # feet
    for x, side in ((96, -1), (96, 1), (146, -1), (146, 1)):
        draw.ellipse((x - 12, cy + side * 44 - 8, x + 12, cy + side * 44 + 8), fill=dark)
    draw.ellipse((222, cy - 7, 238, cy + 7), fill=(210, 140, 140, 255))
    draw.ellipse((196, cy - 18, 208, cy - 6), fill=(230, 220, 200, 255))
    draw.line((84, cy - 10, 160, cy - 12), fill=(98, 80, 66, 255), width=6)
    image = image.filter(ImageFilter.GaussianBlur(1.2))
    if mirror:
        image = image.transpose(Image.FLIP_LEFT_RIGHT)
    return image


def sprite_rat(path):
    finish(draw_rat(False), path)


def sprite_rat_left(path):
    finish(draw_rat(True), path)


def sprite_rumple(path):
    """Folds of a rumpled blanket seen from above: soft ridges and creases (alpha blended)."""
    rng = random.Random(11)
    image, draw = new_canvas()
    radial(image, (150, 126, 100, 150), 120, 0.8)
    for _ in range(9):
        x0 = rng.uniform(40, 216)
        y0 = rng.uniform(40, 216)
        angle = rng.uniform(0, math.pi)
        length = rng.uniform(60, 120)
        points = []
        for i in range(12):
            t = i / 11.0 - 0.5
            bend = math.sin(t * 6.0 + x0) * 10
            points.append((x0 + math.cos(angle) * length * t - math.sin(angle) * bend,
                           y0 + math.sin(angle) * length * t + math.cos(angle) * bend))
        for i in range(len(points) - 1):
            draw.line((points[i], points[i + 1]), fill=(66, 50, 40, 190), width=7)
        shifted = [(px + 6, py - 6) for px, py in points]
        for i in range(len(shifted) - 1):
            draw.line((shifted[i], shifted[i + 1]), fill=(214, 190, 160, 160), width=4)
    image = image.filter(ImageFilter.GaussianBlur(3.5))
    finish(image, path)


def sprite_ring(path):
    """A thin soft ring, white so the particle colour tints it (drawn for additive blending)."""
    image, draw = new_canvas()
    c = CANVAS / 2
    for radius, width, alpha in ((112, 16, 90), (108, 8, 255)):
        draw.ellipse((c - radius, c - radius, c + radius, c + radius), outline=(255, 255, 255, alpha), width=width)
    image = image.filter(ImageFilter.GaussianBlur(4))
    finish(image, path)


SPRITES = {
    "RoomAmbFlame": sprite_flame,
    "RoomAmbBracket": sprite_bracket,
    "RoomAmbGear": sprite_gear,
    "RoomAmbRat": sprite_rat,
    "RoomAmbRatL": sprite_rat_left,
    "RoomAmbRumple": sprite_rumple,
    "RoomAmbSoftRing": sprite_ring,
}


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    folder = os.path.join(root, "materials", "textures")
    for name, function in SPRITES.items():
        function(os.path.join(folder, name + ".png"))
        print("wrote", name)


if __name__ == "__main__":
    main()
