"""Materials, frames and motifs of the forged emblems (see generate_forged_icons.py).

Every motif draws into a forged_emblem.Canvas in design space -1..1 (y down). Slot emblems fill a
square tile whose dark well reaches 0.9, so motifs keep inside about 0.85. Small symbols stand
without a frame and may use the whole square.
"""
import numpy as np

from forged_emblem import Mat

# ---------------------------------------------------------------------------------------------
# Materials: the one place for colours (RGB 0..255)
# ---------------------------------------------------------------------------------------------
BRONZE = Mat((196, 128, 60), (146, 86, 36), spec=0.62, shin=30, metal=1.0, mottle=0.10, brushed=0.06)
BRONZE_DARK = Mat((132, 82, 38), (84, 48, 22), spec=0.5, shin=24, metal=1.0, mottle=0.12, brushed=0.06)
GOLD = Mat((250, 202, 88), (218, 152, 52), spec=1.0, shin=46, metal=1.0, mottle=0.06, brushed=0.04)
GOLD_DARK = Mat((196, 140, 50), (140, 90, 30), spec=0.8, shin=34, metal=1.0, mottle=0.08)
IRON = Mat((104, 94, 88), (66, 58, 54), spec=0.55, shin=26, metal=0.85, mottle=0.10, brushed=0.10)
IRON_DARK = Mat((66, 58, 54), (38, 32, 30), spec=0.45, shin=22, metal=0.8, mottle=0.12, brushed=0.08)
STEEL = Mat((196, 186, 174), (122, 112, 104), spec=1.0, shin=54, metal=0.95, mottle=0.05, brushed=0.14)
BONE = Mat((240, 226, 194), (196, 174, 136), spec=0.28, shin=14, mottle=0.08)
PARCHMENT = Mat((228, 202, 150), (194, 160, 106), spec=0.14, shin=10, mottle=0.14)
WOOD = Mat((150, 96, 50), (104, 62, 32), spec=0.22, shin=12, mottle=0.10, stripes=0.07, stripe_scale=0.06)
WOOD_DARK = Mat((102, 62, 34), (64, 38, 22), spec=0.18, shin=10, mottle=0.10, stripes=0.07, stripe_scale=0.05)
LEATHER = Mat((136, 78, 44), (94, 52, 30), spec=0.3, shin=16, mottle=0.14)
CLOTH_RED = Mat((186, 48, 40), (122, 26, 26), spec=0.16, shin=10, mottle=0.16)
RUBY = Mat((240, 56, 58), (132, 14, 26), spec=1.0, shin=64, mottle=0.0, grain=0.0)
RUBY_LIGHT = Mat((255, 120, 116), (204, 44, 52), spec=0.8, shin=40, mottle=0.0, grain=0.0)
AMBER = Mat((255, 182, 60), (214, 100, 22), spec=0.9, shin=50, mottle=0.0, grain=0.0, emit=(38, 16, 0))
EMBER = Mat((255, 170, 54), (232, 84, 18), spec=0.2, shin=10, mottle=0.2, emit=(96, 40, 8), emit_noise=0.5)
EMBER_HOT = Mat((255, 238, 174), (255, 174, 62), spec=0.0, shin=4, mottle=0.1, emit=(150, 96, 30), emit_noise=0.4)
STONE = Mat((144, 126, 108), (98, 84, 72), spec=0.16, shin=12, mottle=0.18, grain=0.10)
STONE_DARK = Mat((96, 82, 70), (60, 50, 44), spec=0.12, shin=10, mottle=0.2, grain=0.10)
BLOOD = Mat((160, 32, 30), (100, 16, 18), spec=0.5, shin=26, mottle=0.1)
CONTOUR = Mat((16, 10, 8), spec=0.0, shin=4, mottle=0.0, grain=0.0)
WORKER = Mat((238, 120, 56), (164, 58, 26), spec=0.4, shin=20, mottle=0.14, emit=(30, 8, 0))
SKIN = Mat((232, 158, 108), (178, 104, 66), spec=0.3, shin=16, mottle=0.12)
FLESH = Mat((214, 170, 120), (160, 116, 76), spec=0.25, shin=14, mottle=0.12)
YELLOW = Mat((255, 222, 96), (240, 176, 50), spec=0.4, shin=16, mottle=0.08)
GLASS = Mat((90, 60, 36), (40, 24, 14), spec=0.9, shin=60, mottle=0.0, grain=0.0)

# Well colours of the square tiles: centre and edge, per category
WELL = {
    "room": ((78, 52, 32), (22, 14, 10)),
    "spell": ((100, 40, 24), (26, 10, 8)),
    "trap": ((72, 60, 52), (18, 14, 12)),
    "creature": ((84, 56, 34), (22, 14, 10)),
}
WELL_GLOW = {"room": (74, 36, 10), "spell": (104, 36, 10), "trap": (46, 36, 28), "creature": (74, 38, 12)}


def deg(a):
    return a * np.pi / 180.0


class Xf:
    """Local frame: origin (cx, cy), rotation ang, scale k. Local +x points along the object."""

    def __init__(self, cx, cy, ang=0.0, k=1.0):
        self.cx, self.cy, self.k = cx, cy, k
        self.co, self.si = np.cos(ang), np.sin(ang)

    def p(self, x, y):
        x, y = x * self.k, y * self.k
        return (self.cx + x * self.co - y * self.si, self.cy + x * self.si + y * self.co)

    def pts(self, lst):
        return [self.p(x, y) for x, y in lst]


def star(n, ro, ri, cx=0.0, cy=0.0, rot=0.0):
    pts = []
    for i in range(2 * n):
        r = ro if i % 2 == 0 else ri
        a = rot + i * np.pi / n
        pts.append((cx + r * np.cos(a), cy + r * np.sin(a)))
    return pts


# ---------------------------------------------------------------------------------------------
# Frames
# ---------------------------------------------------------------------------------------------
TILE_HALF = 0.90      # half width of the drawing area inside the frame of a square tile
TILE_RAD = 0.10


def tile(c, kind, rivets=True):
    """Square tile: dark stone well with a category tint, a bronze edge line with a bevel, small corner rivets."""
    centre, edge = WELL[kind]
    c.clip_box = (TILE_HALF, TILE_RAD)
    c.add(c.box(0, 0, 0.995, 0.995, 0.135), CONTOUR, z=0.0, bevel=0.01)
    c.add(c.box(0, 0, 0.965, 0.965, 0.115), BRONZE, z=0.10, bevel=0.05)
    c.fill_well(c.box(0, 0, TILE_HALF + 0.012, TILE_HALF + 0.012, TILE_RAD + 0.012), centre, edge, r=1.30, z=0.0)
    c.add(c.ring_box(0, 0, TILE_HALF + 0.012, TILE_RAD + 0.012, 0.012), GOLD_DARK, z=0.07, bevel=0.012)
    if rivets:
        for sx in (-1, 1):
            for sy in (-1, 1):
                c.add(c.circle(0.82 * sx, 0.82 * sy, 0.028), GOLD, z=0.04, bevel=0.028, base=0.05)
    c.glow(0.0, 0.05, 0.85, WELL_GLOW[kind], 0.6)


def tile_vignette(c):
    """Darkens the edge of the tile so that the motif reads as lit from within."""
    d = np.maximum(np.abs(c.X0), np.abs(c.Y0))
    k = np.clip((d - 0.62) / 0.28, 0, 1) ** 1.6 * (d < 0.92)
    c.alb *= (1.0 - 0.40 * k)[..., None]


# ---------------------------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------------------------
def coin_stack(c, cx, cy, n, r=0.2, thick=0.058, tilt=0.42):
    ry = r * tilt
    for i in range(n):
        y = cy - i * thick
        side = c.union(c.box(cx, y + thick * 0.5, r, thick * 0.5), c.ellipse(cx, y + thick, r, ry))
        c.add(side, GOLD_DARK, z=0.04, bevel=0.02, base=0.10 + i * 0.03, shadow=0.35 if i == 0 else 0.0)
        c.add(c.ellipse(cx, y, r, ry), GOLD, z=0.02, bevel=0.03, base=0.13 + i * 0.03)


def z_letter(c, cx, cy, s, mat=GOLD, w=0.04):
    pts = [(cx - s, cy - s), (cx + s, cy - s), (cx - s, cy + s), (cx + s, cy + s)]
    shape = c.union(c.seg(*pts[0], *pts[1], w), c.seg(*pts[1], *pts[2], w), c.seg(*pts[2], *pts[3], w))
    c.add(shape, mat, z=0.05, bevel=w * 0.9, base=0.14, shadow=0.5, sh_dx=0.03, sh_dy=0.04, sh_soft=0.02)


def sword(c, cx, cy, ang, length=0.9, k=1.0, blade=STEEL, shadow=0.5, base=0.12, wid=1.0):
    f = Xf(cx, cy, ang, k)
    w = 0.062 * wid
    bl = c.poly(f.pts([(0.10, -w), (length - 0.14, -w * 0.9), (length, 0.0), (length - 0.14, w * 0.9), (0.10, w)]))
    c.add(bl, blade, z=0.05, bevel=0.06 * k, base=base, shadow=shadow)
    c.add(c.seg(*f.p(0.16, 0), *f.p(length - 0.22, 0), 0.011 * k * wid), IRON_DARK, z=0.01, bevel=0.01, base=base + 0.045)
    c.add(c.seg(*f.p(0.09, -0.17 * wid), *f.p(0.09, 0.17 * wid), 0.033 * k * wid), GOLD, z=0.05, bevel=0.03, base=base + 0.05, shadow=0.4)
    c.add(c.seg(*f.p(0.08, 0), *f.p(-0.15, 0), 0.032 * k), LEATHER, z=0.05, bevel=0.03, base=base + 0.04)
    c.add(c.circle(*f.p(-0.19, 0), 0.05 * k), GOLD, z=0.05, bevel=0.05, base=base + 0.04)


def heart_sdf(c, cx, cy, s):
    lobes = c.union(c.circle(cx - 0.17 * s, cy - 0.10 * s, 0.24 * s), c.circle(cx + 0.17 * s, cy - 0.10 * s, 0.24 * s))
    tip = c.poly([(cx - 0.39 * s, cy - 0.03 * s), (cx + 0.39 * s, cy - 0.03 * s), (cx, cy + 0.52 * s)])
    return c.smooth_union(lobes, tip, 0.05 * s)


def flame_sdf(c, cx, cy, s):
    body = c.ellipse(cx, cy + 0.10 * s, 0.15 * s, 0.19 * s)
    tip = c.poly([(cx - 0.14 * s, cy + 0.04 * s), (cx + 0.02 * s, cy - 0.44 * s), (cx + 0.14 * s, cy + 0.04 * s)])
    return c.smooth_union(body, tip, 0.06 * s)


def gear_sdf(c, cx, cy, r, teeth=8, depth=0.07, hole=0.0):
    ang = np.arctan2(c.Y - cy, c.X - cx)
    wave = np.clip(np.cos(teeth * ang) * 3.0, -1, 1) * 0.5 + 0.5
    d = np.hypot(c.X - cx, c.Y - cy) - (r + depth * wave)
    if hole:
        d = np.maximum(d, hole - np.hypot(c.X - cx, c.Y - cy))
    return d


def arch_sdf(c, cx, top, bottom, hw):
    """A door or window with a round top: the semicircle sits on the straight sides."""
    return c.union(c.box(cx, (top + hw + bottom) / 2, hw, (bottom - top - hw) / 2), c.circle(cx, top + hw, hw))


def sparks(c, pts, mat=None):
    for x, y, r in pts:
        c.add(c.circle(x, y, r), EMBER_HOT if mat is None else mat, z=0.02, bevel=r, base=0.3)
        c.glow(x, y, r * 4, (255, 150, 40), 0.5)


# ---------------------------------------------------------------------------------------------
# Rooms
# ---------------------------------------------------------------------------------------------
def m_treasury(c):
    body = c.smooth_union(c.ellipse(-0.22, 0.02, 0.36, 0.34), c.ellipse(-0.22, -0.30, 0.16, 0.16), 0.12)
    c.add(body, LEATHER, z=0.16, bevel=0.20, base=0.05, shadow=0.55)
    c.add(c.box(-0.22, -0.22, 0.17, 0.035, 0.03), BRONZE, z=0.05, bevel=0.03, base=0.20)
    c.add(c.poly([(-0.42, -0.56), (-0.02, -0.56), (-0.10, -0.32), (-0.34, -0.32)]), LEATHER, z=0.07, bevel=0.06, base=0.17, shadow=0.4)
    c.paint(c.seg(-0.32, -0.54, -0.28, -0.36, 0.008), (70, 38, 20), 0.8)
    c.paint(c.seg(-0.12, -0.54, -0.16, -0.36, 0.008), (70, 38, 20), 0.8)
    c.add(c.circle(-0.22, 0.05, 0.11), GOLD_DARK, z=0.03, bevel=0.03, base=0.21)
    c.add(c.circle(-0.22, 0.05, 0.05), GOLD, z=0.03, bevel=0.03, base=0.24)
    coin_stack(c, 0.30, 0.34, 6, r=0.21)
    coin_stack(c, 0.02, 0.50, 3, r=0.19)
    c.add(c.ellipse(0.50, 0.08, 0.13, 0.19, deg(18)), GOLD, z=0.05, bevel=0.07, base=0.16, shadow=0.5)
    c.add(c.ring(0.50, 0.08, 0.08, 0.015), GOLD_DARK, z=0.02, bevel=0.015, base=0.2)


def m_dormitory(c):
    c.add(c.box(-0.05, 0.30, 0.66, 0.05, 0.03), WOOD_DARK, z=0.05, bevel=0.04, base=0.06, shadow=0.6)
    for x in (-0.58, 0.48):
        c.add(c.box(x, 0.44, 0.05, 0.10, 0.02), WOOD_DARK, z=0.06, bevel=0.03, base=0.06)
    c.add(c.box(-0.62, 0.0, 0.06, 0.36, 0.03), WOOD, z=0.09, bevel=0.05, base=0.08, shadow=0.5)
    c.add(c.box(0.52, 0.12, 0.05, 0.24, 0.03), WOOD, z=0.08, bevel=0.05, base=0.08, shadow=0.4)
    c.add(c.box(-0.04, 0.16, 0.54, 0.13, 0.08), CLOTH_RED, z=0.13, bevel=0.10, base=0.10, shadow=0.55)
    c.add(c.box(-0.04, 0.05, 0.54, 0.02, 0.01), BRONZE, z=0.03, bevel=0.02, base=0.20)
    c.add(c.ellipse(-0.40, 0.02, 0.17, 0.09, deg(-6)), BONE, z=0.12, bevel=0.12, base=0.16, shadow=0.5)
    for x, y, s in ((0.05, -0.28, 0.07), (0.27, -0.42, 0.095), (0.52, -0.58, 0.12)):
        z_letter(c, x, y, s)


