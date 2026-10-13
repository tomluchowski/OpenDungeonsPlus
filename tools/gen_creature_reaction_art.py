#!/usr/bin/env python3
"""Draws the emote icons and particle textures used by the creature reactions.

Everything is drawn from simple shapes by this script, nothing is taken from another
work. Run it from the repository root to write the images again:

    python tools/gen_creature_reaction_art.py

Output (all 64x64 RGBA, in materials/textures):
    CreatureEmote<Name>.png        icon in a speech bubble, shown above the creature
    ReactionParticle<Name>.png     sprite of the particle systems in particles/CreatureReactions.particle

Needs Pillow.
"""

import argparse
import math
import os

from PIL import Image, ImageDraw

SIZE = 64
SUPER = 4
CANVAS = 64 * SUPER  # everything below is drawn on a 256x256 canvas and scaled down

OUTLINE = (52, 40, 46, 255)
WHITE = (255, 255, 255, 255)


def new_canvas():
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def finish(image, path):
    image.resize((SIZE, SIZE), Image.LANCZOS).save(path)


def bubble(draw):
    """Rounded speech bubble with a tail pointing down."""
    draw.rounded_rectangle((6, 6, 250, 200), radius=60, fill=OUTLINE)
    draw.polygon([(84, 188), (128, 252), (170, 188)], fill=OUTLINE)
    draw.rounded_rectangle((18, 18, 238, 188), radius=50, fill=WHITE)
    draw.polygon([(100, 184), (128, 232), (154, 184)], fill=WHITE)


def glyph_exclamation(draw):
    red = (205, 40, 40, 255)
    draw.rounded_rectangle((112, 38, 144, 128), radius=14, fill=red)
    draw.ellipse((111, 142, 145, 176), fill=red)


def glyph_question(draw):
    blue = (40, 90, 200, 255)
    draw.arc((80, 36, 176, 124), 180, 450, fill=blue, width=22)
    draw.line((128, 124, 128, 140), fill=blue, width=22)
    draw.ellipse((113, 154, 143, 184), fill=blue)


def glyph_bulb(draw):
    yellow = (252, 214, 56, 255)
    amber = (190, 132, 20, 255)
    gray = (120, 120, 130, 255)
    draw.ellipse((84, 46, 172, 134), fill=yellow, outline=amber, width=7)
    draw.rectangle((108, 130, 148, 162), fill=gray)
    draw.line((110, 174, 146, 174), fill=gray, width=12)
    draw.ellipse((100, 62, 120, 82), fill=(255, 245, 190, 255))
    for angle in (-60, -30, 0, 30, 60):
        a = math.radians(angle - 90)
        x1, y1 = 128 + math.cos(a) * 62, 90 + math.sin(a) * 62
        x2, y2 = 128 + math.cos(a) * 80, 90 + math.sin(a) * 80
        draw.line((x1, y1, x2, y2), fill=amber, width=7)


def glyph_heart(draw):
    red = (214, 40, 74, 255)
    draw.ellipse((72, 56, 130, 114), fill=red)
    draw.ellipse((126, 56, 184, 114), fill=red)
    draw.polygon([(73, 96), (183, 96), (128, 170)], fill=red)
    draw.ellipse((88, 70, 106, 88), fill=(255, 190, 200, 255))


def coin(draw, cx, cy, radius):
    gold = (246, 198, 44, 255)
    dark = (166, 112, 12, 255)
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=gold, outline=dark, width=8)
    inner = radius * 0.62
    draw.ellipse((cx - inner, cy - inner, cx + inner, cy + inner), outline=(206, 146, 22, 255), width=6)
    draw.arc((cx - radius + 14, cy - radius + 14, cx + radius - 14, cy + radius - 14), 200, 270,
             fill=(255, 244, 190, 255), width=7)


def glyph_coin(draw):
    coin(draw, 128, 104, 54)


def zed(draw, x, y, size, width, color):
    draw.line([(x, y), (x + size, y), (x, y + size), (x + size, y + size)], fill=color, width=width, joint="curve")


def glyph_z(draw):
    blue = (58, 96, 200, 255)
    zed(draw, 66, 100, 64, 16, blue)
    zed(draw, 130, 68, 44, 13, blue)
    zed(draw, 164, 44, 28, 10, blue)


def glyph_sweat(draw):
    blue = (62, 148, 232, 255)
    draw.polygon([(128, 34), (96, 108), (160, 108)], fill=blue)
    draw.ellipse((92, 88, 164, 160), fill=blue)
    draw.ellipse((108, 108, 124, 126), fill=(200, 232, 255, 255))


