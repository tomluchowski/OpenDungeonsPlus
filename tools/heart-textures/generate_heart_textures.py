#!/usr/bin/env python3
"""Generates the textures of the dungeon heart (materials/textures/DungeonHeart*.png).

The heart mesh (tools/heart-on-temple) is mapped by a spherical projection around its x axis onto the upper part
of the image; the lower part holds the walls of the closed vessel stubs. Every texel of the body knows
its point on the heart's surface (taken from the mesh), so the veins, cracks and injuries are painted in 3D and
match the geometry that belongs to them. Everything is procedural and seeded, running the script again gives
byte-identical files.

  DungeonHeart<Tier>.png        colour: crimson muscle with dark veins; the injured tiers get bruises, scars,
                                cracks and a little blood, but keep the colours of the heart
  DungeonHeart<Tier>Normal.png  tangent space normal map (fibres, veins, creases, cracks, scars, wounds)
  DungeonHeart<Tier>Glow.png    faint embers in cracks and wounds, added on top of the lit surface

Usage: python generate_heart_textures.py [output folder]
Needs numpy and Pillow.
"""

import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'heart-on-temple'))
import heart_shape as hs  # noqa: E402
import generate_heart_geometry as gg  # noqa: E402

WIDTH = 1024
HEIGHT = 512
BODY_ROWS = int(round(hs.BODY_V * HEIGHT))
SEED = 11

# Colours (the palette follows the heart icon of the HUD: deep red with darker veins)
DARK = np.array([0.20, 0.020, 0.035])
MID = np.array([0.52, 0.075, 0.085])
LIGHT = np.array([0.72, 0.18, 0.16])
VEIN = np.array([0.14, 0.010, 0.030])
VEIN_EDGE = np.array([0.72, 0.13, 0.12])
GROOVE = np.array([0.20, 0.020, 0.035])
BRUISE = np.array([0.30, 0.035, 0.17])
BLOOD = np.array([0.42, 0.000, 0.010])
BLOOD_EDGE = np.array([0.90, 0.14, 0.12])
SCAR = np.array([0.70, 0.36, 0.32])
THREAD = np.array([0.10, 0.045, 0.045])
CRACK = np.array([0.075, 0.012, 0.015])
ASH = np.array([0.30, 0.11, 0.10])
EMBER = np.array([1.00, 0.34, 0.06])
SINEW = np.array([0.78, 0.40, 0.32])
SINEW_DARK = np.array([0.42, 0.11, 0.11])
VEIN_GLOW = np.array([0.85, 0.10, 0.05])

# Per tier: tone towards ash, brightness, amount of cracks, brightness of the embers
TIER_LOOK = {
    'Healthy': (0.00, 1.00, 0.00, 0.35),
    'Damaged': (0.22, 0.95, 0.50, 0.25),
    'Critical': (0.40, 0.88, 0.80, 0.40),
}


def clamp01(x):
    return np.clip(x, 0.0, 1.0)


def colour_of(rgb, n):
    return np.broadcast_to(rgb, (n, 3))


def mix(a, b, t):
    return a * (1.0 - t[:, None]) + b * t[:, None]


def hash3(ix, iy, iz, seed):
    h = (ix.astype(np.uint64) * np.uint64(374761393)) ^ (iy.astype(np.uint64) * np.uint64(668265263)) \
        ^ (iz.astype(np.uint64) * np.uint64(2147483629)) ^ np.uint64(seed * 1274126177 & 0xFFFFFFFF)
    h &= np.uint64(0xFFFFFFFF)
    h = ((h ^ (h >> np.uint64(13))) * np.uint64(1274126177)) & np.uint64(0xFFFFFFFF)
    h ^= h >> np.uint64(16)
    return (h & np.uint64(0xFFFFFF)).astype(np.float64) / 16777215.0