def m_training(c):
    sword(c, -0.60, 0.66, deg(-45), 1.45, 0.9, wid=1.25)
    sword(c, 0.60, 0.66, deg(-135), 1.45, 0.9, wid=1.25)
    c.add(c.circle(0, 0.02, 0.50), IRON, z=0.08, bevel=0.06, base=0.20, shadow=0.6)
    for r, m in ((0.44, BONE), (0.35, CLOTH_RED), (0.26, BONE), (0.17, CLOTH_RED)):
        c.add(c.circle(0, 0.02, r), m, z=0.01, bevel=0.01, base=0.27)
    c.add(c.circle(0, 0.02, 0.075), GOLD, z=0.05, bevel=0.07, base=0.28)
    f = Xf(0.03, 0.04, deg(-40))
    c.add(c.seg(*f.p(0.06, 0), *f.p(0.66, 0), 0.016), WOOD, z=0.03, bevel=0.016, base=0.32, shadow=0.5, sh_dx=0.03, sh_dy=0.04)
    c.add(c.poly(f.pts([(0.52, 0), (0.70, -0.09), (0.66, 0), (0.70, 0.09)])), CLOTH_RED, z=0.02, bevel=0.02, base=0.33)
    c.add(c.poly(f.pts([(0.0, -0.03), (0.09, 0.0), (0.0, 0.03)])), STEEL, z=0.02, bevel=0.01, base=0.33)


def anvil(c, cx, cy, k=1.0):
    def q(x, y):
        return (cx + x * k, cy + y * k)
    top = c.poly([q(-0.72, -0.14), q(-0.40, -0.14), q(0.42, -0.14), q(0.42, 0.02), q(-0.32, 0.02), q(-0.72, -0.14)])
    horn = c.poly([q(-0.42, -0.14), q(-0.74, -0.12), q(-0.56, -0.08), q(-0.40, 0.0)])
    waist = c.poly([q(-0.26, 0.0), q(0.26, 0.0), q(0.34, 0.28), q(-0.34, 0.28)])
    base = c.box(cx, cy + 0.36 * k, 0.46 * k, 0.09 * k, 0.03)
    c.add(c.union(top, horn, waist, base), IRON, z=0.12, bevel=0.06, base=0.06, shadow=0.6)
    c.add(c.box(cx - 0.12 * k, cy - 0.15 * k, 0.55 * k, 0.03 * k, 0.015), STEEL, z=0.03, bevel=0.03, base=0.18)
    c.add(c.circle(cx + 0.30 * k, cy - 0.08 * k, 0.03 * k), IRON_DARK, z=0.01, bevel=0.02, base=0.19)


def hammer(c, cx, cy, ang, k=1.0, head=STEEL):
    f = Xf(cx, cy, ang, k)
    c.add(c.seg(*f.p(0, 0), *f.p(0.85, 0), 0.05 * k), WOOD, z=0.06, bevel=0.05, base=0.20, shadow=0.55)
    c.add(c.box(*f.p(0.0, 0.0), 0.16 * k, 0.11 * k, 0.03, rot=ang), head, z=0.09, bevel=0.06, base=0.24, shadow=0.5)
    c.add(c.box(*f.p(0.0, 0.0), 0.02 * k, 0.11 * k, 0.01, rot=ang), IRON_DARK, z=0.02, bevel=0.01, base=0.33)


def m_workshop(c):
    c.glow(0.0, 0.0, 0.6, (120, 48, 10), 0.5)
    anvil(c, -0.06, 0.24, 0.95)
    hammer(c, 0.56, -0.50, deg(146), 0.95)
    sparks(c, ((-0.26, -0.10, 0.03), (0.02, -0.18, 0.025), (0.16, -0.06, 0.03), (-0.44, -0.20, 0.02), (0.30, -0.22, 0.02)))


def m_forge(c):
    c.glow(0.0, 0.0, 0.6, (140, 50, 10), 0.6)
    anvil(c, 0.0, 0.30, 1.0)
    sword(c, -0.36, -0.06, deg(-8), 0.95, 0.85, blade=STEEL)
    sparks(c, ((-0.2, -0.22, 0.03), (0.2, -0.20, 0.025), (0.06, -0.32, 0.02)))


def m_library(c):
    cover = c.poly([(-0.68, -0.20), (0.0, -0.08), (0.68, -0.20), (0.68, 0.46), (0.0, 0.34), (-0.68, 0.46)])
    c.add(cover, LEATHER, z=0.06, bevel=0.05, base=0.06, shadow=0.6)
    left = c.poly([(-0.60, -0.16), (-0.02, -0.05), (-0.02, 0.38), (-0.60, 0.40)])
    right = c.poly([(0.60, -0.16), (0.02, -0.05), (0.02, 0.38), (0.60, 0.40)])
    c.add(left, PARCHMENT, z=0.07, bevel=0.06, base=0.12, shadow=0.35)
    c.add(right, PARCHMENT, z=0.07, bevel=0.06, base=0.12, shadow=0.35)
    c.add(c.seg(0.0, -0.06, 0.0, 0.36, 0.012), IRON_DARK, z=0.02, bevel=0.012, base=0.19)
    for i in range(5):
        y0 = -0.06 + i * 0.09
        c.paint(c.seg(-0.52, y0 - 0.02, -0.12, y0 + 0.03, 0.008), (86, 50, 30), 0.85)
        c.paint(c.seg(0.12, y0 + 0.03, 0.52, y0 - 0.02, 0.008), (86, 50, 30), 0.85)
    c.add(c.circle(0.0, -0.46, 0.15), EMBER_HOT, z=0.04, bevel=0.06, base=0.25)
    c.glow(0.0, -0.46, 0.34, (255, 140, 30), 0.9)
    for a in range(6):
        c.add(c.seg(0.0, -0.46, 0.20 * np.cos(deg(60 * a + 20)), -0.46 + 0.20 * np.sin(deg(60 * a + 20)), 0.012), GOLD, z=0.02, bevel=0.01, base=0.28)


def m_hatchery(c):
    c.add(c.ellipse(0.0, 0.50, 0.55, 0.14), LEATHER, z=0.05, bevel=0.10, base=0.06, shadow=0.5)
    for i in range(9):
        a = deg(-160 + i * 40)
        c.add(c.seg(0.0, 0.52, 0.42 * np.cos(a), 0.52 + 0.20 * np.sin(a), 0.02), YELLOW, z=0.02, bevel=0.02, base=0.08)
    c.add(c.circle(0.0, -0.08, 0.22), YELLOW, z=0.16, bevel=0.20, base=0.10)
    c.add(c.poly([(-0.06, -0.02), (0.06, -0.02), (0.0, 0.08)]), EMBER, z=0.03, bevel=0.02, base=0.24)
    for sx in (-1, 1):
        c.add(c.circle(sx * 0.09, -0.12, 0.028), CONTOUR, z=0.02, bevel=0.02, base=0.24)
        c.add(c.circle(sx * 0.09 - 0.008, -0.13, 0.008), BONE, z=0.01, bevel=0.01, base=0.27)
    egg = c.ellipse(0.0, 0.16, 0.34, 0.42)
    zig = c.poly([(-0.6, -0.02), (-0.30, -0.04), (-0.22, 0.10), (-0.10, -0.02), (0.02, 0.12), (0.14, -0.02), (0.24, 0.10), (0.32, -0.04), (0.6, -0.02), (0.6, 0.9), (-0.6, 0.9)])
    c.add(c.intersect(egg, zig), BONE, z=0.18, bevel=0.20, base=0.10, shadow=0.6)
    for x, y, r in ((-0.14, 0.32, 0.03), (0.16, 0.24, 0.025), (0.06, 0.45, 0.025), (-0.20, 0.16, 0.02)):
        c.paint(c.circle(x, y, r), (160, 120, 80), 0.9)
    c.add(c.poly([(-0.42, -0.16), (-0.24, -0.22), (-0.30, -0.02)]), BONE, z=0.04, bevel=0.03, base=0.14, shadow=0.4)
    c.add(c.poly([(0.42, -0.14), (0.26, -0.24), (0.30, -0.02)]), BONE, z=0.04, bevel=0.03, base=0.14, shadow=0.4)


def skull(c, cx, cy, s, mat=BONE, base=0.2, eye=CONTOUR):
    c.add(c.smooth_union(c.ellipse(cx, cy - 0.02 * s, 0.30 * s, 0.27 * s), c.box(cx, cy + 0.24 * s, 0.16 * s, 0.10 * s, 0.04 * s), 0.08 * s),
          mat, z=0.10, bevel=0.10 * s, base=base, shadow=0.5)
    for sx in (-1, 1):
        c.add(c.ellipse(cx + sx * 0.12 * s, cy + 0.0, 0.085 * s, 0.10 * s), eye, z=0.02, bevel=0.04 * s, base=base + 0.1)
    c.add(c.poly([(cx - 0.035 * s, cy + 0.12 * s), (cx + 0.035 * s, cy + 0.12 * s), (cx, cy + 0.06 * s)]), eye, z=0.02, bevel=0.02, base=base + 0.1)
    for i in range(4):
        x = cx + (i - 1.5) * 0.07 * s
        c.paint(c.seg(x, cy + 0.20 * s, x, cy + 0.30 * s, 0.010 * s), (70, 50, 36), 0.9)


def m_crypt(c):
    c.glow(0.0, 0.1, 0.7, (110, 40, 10), 0.6)
    stone = c.union(c.box(0.0, 0.22, 0.42, 0.44), c.circle(0.0, -0.20, 0.42))
    c.add(c.union(stone, c.box(0.0, 0.62, 0.55, 0.06, 0.02)), STONE, z=0.16, bevel=0.06, base=0.05, shadow=0.6)
    c.add(stone + 0.08, STONE_DARK, z=0.02, bevel=0.03, base=0.20)
    skull(c, 0.0, -0.10, 0.85, base=0.24)
    c.add(c.seg(-0.30, 0.16, 0.30, 0.44, 0.040), BONE, z=0.05, bevel=0.04, base=0.24, shadow=0.4, sh_dx=0.03, sh_dy=0.03)
    c.add(c.seg(0.30, 0.16, -0.30, 0.44, 0.040), BONE, z=0.05, bevel=0.04, base=0.25, shadow=0.4, sh_dx=0.03, sh_dy=0.03)
    for sx in (-1, 1):
        for sy in (0.16, 0.44):
            c.add(c.circle(sx * 0.31, sy, 0.05), BONE, z=0.05, bevel=0.05, base=0.24)


def m_destroy_room(c):
    c.glow(0.0, 0.2, 0.6, (140, 50, 10), 0.6)
    blocks = ((-0.52, 0.30, 0.28, 0.20, -6), (-0.05, 0.36, 0.30, 0.18, 4), (0.42, 0.30, 0.26, 0.20, 12), (-0.28, 0.06, 0.24, 0.16, -14), (0.20, 0.10, 0.22, 0.15, 10))
    for x, y, hw, hh, r in blocks:
        c.add(c.box(x, y, hw, hh, 0.04, deg(r)), STONE, z=0.10, bevel=0.06, base=0.06, shadow=0.55)
    c.add(c.seg(-0.02, 0.0, 0.05, 0.40, 0.014), CONTOUR, z=0.0, bevel=0.01, base=0.2)
    c.glow(0.03, 0.20, 0.16, (255, 120, 20), 0.8)
    hammer(c, 0.66, -0.52, deg(140), 1.0, head=IRON)
    sparks(c, ((0.12, 0.02, 0.03), (-0.14, -0.10, 0.02), (0.30, -0.06, 0.025), (-0.34, 0.0, 0.02)))


def m_destroy_trap(c):
    c.glow(0.0, 0.2, 0.6, (140, 50, 10), 0.6)
    c.add(c.box(0.0, 0.38, 0.62, 0.10, 0.03), IRON, z=0.08, bevel=0.05, base=0.06, shadow=0.6)
    for i, x in enumerate((-0.44, -0.16, 0.12, 0.40)):
        c.add(c.poly([(x - 0.09, 0.32), (x + 0.09, 0.32), (x + (0.04 if i % 2 else -0.02), -0.08)]), STEEL, z=0.06, bevel=0.06, base=0.14, shadow=0.4)
    c.add(c.seg(0.10, 0.20, 0.26, 0.36, 0.015), CONTOUR, z=0.0, bevel=0.01, base=0.2)
    hammer(c, 0.66, -0.52, deg(140), 1.0, head=IRON)
    sparks(c, ((0.20, 0.10, 0.03), (-0.06, -0.14, 0.02), (0.34, -0.06, 0.025)))


def m_prison(c):
    c.glow(0.0, 0.0, 0.5, (110, 44, 12), 0.5)
    frame = arch_sdf(c, 0.0, -0.62, 0.55, 0.52)
    c.add(frame, STONE, z=0.10, bevel=0.06, base=0.05, shadow=0.6)
    c.fill_well(frame + 0.10, (60, 26, 12), (12, 6, 4), r=0.6, z=0.02)
    for sx in (-1, 1):
        c.add(c.ellipse(sx * 0.13, -0.05, 0.055, 0.035), EMBER_HOT, z=0.02, bevel=0.03, base=0.16)
        c.glow(sx * 0.13, -0.05, 0.10, (255, 140, 30), 0.8)
    for x in (-0.33, -0.11, 0.11, 0.33):
        c.add(c.seg(x, -0.62, x, 0.58, 0.036), STEEL, z=0.06, bevel=0.036, base=0.22, shadow=0.55, sh_dx=0.03, sh_dy=0.02)
    for y in (-0.22, 0.30):
        c.add(c.box(0.0, y, 0.50, 0.032, 0.015), IRON, z=0.05, bevel=0.03, base=0.27, shadow=0.5, sh_dx=0.02, sh_dy=0.03)
    c.add(c.box(0.0, 0.58, 0.56, 0.05, 0.02), IRON, z=0.06, bevel=0.03, base=0.20)