def glyph_steam(draw):
    red = (206, 72, 58, 255)
    for x in (88, 128, 168):
        points = []
        for i in range(0, 17):
            t = i / 16.0
            points.append((x + math.sin(t * math.pi * 3.0) * 12, 170 - t * 120))
        draw.line(points, fill=red, width=14, joint="curve")


def glyph_note(draw):
    purple = (74, 48, 130, 255)
    draw.ellipse((74, 122, 124, 162), fill=purple)
    draw.rectangle((112, 52, 124, 142), fill=purple)
    draw.polygon([(124, 52), (172, 82), (166, 112), (124, 86)], fill=purple)


def glyph_star(draw):
    gold = (246, 190, 40, 255)
    dark = (176, 116, 12, 255)
    points = []
    for i in range(10):
        radius = 62 if i % 2 == 0 else 26
        angle = -math.pi / 2 + i * math.pi / 5
        points.append((128 + radius * math.cos(angle), 108 + radius * math.sin(angle)))
    draw.polygon(points, fill=gold, outline=dark)
    draw.line(points + [points[0]], fill=dark, width=6, joint="curve")


def glyph_fist(draw):
    red = (200, 52, 44, 255)
    dark = (120, 24, 20, 255)
    # Four fingers side by side, the thumb across them and the wrist below
    for i in range(4):
        x = 78 + i * 25
        draw.rounded_rectangle((x, 48, x + 24, 108), radius=11, fill=red, outline=dark, width=4)
    draw.rounded_rectangle((70, 90, 184, 138), radius=18, fill=red, outline=dark, width=4)
    draw.rounded_rectangle((72, 118, 170, 150), radius=14, fill=red, outline=dark, width=4)
    draw.rectangle((92, 146, 160, 176), fill=dark)


def glyph_book(draw):
    cover = (60, 98, 170, 255)
    dark = (28, 48, 100, 255)
    page = (250, 244, 224, 255)
    # An open book seen from the front: two pages, a spine and a few lines of text
    draw.polygon([(128, 62), (60, 78), (60, 158), (128, 142)], fill=page, outline=dark)
    draw.polygon([(128, 62), (196, 78), (196, 158), (128, 142)], fill=page, outline=dark)
    draw.line([(128, 62), (128, 142)], fill=dark, width=6)
    draw.line([(52, 84), (52, 166), (128, 150), (204, 166), (204, 84)], fill=cover, width=10, joint="curve")
    for i in range(3):
        draw.line([(74, 96 + i * 16), (116, 88 + i * 16)], fill=dark, width=4)
        draw.line([(140, 88 + i * 16), (182, 96 + i * 16)], fill=dark, width=4)


def glyph_gear(draw):
    steel = (110, 120, 134, 255)
    dark = (54, 60, 72, 255)
    centre = (128, 108)
    # Eight teeth, then the round body and the hole in the middle
    for i in range(8):
        angle = i * math.pi / 4
        tip = (centre[0] + 62 * math.cos(angle), centre[1] + 62 * math.sin(angle))
        side = (-math.sin(angle) * 16, math.cos(angle) * 16)
        base = (centre[0] + 36 * math.cos(angle), centre[1] + 36 * math.sin(angle))
        draw.polygon([(base[0] - side[0], base[1] - side[1]), (tip[0] - side[0] * 0.7, tip[1] - side[1] * 0.7),
                      (tip[0] + side[0] * 0.7, tip[1] + side[1] * 0.7), (base[0] + side[0], base[1] + side[1])],
                     fill=steel, outline=dark)
    draw.ellipse((centre[0] - 46, centre[1] - 46, centre[0] + 46, centre[1] + 46), fill=steel, outline=dark, width=5)
    draw.ellipse((centre[0] - 18, centre[1] - 18, centre[0] + 18, centre[1] + 18), fill=WHITE, outline=dark, width=5)


def glyph_smoke(draw):
    grey = (150, 150, 158, 255)
    dark = (88, 88, 98, 255)
    # A small cloud of three puffs
    for box in ((62, 96, 128, 160), (100, 62, 176, 138), (132, 96, 200, 160), (86, 104, 170, 164)):
        draw.ellipse(box, fill=grey, outline=dark, width=4)
    draw.ellipse((90, 94, 170, 158), fill=grey)


