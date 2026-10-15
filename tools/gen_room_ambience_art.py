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


def sprite_page(path):
    """Loose page of parchment with a few lines of writing."""
    image, draw = new_canvas()
    draw.polygon([(70, 30), (190, 20), (200, 220), (60, 232)], fill=(232, 220, 180, 255), outline=(120, 96, 60, 255))
    for i in range(6):
        y = 60 + i * 26
        draw.line((84, y + 2, 176 - (i % 3) * 14, y), fill=(110, 84, 60, 255), width=5)
    finish(image, path)


def sprite_rune(path):
    """Glowing ring with an angular mark, white so the particle colour tints it."""
    image, draw = new_canvas()
    c = CANVAS // 2
    draw.ellipse((c - 100, c - 100, c + 100, c + 100), outline=(255, 255, 255, 255), width=10)
    draw.line((c - 40, c - 60, c + 40, c - 20), fill=(255, 255, 255, 255), width=12)
    draw.line((c + 40, c - 20, c - 30, c + 10), fill=(255, 255, 255, 255), width=12)
    draw.line((c - 30, c + 10, c + 36, c + 64), fill=(255, 255, 255, 255), width=12)
    image = image.filter(ImageFilter.GaussianBlur(4))
    radial(image, (255, 255, 255, 120), 126, 2.0)
    finish(image, path)


def sprite_coin(path):
    """Gold coin seen from the front with a bright rim and a highlight."""
    image, draw = new_canvas()
    c = CANVAS // 2
    draw.ellipse((c - 90, c - 90, c + 90, c + 90), fill=(150, 104, 24, 255))
    draw.ellipse((c - 80, c - 80, c + 80, c + 80), fill=(236, 190, 52, 255))
    draw.ellipse((c - 56, c - 56, c + 56, c + 56), outline=(184, 132, 30, 255), width=8)
    draw.polygon([(c - 60, c - 30), (c - 24, c - 66), (c - 8, c - 56), (c - 50, c - 12)], fill=(255, 244, 170, 255))
    finish(image, path)


def sprite_drop(path):
    """Water drop."""
    image, draw = new_canvas()
    c = CANVAS // 2
    draw.polygon([(c, 30), (c - 56, 150), (c + 56, 150)], fill=(150, 200, 235, 230))
    draw.ellipse((c - 60, 100, c + 60, 220), fill=(150, 200, 235, 230))
    draw.ellipse((c - 34, 130, c - 12, 168), fill=(255, 255, 255, 220))
    image = image.filter(ImageFilter.GaussianBlur(1.5))
    finish(image, path)


def sprite_feather(path):
    """Small curved feather."""
    image, draw = new_canvas()
    draw.polygon([(60, 214), (80, 130), (130, 50), (196, 28), (180, 100), (140, 170), (84, 222)], fill=(240, 236, 226, 255),
                 outline=(150, 140, 120, 255))
    draw.line((62, 216, 190, 34), fill=(170, 156, 130, 255), width=5)
    finish(image, path)


def sprite_grain(path):
    """A few grains of seed lying together."""
    image, draw = new_canvas()
    for x, y, a in ((90, 100, 20), (150, 80, -15), (120, 150, 35), (170, 150, -30), (80, 170, 10)):
        draw.ellipse((x - 18, y - 9, x + 18, y + 9), fill=(214, 178, 96, 255), outline=(150, 118, 60, 255))
    finish(image, path)


def sprite_z(path):
    """Letter Z, shown rising over a sleeping creature."""
    image, draw = new_canvas()
    pts = [(70, 70), (186, 70), (70, 190), (190, 190)]
    outline = (60, 80, 140, 255)
    for width, color in ((34, outline), (20, (235, 242, 255, 255))):
        draw.line((pts[0], pts[1]), fill=color, width=width)
        draw.line((pts[1], pts[2]), fill=color, width=width)
        draw.line((pts[2], pts[3]), fill=color, width=width)
    image = image.filter(ImageFilter.GaussianBlur(1.2))
    finish(image, path)


def sprite_swirl(path):
    """Spiral of light arms around a bright centre, white so the particle colour tints it."""
    image, draw = new_canvas()
    c = CANVAS / 2
    for arm in range(3):
        points = []
        for i in range(60):
            t = i / 59.0
            angle = arm * 2.0944 + t * 5.2
            radius = 14 + t * 108
            points.append((c + radius * math.cos(angle), c + radius * math.sin(angle)))
        for i in range(len(points) - 1):
            width = int(16 - 12 * (i / 59.0)) + 2
            alpha = int(255 * (1.0 - 0.7 * (i / 59.0)))
            draw.line((points[i], points[i + 1]), fill=(255, 255, 255, alpha), width=width)
    image = image.filter(ImageFilter.GaussianBlur(4))
    radial(image, (255, 255, 255, 200), 44, 1.5)
    finish(image, path)


SPRITES = {
    "RoomAmbSpark": sprite_spark,
    "RoomAmbGlow": sprite_glow,
    "RoomAmbSmoke": sprite_smoke,
    "RoomAmbDust": sprite_dust,
    "RoomAmbWeb": sprite_web,
    "RoomAmbPage": sprite_page,
    "RoomAmbRune": sprite_rune,
    "RoomAmbCoin": sprite_coin,
    "RoomAmbDrop": sprite_drop,
    "RoomAmbFeather": sprite_feather,
    "RoomAmbGrain": sprite_grain,
    "RoomAmbZ": sprite_z,
    "RoomAmbSwirl": sprite_swirl,
}


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    folder = os.path.join(root, "materials", "textures")
    for name, function in SPRITES.items():
        function(os.path.join(folder, name + ".png"))
        print("wrote", name)


if __name__ == "__main__":
    main()