def m_torture(c):
    c.glow(0.0, 0.0, 0.7, (140, 30, 10), 0.5)
    c.add(c.ring(0.0, 0.0, 0.46, 0.05), WOOD_DARK, z=0.09, bevel=0.05, base=0.10, shadow=0.6)
    for k in range(8):
        a = deg(22.5 + 45 * k)
        c.add(c.seg(0.0, 0.0, 0.46 * np.cos(a), 0.46 * np.sin(a), 0.028), WOOD_DARK, z=0.05, bevel=0.028, base=0.12)
    for k in range(16):
        a = deg(11.25 + 22.5 * k)
        f = Xf(0.51 * np.cos(a), 0.51 * np.sin(a), a)
        c.add(c.poly(f.pts([(-0.02, -0.035), (0.13, 0.0), (-0.02, 0.035)])), STEEL, z=0.03, bevel=0.03, base=0.16, shadow=0.3, sh_dx=0.02, sh_dy=0.02)
    c.add(c.circle(0.0, 0.0, 0.10), IRON, z=0.07, bevel=0.07, base=0.16, shadow=0.4)
    c.add(c.circle(0.0, 0.0, 0.035), GOLD, z=0.03, bevel=0.035, base=0.24)
    for a0 in (deg(20), deg(200)):
        c.add(c.arc(0.0, 0.0, 0.46, 0.028, a0, a0 + deg(40)), BLOOD, z=0.02, bevel=0.02, base=0.20)


def m_arena(c):
    c.glow(0.0, 0.1, 0.7, (120, 44, 10), 0.5)
    c.add(c.box(0.0, 0.14, 0.64, 0.40, 0.03), STONE, z=0.12, bevel=0.06, base=0.05, shadow=0.6)
    for i in range(4):
        x = -0.45 + i * 0.30
        arch = arch_sdf(c, x, -0.06, 0.40, 0.11)
        c.add(arch, CONTOUR, z=0.0, bevel=0.02, base=0.10)
        c.add(arch + 0.02, STONE_DARK, z=0.0, bevel=0.02, base=0.09)
        c.glow(x, 0.22, 0.14, (255, 120, 24), 0.5)
    c.add(c.box(0.0, -0.30, 0.68, 0.045, 0.02), STONE, z=0.10, bevel=0.04, base=0.10, shadow=0.5)
    c.add(c.box(0.0, 0.58, 0.72, 0.06, 0.02), STONE, z=0.10, bevel=0.04, base=0.08, shadow=0.5)
    sword(c, -0.30, -0.02, deg(-50), 0.85, 0.75, base=0.30)
    sword(c, 0.30, -0.02, deg(-130), 0.85, 0.75, base=0.30)


def m_casino(c):
    c.add(c.box(-0.16, -0.02, 0.28, 0.40, 0.04, deg(-16)), PARCHMENT, z=0.05, bevel=0.04, base=0.10, shadow=0.6)
    f = Xf(-0.16, -0.02, deg(-16))
    c.add(c.poly(f.pts([(0.0, -0.17), (0.11, 0.0), (0.0, 0.05), (-0.11, 0.0)])), CONTOUR, z=0.01, bevel=0.01, base=0.16)
    c.add(c.circle(*f.p(-0.045, 0.02), 0.06), CONTOUR, z=0.01, bevel=0.01, base=0.16)
    c.add(c.circle(*f.p(0.045, 0.02), 0.06), CONTOUR, z=0.01, bevel=0.01, base=0.16)
    c.add(c.seg(*f.p(0, 0.06), *f.p(0, 0.20), 0.02), CONTOUR, z=0.01, bevel=0.01, base=0.16)
    c.add(c.box(0.30, 0.22, 0.27, 0.27, 0.07, deg(14)), BONE, z=0.10, bevel=0.07, base=0.12, shadow=0.6)
    g = Xf(0.30, 0.22, deg(14))
    for px, py in ((-0.13, -0.13), (0.13, -0.13), (0.0, 0.0), (-0.13, 0.13), (0.13, 0.13)):
        c.add(c.circle(*g.p(px, py), 0.045), CLOTH_RED, z=0.02, bevel=0.03, base=0.20)
    c.add(c.ellipse(-0.42, 0.50, 0.15, 0.06), GOLD, z=0.04, bevel=0.05, base=0.12, shadow=0.5)
    c.add(c.ellipse(-0.42, 0.45, 0.15, 0.06), GOLD_DARK, z=0.04, bevel=0.05, base=0.16)


def m_temple(c):
    c.glow(0.0, 0.14, 0.5, (150, 56, 10), 0.8)
    c.add(c.poly([(-0.66, -0.16), (0.0, -0.58), (0.66, -0.16)]), STONE, z=0.10, bevel=0.05, base=0.10, shadow=0.55)
    c.add(c.box(0.0, -0.16, 0.66, 0.04, 0.015), STONE_DARK, z=0.06, bevel=0.03, base=0.14)
    c.add(c.poly([(0.0, -0.46), (0.28, -0.20), (-0.28, -0.20)]), STONE_DARK, z=0.02, bevel=0.03, base=0.15)
    for x in (-0.50, -0.17, 0.17, 0.50):
        c.add(c.box(x, 0.18, 0.055, 0.31), BONE, z=0.10, bevel=0.05, base=0.08, shadow=0.5, sh_dx=0.03)
        c.add(c.box(x, -0.10, 0.09, 0.03, 0.01), STONE, z=0.05, bevel=0.03, base=0.12)
    c.add(c.box(0.0, 0.52, 0.66, 0.05, 0.015), STONE, z=0.09, bevel=0.04, base=0.06, shadow=0.4)
    c.add(c.box(0.0, 0.62, 0.72, 0.05, 0.015), STONE, z=0.09, bevel=0.04, base=0.04)
    c.add(flame_sdf(c, 0.0, 0.26, 1.15), EMBER, z=0.08, bevel=0.10, base=0.12)
    c.add(flame_sdf(c, 0.0, 0.32, 0.55), EMBER_HOT, z=0.05, bevel=0.06, base=0.22)


def swirl(c, cx, cy, r, mat_a, mat_b, turns=2.4, width=0.07):
    t = np.linspace(0, 1, 46)
    for m, off in ((mat_a, 0.0), (mat_b, 2.0 * np.pi / 3.0), (mat_a, 4.0 * np.pi / 3.0)):
        pts = [(cx + r * (0.12 + 0.88 * tt) * np.cos(off + turns * 2 * np.pi * tt), cy + r * (0.12 + 0.88 * tt) * np.sin(off + turns * 2 * np.pi * tt)) for tt in t]
        shape = c.seg(*pts[0], *pts[1], width)
        for i in range(1, len(pts) - 1):
            shape = np.minimum(shape, c.seg(*pts[i], *pts[i + 1], width * (0.5 + 0.9 * (1 - i / len(pts)))))
        c.add(shape, m, z=0.05, bevel=0.05, base=0.14)


def m_portal(c):
    frame = arch_sdf(c, 0.0, -0.66, 0.60, 0.50)
    c.add(frame, STONE, z=0.10, bevel=0.06, base=0.05, shadow=0.6)
    c.fill_well(frame + 0.10, (150, 56, 14), (26, 10, 6), r=0.5, z=0.02)
    c.glow(0.0, -0.02, 0.44, (255, 120, 24), 0.9)
    swirl(c, 0.0, -0.04, 0.44, EMBER_HOT, EMBER)
    c.add(c.circle(0.0, -0.04, 0.05), EMBER_HOT, z=0.02, bevel=0.04, base=0.24)
    c.add(c.box(0.0, 0.62, 0.62, 0.05, 0.02), STONE, z=0.09, bevel=0.04, base=0.04, shadow=0.4)


def m_wave_portal(c):
    c.add(c.circle(0.0, 0.0, 0.58), IRON, z=0.12, bevel=0.08, base=0.06, shadow=0.6)
    c.fill_well(c.circle(0.0, 0.0, 0.44), (180, 40, 22), (30, 8, 6), r=0.44, z=0.06)
    c.glow(0.0, 0.0, 0.42, (255, 80, 24), 0.9)
    swirl(c, 0.0, 0.0, 0.42, EMBER_HOT, RUBY, turns=2.0, width=0.06)
    for k in range(8):
        a = deg(22.5 + 45 * k)
        f = Xf(0.58 * np.cos(a), 0.58 * np.sin(a), a)
        c.add(c.poly(f.pts([(-0.02, -0.05), (0.14, 0.0), (-0.02, 0.05)])), STEEL, z=0.05, bevel=0.04, base=0.16, shadow=0.3)


def m_bridge_wood(c):
    c.glow(0.0, 0.5, 0.7, (170, 60, 10), 0.8)
    c.add(c.box(0.0, 0.62, 0.8, 0.10), EMBER, z=0.02, bevel=0.02, base=0.02)
    c.add(c.box(0.0, 0.12, 0.72, 0.09, 0.02), WOOD, z=0.07, bevel=0.05, base=0.10, shadow=0.6)
    for i in range(9):
        x = -0.60 + i * 0.15
        c.paint(c.seg(x, 0.04, x, 0.20, 0.007), (60, 34, 18), 0.9)
    for x in (-0.62, 0.0, 0.62):
        c.add(c.box(x, -0.10, 0.045, 0.30, 0.02), WOOD_DARK, z=0.08, bevel=0.04, base=0.12, shadow=0.5)
        c.add(c.box(x, 0.40, 0.055, 0.14, 0.02), WOOD_DARK, z=0.08, bevel=0.04, base=0.06)
    c.add(c.seg(-0.62, -0.30, 0.0, -0.20, 0.022), LEATHER, z=0.03, bevel=0.022, base=0.18, shadow=0.4)
    c.add(c.seg(0.0, -0.20, 0.62, -0.30, 0.022), LEATHER, z=0.03, bevel=0.022, base=0.18, shadow=0.4)


def m_bridge_stone(c):
    c.glow(0.0, 0.5, 0.7, (170, 60, 10), 0.8)
    hole = c.union(c.box(0.0, 0.34, 0.30, 0.20), c.circle(0.0, 0.14, 0.30))
    c.add(c.subtract(c.box(0.0, 0.10, 0.76, 0.34, 0.03), hole), STONE, z=0.12, bevel=0.06, base=0.06, shadow=0.6)
    c.add(c.box(0.0, -0.28, 0.80, 0.06, 0.02), STONE_DARK, z=0.08, bevel=0.04, base=0.16, shadow=0.5)
    for x in (-0.60, -0.40, 0.40, 0.60):
        c.add(c.box(x, -0.14, 0.055, 0.10, 0.015), STONE_DARK, z=0.04, bevel=0.03, base=0.18)
    c.paint(c.seg(-0.72, 0.04, -0.40, 0.04, 0.006), (60, 46, 36), 0.7)
    c.paint(c.seg(0.40, 0.14, 0.72, 0.14, 0.006), (60, 46, 36), 0.7)


# ---------------------------------------------------------------------------------------------
# Traps
# ---------------------------------------------------------------------------------------------
def m_cannon(c):
    c.glow(0.5, -0.36, 0.3, (255, 130, 30), 0.7)
    f = Xf(-0.28, 0.10, deg(-22))
    c.add(c.seg(*f.p(0.0, 0), *f.p(0.92, 0), 0.19, 0.145), BRONZE, z=0.14, bevel=0.14, base=0.10, shadow=0.6)
    for x in (0.26, 0.66):
        c.add(c.seg(*f.p(x, 0), *f.p(x + 0.03, 0), 0.20 - x * 0.06), GOLD, z=0.05, bevel=0.03, base=0.20, shadow=0.3)
    c.add(c.seg(*f.p(0.92, 0), *f.p(0.95, 0), 0.165), GOLD_DARK, z=0.06, bevel=0.04, base=0.20)
    c.add(c.circle(*f.p(0.96, 0), 0.075), CONTOUR, z=0.0, bevel=0.02, base=0.22)
    c.add(c.circle(*f.p(-0.04, 0), 0.14), BRONZE_DARK, z=0.1, bevel=0.1, base=0.10)
    c.add(c.box(-0.22, 0.36, 0.30, 0.055, 0.02), WOOD_DARK, z=0.06, bevel=0.04, base=0.06, shadow=0.5)
    c.add(c.circle(-0.14, 0.36, 0.25), WOOD, z=0.10, bevel=0.06, base=0.12, shadow=0.6)
    c.add(c.ring(-0.14, 0.36, 0.22, 0.03), IRON, z=0.05, bevel=0.03, base=0.20)
    for k in range(8):
        a = deg(45 * k + 8)
        c.add(c.seg(-0.14, 0.36, -0.14 + 0.22 * np.cos(a), 0.36 + 0.22 * np.sin(a), 0.016), WOOD_DARK, z=0.03, bevel=0.016, base=0.22)
    c.add(c.circle(-0.14, 0.36, 0.05), GOLD, z=0.04, bevel=0.05, base=0.24)
    for x, y in ((0.36, 0.56), (0.55, 0.56), (0.46, 0.44)):
        c.add(c.circle(x, y, 0.085), IRON_DARK, z=0.09, bevel=0.09, base=0.08, shadow=0.5)


def m_spike_trap(c):
    c.glow(0.0, 0.2, 0.6, (140, 54, 14), 0.4)
    c.add(c.box(0.0, 0.40, 0.66, 0.12, 0.03), IRON, z=0.08, bevel=0.05, base=0.06, shadow=0.6)
    for x in (-0.55, 0.55):
        c.add(c.circle(x, 0.40, 0.032), GOLD_DARK, z=0.03, bevel=0.03, base=0.14)
    for i, x in enumerate((-0.48, -0.24, 0.0, 0.24, 0.48)):
        h = -0.42 if i % 2 == 0 else -0.30
        c.add(c.poly([(x - 0.10, 0.30), (x + 0.10, 0.30), (x, h)]), STEEL, z=0.08, bevel=0.09, base=0.10, shadow=0.45, sh_dx=0.04, sh_dy=0.03)
        c.add(c.poly([(x - 0.02, h + 0.20), (x + 0.02, h + 0.20), (x, h)]), BLOOD, z=0.01, bevel=0.01, base=0.25)


def m_boulder(c):
    for y0, x1, y1 in ((-0.36, -0.20, -0.20), (-0.06, -0.30, -0.02), (0.22, -0.28, 0.18)):
        c.add(c.seg(-0.62, y0, x1, y1, 0.03), GOLD_DARK, z=0.02, bevel=0.03, base=0.04)
    c.add(c.circle(0.10, 0.02, 0.50), STONE, z=0.30, bevel=0.50, base=0.06, shadow=0.6)
    c.carve(c.union(c.seg(0.0, -0.34, 0.14, -0.10, 0.012), c.seg(0.14, -0.10, 0.02, 0.10, 0.012), c.seg(0.02, 0.10, 0.20, 0.30, 0.012)), 0.05, 0.012, colour=(30, 20, 14))
    c.carve(c.union(c.seg(0.14, -0.10, 0.36, -0.06, 0.010), c.seg(0.20, 0.30, 0.34, 0.24, 0.010)), 0.04, 0.010, colour=(30, 20, 14))
    for x, y, r in ((0.44, 0.44, 0.06), (0.58, 0.32, 0.04), (0.30, 0.52, 0.045)):
        c.add(c.circle(x, y, r), STONE_DARK, z=0.05, bevel=r, base=0.06, shadow=0.4)