def noise3(p, seed=0):
    """Value noise in [0, 1] at the points p (N x 3)."""
    base = np.floor(p)
    f = p - base
    i = base.astype(np.int64) + 1000
    u = f * f * (3.0 - 2.0 * f)
    result = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (u[:, 0] if dx else 1.0 - u[:, 0]) * (u[:, 1] if dy else 1.0 - u[:, 1]) \
                    * (u[:, 2] if dz else 1.0 - u[:, 2])
                result = result + w * hash3(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return result


def fbm3(p, octaves, seed=0, gain=0.5):
    total = 0.0
    amplitude = 1.0
    norm = 0.0
    for octave in range(octaves):
        total = total + amplitude * noise3(p * (2.0 ** octave), seed + octave * 17)
        norm += amplitude
        amplitude *= gain
    return total / norm


def worley_edges(p, seed=0):
    """F2 - F1 of a 3D cellular noise: small along the boundaries of the cells (a network of thin lines)."""
    base = np.floor(p)
    f1 = np.full(len(p), 9.0)
    f2 = np.full(len(p), 9.0)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                cell = base + np.array([dx, dy, dz])
                i = cell.astype(np.int64) + 1000
                point = cell + np.stack([hash3(i[:, 0], i[:, 1], i[:, 2], seed + 1),
                                         hash3(i[:, 0], i[:, 1], i[:, 2], seed + 2),
                                         hash3(i[:, 0], i[:, 1], i[:, 2], seed + 3)], axis=1)
                d = np.linalg.norm(p - point, axis=1)
                closer = d < f1
                f2 = np.where(closer, f1, np.minimum(f2, d))
                f1 = np.where(closer, d, f1)
    return f2 - f1


def rasterise(image, covered, uv, values):
    """Interpolates the vertex values of one triangle over its texels (u wraps around the image)."""
    u = uv[:, 0] * WIDTH
    v = uv[:, 1] * HEIGHT
    x0, x1 = int(np.floor(u.min())), int(np.ceil(u.max()))
    y0, y1 = max(int(np.floor(v.min())), 0), min(int(np.ceil(v.max())), HEIGHT - 1)
    gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    area = (u[1] - u[0]) * (v[2] - v[0]) - (u[2] - u[0]) * (v[1] - v[0])
    if abs(area) < 1.0e-9:
        return
    w0 = ((u[1] - gx) * (v[2] - gy) - (u[2] - gx) * (v[1] - gy)) / area
    w1 = ((u[2] - gx) * (v[0] - gy) - (u[0] - gx) * (v[2] - gy)) / area
    w2 = 1.0 - w0 - w1
    inside = (w0 >= -0.03) & (w1 >= -0.03) & (w2 >= -0.03)
    if not inside.any():
        return
    px = gx[inside].astype(int) % WIDTH
    py = gy[inside].astype(int)
    image[py, px] = w0[inside][:, None] * values[0] + w1[inside][:, None] * values[1] + w2[inside][:, None] * values[2]
    covered[py, px] = True


def fill_holes(image, covered):
    """Copies values into texels no triangle reached (next to the seam and the poles)."""
    image = image.copy()
    covered = covered.copy()
    for _ in range(4):
        if covered.all():
            break
        for shift, axis in ((1, 0), (-1, 0), (1, 1), (-1, 1)):
            neighbour = np.roll(image, shift, axis=axis)
            neighbour_covered = np.roll(covered, shift, axis=axis)
            take = (~covered) & neighbour_covered
            image[take] = neighbour[take]
            covered = covered | take
    return image


def body_positions(heart):
    """The point of the heart's surface (local coordinates) of every texel of the body part of the image."""
    dirs, tris = gg.icosphere(gg.LEVEL)
    dirs = dirs @ gg.rotation(0.043, 0.061).T
    pos = hs.surface(dirs, heart.f)
    u, v = hs.uv_from_direction(pos)
    image = np.zeros((HEIGHT, WIDTH, 3))
    covered = np.zeros((HEIGHT, WIDTH), dtype=bool)
    for tri in tris:
        uu = u[tri].copy()
        if uu.max() - uu.min() > 0.5:
            uu = np.where(uu < 0.5, uu + 1.0, uu)
        rasterise(image, covered, np.stack([uu, v[tri]], axis=1), pos[tri])
    image = fill_holes(image, covered)
    return image[:BODY_ROWS].reshape(-1, 3)


def paint_body(tier, heart, p):
    """Colour, height (in tile units) and glow of the body texels p (N x 3, local surface points) for one tier."""
    ash, dark, crack_amount, ember_amount = TIER_LOOK[tier]
    n = len(p)
    injuries = hs.INJURIES[tier]

    # Muscle: large mottling, fibres running slanted along the height, fine grain
    mottle = fbm3(p * 2.2, 4, SEED)
    fibres = fbm3(np.stack([p[:, 0] * 9.0 + 3.0 * p[:, 2], p[:, 1] * 9.0 - 2.0 * p[:, 2], p[:, 2] * 2.0], axis=1), 3, SEED + 40)
    grain = fbm3(p * 24.0, 2, SEED + 80)
    t = clamp01((mottle - 0.30) / 0.42) * 0.75 + 0.25 * fibres
    colour = mix(colour_of(DARK, n), colour_of(MID, n), clamp01(t * 1.6))
    colour = mix(colour, colour_of(LIGHT, n), clamp01((t - 0.62) * 2.2) * 0.55)
    colour = colour * (0.78 + 0.44 * grain)[:, None]
    height = 0.012 * (fibres - 0.5) + 0.006 * (grain - 0.5) + 0.016 * (mottle - 0.5)
    glow = np.zeros((n, 3))

    # Grooves between the chambers
    groove = np.zeros(n)
    for radius, points in heart.grooves:
        d, _ = hs.dist_polyline(p, points)
        groove = np.maximum(groove, 1.0 - hs.smoothstep(radius * 0.6, radius * 2.2, d))
    colour = mix(colour, colour_of(GROOVE, n), groove * 0.95)
    height -= 0.02 * groove

    # Veins: dark ridges with a lighter edge (the ridges themselves are geometry, this is their paint)
    vein_mask = np.zeros(n)
    vein_edge = np.zeros(n)
    for r0, r1, control in hs.VEINS:
        path = hs.path_points(heart.f, control, gg.VEIN_SAMPLES)
        d, along = hs.dist_polyline(p, path)
        radius = (r0 + (r1 - r0) * along) * gg.VEIN_SCALE
        vein_mask = np.maximum(vein_mask, 1.0 - hs.smoothstep(radius * 0.55, radius * 0.95, d))
        vein_edge = np.maximum(vein_edge, (1.0 - hs.smoothstep(radius * 0.95, radius * 1.9, d)) * (d > radius * 0.7))
    colour = mix(colour, colour_of(VEIN_EDGE, n), vein_edge * 0.30)
    colour = mix(colour, colour_of(VEIN, n), vein_mask * 0.97)
    height += 0.02 * vein_mask

    # Glow of the blood in the veins, seen through the wrapping
    glow += VEIN_GLOW[None, :] * (vein_mask * (0.5 + 0.5 * mottle) * (0.6 if tier == 'Healthy' else 0.4))[:, None]

    # Sinew strands wrapped round the body: pale fibrous bands with dark edges (the bands themselves are geometry)
    strand_mask = np.zeros(n)
    strand_edge = np.zeros(n)
    for path in gg.strand_paths(heart):
        d, along = hs.dist_polyline(p, path)
        taper = 0.35 + 0.65 * np.clip(4.0 * np.minimum(along, 1.0 - along), 0.0, 1.0)
        width = gg.STRAND_WIDTH * taper
        strand_mask = np.maximum(strand_mask, 1.0 - hs.smoothstep(width * 0.55, width * 0.95, d))
        strand_edge = np.maximum(strand_edge, (1.0 - hs.smoothstep(width * 0.95, width * 1.7, d)) * (d > width * 0.6))
    threads = fbm3(np.stack([p[:, 0] * 30.0, p[:, 1] * 30.0, p[:, 2] * 30.0], axis=1), 2, SEED + 60)
    colour = mix(colour, colour_of(SINEW_DARK, n), strand_edge * 0.75)
    colour = mix(colour, mix(colour_of(SINEW, n), colour_of(SINEW_DARK, n), clamp01(threads * 1.6 - 0.3)), strand_mask * 0.92)
    height += 0.02 * strand_mask + 0.008 * strand_mask * (threads - 0.5) - 0.008 * strand_edge
    glow *= (1.0 - 0.7 * strand_mask)[:, None]

    # Capillaries: thin dark lines
    capillary = (1.0 - hs.smoothstep(0.012, 0.045, worley_edges(p * 9.0, SEED + 100))) \
        * hs.smoothstep(0.42, 0.62, fbm3(p * 3.0, 2, SEED + 120)) * 0.8
    colour = mix(colour, colour_of(VEIN, n), capillary * 0.55)
    height -= 0.004 * capillary

    # Cracks with embers (the healthy heart has only a few hairline ones)
    edges = worley_edges(p * 4.2, SEED + 200)
    zone = fbm3(p * 1.7, 3, SEED + 210)
    if tier == 'Healthy':
        crack = (1.0 - hs.smoothstep(0.006, 0.022, edges)) * hs.smoothstep(0.76, 0.86, zone) * 0.7
    else:
        crack = (1.0 - hs.smoothstep(0.018, 0.070, edges)) \
            * hs.smoothstep(0.62 - 0.30 * crack_amount, 0.74 - 0.30 * crack_amount, zone) * crack_amount
    colour = mix(colour, colour_of(CRACK, n), crack * 0.95)
    height -= 0.014 * crack
    glow += EMBER[None, :] * (crack * ember_amount * 0.35)[:, None]

    # Tone of the tier: duller, browner, darker (the injuries below keep their own colours)
    colour = mix(colour, colour_of(ASH, n), np.full(n, ash)) * dark

    # Injuries: dents (in the geometry), bruises, cuts, scars with stitches
    for centre, radius, depth in heart.dent_points:
        d = np.linalg.norm(p - centre, axis=1)
        rim = 1.0 - hs.smoothstep(radius * 0.8, radius * 1.5, d)
        inner = 1.0 - hs.smoothstep(radius * 0.3, radius * 0.95, d)
        colour = mix(colour, colour_of(BRUISE, n), rim * 0.5)
        colour = mix(colour, colour_of(BLOOD, n), inner * 0.55)
        height += 0.008 * rim
    for centre_dir, radius in injuries['bruises']:
        centre = hs.surface_point([centre_dir], heart.f)[0]
        d = np.linalg.norm(p - centre, axis=1)
        wobble = fbm3(p * 7.0, 3, SEED + 300)
        radius = radius * 0.62
        mask = clamp01((1.0 - hs.smoothstep(radius * 0.7, radius * (1.3 + 0.4 * wobble), d)) * (0.95 + 0.35 * wobble))
        colour = mix(colour, colour_of(BRUISE, n), mask)
        core = clamp01((1.0 - hs.smoothstep(radius * 0.25, radius * 0.75, d)) * mask)
        colour = mix(colour, colour_of(BRUISE, n) * 0.45, core * 0.7)
    for points in heart.slit_points:
        d, _ = hs.dist_polyline(p, points)
        raw = 1.0 - hs.smoothstep(0.030, 0.11, d)
        core = 1.0 - hs.smoothstep(0.014, 0.045, d)
        colour = mix(colour, colour_of(BLOOD, n), raw * 0.9)
        colour = mix(colour, colour_of(CRACK, n), core)
        height -= 0.02 * core
        glow += EMBER[None, :] * (core * ember_amount * 0.15)[:, None]
    for control in injuries['scars']:
        line = hs.path_points(heart.f, control, 24)
        d, along = hs.dist_polyline(p, line)
        ridge = 1.0 - hs.smoothstep(0.022, 0.045, d)
        colour = mix(colour, colour_of(SCAR, n), ridge * 0.85)
        height += 0.012 * ridge
        length = np.linalg.norm(np.diff(line, axis=0), axis=1).sum()
        tick = np.abs(((along * length) / 0.07) % 1.0 - 0.5) < 0.10
        stitch = tick * (d < 0.07) * (d > 0.006)
        colour = mix(colour, colour_of(THREAD, n), stitch * 0.9)
        height += 0.004 * stitch

    # Blood: trickles running down from the wounds and a wet spot at each (no pools)
    blood = np.zeros(n)
    wet = np.zeros(n)
    length = {'Damaged': 0.55, 'Critical': 0.80}.get(tier, 0.0)
    for w in heart.wound_points():
        # across the trickle: along x on the front and back of the heart, along y on its sides
        across_axis = 0 if abs(w[1]) > abs(w[0]) else 1
        horizontal = np.abs(p[:, across_axis] - w[across_axis])
        down = w[2] - p[:, 2]
        near = np.linalg.norm(p - w, axis=1)
        wobble = 0.012 * np.sin(down * 38.0 + w[0] * 20.0)
        width = 0.075 * (1.0 - 0.40 * clamp01(down / length))
        streak = (1.0 - hs.smoothstep(width * 0.6, width, np.abs(horizontal - wobble))) \
            * (down > 0.0) * (down < length) * hs.smoothstep(length, length * 0.6, down)
        drop = 1.0 - hs.smoothstep(0.045, 0.085, np.hypot(horizontal - wobble, down - length * 0.92))
        spot = 1.0 - hs.smoothstep(0.07, 0.14, near)
        glint = (1.0 - hs.smoothstep(0.004, 0.014, np.abs(horizontal - wobble - 0.02))) * (down > 0.04)             * (down < length * 0.85)
        wet = np.maximum(wet, glint * streak)
        blood = np.maximum(blood, np.maximum(np.maximum(streak, drop), spot))
    colour = mix(colour, colour_of(BLOOD, n), blood * 0.95)
    # wet look: a bright rim along the edge of the blood and a glint down the middle of the trickles
    colour = mix(colour, colour_of(BLOOD_EDGE, n), blood * (1.0 - hs.smoothstep(0.35, 0.8, blood)) * 0.8)
    colour = mix(colour, colour_of(BLOOD_EDGE, n), wet * 0.5)
    height += 0.003 * blood

    return colour, height, glow


def paint_bands():
    """Rows below the body: the walls of the vessel stubs and the sinew bands wrapped round them (colour, height, glow)."""
    rows = HEIGHT - BODY_ROWS
    ys, xs = np.mgrid[BODY_ROWS:HEIGHT, 0:WIDTH]
    angle = xs / WIDTH * 2.0 * np.pi
    circle = np.stack([np.cos(angle), np.sin(angle)], axis=-1).reshape(-1, 2)
    v = (ys.reshape(-1) + 0.5) / HEIGHT
    n = len(v)
    fibres = fbm3(np.stack([circle[:, 0] * 6.0, circle[:, 1] * 6.0, v * 3.0], axis=1), 3, SEED + 500)
    grain = fbm3(np.stack([circle[:, 0] * 14.0, circle[:, 1] * 14.0, v * 30.0], axis=1), 2, SEED + 510)
    streaks = noise3(np.stack([circle[:, 0] * 10.0, circle[:, 1] * 10.0, np.zeros(n)], axis=1), SEED + 520)
    stripes = clamp01(np.abs(streaks - 0.5) * 6.0)
    colour = mix(colour_of(DARK, n), colour_of(LIGHT, n), clamp01(fibres * 1.5 - 0.1))
    colour = mix(colour_of(VEIN, n), colour, 0.35 + 0.65 * stripes)
    sinew = v >= hs.SINEW_V[0] - 0.005
    band = mix(colour_of(SINEW_DARK, n), colour_of(SINEW, n), clamp01(grain * 1.6))
    colour = np.where(sinew[:, None], band, colour)
    height = 0.008 * (fibres - 0.5) + 0.004 * (grain - 0.5)
    glow = np.zeros((n, 3))
    return colour.reshape(rows, WIDTH, 3), height.reshape(rows, WIDTH), glow.reshape(rows, WIDTH, 3)


def normal_map(height, strength):
    """Tangent space normal map; the image's rows run along +v (down), +u to the right."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    nx = -dx * strength
    ny = -dy * strength
    nz = np.ones_like(nx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / length, ny / length, nz / length], axis=2) * 0.5 + 0.5


def to_png(array, path):
    data = (np.clip(array, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    Image.fromarray(data, 'RGB').save(path, optimize=True)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'materials', 'textures')
    band_colour, band_height, band_glow = paint_bands()
    for tier in hs.TIERS:
        heart = hs.Heart(tier)
        colour, height, glow = paint_body(tier, heart, body_positions(heart))
        image = np.zeros((HEIGHT, WIDTH, 3))
        heights = np.zeros((HEIGHT, WIDTH))
        glows = np.zeros((HEIGHT, WIDTH, 3))
        image[:BODY_ROWS] = colour.reshape(BODY_ROWS, WIDTH, 3)
        heights[:BODY_ROWS] = height.reshape(BODY_ROWS, WIDTH)
        glows[:BODY_ROWS] = glow.reshape(BODY_ROWS, WIDTH, 3)
        image[BODY_ROWS:] = band_colour
        heights[BODY_ROWS:] = band_height
        glows[BODY_ROWS:] = band_glow
        to_png(image, os.path.join(out, 'DungeonHeart%s.png' % tier))
        to_png(normal_map(heights, 110.0), os.path.join(out, 'DungeonHeart%sNormal.png' % tier))
        to_png(glows, os.path.join(out, 'DungeonHeart%sGlow.png' % tier))
        print('%s done' % tier)


if __name__ == '__main__':
    main()