def glyph_chicken(draw):
    cream = (250, 242, 220, 255)
    wing = (232, 218, 184, 255)
    dark = (120, 100, 70, 255)
    red = (205, 40, 40, 255)
    orange = (240, 150, 30, 255)
    # Tail, round body with a wing, head with comb, beak, eye and two legs
    draw.polygon([(70, 104), (42, 66), (92, 92)], fill=cream, outline=dark)
    draw.ellipse((62, 84, 178, 166), fill=cream, outline=dark, width=5)
    draw.ellipse((88, 108, 146, 150), fill=wing, outline=dark, width=4)
    draw.line((112, 164, 112, 184), fill=orange, width=7)
    draw.line((142, 164, 142, 184), fill=orange, width=7)
    for cx in (162, 176, 190):
        draw.ellipse((cx - 10, 34, cx + 10, 60), fill=red, outline=dark, width=3)
    draw.ellipse((146, 50, 206, 108), fill=cream, outline=dark, width=5)
    draw.polygon([(202, 74), (230, 82), (202, 92)], fill=orange, outline=dark)
    draw.ellipse((182, 68, 192, 78), fill=OUTLINE)


def glyph_sack(draw):
    brown = (176, 130, 76, 255)
    dark = (96, 64, 30, 255)
    # A bag tied at the top with a coin on it
    points = [(104, 66), (152, 66), (172, 88), (198, 132), (182, 170), (74, 170), (58, 132), (84, 88)]
    draw.polygon(points, fill=brown)
    draw.line(points + [points[0]], fill=dark, width=6, joint="curve")
    draw.rounded_rectangle((98, 54, 158, 76), radius=9, fill=(150, 106, 56, 255), outline=dark, width=4)
    coin(draw, 128, 128, 28)


EMOTES = [
    ("Exclamation", glyph_exclamation),
    ("Question", glyph_question),
    ("Bulb", glyph_bulb),
    ("Heart", glyph_heart),
    ("Coin", glyph_coin),
    ("Z", glyph_z),
    ("Sweat", glyph_sweat),
    ("Steam", glyph_steam),
    ("Note", glyph_note),
    ("Star", glyph_star),
    ("Fist", glyph_fist),
    ("Book", glyph_book),
    ("Gear", glyph_gear),
    ("Smoke", glyph_smoke),
    ("Chicken", glyph_chicken),
    ("Sack", glyph_sack),
]


def soft_disc(color, power, strength=1.0):
    """Disc of the given color that fades out towards the edge, as an RGBA canvas."""
    gradient = Image.radial_gradient("L")  # black in the middle, 255 in the corners
    gradient = gradient.resize((CANVAS, CANVAS), Image.BILINEAR)
    # The edge of the canvas (not its corner) is where the disc has to be gone
    mask = gradient.point(lambda v: int(255 * strength * max(0.0, 1.0 - v / 180.0) ** power))
    image = Image.new("RGBA", (CANVAS, CANVAS), color + (255,))
    image.putalpha(mask)
    return image


def particle_spark():
    image = soft_disc((255, 238, 170), 2.0)
    draw = ImageDraw.Draw(image)
    white = (255, 255, 235, 255)
    draw.polygon([(128, 20), (140, 116), (236, 128), (140, 140), (128, 236), (116, 140), (20, 128), (116, 116)],
                 fill=white)
    return image


def particle_coin():
    image, draw = new_canvas()
    coin(draw, 128, 128, 108)
    return image


def particle_dust():
    return soft_disc((176, 156, 128), 1.3, 0.8)


def particle_z():
    image, draw = new_canvas()
    zed(draw, 52, 52, 152, 46, OUTLINE)
    zed(draw, 52, 52, 152, 26, (232, 240, 255, 255))
    return image


def particle_ring():
    """Thin glowing ring, seen from the side as a flat ring of light."""
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    for width, alpha in ((34, 70), (22, 130), (10, 255)):
        draw.ellipse((20, 20, CANVAS - 20, CANVAS - 20), outline=(255, 244, 190, alpha), width=width)
    return image


def particle_steam():
    return soft_disc((250, 244, 240), 1.1, 0.85)


def particle_glow():
    return soft_disc((255, 226, 130), 1.6, 0.9)


def particle_smoke():
    return soft_disc((118, 112, 108), 1.1, 1.0)


PARTICLES = [
    ("Spark", particle_spark),
    ("Coin", particle_coin),
    ("Dust", particle_dust),
    ("Z", particle_z),
    ("Steam", particle_steam),
    ("Ring", particle_ring),
    ("Glow", particle_glow),
    ("Smoke", particle_smoke),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=os.path.join("materials", "textures"),
                        help="directory the images are written to")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    for name, glyph in EMOTES:
        image, draw = new_canvas()
        bubble(draw)
        glyph(draw)
        finish(image, os.path.join(args.out, "CreatureEmote%s.png" % name))

    for name, make in PARTICLES:
        finish(make(), os.path.join(args.out, "ReactionParticle%s.png" % name))

    print("Wrote %d emote icons and %d particle textures to %s" % (len(EMOTES), len(PARTICLES), args.out))


if __name__ == "__main__":
    main()