def m_door(c):
    door = arch_sdf(c, 0.0, -0.62, 0.58, 0.46)
    c.add(door - 0.07, STONE, z=0.08, bevel=0.05, base=0.04, shadow=0.6)
    c.add(door, WOOD, z=0.08, bevel=0.06, base=0.10)
    for x in (-0.15, 0.15):
        c.paint(c.seg(x, -0.6, x, 0.58, 0.008), (50, 30, 16), 0.9)
    for y in (-0.30, 0.20):
        c.add(c.box(0.0, y, 0.46, 0.05, 0.02), IRON, z=0.05, bevel=0.03, base=0.20, shadow=0.4)
        for x in (-0.34, -0.12, 0.12, 0.34):
            c.add(c.circle(x, y, 0.022), STEEL, z=0.03, bevel=0.022, base=0.24)
    c.add(c.ring(0.24, 0.02, 0.07, 0.016), GOLD, z=0.04, bevel=0.016, base=0.22, shadow=0.4)
    c.add(c.circle(0.24, -0.06, 0.03), GOLD, z=0.03, bevel=0.03, base=0.22)


# ---------------------------------------------------------------------------------------------
# Spells
# ---------------------------------------------------------------------------------------------
def m_worker_imp(c):
    c.add(c.ring(0, 0.0, 0.62, 0.018), GOLD_DARK, z=0.03, bevel=0.018, base=0.05)
    for k in range(12):
        a = deg(30 * k)
        c.add(c.seg(0.56 * np.cos(a), 0.56 * np.sin(a), 0.68 * np.cos(a), 0.68 * np.sin(a), 0.012), GOLD_DARK, z=0.03, bevel=0.012, base=0.05)
    c.glow(0, 0.1, 0.6, (150, 60, 14), 0.6)
    for sx in (-1, 1):
        c.add(c.seg(sx * 0.20, -0.16, sx * 0.40, -0.52, 0.075, 0.018), BONE, z=0.10, bevel=0.07, base=0.10, shadow=0.5)
    head = c.smooth_union(c.ellipse(0, 0.02, 0.34, 0.31), c.ellipse(0, 0.26, 0.20, 0.17), 0.10)
    ears = c.union(c.poly([(-0.28, -0.02), (-0.62, -0.20), (-0.34, 0.16)]), c.poly([(0.28, -0.02), (0.62, -0.20), (0.34, 0.16)]))
    c.add(ears, WORKER, z=0.06, bevel=0.06, base=0.08, shadow=0.5)
    c.add(head, WORKER, z=0.16, bevel=0.20, base=0.10, shadow=0.6)
    for sx in (-1, 1):
        c.add(c.ellipse(sx * 0.15, 0.0, 0.085, 0.06, deg(sx * 18)), CONTOUR, z=0.02, bevel=0.03, base=0.24)
        c.add(c.ellipse(sx * 0.15, 0.0, 0.06, 0.04, deg(sx * 18)), EMBER_HOT, z=0.02, bevel=0.03, base=0.25)
        c.add(c.seg(sx * 0.05, -0.04, sx * 0.26, -0.15, 0.02), CONTOUR, z=0.02, bevel=0.02, base=0.26)
    c.add(c.ellipse(0, 0.30, 0.12, 0.045), CONTOUR, z=0.02, bevel=0.03, base=0.24)
    for sx in (-1, 1):
        c.add(c.poly([(sx * 0.09, 0.28), (sx * 0.035, 0.28), (sx * 0.06, 0.35)]), BONE, z=0.02, bevel=0.01, base=0.26)
    c.add(c.ellipse(0, 0.14, 0.03, 0.02), CONTOUR, z=0.02, bevel=0.02, base=0.25)


def m_call_to_war(c):
    c.glow(0.0, -0.1, 0.6, (130, 40, 12), 0.5)
    c.add(c.seg(-0.40, 0.62, -0.40, -0.62, 0.035), GOLD, z=0.06, bevel=0.035, base=0.12, shadow=0.5)
    c.add(c.circle(-0.40, -0.64, 0.06), GOLD, z=0.05, bevel=0.06, base=0.14)
    flag = c.poly([(-0.37, -0.52), (-0.10, -0.60), (0.20, -0.52), (0.50, -0.58), (0.44, -0.14), (0.50, 0.22), (0.20, 0.28), (-0.10, 0.20), (-0.37, 0.28)])
    c.add(flag, CLOTH_RED, z=0.08, bevel=0.07, base=0.10, shadow=0.6)
    c.add(c.ring(0.07, -0.16, 0.20, 0.022), GOLD, z=0.03, bevel=0.022, base=0.20)
    sword(c, 0.07, 0.02, deg(-90), 0.36, 0.55, base=0.24, shadow=0.0)
    for x in (-0.20, -0.02, 0.16, 0.34):
        c.add(c.poly([(x - 0.05, 0.62), (x + 0.05, 0.62), (x, 0.36)]), STEEL, z=0.04, bevel=0.04, base=0.10, shadow=0.4)
        c.add(c.seg(x, 0.62, x, 0.50, 0.012), WOOD, z=0.02, bevel=0.012, base=0.10)


def m_heal(c):
    c.glow(0.0, 0.0, 0.7, (200, 40, 30), 0.7)
    c.add(heart_sdf(c, 0.0, -0.02, 1.35), RUBY, z=0.22, bevel=0.30, base=0.08, shadow=0.6)
    c.add(c.ellipse(-0.24, -0.26, 0.11, 0.06, deg(-40)), BONE, z=0.02, bevel=0.03, base=0.30)
    c.add(c.box(0.0, 0.02, 0.26, 0.065, 0.02), GOLD, z=0.05, bevel=0.03, base=0.30, shadow=0.4)
    c.add(c.box(0.0, 0.02, 0.065, 0.26, 0.02), GOLD, z=0.05, bevel=0.03, base=0.30, shadow=0.4)
    for x, y, r in ((0.56, -0.44, 0.03), (-0.56, -0.40, 0.025), (0.50, 0.30, 0.02)):
        c.add(c.circle(x, y, r), GOLD, z=0.02, bevel=r, base=0.2)
        c.glow(x, y, r * 4, (255, 190, 60), 0.5)


def m_explosion(c):
    c.glow(0.0, 0.0, 0.8, (255, 100, 20), 0.6)
    c.add(c.poly(star(12, 0.66, 0.36, 0.0, 0.0, deg(8))), EMBER, z=0.10, bevel=0.10, base=0.06, shadow=0.4)
    c.add(c.poly(star(9, 0.44, 0.26, 0.0, 0.0, deg(-10))), EMBER_HOT, z=0.10, bevel=0.10, base=0.16)
    c.add(c.circle(0.0, 0.0, 0.20), EMBER_HOT, z=0.10, bevel=0.20, base=0.26)
    c.glow(0.0, 0.0, 0.3, (255, 200, 120), 0.35)
    for k in range(9):
        a = deg(20 + 40 * k)
        c.add(c.circle(0.72 * np.cos(a), 0.72 * np.sin(a), 0.03), EMBER_HOT, z=0.02, bevel=0.03, base=0.1)


def m_haste(c):
    c.glow(0.0, 0.0, 0.6, (200, 130, 20), 0.6)
    bolt = c.poly([(0.06, -0.66), (-0.36, 0.06), (-0.04, 0.06), (-0.16, 0.66), (0.40, -0.16), (0.06, -0.16), (0.30, -0.66)])
    c.add(bolt, GOLD, z=0.10, bevel=0.06, base=0.10, shadow=0.6)
    for y, x0, x1 in ((-0.34, -0.66, -0.36), (-0.02, -0.62, -0.44), (0.30, -0.66, -0.30)):
        c.add(c.seg(x0, y, x1, y, 0.028), EMBER, z=0.03, bevel=0.028, base=0.08)


def m_defense(c):
    outline = [(-0.46, -0.52), (0.46, -0.52), (0.46, 0.0), (0.38, 0.28), (0.20, 0.52), (0.0, 0.64), (-0.20, 0.52), (-0.38, 0.28), (-0.46, 0.0)]
    shield = c.poly(outline)
    face = shield + 0.08
    c.add(shield, BRONZE, z=0.10, bevel=0.06, base=0.06, shadow=0.6)
    c.add(face, STEEL, z=0.06, bevel=0.05, base=0.14)
    c.add(c.intersect(face, c.box(0.25, 0.0, 0.25, 1.0)), IRON, z=0.02, bevel=0.03, base=0.2)
    c.add(c.seg(0.0, -0.44, 0.0, 0.52, 0.022), GOLD, z=0.04, bevel=0.022, base=0.24, shadow=0.3)
    c.add(c.seg(-0.38, -0.10, 0.38, -0.10, 0.022), GOLD, z=0.04, bevel=0.022, base=0.24, shadow=0.3)
    c.add(c.circle(0.0, -0.10, 0.12), GOLD, z=0.10, bevel=0.12, base=0.26, shadow=0.4)
    c.add(c.circle(0.0, -0.10, 0.045), BRONZE, z=0.04, bevel=0.045, base=0.38)
    for x, y in ((-0.34, -0.42), (0.34, -0.42), (-0.30, 0.14), (0.30, 0.14)):
        c.add(c.circle(x, y, 0.030), GOLD, z=0.03, bevel=0.03, base=0.24)


def m_slow(c):
    body = c.smooth_union(c.ellipse(0.10, 0.36, 0.58, 0.14), c.seg(0.44, 0.34, 0.60, 0.04, 0.10, 0.07), 0.08)
    c.add(body, FLESH, z=0.10, bevel=0.12, base=0.06, shadow=0.55)
    for x, y in ((0.56, -0.14), (0.66, -0.06)):
        c.add(c.seg(0.58, 0.02, x, y, 0.022), FLESH, z=0.02, bevel=0.02, base=0.12)
        c.add(c.circle(x, y - 0.02, 0.04), FLESH, z=0.04, bevel=0.04, base=0.12)
        c.add(c.circle(x, y - 0.02, 0.014), CONTOUR, z=0.01, bevel=0.01, base=0.19)
    c.add(c.circle(-0.02, 0.06, 0.44), BRONZE, z=0.20, bevel=0.30, base=0.10, shadow=0.6)
    t = np.linspace(0, 1, 40)
    pts = [(-0.02 + (0.03 + 0.38 * tt) * np.cos(3.5 * np.pi * tt + 0.8), 0.06 + (0.03 + 0.38 * tt) * np.sin(3.5 * np.pi * tt + 0.8)) for tt in t]
    spiral = c.seg(*pts[0], *pts[1], 0.03)
    for i in range(1, len(pts) - 1):
        spiral = np.minimum(spiral, c.seg(*pts[i], *pts[i + 1], 0.012 + 0.03 * (i / len(pts))))
    c.carve(spiral, 0.05, 0.02, colour=(60, 34, 14))
    c.add(c.circle(-0.02, 0.06, 0.05), GOLD, z=0.05, bevel=0.05, base=0.30)


def m_strength(c):
    c.glow(0.0, -0.1, 0.7, (170, 70, 14), 0.6)
    c.add(c.box(0.0, 0.50, 0.24, 0.24, 0.05), STEEL, z=0.10, bevel=0.06, base=0.06, shadow=0.6)
    c.add(c.box(0.0, 0.30, 0.34, 0.07, 0.03), GOLD, z=0.08, bevel=0.05, base=0.16, shadow=0.5)
    fist = c.box(0.0, -0.10, 0.34, 0.30, 0.10)
    c.add(fist, SKIN, z=0.20, bevel=0.16, base=0.08, shadow=0.6)
    for k in range(4):
        x = -0.255 + k * 0.17
        c.add(c.circle(x, -0.36, 0.095), SKIN, z=0.12, bevel=0.09, base=0.20)
        c.paint(c.seg(x - 0.085, -0.13, x + 0.085, -0.13, 0.008), (120, 60, 34), 0.8)
    c.add(c.seg(-0.34, 0.10, -0.06, -0.02, 0.085), SKIN, z=0.12, bevel=0.08, base=0.26, shadow=0.4)
    c.paint(c.ellipse(-0.10, -0.20, 0.16, 0.06, deg(-8)), (255, 220, 180), 0.28)
    for a in (-30, 0, 30):
        f = Xf(0.0, -0.66, deg(a - 90))
        c.add(c.seg(*f.p(0.0, 0), *f.p(0.16, 0), 0.02), EMBER, z=0.02, bevel=0.02, base=0.06)


def m_weak(c):
    f = Xf(-0.30, 0.56, deg(-58))
    c.add(c.poly(f.pts([(0.10, -0.062), (0.62, -0.075), (0.72, -0.02), (0.66, 0.0), (0.60, 0.05), (0.10, 0.078)])), STEEL, z=0.05, bevel=0.06, base=0.12, shadow=0.5)
    c.add(c.seg(*f.p(0.09, -0.17), *f.p(0.09, 0.17), 0.033), GOLD, z=0.05, bevel=0.03, base=0.17, shadow=0.4)
    c.add(c.seg(*f.p(0.08, 0), *f.p(-0.15, 0), 0.032), LEATHER, z=0.05, bevel=0.03, base=0.16)
    c.add(c.circle(*f.p(-0.19, 0), 0.05), GOLD, z=0.05, bevel=0.05, base=0.16)
    g = Xf(0.24, -0.20, deg(-22))
    c.add(c.poly(g.pts([(0.0, -0.08), (0.44, -0.05), (0.52, 0.0), (0.36, 0.04), (0.0, 0.08)])), STEEL, z=0.04, bevel=0.05, base=0.12, shadow=0.5)
    for x, y, r in ((0.32, 0.30, 0.03), (0.46, 0.18, 0.02), (0.20, 0.52, 0.025)):
        c.add(c.poly(star(3, r * 2, r, x, y, deg(10))), STEEL, z=0.02, bevel=r, base=0.14)
    c.glow(0.10, 0.0, 0.3, (170, 40, 20), 0.5)


