#!/usr/bin/env python3
"""Draws the particle sprites used by the room ambience (config/roomAmbience.cfg).

Everything is drawn from simple shapes and soft gradients by this script, nothing is taken from
another work. Run it from the repository root to write the images again:

    python tools/gen_room_ambience_art.py

Output (all 64x64 RGBA, in materials/textures): RoomAmb<Name>.png, the sprites of the particle
systems in particles/RoomAmbience.particle.

Needs Pillow.
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

SIZE = 64
SUPER = 4
CANVAS = SIZE * SUPER  # everything is drawn on a 256x256 canvas and scaled down


def new_canvas():
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def finish(image, path):
    image.resize((SIZE, SIZE), Image.LANCZOS).save(path)


def radial(image, color, radius, power=2.0, center=None):
    """Adds a soft round blob: alpha falls from the centre to the radius."""
    cx, cy = center if center else (CANVAS / 2, CANVAS / 2)
    pixels = image.load()
    for y in range(CANVAS):
        for x in range(CANVAS):
            d = math.hypot(x - cx, y - cy) / radius
            if d >= 1.0:
                continue
            a = (1.0 - d) ** power
            old = pixels[x, y]
            alpha = min(255, old[3] + int(color[3] * a))
            pixels[x, y] = (color[0], color[1], color[2], alpha)


def sprite_spark(path):
    """Bright four-pointed star with a soft core."""
    image, draw = new_canvas()
    c = CANVAS // 2
    for length, width, alpha in ((120, 10, 255), (84, 7, 255)):
        draw.polygon([(c - length, c), (c, c - width), (c + length, c), (c, c + width)], fill=(255, 235, 170, alpha))
        draw.polygon([(c, c - length), (c - width, c), (c, c + length), (c + width, c)], fill=(255, 235, 170, alpha))
    image = image.filter(ImageFilter.GaussianBlur(3))
    radial(image, (255, 250, 220, 255), 46, 1.6)
    finish(image, path)


def sprite_glow(path):
    """Soft round glow, white; the particle colour tints it."""
    image, _ = new_canvas()
    radial(image, (255, 255, 255, 255), 126, 2.2)
    finish(image, path)


def sprite_smoke(path):
    """Soft cloudy puff made of overlapping blobs."""
    rng = random.Random(7)
    image, _ = new_canvas()
    for _ in range(9):
        cx = CANVAS / 2 + rng.uniform(-44, 44)
        cy = CANVAS / 2 + rng.uniform(-44, 44)
        radial(image, (235, 235, 235, 150), rng.uniform(52, 84), 1.5, (cx, cy))
    image = image.filter(ImageFilter.GaussianBlur(5))
    finish(image, path)


def sprite_dust(path):
    """Small soft speck."""
    image, _ = new_canvas()
    radial(image, (255, 255, 255, 255), 80, 1.4)
    finish(image, path)


def sprite_web(path):
    """Cobweb in a corner: threads from the corner and curved cross threads."""
    image, draw = new_canvas()
    thread = (235, 235, 235, 215)
    # the corner is at the lower left, the web spreads over the upper right quarter
    ox, oy = 8, CANVAS - 8
    angles = [0, 18, 36, 54, 72, 90]
    reach = 236
    for a in angles:
        rad = math.radians(a)
        draw.line((ox, oy, ox + reach * math.cos(rad), oy - reach * math.sin(rad)), fill=thread, width=3)
    for ring in (60, 110, 160, 210):
        points = []
        for a in angles:
            rad = math.radians(a)
            points.append((ox + ring * math.cos(rad), oy - ring * math.sin(rad)))
        # slightly sagging connections between the threads
        for p, q in zip(points, points[1:]):
            mx = (p[0] + q[0]) / 2 - 5
            my = (p[1] + q[1]) / 2 + 5
            draw.line((p, (mx, my), q), fill=thread, width=2)
    finish(image, path)


SPRITES = {
    "RoomAmbSpark": sprite_spark,
    "RoomAmbGlow": sprite_glow,
    "RoomAmbSmoke": sprite_smoke,
    "RoomAmbDust": sprite_dust,
    "RoomAmbWeb": sprite_web,
}


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    folder = os.path.join(root, "materials", "textures")
    for name, function in SPRITES.items():
        function(os.path.join(folder, name + ".png"))
        print("wrote", name)


if __name__ == "__main__":
    main()