def m_eye(c):
    c.glow(0.0, 0.0, 0.8, (200, 70, 14), 0.9)
    lens = c.intersect(c.circle(0.0, 0.52, 0.86), c.circle(0.0, -0.52, 0.86))
    c.add(lens - 0.09, BRONZE, z=0.10, bevel=0.06, base=0.06, shadow=0.6)
    c.add(lens, BONE, z=0.06, bevel=0.12, base=0.12)
    c.add(c.circle(0.0, 0.0, 0.31), EMBER, z=0.10, bevel=0.20, base=0.14)
    c.add(c.circle(0.0, 0.0, 0.18), EMBER_HOT, z=0.04, bevel=0.10, base=0.22)
    c.add(c.ellipse(0.0, 0.0, 0.055, 0.22), CONTOUR, z=0.02, bevel=0.03, base=0.26)
    c.add(c.circle(-0.09, -0.10, 0.04), BONE, z=0.02, bevel=0.04, base=0.30)


# ---------------------------------------------------------------------------------------------
# Creatures and small symbols
# ---------------------------------------------------------------------------------------------
def pickaxe(c, cx, cy, ang, k=1.0, base=0.14):
    f = Xf(cx, cy, ang, k)
    c.add(c.seg(*f.p(-0.10, 0), *f.p(0.95, 0), 0.06 * k), WOOD, z=0.06, bevel=0.045, base=base, shadow=0.5)
    head = c.poly(f.pts([(0.82, -0.05), (0.90, -0.30), (1.06, -0.40), (0.98, -0.12), (1.0, 0.12), (1.06, 0.40), (0.90, 0.30), (0.82, 0.05)]))
    c.add(head, STEEL, z=0.08, bevel=0.06 * k, base=base + 0.06, shadow=0.5)


def shovel(c, cx, cy, ang, k=1.0, base=0.12):
    f = Xf(cx, cy, ang, k)
    c.add(c.seg(*f.p(-0.10, 0), *f.p(0.70, 0), 0.055 * k), WOOD, z=0.06, bevel=0.04, base=base, shadow=0.5)
    c.add(c.seg(*f.p(-0.10, -0.06), *f.p(-0.10, 0.06), 0.03 * k), IRON, z=0.04, bevel=0.03, base=base + 0.04)
    blade = c.smooth_union(c.poly(f.pts([(0.62, -0.14), (0.66, 0.14), (0.92, 0.10), (1.02, 0.0), (0.92, -0.10)])), c.circle(*f.p(0.86, 0), 0.14 * k), 0.05)
    c.add(blade, BRONZE, z=0.07, bevel=0.06, base=base + 0.04, shadow=0.5)


def m_worker(c):
    c.glow(0.0, 0.0, 0.6, (110, 48, 12), 0.5)
    shovel(c, -0.62, 0.66, deg(-48), 1.02)
    pickaxe(c, 0.62, 0.66, deg(-132), 1.02)
    c.add(c.circle(0.0, 0.0, 0.05), GOLD, z=0.04, bevel=0.05, base=0.32)


def m_fighter(c):
    c.glow(0.0, 0.0, 0.6, (110, 48, 12), 0.5)
    sword(c, -0.62, 0.66, deg(-46), 1.42, 0.95, wid=1.35)
    sword(c, 0.62, 0.66, deg(-134), 1.42, 0.95, wid=1.35)


# ---------------------------------------------------------------------------------------------
# Small symbols and navigation emblems
# ---------------------------------------------------------------------------------------------
def m_gold_coin(c):
    c.add(c.circle(0, 0, 0.84), GOLD_DARK, z=0.12, bevel=0.08, base=0.04, shadow=0.5)
    c.add(c.circle(0, 0, 0.72), GOLD, z=0.03, bevel=0.06, base=0.14)
    c.add(c.ring(0, 0, 0.58, 0.03), GOLD_DARK, z=0.03, bevel=0.03, base=0.19)
    skull(c, 0.0, -0.02, 0.62, mat=GOLD_DARK, base=0.2, eye=CONTOUR)


def m_help(c):
    c.add(c.circle(0, 0, 0.84), BRONZE, z=0.12, bevel=0.08, base=0.04, shadow=0.5)
    c.fill_well(c.circle(0, 0, 0.68), (86, 54, 30), (26, 16, 10), r=0.68, z=0.06)
    hook = c.arc(0.0, -0.22, 0.22, 0.085, deg(160), deg(400))
    stem = c.union(c.seg(0.15, -0.06, 0.0, 0.10, 0.085), c.seg(0.0, 0.10, 0.0, 0.20, 0.085))
    c.add(c.union(hook, stem), GOLD, z=0.10, bevel=0.08, base=0.10, shadow=0.5, sh_dx=0.04, sh_dy=0.05)
    c.add(c.circle(0.0, 0.46, 0.10), GOLD, z=0.10, bevel=0.10, base=0.10, shadow=0.5, sh_dx=0.04, sh_dy=0.05)


def m_check(c):
    mark = c.union(c.seg(-0.50, 0.04, -0.16, 0.42, 0.135), c.seg(-0.16, 0.42, 0.52, -0.40, 0.135))
    c.add(mark, GOLD, z=0.12, bevel=0.10, base=0.06, shadow=0.6, sh_dx=0.05, sh_dy=0.07)


def m_abort(c):
    x = c.union(c.seg(-0.46, -0.46, 0.46, 0.46, 0.135), c.seg(0.46, -0.46, -0.46, 0.46, 0.135))
    c.add(x, EMBER, z=0.12, bevel=0.10, base=0.06, shadow=0.6, sh_dx=0.05, sh_dy=0.07)
    c.add(x + 0.06, EMBER_HOT, z=0.03, bevel=0.05, base=0.18)


def m_gear(c):
    c.add(gear_sdf(c, 0.0, 0.0, 0.54, 8, 0.16, hole=0.20), GOLD, z=0.12, bevel=0.06, base=0.06, shadow=0.6)
    c.add(c.ring(0, 0, 0.20, 0.02), GOLD_DARK, z=0.04, bevel=0.03, base=0.16)
    for k in range(6):
        a = deg(30 + 60 * k)
        c.paint(c.circle(0.38 * np.cos(a), 0.38 * np.sin(a), 0.05), (110, 66, 24), 0.9)


def _floppy(c, down):
    c.add(c.box(0, 0, 0.72, 0.72, 0.07), IRON, z=0.10, bevel=0.06, base=0.06, shadow=0.6)
    c.add(c.box(0.0, -0.46, 0.46, 0.24, 0.03), STEEL, z=0.04, bevel=0.03, base=0.16)
    c.add(c.box(0.22, -0.46, 0.06, 0.16, 0.01), IRON_DARK, z=0.02, bevel=0.02, base=0.20)
    c.add(c.box(0.0, 0.34, 0.52, 0.30, 0.03), PARCHMENT, z=0.03, bevel=0.03, base=0.14)
    tail, tip = (0.14, 0.60) if down else (0.60, 0.14)
    head_a = tip - 0.16 if down else tip + 0.16
    arrow = c.union(c.seg(0.0, tail, 0.0, head_a, 0.05), c.poly([(-0.15, head_a), (0.15, head_a), (0.0, tip)]))
    c.add(arrow, EMBER, z=0.05, bevel=0.04, base=0.2, shadow=0.4)


def m_save(c):
    _floppy(c, True)


def m_load(c):
    _floppy(c, False)


def m_scroll(c):
    c.add(c.box(0.0, 0.0, 0.50, 0.64, 0.04), PARCHMENT, z=0.06, bevel=0.06, base=0.08, shadow=0.6)
    for y in (-0.66, 0.66):
        c.add(c.box(0.0, y, 0.62, 0.075, 0.07), WOOD, z=0.08, bevel=0.07, base=0.10, shadow=0.5)
        c.add(c.circle(-0.66, y, 0.06), GOLD, z=0.04, bevel=0.06, base=0.16)
        c.add(c.circle(0.66, y, 0.06), GOLD, z=0.04, bevel=0.06, base=0.16)
    for y in (-0.36, -0.20, -0.04, 0.12):
        c.paint(c.seg(-0.34, y, 0.34, y + 0.01, 0.014), (96, 56, 32), 0.85)
    c.add(c.circle(0.10, 0.40, 0.14), RUBY, z=0.06, bevel=0.06, base=0.14, shadow=0.4)


def m_flask(c):
    c.glow(0.0, 0.2, 0.6, (200, 100, 20), 0.6)
    body = c.smooth_union(c.circle(0.0, 0.26, 0.42), c.box(0.0, -0.30, 0.12, 0.30), 0.10)
    c.add(body, GLASS, z=0.14, bevel=0.10, base=0.06, shadow=0.6)
    liquid = c.intersect(c.circle(0.0, 0.26, 0.36), c.box(0.0, 0.40, 1.0, 0.36))
    c.add(liquid, AMBER, z=0.10, bevel=0.08, base=0.14)
    c.add(c.ellipse(0.0, 0.09, 0.30, 0.04), EMBER_HOT, z=0.02, bevel=0.03, base=0.22)
    c.add(c.box(0.0, -0.62, 0.15, 0.06, 0.02), BRONZE, z=0.06, bevel=0.04, base=0.12, shadow=0.4)
    c.add(c.ellipse(-0.20, 0.12, 0.06, 0.16, deg(20)), BONE, z=0.01, bevel=0.03, base=0.3)
    for x, y, r in ((0.40, -0.30, 0.05), (0.56, -0.48, 0.035), (0.30, -0.56, 0.03)):
        c.add(c.circle(x, y, r), GOLD, z=0.02, bevel=r, base=0.1)


def m_banner(c):
    c.add(c.seg(-0.44, 0.74, -0.44, -0.70, 0.045), GOLD, z=0.06, bevel=0.045, base=0.12, shadow=0.5)
    c.add(c.circle(-0.44, -0.72, 0.08), GOLD, z=0.05, bevel=0.08, base=0.14)
    flag = c.poly([(-0.41, -0.56), (-0.10, -0.64), (0.22, -0.56), (0.62, -0.62), (0.56, -0.10), (0.62, 0.36), (0.22, 0.32), (-0.10, 0.40), (-0.41, 0.32)])
    c.add(flag, CLOTH_RED, z=0.08, bevel=0.07, base=0.10, shadow=0.6)
    c.add(c.circle(0.10, -0.12, 0.16), GOLD, z=0.05, bevel=0.06, base=0.19, shadow=0.3)


def m_creatures(c):
    c.add(c.seg(-0.28, -0.20, -0.50, -0.70, 0.11, 0.03), BONE, z=0.10, bevel=0.07, base=0.10, shadow=0.5)
    c.add(c.seg(0.28, -0.20, 0.50, -0.70, 0.11, 0.03), BONE, z=0.10, bevel=0.07, base=0.10, shadow=0.5)
    c.add(c.smooth_union(c.ellipse(0, 0.0, 0.52, 0.44), c.ellipse(0, 0.32, 0.30, 0.24), 0.12), WORKER, z=0.18, bevel=0.22, base=0.10, shadow=0.6)
    for sx in (-1, 1):
        c.add(c.ellipse(sx * 0.23, -0.04, 0.13, 0.09, deg(sx * 20)), CONTOUR, z=0.02, bevel=0.04, base=0.28)
        c.add(c.ellipse(sx * 0.23, -0.04, 0.09, 0.055, deg(sx * 20)), EMBER_HOT, z=0.02, bevel=0.04, base=0.29)
    c.add(c.ellipse(0, 0.44, 0.20, 0.07), CONTOUR, z=0.02, bevel=0.03, base=0.28)
    for sx in (-1, 1):
        c.add(c.poly([(sx * 0.14, 0.39), (sx * 0.06, 0.39), (sx * 0.10, 0.50)]), BONE, z=0.02, bevel=0.01, base=0.3)


def m_hourglass(c):
    for y in (-0.64, 0.64):
        c.add(c.box(0.0, y, 0.46, 0.08, 0.04), GOLD, z=0.08, bevel=0.06, base=0.10, shadow=0.5)
    glass = c.poly([(-0.36, -0.58), (0.36, -0.58), (0.05, -0.02), (0.05, 0.02), (0.36, 0.58), (-0.36, 0.58), (-0.05, 0.02), (-0.05, -0.02)])
    c.add(glass, GLASS, z=0.10, bevel=0.06, base=0.10, shadow=0.4)
    c.add(c.poly([(-0.20, 0.54), (0.20, 0.54), (0.10, 0.28), (-0.10, 0.28)]), AMBER, z=0.06, bevel=0.05, base=0.18)
    c.add(c.poly([(-0.24, -0.50), (0.24, -0.50), (0.04, -0.14), (-0.04, -0.14)]), AMBER, z=0.06, bevel=0.05, base=0.18)
    c.add(c.seg(0.0, -0.10, 0.0, 0.34, 0.012), AMBER, z=0.02, bevel=0.01, base=0.2)
    for x in (-0.44, 0.44):
        c.add(c.seg(x, -0.60, x, 0.60, 0.028), GOLD_DARK, z=0.05, bevel=0.028, base=0.12, shadow=0.3)


def m_camera(c):
    c.add(c.box(0.0, 0.10, 0.74, 0.50, 0.10), BRONZE_DARK, z=0.12, bevel=0.08, base=0.06, shadow=0.6)
    c.add(c.box(-0.40, -0.46, 0.20, 0.10, 0.04), BRONZE, z=0.06, bevel=0.05, base=0.14, shadow=0.4)
    c.add(c.circle(0.0, 0.12, 0.34), IRON_DARK, z=0.10, bevel=0.06, base=0.16, shadow=0.4)
    c.add(c.circle(0.0, 0.12, 0.24), GLASS, z=0.06, bevel=0.08, base=0.22)
    c.add(c.circle(0.0, 0.12, 0.10), EMBER, z=0.04, bevel=0.06, base=0.26)
    c.add(c.circle(-0.06, 0.06, 0.04), BONE, z=0.01, bevel=0.03, base=0.30)
    c.add(c.circle(0.52, -0.06, 0.05), EMBER, z=0.04, bevel=0.05, base=0.18)


def m_play(c):
    tri = c.poly([(-0.36, -0.58), (0.60, 0.0), (-0.36, 0.58)])
    c.add(tri, GOLD, z=0.12, bevel=0.10, base=0.06, shadow=0.6, sh_dx=0.05, sh_dy=0.07)
    c.add(c.poly([(-0.20, -0.32), (0.30, 0.0), (-0.20, 0.32)]), GOLD_DARK, z=0.03, bevel=0.04, base=0.16)


def m_lantern(c):
    c.glow(0.0, 0.0, 0.6, (255, 150, 30), 0.9)
    c.add(c.ring(0.0, -0.74, 0.14, 0.03), IRON, z=0.05, bevel=0.03, base=0.14)
    c.add(c.poly([(-0.30, -0.56), (0.30, -0.56), (0.20, -0.44), (-0.20, -0.44)]), IRON, z=0.08, bevel=0.05, base=0.12, shadow=0.5)
    c.add(c.box(0.0, 0.10, 0.34, 0.50, 0.03), GLASS, z=0.06, bevel=0.05, base=0.10, shadow=0.5)
    c.add(flame_sdf(c, 0.0, 0.16, 1.1), EMBER, z=0.08, bevel=0.10, base=0.18)
    c.add(flame_sdf(c, 0.0, 0.22, 0.5), EMBER_HOT, z=0.05, bevel=0.06, base=0.26)
    for x in (-0.34, 0.34):
        c.add(c.seg(x, -0.42, x, 0.62, 0.030), IRON, z=0.06, bevel=0.03, base=0.20, shadow=0.4)
    c.add(c.box(0.0, 0.66, 0.42, 0.07, 0.03), IRON, z=0.08, bevel=0.05, base=0.12, shadow=0.5)


def m_territory(c):
    c.add(c.poly([(-0.72, 0.30), (0.0, 0.62), (0.72, 0.30), (0.0, -0.02)]), STONE, z=0.10, bevel=0.05, base=0.04, shadow=0.5)
    c.add(c.poly([(-0.72, 0.30), (0.0, 0.62), (0.0, 0.72), (-0.72, 0.40)]), STONE_DARK, z=0.02, bevel=0.02, base=0.04)
    c.add(c.poly([(0.72, 0.30), (0.0, 0.62), (0.0, 0.72), (0.72, 0.40)]), STONE_DARK, z=0.02, bevel=0.02, base=0.04)
    c.add(c.seg(0.0, 0.28, 0.0, -0.66, 0.04), GOLD, z=0.06, bevel=0.04, base=0.14, shadow=0.5)
    c.add(c.poly([(0.03, -0.62), (0.56, -0.44), (0.03, -0.22)]), CLOTH_RED, z=0.06, bevel=0.05, base=0.16, shadow=0.4)


def m_mana(c):
    c.glow(0.0, 0.0, 0.6, (200, 40, 40), 0.6)
    gem = c.poly([(0.0, -0.74), (0.52, -0.16), (0.30, 0.66), (-0.30, 0.66), (-0.52, -0.16)])
    c.add(gem, RUBY, z=0.18, bevel=0.20, base=0.06, shadow=0.5)
    c.add(c.poly([(0.0, -0.66), (-0.24, -0.16), (0.0, 0.56), (0.24, -0.16)]), RUBY_LIGHT, z=0.05, bevel=0.06, base=0.22)


def m_person(c):
    c.add(c.circle(0.0, -0.52, 0.20), GOLD, z=0.12, bevel=0.16, base=0.06, shadow=0.5)
    body = c.poly([(-0.12, -0.28), (0.12, -0.28), (0.28, -0.22), (0.66, 0.20), (0.54, 0.32), (0.24, 0.04), (0.24, 0.30), (0.32, 0.78), (0.10, 0.78), (0.0, 0.42), (-0.10, 0.78), (-0.32, 0.78), (-0.24, 0.30), (-0.24, 0.04), (-0.54, 0.32), (-0.66, 0.20), (-0.28, -0.22)])
    c.add(body, GOLD, z=0.10, bevel=0.06, base=0.06, shadow=0.5)


def m_house(c):
    c.add(c.poly([(-0.72, -0.06), (0.0, -0.70), (0.72, -0.06), (0.54, -0.06), (0.54, 0.68), (-0.54, 0.68), (-0.54, -0.06)]), GOLD, z=0.10, bevel=0.06, base=0.06, shadow=0.6)
    c.carve(arch_sdf(c, 0.0, 0.10, 0.68, 0.17), 0.10, 0.03, colour=(60, 34, 12))


def m_wand(c):
    c.glow(0.36, -0.38, 0.4, (255, 200, 90), 0.8)
    f = Xf(-0.58, 0.62, deg(-45))
    c.add(c.seg(*f.p(0, 0), *f.p(1.10, 0), 0.085, 0.06), WOOD, z=0.08, bevel=0.07, base=0.08, shadow=0.6)
    c.add(c.seg(*f.p(0.86, 0), *f.p(1.0, 0), 0.085), GOLD, z=0.06, bevel=0.06, base=0.16)
    c.add(c.poly(star(4, 0.36, 0.10, 0.38, -0.42, 0.0)), GOLD, z=0.06, bevel=0.06, base=0.2, shadow=0.3)
    c.add(c.poly(star(4, 0.15, 0.045, 0.66, 0.02, 0.0)), GOLD, z=0.04, bevel=0.04, base=0.2)
    c.add(c.poly(star(4, 0.13, 0.04, 0.06, -0.66, 0.0)), GOLD, z=0.04, bevel=0.04, base=0.2)


def m_pick_nav(c):
    pickaxe(c, -0.60, 0.62, deg(-45), 1.0, base=0.10)


def m_chevrons(c):
    for y, d in ((-0.86, 1), (0.86, -1)):
        c.add(c.union(c.seg(-0.62, y + 0.34 * d, 0.0, y - 0.20 * d, 0.16), c.seg(0.62, y + 0.34 * d, 0.0, y - 0.20 * d, 0.16)), GOLD, z=0.10, bevel=0.10, base=0.06, shadow=0.6)


def m_objectives_eye(c):
    lens = c.intersect(c.circle(0.0, 0.62, 0.92), c.circle(0.0, -0.62, 0.92))
    c.add(lens - 0.09, GOLD_DARK, z=0.08, bevel=0.06, base=0.04, shadow=0.5)
    c.add(lens, BONE, z=0.06, bevel=0.10, base=0.10)
    c.add(c.circle(0.0, 0.0, 0.34), EMBER, z=0.10, bevel=0.16, base=0.14)
    c.add(c.circle(0.0, 0.0, 0.15), CONTOUR, z=0.02, bevel=0.06, base=0.24)
    c.add(c.circle(-0.09, -0.10, 0.06), BONE, z=0.01, bevel=0.04, base=0.28)


def m_return(c):
    arrow = c.union(c.poly([(0.60, -0.26), (0.18, -0.66), (0.18, 0.14)]), c.seg(0.20, -0.26, -0.34, -0.26, 0.13),
                    c.seg(-0.34, -0.26, -0.34, 0.50, 0.13), c.seg(-0.34, 0.50, -0.66, 0.50, 0.13))
    c.add(arrow, GOLD, z=0.10, bevel=0.08, base=0.06, shadow=0.6)


def _plaque(c, glow, read):
    ring = BRONZE_DARK if read else BRONZE
    rim = BRONZE_DARK if read else GOLD_DARK
    c.add(c.circle(0.0, 0.0, 0.98), CONTOUR, z=0.0, bevel=0.01)
    c.add(c.circle(0.0, 0.0, 0.94), ring, z=0.11, bevel=0.10)
    well = (46, 28, 16) if read else ((130, 58, 16) if glow else (84, 50, 26))
    c.fill_well(c.circle(0.0, 0.0, 0.72), well, (16, 9, 6), r=0.72, z=0.0)
    c.add(c.ring(0.0, 0.0, 0.72, 0.024), rim, z=0.10, bevel=0.02)
    mat = GOLD_DARK if read else GOLD
    if glow:
        c.glow(0.0, 0.0, 0.66, (255, 120, 24), 1.0)
    c.add(c.circle(0.0, -0.40, 0.12), mat, z=0.10, bevel=0.10, base=0.08, shadow=0.5, sh_dx=0.03, sh_dy=0.04, sh_soft=0.02)
    stem = c.union(c.box(0.0, 0.12, 0.11, 0.35, 0.02), c.box(0.0, 0.44, 0.22, 0.06, 0.02), c.box(0.0, -0.04, 0.18, 0.05, 0.02))
    c.add(stem, mat, z=0.10, bevel=0.06, base=0.08, shadow=0.5, sh_dx=0.03, sh_dy=0.04, sh_soft=0.02)
    for k in range(8):
        a = deg(22.5 + 45 * k)
        c.add(c.circle(0.84 * np.cos(a), 0.84 * np.sin(a), 0.03), rim, z=0.03, bevel=0.03, base=0.12)


def m_message(c):
    _plaque(c, True, False)


def m_message_read(c):
    _plaque(c, False, True)


# ---------------------------------------------------------------------------------------------
# Traps, doors, spells and rooms that used to share the emblem of another entry
# ---------------------------------------------------------------------------------------------
GAS = Mat((190, 196, 84), (118, 130, 44), spec=0.12, shin=8, mottle=0.2)
GAS_DARK = Mat((146, 152, 60), (88, 98, 34), spec=0.10, shin=8, mottle=0.2)
FROST = Mat((244, 236, 214), (206, 190, 164), spec=0.9, shin=60, mottle=0.04, grain=0.02)
CLOUD = Mat((118, 102, 92), (74, 62, 56), spec=0.1, shin=8, mottle=0.2)
FEATHER = Mat((246, 236, 214), (206, 188, 156), spec=0.15, shin=10, mottle=0.1)


def floor_plate(c, y=0.42, hw=0.64):
    c.add(c.box(0.0, y, hw, 0.12, 0.03), IRON, z=0.08, bevel=0.05, base=0.06, shadow=0.6)
    for sx in (-1, 1):
        c.add(c.circle(sx * (hw - 0.09), y, 0.032), GOLD_DARK, z=0.03, bevel=0.03, base=0.14)


def m_alarm_trap(c):
    c.glow(0.0, -0.05, 0.7, (150, 70, 14), 0.5)
    c.add(c.box(0.0, -0.60, 0.20, 0.05, 0.02), IRON, z=0.06, bevel=0.04, base=0.06, shadow=0.5)
    bell = c.smooth_union(c.ellipse(0.0, -0.12, 0.30, 0.38), c.poly([(-0.26, 0.05), (0.26, 0.05), (0.46, 0.34), (-0.46, 0.34)]), 0.10)
    c.add(bell, BRONZE, z=0.22, bevel=0.20, base=0.08, shadow=0.6)
    c.add(c.box(0.0, 0.36, 0.48, 0.04, 0.02), GOLD, z=0.05, bevel=0.03, base=0.20, shadow=0.4)
    c.add(c.ellipse(-0.12, -0.22, 0.05, 0.14, deg(14)), GOLD, z=0.02, bevel=0.03, base=0.34)
    c.add(c.circle(0.0, 0.46, 0.085), GOLD_DARK, z=0.08, bevel=0.085, base=0.10, shadow=0.5)
    for sx in (-1, 1):
        for r in (0.64, 0.78):
            a = 0.0 if sx > 0 else np.pi
            c.add(c.arc(0.0, -0.02, r, 0.02, a - deg(26), a + deg(26)), GOLD, z=0.03, bevel=0.02, base=0.10)


def m_fear_trap(c):
    c.glow(0.0, -0.05, 0.8, (170, 40, 12), 0.6)
    for k in range(12):
        a = deg(15 + 30 * k)
        c.add(c.seg(0.50 * np.cos(a), -0.05 + 0.50 * np.sin(a), 0.78 * np.cos(a), -0.05 + 0.78 * np.sin(a), 0.016), EMBER, z=0.03, bevel=0.016, base=0.06)
    skull(c, 0.0, -0.05, 1.15, base=0.20, eye=EMBER_HOT)
    for sx in (-1, 1):
        c.glow(sx * 0.14, -0.05, 0.12, (255, 150, 40), 0.9)
    c.add(c.box(0.0, 0.50, 0.30, 0.07, 0.02), IRON, z=0.06, bevel=0.04, base=0.06, shadow=0.5)


def m_gas_trap(c):
    c.glow(0.0, -0.1, 0.7, (150, 160, 30), 0.5)
    floor_plate(c)
    for x in (-0.30, -0.10, 0.10, 0.30):
        c.paint(c.box(x, 0.42, 0.025, 0.07), (24, 18, 14), 0.9)
    for x, y, r, m in ((-0.26, -0.02, 0.22, GAS_DARK), (0.26, -0.10, 0.24, GAS_DARK), (0.0, -0.22, 0.30, GAS), (-0.18, -0.44, 0.17, GAS),
                       (0.20, -0.46, 0.18, GAS), (0.0, 0.16, 0.20, GAS)):
        c.add(c.circle(x, y, r), m, z=0.14, bevel=r * 0.9, base=0.06, shadow=0.35)


def m_lightning_trap(c):
    c.glow(0.0, -0.05, 0.7, (230, 150, 30), 0.6)
    floor_plate(c, 0.50)
    for sx in (-1, 1):
        c.add(c.box(sx * 0.46, 0.14, 0.055, 0.34), IRON, z=0.08, bevel=0.05, base=0.08, shadow=0.5)
        for i in range(4):
            c.add(c.seg(sx * 0.46 - 0.075, -0.18 + 0.06 * i, sx * 0.46 + 0.075, -0.15 + 0.06 * i, 0.020), GOLD, z=0.03, bevel=0.02, base=0.18)
        c.add(c.circle(sx * 0.46, -0.28, 0.07), GOLD, z=0.08, bevel=0.07, base=0.14, shadow=0.4)
    zig = [(-0.40, -0.28), (-0.22, -0.14), (-0.12, -0.38), (0.02, -0.10), (0.14, -0.36), (0.24, -0.16), (0.40, -0.28)]
    for (ax, ay), (bx, by) in zip(zig, zig[1:]):
        c.add(c.seg(ax, ay, bx, by, 0.034), EMBER_HOT, z=0.03, bevel=0.034, base=0.30)
        c.glow((ax + bx) / 2, (ay + by) / 2, 0.14, (255, 190, 60), 0.7)


def m_fireburst_trap(c):
    c.glow(0.0, 0.0, 0.8, (255, 100, 20), 0.6)
    floor_plate(c)
    for x in (-0.36, 0.0, 0.36):
        c.add(c.box(x, 0.30, 0.07, 0.05, 0.02), IRON_DARK, z=0.05, bevel=0.03, base=0.14)
    for x, s in ((-0.36, 1.5), (0.36, 1.5), (0.0, 2.3)):
        c.add(flame_sdf(c, x, 0.02 - 0.12 * (s - 1.5), s), EMBER, z=0.10, bevel=0.10, base=0.10, shadow=0.3)
        c.add(flame_sdf(c, x, 0.08 - 0.12 * (s - 1.5), s * 0.5), EMBER_HOT, z=0.06, bevel=0.06, base=0.22)
    sparks(c, ((-0.12, -0.50, 0.025), (0.22, -0.54, 0.02), (0.50, -0.30, 0.02), (-0.52, -0.26, 0.02)))


def m_freeze_trap(c):
    c.glow(0.0, 0.0, 0.7, (200, 150, 110), 0.4)
    floor_plate(c, 0.58, 0.60)
    for k in range(6):
        a = deg(60 * k - 90)
        ex, ey = 0.62 * np.cos(a), -0.08 + 0.62 * np.sin(a)
        c.add(c.seg(0.0, -0.08, ex, ey, 0.04), FROST, z=0.08, bevel=0.04, base=0.12, shadow=0.4)
        mx, my = 0.36 * np.cos(a), -0.08 + 0.36 * np.sin(a)
        for s in (-1, 1):
            b = a + s * deg(50)
            c.add(c.seg(mx, my, mx + 0.20 * np.cos(b), my + 0.20 * np.sin(b), 0.026), FROST, z=0.05, bevel=0.026, base=0.14)
    c.add(c.poly(star(6, 0.16, 0.10, 0.0, -0.08, deg(0))), FROST, z=0.06, bevel=0.05, base=0.20)
    for x, h in ((-0.40, 0.18), (-0.18, 0.28), (0.14, 0.22), (0.38, 0.16)):
        c.add(c.poly([(x - 0.06, 0.50), (x + 0.06, 0.50), (x, 0.50 - h)]), FROST, z=0.05, bevel=0.05, base=0.10)


def m_watch_banner_trap(c):
    c.glow(0.0, 0.0, 0.7, (130, 56, 14), 0.5)
    c.add(c.seg(0.50, 0.66, 0.50, -0.46, 0.028), WOOD, z=0.06, bevel=0.028, base=0.06, shadow=0.5)
    c.add(c.poly([(0.50, -0.78), (0.58, -0.46), (0.42, -0.46)]), STEEL, z=0.05, bevel=0.05, base=0.10, shadow=0.4)
    shield = c.poly([(-0.46, -0.44), (0.34, -0.44), (0.34, 0.06), (-0.06, 0.66), (-0.46, 0.06)])
    c.add(shield, GOLD_DARK, z=0.10, bevel=0.07, base=0.08, shadow=0.6)
    c.add(shield + 0.06, IRON, z=0.04, bevel=0.04, base=0.20)
    c.add(c.box(-0.06, -0.10, 0.045, 0.40), GOLD, z=0.04, bevel=0.03, base=0.26, shadow=0.3)
    c.add(c.box(-0.06, -0.14, 0.30, 0.045), GOLD, z=0.04, bevel=0.03, base=0.27, shadow=0.3)
    c.add(c.circle(-0.06, -0.14, 0.07), RUBY, z=0.05, bevel=0.07, base=0.30)


def m_trigger_trap(c):
    c.glow(0.0, 0.2, 0.6, (130, 54, 14), 0.4)
    c.add(c.box(0.0, 0.34, 0.66, 0.30, 0.04), IRON, z=0.08, bevel=0.05, base=0.04, shadow=0.6)
    c.add(c.box(0.0, 0.38, 0.54, 0.20, 0.03), STONE, z=0.05, bevel=0.04, base=0.14)
    c.add(c.ring(0.0, 0.38, 0.12, 0.02), GOLD, z=0.04, bevel=0.02, base=0.24)
    c.add(c.circle(0.0, 0.38, 0.05), GOLD, z=0.04, bevel=0.05, base=0.26)
    c.add(c.seg(-0.42, 0.20, -0.18, -0.46, 0.030), IRON, z=0.05, bevel=0.03, base=0.14, shadow=0.5)
    c.add(c.circle(-0.18, -0.46, 0.10), RUBY, z=0.08, bevel=0.10, base=0.16, shadow=0.5)
    c.add(c.circle(-0.42, 0.22, 0.06), GOLD_DARK, z=0.05, bevel=0.06, base=0.12)
    for k in range(3):
        c.add(c.arc(0.14, -0.50, 0.14 + 0.12 * k, 0.014, deg(-70), deg(30)), EMBER, z=0.02, bevel=0.014, base=0.10)


def _door_body(c, mat):
    door = arch_sdf(c, 0.0, -0.62, 0.58, 0.46)
    c.add(door - 0.07, STONE, z=0.08, bevel=0.05, base=0.04, shadow=0.6)
    c.add(door, mat, z=0.08, bevel=0.06, base=0.10)
    return door


def m_door_ironbound(c):
    _door_body(c, WOOD)
    for x in (-0.15, 0.15):
        c.paint(c.seg(x, -0.6, x, 0.58, 0.008), (50, 30, 16), 0.9)
    c.add(c.seg(-0.38, -0.30, 0.38, 0.50, 0.060), WOOD_DARK, z=0.05, bevel=0.05, base=0.20, shadow=0.5)
    c.add(c.seg(0.38, -0.30, -0.38, 0.50, 0.060), WOOD_DARK, z=0.05, bevel=0.05, base=0.20, shadow=0.5)
    for x, y in ((-0.38, -0.30), (0.38, -0.30), (-0.38, 0.50), (0.38, 0.50), (0.0, 0.10)):
        c.add(c.circle(x, y, 0.035), IRON, z=0.04, bevel=0.035, base=0.26)
    c.add(c.ring(0.0, 0.10, 0.07, 0.016), GOLD, z=0.04, bevel=0.016, base=0.28)


def m_door_steel(c):
    door = _door_body(c, STEEL)
    c.add(door - 0.10, IRON, z=0.04, bevel=0.04, base=0.20)
    for x in (-0.28, 0.0, 0.28):
        c.paint(c.seg(x, -0.6, x, 0.58, 0.008), (30, 24, 20), 0.9)
    for y in (-0.30, 0.0, 0.30):
        c.paint(c.seg(-0.4, y, 0.4, y, 0.008), (30, 24, 20), 0.9)
    for x in (-0.28, 0.0, 0.28):
        for y in (-0.30, 0.0, 0.30, 0.50):
            c.add(c.circle(x, y, 0.02), STEEL, z=0.03, bevel=0.02, base=0.26)
    c.add(c.ring(0.0, 0.02, 0.13, 0.02), GOLD, z=0.05, bevel=0.02, base=0.26, shadow=0.4)
    for k in range(4):
        a = deg(45 + 90 * k)
        c.add(c.seg(0.0, 0.02, 0.13 * np.cos(a), 0.02 + 0.13 * np.sin(a), 0.014), GOLD, z=0.04, bevel=0.014, base=0.27)


def m_barricade(c):
    c.glow(0.0, 0.1, 0.7, (130, 54, 14), 0.4)
    for x, a, hh in ((-0.52, -8, 0.50), (-0.18, 6, 0.58), (0.20, -5, 0.54), (0.54, 9, 0.48)):
        f = Xf(x, 0.10, deg(a))
        c.add(c.poly(f.pts([(-0.07, hh), (0.07, hh), (0.07, -hh + 0.12), (0.0, -hh - 0.12), (-0.07, -hh + 0.12)])), WOOD, z=0.10, bevel=0.06, base=0.06, shadow=0.5)
    for y, a in ((-0.22, -6), (0.26, 5)):
        c.add(c.box(0.0, y, 0.70, 0.075, 0.02, deg(a)), WOOD_DARK, z=0.08, bevel=0.05, base=0.20, shadow=0.55)
        for x in (-0.50, -0.18, 0.20, 0.54):
            c.add(c.circle(x, y + x * np.tan(deg(a)), 0.026), IRON, z=0.03, bevel=0.026, base=0.30)


def m_door_secret(c):
    c.glow(0.0, 0.0, 0.7, (130, 54, 14), 0.4)
    c.add(c.box(0.0, 0.0, 0.74, 0.74, 0.04), STONE, z=0.08, bevel=0.05, base=0.04, shadow=0.5)
    for j, y in enumerate((-0.52, -0.24, 0.04, 0.32, 0.60)):
        c.paint(c.seg(-0.72, y, 0.72, y, 0.010), (46, 34, 26), 0.9)
        off = 0.0 if j % 2 == 0 else 0.22
        for x in (-0.50 + off, -0.06 + off, 0.38 + off):
            if abs(x) < 0.72:
                c.paint(c.seg(x, y, x, y + 0.28, 0.010), (46, 34, 26), 0.9)
    door = arch_sdf(c, 0.0, -0.46, 0.60, 0.34)
    c.carve(np.abs(door) - 0.012, 0.05, 0.012, colour=(28, 18, 12))
    c.add(c.circle(0.16, 0.10, 0.045), GOLD, z=0.05, bevel=0.045, base=0.10, shadow=0.4)
    c.paint(c.box(0.16, 0.12, 0.014, 0.04), (24, 16, 10), 0.95)
    c.add(c.ring(-0.30, -0.10, 0.06, 0.014), GOLD_DARK, z=0.03, bevel=0.014, base=0.08)


def m_door_magic(c):
    c.glow(0.0, 0.0, 0.8, (200, 110, 20), 0.7)
    _door_body(c, WOOD_DARK)
    c.add(c.box(0.0, -0.30, 0.46, 0.04, 0.02), IRON, z=0.05, bevel=0.03, base=0.20)
    c.add(c.box(0.0, 0.40, 0.46, 0.04, 0.02), IRON, z=0.05, bevel=0.03, base=0.20)
    c.add(c.ring(0.0, 0.04, 0.30, 0.022), GOLD, z=0.04, bevel=0.022, base=0.22, shadow=0.4)
    c.add(c.poly(star(5, 0.30, 0.115, 0.0, 0.04, deg(-90))), AMBER, z=0.04, bevel=0.03, base=0.24)
    c.glow(0.0, 0.04, 0.34, (255, 170, 40), 0.9)
    sparks(c, ((-0.56, -0.40, 0.025), (0.56, -0.20, 0.02), (0.50, 0.46, 0.025), (-0.52, 0.38, 0.02)))


def m_lightning_spell(c):
    c.glow(0.0, 0.1, 0.8, (230, 150, 30), 0.7)
    for x, y, r in ((-0.38, -0.60, 0.20), (-0.10, -0.68, 0.26), (0.24, -0.62, 0.22), (0.46, -0.54, 0.15)):
        c.add(c.circle(x, y, r), CLOUD, z=0.14, bevel=r, base=0.06, shadow=0.4)
    bolt = c.poly([(0.10, -0.52), (-0.20, -0.04), (0.0, -0.04), (-0.18, 0.46), (0.28, -0.14), (0.06, -0.14), (0.30, -0.52)])
    c.add(bolt, EMBER_HOT, z=0.10, bevel=0.05, base=0.14, shadow=0.5)
    c.add(c.seg(0.06, -0.20, 0.40, 0.10, 0.026), AMBER, z=0.04, bevel=0.026, base=0.22)
    c.add(c.seg(-0.14, 0.14, -0.46, 0.30, 0.024), AMBER, z=0.04, bevel=0.024, base=0.22)
    c.add(c.ellipse(-0.18, 0.58, 0.34, 0.07), EMBER, z=0.03, bevel=0.05, base=0.06)
    sparks(c, ((0.20, 0.50, 0.03), (-0.50, 0.50, 0.025), (0.46, 0.34, 0.02)))


def m_tremor(c):
    c.glow(0.0, 0.3, 0.7, (255, 100, 20), 0.7)
    c.add(c.poly([(-0.72, 0.0), (-0.10, -0.02), (-0.20, 0.30), (0.02, 0.50), (-0.12, 0.74), (-0.72, 0.74)]), STONE, z=0.10, bevel=0.06, base=0.04, shadow=0.6)
    c.add(c.poly([(0.72, 0.0), (0.12, -0.02), (0.02, 0.28), (0.20, 0.50), (0.08, 0.74), (0.72, 0.74)]), STONE_DARK, z=0.10, bevel=0.06, base=0.04, shadow=0.6)
    c.add(c.union(c.seg(-0.04, 0.0, -0.08, 0.30, 0.020), c.seg(-0.08, 0.30, 0.06, 0.50, 0.020), c.seg(0.06, 0.50, -0.02, 0.74, 0.020)), EMBER_HOT, z=0.0, bevel=0.02, base=0.02)
    for x, y, r in ((-0.30, -0.30, 0.10), (0.18, -0.46, 0.12), (0.40, -0.22, 0.08), (-0.06, -0.14, 0.06)):
        c.add(c.circle(x, y, r), STONE, z=0.10, bevel=r, base=0.10, shadow=0.5)
    for sx in (-1, 1):
        for k in range(2):
            c.add(c.arc(0.0, 0.34, 0.80 + 0.07 * k, 0.018, deg(-20) if sx > 0 else deg(160), deg(20) if sx > 0 else deg(200)), EMBER, z=0.03, bevel=0.018, base=0.08)


def m_defector(c):
    c.glow(0.0, 0.0, 0.7, (170, 60, 20), 0.6)
    disc = c.circle(0.0, 0.0, 0.34)
    c.add(c.intersect(disc, c.box(-0.5, 0.0, 0.5, 1.0)), RUBY, z=0.14, bevel=0.25, base=0.10, shadow=0.5)
    c.add(c.intersect(disc, c.box(0.5, 0.0, 0.5, 1.0)), IRON, z=0.14, bevel=0.25, base=0.10, shadow=0.5)
    c.add(c.seg(0.0, -0.34, 0.0, 0.34, 0.014), CONTOUR, z=0.0, bevel=0.01, base=0.24)
    for a0 in (deg(200), deg(20)):
        c.add(c.arc(0.0, 0.0, 0.58, 0.05, a0, a0 + deg(120)), GOLD, z=0.05, bevel=0.05, base=0.12, shadow=0.4)
        e = a0 + deg(120)
        f = Xf(0.58 * np.cos(e), 0.58 * np.sin(e), e + np.pi / 2)
        c.add(c.poly(f.pts([(-0.12, -0.02), (0.12, -0.02), (0.0, 0.18)])), GOLD, z=0.05, bevel=0.04, base=0.14, shadow=0.4)


def m_possess(c):
    c.glow(0.0, -0.05, 0.8, (150, 70, 40), 0.6)
    swirl(c, 0.0, 0.46, 0.50, EMBER, GOLD_DARK, turns=1.3, width=0.04)
    wave = c.poly([(-0.34, 0.14), (-0.34, 0.36), (-0.17, 0.28), (0.0, 0.40), (0.17, 0.28), (0.34, 0.36), (0.34, 0.14)])
    c.add(c.union(c.circle(0.0, -0.26, 0.34), c.box(0.0, -0.02, 0.34, 0.20), wave), FEATHER, z=0.18, bevel=0.20, base=0.12, shadow=0.5)
    for sx in (-1, 1):
        c.add(c.ellipse(sx * 0.13, -0.28, 0.065, 0.09), EMBER_HOT, z=0.02, bevel=0.04, base=0.30)
        c.glow(sx * 0.13, -0.28, 0.12, (255, 150, 40), 0.9)
    c.add(c.ellipse(0.0, -0.06, 0.07, 0.10), CONTOUR, z=0.02, bevel=0.04, base=0.30)


def m_create_gold(c):
    c.glow(0.0, 0.1, 0.8, (230, 170, 40), 0.45)
    coin_stack(c, -0.34, 0.50, 3, r=0.24)
    coin_stack(c, 0.30, 0.50, 5, r=0.24)
    coin_stack(c, -0.02, 0.58, 2, r=0.2)
    for x, y, s in ((-0.40, -0.26, 0.16), (0.10, -0.50, 0.20), (0.54, -0.20, 0.13)):
        c.add(c.poly(star(4, s, s * 0.28, x, y, deg(0))), GOLD, z=0.04, bevel=0.04, base=0.28)
        c.glow(x, y, s * 1.6, (255, 220, 100), 0.6)


def m_inferno(c):
    c.glow(0.0, 0.0, 0.9, (255, 90, 14), 0.8)
    for x, y, s in ((-0.50, 0.26, 1.7), (0.50, 0.26, 1.7), (0.0, 0.18, 3.0)):
        c.add(flame_sdf(c, x, y, s), EMBER, z=0.12, bevel=0.12, base=0.08, shadow=0.3)
    c.add(flame_sdf(c, 0.0, 0.26, 1.7), EMBER_HOT, z=0.12, bevel=0.12, base=0.20)
    c.add(flame_sdf(c, 0.0, 0.36, 0.9), EMBER_HOT, z=0.06, bevel=0.06, base=0.30)
    sparks(c, ((-0.62, -0.20, 0.025), (0.60, -0.30, 0.03), (0.0, -0.70, 0.025), (-0.24, -0.58, 0.02), (0.30, -0.62, 0.02)))


def m_guard_room(c):
    c.glow(0.0, 0.1, 0.7, (130, 56, 14), 0.5)
    c.add(c.box(0.0, 0.24, 0.44, 0.50, 0.02), STONE, z=0.12, bevel=0.05, base=0.05, shadow=0.6)
    for x in (-0.44, -0.15, 0.15, 0.44):
        c.add(c.box(x, -0.34, 0.08, 0.10, 0.01), STONE, z=0.12, bevel=0.04, base=0.05, shadow=0.5)
    c.add(c.box(0.0, -0.20, 0.52, 0.06, 0.015), STONE_DARK, z=0.08, bevel=0.04, base=0.12, shadow=0.4)
    door = arch_sdf(c, 0.0, 0.22, 0.74, 0.17)
    c.fill_well(door, (60, 26, 12), (12, 6, 4), r=0.4, z=0.02)
    c.add(c.box(0.0, -0.02, 0.025, 0.12), CONTOUR, z=0.02, bevel=0.02, base=0.2)
    c.add(c.seg(0.0, 0.0, 0.0, -0.70, 0.020), WOOD, z=0.05, bevel=0.02, base=0.10)
    c.add(c.poly([(0.02, -0.70), (0.40, -0.60), (0.02, -0.50)]), CLOTH_RED, z=0.05, bevel=0.04, base=0.14, shadow=0.4)
    c.add(c.seg(-0.64, 0.66, -0.64, 0.10, 0.026), WOOD, z=0.05, bevel=0.026, base=0.08, shadow=0.5)
    c.add(c.poly([(-0.64, -0.06), (-0.58, 0.12), (-0.70, 0.12)]), STEEL, z=0.04, bevel=0.04, base=0.12)


def m_prayer_temple(c):
    c.glow(0.0, -0.1, 0.7, (200, 100, 20), 0.8)
    win = arch_sdf(c, 0.0, -0.62, 0.20, 0.34)
    c.add(win, STONE, z=0.10, bevel=0.05, base=0.05, shadow=0.55)
    c.fill_well(win - 0.07, (200, 120, 30), (110, 50, 14), r=0.5, z=0.02)
    c.add(c.box(0.0, -0.22, 0.03, 0.30), GOLD, z=0.04, bevel=0.03, base=0.18, shadow=0.3)
    c.add(c.box(0.0, -0.30, 0.17, 0.03), GOLD, z=0.04, bevel=0.03, base=0.19, shadow=0.3)
    c.add(c.ring(0.0, -0.62, 0.40, 0.014), GOLD, z=0.03, bevel=0.014, base=0.14)
    c.add(c.box(0.0, 0.42, 0.56, 0.22, 0.02), STONE, z=0.12, bevel=0.05, base=0.05, shadow=0.55)
    c.add(c.box(0.0, 0.24, 0.62, 0.04, 0.015), STONE_DARK, z=0.06, bevel=0.03, base=0.14)
    c.add(c.box(0.0, 0.14, 0.18, 0.045, 0.015), PARCHMENT, z=0.05, bevel=0.03, base=0.20, shadow=0.3)
    for sx in (-1, 1):
        c.add(c.box(sx * 0.40, 0.08, 0.03, 0.12), BONE, z=0.06, bevel=0.03, base=0.18, shadow=0.4)
        c.add(flame_sdf(c, sx * 0.40, -0.10, 0.55), EMBER, z=0.05, bevel=0.05, base=0.24)
        c.glow(sx * 0.40, -0.08, 0.14, (255, 170, 50), 0.7)


def m_chicken(c):
    c.glow(0.0, 0.1, 0.7, (200, 110, 30), 0.5)
    c.add(c.ellipse(0.0, 0.64, 0.46, 0.06), LEATHER, z=0.03, bevel=0.05, base=0.04, shadow=0.4)
    for sx in (-0.12, 0.10):
        c.add(c.seg(sx, 0.38, sx - 0.02, 0.62, 0.020), YELLOW, z=0.04, bevel=0.02, base=0.08, shadow=0.3)
        for dy in (-0.04, 0.0, 0.04):
            c.add(c.seg(sx - 0.02, 0.62, sx + 0.08, 0.62 + dy, 0.014), YELLOW, z=0.03, bevel=0.014, base=0.08)
    c.add(c.poly([(-0.34, 0.0), (-0.66, -0.34), (-0.50, -0.02), (-0.70, -0.12), (-0.44, 0.16)]), FEATHER, z=0.06, bevel=0.05, base=0.10, shadow=0.4)
    c.add(c.ellipse(-0.02, 0.10, 0.40, 0.32, deg(-12)), FEATHER, z=0.20, bevel=0.30, base=0.08, shadow=0.55)
    c.add(c.ellipse(-0.04, 0.12, 0.20, 0.13, deg(-24)), BONE, z=0.04, bevel=0.06, base=0.30, shadow=0.3)
    c.add(c.circle(0.34, -0.26, 0.17), FEATHER, z=0.14, bevel=0.17, base=0.14, shadow=0.5)
    c.add(c.poly([(0.48, -0.30), (0.66, -0.22), (0.48, -0.16)]), EMBER, z=0.04, bevel=0.03, base=0.26, shadow=0.3)
    for i, (x, y) in enumerate(((0.28, -0.46), (0.36, -0.48), (0.43, -0.42))):
        c.add(c.circle(x, y, 0.065 - 0.008 * i), RUBY, z=0.06, bevel=0.06, base=0.24)
    c.add(c.circle(0.40, -0.28, 0.026), CONTOUR, z=0.02, bevel=0.02, base=0.30)
    c.add(c.ellipse(0.46, -0.10, 0.04, 0.07), RUBY, z=0.04, bevel=0.04, base=0.22)


# ---------------------------------------------------------------------------------------------
# The icon table. kind None: a frameless symbol. cells: pixel size of the cell in the atlas.
# ---------------------------------------------------------------------------------------------
def _slot(fn, kind, zoom=1.05):
    return {"draw": fn, "kind": kind, "cells": 128, "zoom": zoom}


def _sym(fn, cells=64, zoom=1.0, extent=(1.0, 1.0)):
    return {"draw": fn, "kind": None, "cells": cells, "zoom": zoom, "extent": extent}


def _mini(fn, kind, zoom=1.02):
    return {"draw": fn, "kind": kind, "cells": 64, "zoom": zoom, "rivets": False}


TALL = (1.0, 1.625)   # the 32 x 52 units of a message tab

ICONS = {
    # rooms
    "DormitoryButton": _slot(m_dormitory, "room"),
    "TrainingHallButton": _slot(m_training, "room"),
    "ForgeButton": _slot(m_forge, "room"),
    "TreasuryButton": _slot(m_treasury, "room"),
    "LibraryButton": _slot(m_library, "room"),
    "HatcheryButton": _slot(m_hatchery, "room"),
    "CryptButton": _slot(m_crypt, "room"),
    "PrayerTempleButton": _slot(m_prayer_temple, "room"),
    "GuardRoomButton": _slot(m_guard_room, "room"),
    "DestroyRoomButton": _slot(m_destroy_room, "room"),
    "WorkshopButton": _slot(m_workshop, "room"),
    "PrisonButton": _slot(m_prison, "room"),
    "TortureButton": _slot(m_torture, "room"),
    "ArenaButton": _slot(m_arena, "room"),
    "CasinoButton": _slot(m_casino, "room"),
    "TempleButton": _slot(m_temple, "room"),
    "PortalButton": _slot(m_portal, "room"),
    "WoodenBridgeButton": _slot(m_bridge_wood, "room"),
    "StoneBridgeButton": _slot(m_bridge_stone, "room"),
    # traps
    "CannonButton": _slot(m_cannon, "trap"),
    "SpikeTrapButton": _slot(m_spike_trap, "trap"),
    "BoulderTrapButton": _slot(m_boulder, "trap"),
    "AlarmTrapButton": _slot(m_alarm_trap, "trap"),
    "FearTrapButton": _slot(m_fear_trap, "trap"),
    "GasTrapButton": _slot(m_gas_trap, "trap"),
    "LightningTrapButton": _slot(m_lightning_trap, "trap"),
    "FireburstTrapButton": _slot(m_fireburst_trap, "trap"),
    "FreezeTrapButton": _slot(m_freeze_trap, "trap"),
    "WatchBannerTrapButton": _slot(m_watch_banner_trap, "trap"),
    "TriggerTrapButton": _slot(m_trigger_trap, "trap"),
    "WavePortalButton": _slot(m_wave_portal, "trap"),
    "WoodenDoorTrapButton": _slot(m_door, "trap"),
    "IronboundDoorTrapButton": _slot(m_door_ironbound, "trap"),
    "SteelDoorTrapButton": _slot(m_door_steel, "trap"),
    "BarricadeTrapButton": _slot(m_barricade, "trap"),
    "SecretDoorTrapButton": _slot(m_door_secret, "trap"),
    "RunedDoorTrapButton": _slot(m_door_magic, "trap"),
    "DestroyTrapButton": _slot(m_destroy_trap, "trap"),
    # spells
    "SummonWorkerButton": _slot(m_worker_imp, "spell"),
    "CallToWarButton": _slot(m_call_to_war, "spell"),
    "CreatureHealButton": _slot(m_heal, "spell"),
    "CreatureExplosionButton": _slot(m_explosion, "spell"),
    "LightningButton": _slot(m_lightning_spell, "spell"),
    "TremorButton": _slot(m_tremor, "spell"),
    "DefectorButton": _slot(m_defector, "spell"),
    "PossessButton": _slot(m_possess, "spell"),
    "CreateGoldButton": _slot(m_create_gold, "spell"),
    "InfernoButton": _slot(m_inferno, "spell"),
    "HexenHenButton": _slot(m_chicken, "spell"),
    "CreatureHasteButton": _slot(m_haste, "spell"),
    "CreatureDefenseButton": _slot(m_defense, "spell"),
    "CreatureSlowButton": _slot(m_slow, "spell"),
    "CreatureStrengthButton": _slot(m_strength, "spell"),
    "CreatureWeakButton": _slot(m_weak, "spell"),
    "SpellEyeEvilButton": _slot(m_eye, "spell"),
    "SummonChampionButton": _slot(m_fighter, "spell"),
    # creatures
    "WorkerButton": _slot(m_worker, "creature"),
    "FighterButton": _slot(m_fighter, "creature"),
    # frameless small symbols (64 px)
    "GoldCoin": _sym(m_gold_coin),
    "TerritoryIcon": _sym(m_territory),
    "ManaIcon": _sym(m_mana),
    "OptionsIcon": _sym(m_gear),
    "CreaturesIcon": _sym(m_creatures),
    "HelpIcon": _sym(m_help),
    "LoadIcon": _sym(m_load),
    "SaveIcon": _sym(m_save),
    "AbortIcon": _sym(m_abort),
    "CheckIcon": _sym(m_check),
    "ObjectivesIcon": _sym(m_scroll),
    "SkillIcon": _sym(m_flask),
    "SeatIcon": _sym(m_banner),
    "CameraIcon": _sym(m_camera),
    "PlayIcon": _sym(m_play),
    "MapLightButton": _sym(m_lantern),
    # small symbols on a mini medallion: the population panel and the research states
    "CogIcon": _mini(m_gear, "room"),
    "HourglassIcon": _mini(m_hourglass, "room"),
    "HammerAnvilIcon": _mini(m_workshop, "room", 1.08),
    # navigation emblems (frameless, 128 px)
    "RoomsButton": _sym(m_house, 128),
    "SpellsButton": _sym(m_wand, 128),
    "NavigationCreatures": _sym(m_person, 128),
    "NavigationRooms": _sym(m_house, 128),
    "NavigationSpells": _sym(m_wand, 128),
    "NavigationWorkshop": _sym(m_pick_nav, 128),
    "NavigationPanel": _sym(m_chevrons, 128, extent=TALL),
    "NavigationObjectives": _sym(m_objectives_eye, 128, extent=TALL),
    "NavigationMessages": _sym(m_message, 128, extent=TALL),
    "NavigationMessagesRead": _sym(m_message_read, 128, extent=TALL),
    "MenuReturn": _sym(m_return, 64),
}
