#!/usr/bin/env python3
"""Generates the floor textures of the hatchery, library, dormitory, dungeon temple, treasury, crypt, training hall,
casino, prison, arena, torture chamber, workshop, portal and portal wave, plus the UV atlas of the wooden bridge
(materials/textures/).

Original work of the project, licence CC0. Everything is procedural and seeded; running the script again gives
byte-identical files. Needs numpy and Pillow.

Every room is built from one base field (the open floor) plus decoration bands that are laid along the exposed
(wall) sides of a piece. All pieces of a room therefore share the very same base pixels, so the seams between
pieces match. The tile borders are made seam-free in two ways:
  - library and dormitory: slabs / planks are separated by gaps that run along the tile border and the tile
    centre lines, the gaps are the same on both sides of a border;
  - hatchery, training hall, casino, prison: every layer is truly periodic (FFT noise on a wrapping grid, loose
    objects are drawn with all tile offsets so they continue on the opposite side), so there is no border band at
    all; the seamless wrap is checked with --check and --seamcheck (texture rolled by half a tile);
  - dungeon temple: irregular slabs (random rectangle subdivision) inside a gap that runs along
    the tile border, so nothing but the constant gap colour touches the border;
  - treasury: marble checkerboard, 4x4 squares per tile with grout along the tile border, four variants; the arena
    uses the former treasury flagstones;
  - dungeon temple, open hatchery piece: four variants each (the temple's ember cracks and the hatchery's straw
    clumps stay away from the tile border, the rest is one shared periodic base), picked at random per tile
    through [oneOf] in config/tilesets.cfg, so no motif repeats from tile to tile;
  - crypt: derived from the original cobble texture (Yughues, CC0) read from git history (commit 2ac74a729^);
    darkened, moss and lichen and cracks are added with periodic noise that fades out at the tile border;
  - prison: derived from the original Prison.png read from git history (commit 6db9aa611), mortar turned to mud,
    plus rust and straw;
  - arena, torture, workshop: every layer is truly periodic (see training hall); the torture slabs and the
    workshop planks come from a wrapping row layout with warped joints.
  - portal, portal wave: every layer is truly periodic (see training hall); slabs from a wrapping row layout with
    hairline rune grooves (portal) or concentric wave grooves on the torus (portal wave);
  - wooden bridge: the UV atlas WoodBridge.png is painted from scratch (one plank per mesh UV quad, nail heads,
    dirt, cracks); the UV layout is read from models/WoodBridge.mesh, WoodBridgeMask.png is not touched.
The room shader has no specular term, so only a diffuse texture and a matching tangent space normal map
(red = -d height / dx, green = +d height / dy, image y pointing down) are written.

Usage: python generate_room_floors.py [output folder] [--check] [--seamcheck=<folder for the rolled previews>]
"""

import heapq
import io
import os
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

N = 512
SUPER = 2
MARGIN = 26


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def mix(a, b, t):
    if np.ndim(t) == 2 and np.ndim(a) == 3:
        t = t[..., None]
    return a * (1.0 - t) + b * t


_IDX = np.arange(N)
_EDGE = np.minimum(_IDX, N - 1 - _IDX)
_LO = np.minimum(_EDGE[:, None], _EDGE[None, :])
_HI = np.maximum(_EDGE[:, None], _EDGE[None, :])
_BORDER_W = 1.0 - smoothstep(2.0, 28.0, _LO.astype(float))
_YY, _XX = np.mgrid[0:N, 0:N]


def fbm(seed, fx, fy=None, octaves=3):
    """Periodic noise with zero mean and unit variance; fx/fy are the feature counts per tile."""
    if fy is None:
        fy = fx
    rng = np.random.RandomState(seed)
    k = np.fft.fftfreq(N) * N
    kx = k[None, :]
    ky = k[:, None]
    total = np.zeros((N, N))
    for octave in range(octaves):
        white = rng.standard_normal((N, N))
        scale = 2.0 ** octave
        filt = np.exp(-(kx / (fx * scale)) ** 2 - (ky / (fy * scale)) ** 2)
        layer = np.fft.ifft2(np.fft.fft2(white) * filt).real
        layer /= layer.std() + 1e-9
        total += layer * 0.55 ** octave
    return total / (total.std() + 1e-9)


def nz(seed, fx, fy=None, octaves=3):
    """fbm that is mirror and transpose symmetric at the tile border (see module docstring)."""
    n = fbm(seed, fx, fy, octaves)
    return n * (1.0 - _BORDER_W) + n[_LO, _HI] * _BORDER_W


_FADE = smoothstep(0.0, 6.0, _LO.astype(float))


def pn(seed, fx, fy=None, octaves=3):
    """Truly periodic noise (no mirroring) that is faded to zero in a thin strip along the tile border, so that
    border pixels do not depend on the rotation of the piece."""
    return fbm(seed, fx, fy, octaves) * _FADE


def normal_map(height, strength):
    gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    gx *= strength / (gx.std() + 1e-9) * 0.30
    gy *= strength / (gy.std() + 1e-9) * 0.30
    nrm = np.stack([-gx, gy, np.ones_like(gx)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    return nrm * 0.5 + 0.5


# Calibrated tone correction: per room (saturation factor, brightness
# factor, colour tint), applied to the finished diffuse colours. The room shader raises saturation (x1.3) and the warm room
# lights push orange tones further, so the raw painted colours of these floors came out too bright and too colourful
# in the game's lighting. Rooms that are not listed are left as painted.
TONE = {
    'library': (0.36, 0.72, (1.03, 0.96, 1.04)),
    'hatchery': (0.15, 0.72, (1.09, 1.00, 0.85)),
    'dormitory': (0.22, 1.14, (1.10, 1.00, 0.88)),
    'treasury': (1.00, 0.76, (0.93, 0.95, 1.06)),
    'trainingHall': (0.50, 0.70, (1.06, 1.00, 0.88)),
    'casino': (0.22, 0.90, (1.03, 1.00, 1.03)),
    'workshop': (0.50, 1.00, (1.00, 1.00, 1.12)),
    'bridgeWooden': (0.45, 0.80, (1.08, 1.00, 0.92)),
}


# Repetition pass: a texture repeats on every tile, so broad blotches
# and brightness gradients show up as a lattice. flatten() divides the colours by a periodic low-pass of their
# luminance (sigma in texture px, strength 0..1), so nothing larger than about a fifth of a tile keeps a different
# mean brightness; the mean luminance stays. Applied to the open floor field before the wall bands are added.
FLATTEN = {
    'dormitory': (48.0, 0.6), 'casino': (48.0, 0.5),
    'crypt': (40.0, 0.8), 'trainingHall': (48.0, 0.8), 'prison': (40.0, 0.8),
    'torture': (48.0, 0.7), 'workshop': (48.0, 0.7), 'portal': (48.0, 0.7),
    'portalWave': (48.0, 0.7),
}


def flatten(arr, sigma, strength):
    lum = (arr * np.array([0.2126, 0.7152, 0.0722])).sum(-1)
    low = blur(lum, sigma)
    ratio = np.clip(lum.mean() / np.maximum(low, 1e-4), 0.6, 1.6) ** strength
    return arr * ratio[..., None]


def tone(arr, room):
    if room not in TONE:
        return arr
    sat, gain, tint = TONE[room]
    lum = (arr * np.array([0.2126, 0.7152, 0.0722])).sum(-1, keepdims=True)
    return (lum + (arr - lum) * sat) * np.array(tint) * gain


def to_image(arr):
    return Image.fromarray((np.clip(arr, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), 'RGB')


# ---------------------------------------------------------------------------------------------------------------
# loose objects (straw, feathers), drawn supersampled with Pillow

class Sprites(object):
    def __init__(self, wrap=False):
        # wrap: objects crossing a tile edge continue on the opposite side (drawn with all tile offsets)
        self.offsets = [(dx, dy) for dx in (-N, 0, N) for dy in (-N, 0, N)] if wrap else [(0, 0)]
        self.colour = Image.new('RGBA', (N * SUPER, N * SUPER), (0, 0, 0, 0))
        self.height = Image.new('L', (N * SUPER, N * SUPER), 0)
        self.cdraw = ImageDraw.Draw(self.colour)
        self.hdraw = ImageDraw.Draw(self.height)

    def line(self, p0, p1, width, rgb, lift, alpha=255):
        for ox, oy in self.offsets:
            pts = [((p0[0] + ox) * SUPER, (p0[1] + oy) * SUPER), ((p1[0] + ox) * SUPER, (p1[1] + oy) * SUPER)]
            self.cdraw.line(pts, fill=tuple(int(c * 255) for c in rgb) + (alpha,), width=max(1, int(width * SUPER)))
            self.hdraw.line(pts, fill=int(lift * 255), width=max(1, int(width * SUPER)))

    def polygon(self, pts, rgb, lift):
        for ox, oy in self.offsets:
            shifted = [((x + ox) * SUPER, (y + oy) * SUPER) for x, y in pts]
            self.cdraw.polygon(shifted, fill=tuple(int(c * 255) for c in rgb) + (255,))
            self.hdraw.polygon(shifted, fill=int(lift * 255))

    def result(self):
        rgba = np.asarray(self.colour.resize((N, N), Image.BOX)).astype(float) / 255.0
        hgt = np.asarray(self.height.resize((N, N), Image.BOX)).astype(float) / 255.0
        return rgba[..., :3], rgba[..., 3], hgt


def straw_segments(seed, count, box, length, angle=None, spread=0.5):
    """Random straw segments inside box=(x0, y0, x1, y1) (canonical, before mapping to a side)."""
    rng = np.random.RandomState(seed)
    segs = []
    for _ in range(count):
        cx = rng.uniform(box[0], box[2])
        cy = rng.uniform(box[1], box[3])
        ang = rng.uniform(0, np.pi) if angle is None else angle + rng.uniform(-spread, spread)
        half = rng.uniform(length[0], length[1]) * 0.5
        dx, dy = np.cos(ang) * half, np.sin(ang) * half
        segs.append((cx - dx, cy - dy, cx + dx, cy + dy, rng.uniform(0.85, 1.25), rng.uniform(0.75, 1.15)))
    return segs


def draw_straws(sprites, segs, rgb, mapper=None):
    for x0, y0, x1, y1, width, tone in segs:
        p0, p1 = (x0, y0), (x1, y1)
        if mapper is not None:
            p0, p1 = mapper(*p0), mapper(*p1)
        tint = np.array(rgb) * tone
        sprites.line(p0, p1, width * 1.6, np.clip(tint, 0, 1), 0.55 + 0.3 * tone)
        sprites.line(p0, p1, width * 0.5, np.clip(tint * 1.25, 0, 1), 0.85)


def side_mapper(side):
    """Maps canonical band coordinates (along, depth from the wall) to tile pixels for one exposed side."""
    return {'T': lambda a, d: (a, d), 'B': lambda a, d: (a, N - 1 - d),
            'L': lambda a, d: (d, a), 'R': lambda a, d: (N - 1 - d, a)}[side]


def depth_map(side):
    return {'T': _YY, 'B': N - 1 - _YY, 'L': _XX, 'R': N - 1 - _XX}[side].astype(float)


def overlay(col, hgt, sprites, height_gain):
    sc, sa, sh = sprites.result()
    col = mix(col, sc, sa)
    hgt = hgt + (sh - 0.0) * sa * height_gain
    return col, hgt


# ---------------------------------------------------------------------------------------------------------------
# straw bundles (hatchery, dormitory): thick, bright strands that stay readable at game camera height

def straw_bundle(sp, rng, cx, cy, ang, count, length, width, rgb, mapper=None, fan=0.0):
    """A loose bundle (fan = 0, parallel strands) or a tuft (fan > 0, strands spreading from one point) of `count`
    slightly bent straw strands around (cx, cy); every strand is a soft shadow, a body and a lighter highlight.
    Widths are in texture pixels (one game tile is about 140 screen px for 512 texture px)."""
    strands = []
    for _ in range(count):
        half = rng.uniform(length[0], length[1]) * 0.5
        if fan > 0.0:
            a = ang + rng.uniform(-fan, fan)
            p0 = (cx + rng.uniform(-4.0, 4.0), cy + rng.uniform(-4.0, 4.0))
            p1 = (p0[0] + np.cos(a) * half * 2.0, p0[1] + np.sin(a) * half * 2.0)
        else:
            off = rng.uniform(-8.0, 8.0)
            shift = rng.uniform(-9.0, 9.0)
            a = ang + rng.uniform(-0.22, 0.22)
            mx = cx - np.sin(ang) * off + np.cos(ang) * shift
            my = cy + np.cos(ang) * off + np.sin(ang) * shift
            p0 = (mx - np.cos(a) * half, my - np.sin(a) * half)
            p1 = (mx + np.cos(a) * half, my + np.sin(a) * half)
        bend = rng.uniform(-4.5, 4.5)
        pm = ((p0[0] + p1[0]) * 0.5 - np.sin(a) * bend, (p0[1] + p1[1]) * 0.5 + np.cos(a) * bend)
        pts = [p0, pm, p1]
        if mapper is not None:
            pts = [mapper(*q) for q in pts]
        strands.append((pts, rng.uniform(0.82, 1.15), rng.uniform(0.85, 1.2)))
    shadow = (0.07, 0.05, 0.03)
    for pts, tn, wd in strands:
        for q0, q1 in ((pts[0], pts[1]), (pts[1], pts[2])):
            sp.line((q0[0] + 2.0, q0[1] + 3.0), (q1[0] + 2.0, q1[1] + 3.0), width * wd + 3.0, shadow, 0.3, 150)
    for pts, tn, wd in strands:
        for q0, q1 in ((pts[0], pts[1]), (pts[1], pts[2])):
            sp.line(q0, q1, width * wd, np.clip(np.array(rgb) * tn, 0, 1), 0.7)
            sp.line(q0, q1, max(1.5, width * wd * 0.38), np.clip(np.array(rgb) * tn * 1.16, 0, 1), 0.95)


def tuft_stalks(rng, cx, cy, ang, count, length, spread):
    stalks = []
    for _ in range(count):
        if rng.uniform() < 0.72:
            a = ang + rng.normal(0.0, spread * 0.45) + (np.pi if rng.uniform() < 0.5 else 0.0)
        else:
            a = ang + rng.uniform(-1.4, 1.4) + (np.pi if rng.uniform() < 0.5 else 0.0)
        ln = rng.uniform(length[0], length[1])
        lat = rng.uniform(-12.0, 12.0)
        bx, by = cx - np.sin(ang) * lat, cy + np.cos(ang) * lat
        start = -ln * rng.uniform(0.35, 0.65)
        bend = rng.uniform(-4.0, 4.0)
        pts = []
        for t in (0.0, 0.33, 0.66, 1.0):
            al = start + t * ln
            off = bend * np.sin(np.pi * t)
            pts.append((bx + np.cos(a) * al - np.sin(a) * off, by + np.sin(a) * al + np.cos(a) * off))
        stalks.append((pts, rng.uniform(0.80, 1.16), rng.uniform(0.85, 1.25)))
    return stalks


def straw_tuft(sp, rng, cx, cy, ang, count, length, width, rgb, mapper=None, spread=0.95):
    """A heap of thin straw stalks (chicken farm): most stalks lie roughly in one direction, a few cross them, all
    overlapping around a darker core; every stalk tapers towards both ends, has its own shade of golden brown and a
    soft shadow, and there is a dark soft patch under the core. Stalks near the centre are darker, the tips lighter
    and sparse, so the outline is ragged and not a solid blob. `spread` (rad) is the scatter of the direction."""
    def mp(q):
        return mapper(*q) if mapper is not None else q
    stalks = tuft_stalks(rng, cx, cy, ang, count, length, spread)
    if sp is None:
        return stalks
    for wd, al in ((1.0, 26), (0.78, 30), (0.55, 34), (0.32, 38)):
        d = length[1] * 0.22
        sp.line(mp((cx - np.cos(ang) * d, cy - np.sin(ang) * d)), mp((cx + np.cos(ang) * d, cy + np.sin(ang) * d)),
                14.0 + 22.0 * wd, (0.05, 0.035, 0.02), 0.0, al)
    taper = (1.0, 0.85, 0.5)
    for pts, tn, wd in stalks:
        for i in range(3):
            q0, q1 = mp(pts[i]), mp(pts[i + 1])
            w = width * taper[i] * wd
            sp.line((q0[0] + 1.5, q0[1] + 2.5), (q1[0] + 1.5, q1[1] + 2.5), w + 1.5, (0.05, 0.035, 0.02), 0.0, 100)
    for pts, tn, wd in stalks:
        for i in range(3):
            q0, q1 = mp(pts[i]), mp(pts[i + 1])
            dist = np.hypot(pts[i][0] - cx, pts[i][1] - cy)
            core = 0.74 + 0.26 * np.clip(dist / (length[1] * 0.5), 0.0, 1.0)
            w = width * taper[i] * wd
            col = np.clip(np.array(rgb) * tn * core, 0, 1)
            sp.line(q0, q1, w, col, 0.6)
            sp.line(q0, q1, max(1.0, w * 0.3), np.clip(col * 1.15, 0, 1), 0.85)


def scatter_straw(sp, seed, bundles, loose, box, rgb, mapper=None, bundle_len=(54, 88), loose_len=(40, 72),
                  width=5.0, angle=None, spread=0.6, fan=0.55, blue=False):
    rng = np.random.RandomState(seed)
    total = bundles + loose
    w, h = box[2] - box[0], box[3] - box[1]
    ny = max(1, int(round(np.sqrt(total * h / float(w)))))
    nx = int(np.ceil(total / float(ny)))
    cells = rng.permutation(nx * ny)[:total]  # jittered grid: spread evenly, no empty areas and no heaps
    centres = blue_noise_points(seed + 5, total, 0) if blue else None  # wrapping blue noise: no lattice at all
    for i in range(total):
        if blue:
            cx, cy = centres[i]
        else:
            cx = box[0] + (cells[i] % nx + 0.5 + rng.uniform(-0.45, 0.45)) * w / nx
            cy = box[1] + (cells[i] // nx + 0.5 + rng.uniform(-0.45, 0.45)) * h / ny
        ang = rng.uniform(0, np.pi) if angle is None else angle + rng.uniform(-spread, spread)
        if i < bundles:
            if i % 2 == 0:
                straw_bundle(sp, rng, cx, cy, ang, rng.randint(6, 11), bundle_len, width, rgb, mapper, fan=fan)
            else:
                straw_bundle(sp, rng, cx, cy, ang, rng.randint(5, 9), bundle_len, width, rgb, mapper)
        else:
            straw_bundle(sp, rng, cx, cy, ang, 1, loose_len, width * 0.9, rgb, mapper)


def interior_points(rng, count, lo, hi, min_dist):
    """`count` random points in the square [lo, hi]^2 that are at least `min_dist` apart (best effort)."""
    pts = []
    tries = 0
    while len(pts) < count:
        c = rng.uniform(lo, hi, 2)
        tries += 1
        if all(np.hypot(*(c - q)) >= min_dist for q in pts) or tries > 400:
            pts.append(c)
    return pts


def scatter_groups(sp, seed, groups, clumps, loose, spread, rgb, stalk_len, width, lo, hi, edge=36.0):
    """Straw in loose groups: `groups` centres inside [lo, hi]^2, each with a few heaps of thin stalks (straw_tuft)
    and some single stalks close by; calm bare ground between the groups. Every heap keeps all its stalks at least
    `edge` px inside the tile, so the variants of a floor match at every border."""
    rng = np.random.RandomState(seed)

    def fits(state, cx, cy, ang, count, length, sp_):
        """True when all stalks of the heap that would be drawn from this random state stay `edge` px inside."""
        saved = rng.get_state()
        rng.set_state(state)
        stalks = tuft_stalks(rng, cx, cy, ang, count, length, sp_)
        rng.set_state(saved)
        return all(edge < x < N - edge and edge < y < N - edge for pts, _, _ in stalks for x, y in pts)

    for gx, gy in interior_points(rng, groups, lo, hi, 110.0):
        n_clumps = rng.randint(clumps[0], clumps[1] + 1)
        for k in range(n_clumps + loose):
            big = k < n_clumps
            length = stalk_len if big else (stalk_len[0] * 0.6, stalk_len[1] * 0.7)
            sp_ = 0.95 if big else 0.3
            placed = False
            for _ in range(40):
                a = rng.uniform(0, 2 * np.pi)
                r = rng.uniform(0.1, 1.0) * spread
                cx, cy = gx + np.cos(a) * r, gy + np.sin(a) * r * 0.8
                ang = rng.uniform(0, np.pi)
                count = rng.randint(22, 31) if big else rng.randint(2, 4)
                state = rng.get_state()
                # the heap draws from the state saved right after `count` was chosen
                if fits(state, cx, cy, ang, count, length, sp_):
                    placed = True
                    break
            if placed:
                straw_tuft(sp, rng, cx, cy, ang, count, length, width if big else width * 0.9, rgb, spread=sp_)


# ---------------------------------------------------------------------------------------------------------------
# dormitory: coarse, dark planks in long boards, staggered butt joints, knots, wear, some straw

PLANK_COL = np.array([0.225, 0.158, 0.112])
GAP = np.array([0.040, 0.029, 0.021])
PLANK_GAP_W = 3.0


def plank_layout(seed):
    """Rows of boards running along x on the wrapping tile. Returns the board id map, the distance to the nearest
    board edge (across the board or at a butt joint), the row index map, and the board list (row, start, length)."""
    rng = np.random.RandomState(seed)
    while True:
        heights, total = [], 0
        while total < N - 40:
            heights.append(int(rng.randint(48, 92)))
            total += heights[-1]
        heights[-1] -= total - N
        if 40 <= heights[-1] <= 100:
            break
    ys = np.concatenate([[0], np.cumsum(heights)])
    row_of = np.zeros(N, dtype=int)
    dy = np.zeros(N)
    for r in range(len(heights)):
        rows = np.arange(ys[r], ys[r + 1])
        row_of[rows] = r
        dy[rows] = np.minimum(rows - ys[r], ys[r + 1] - 1 - rows)
    board_of = np.zeros((len(heights), N), dtype=int)
    dj = np.zeros((len(heights), N))
    boards = []
    x = np.arange(N)
    for r in range(len(heights)):
        u = rng.uniform()
        count = 1 if u < 0.22 else (2 if u < 0.65 else 3)
        w = rng.uniform(0.55, 1.45, count)
        lengths = w / w.sum() * N
        start = rng.uniform(0, N)
        edges = (start + np.concatenate([[0], np.cumsum(lengths)])) % N
        bounds = np.cumsum(lengths)
        rel = (x - start) % N
        idx = np.searchsorted(bounds, rel, side='right')
        idx = np.minimum(idx, count - 1)
        base = len(boards)
        board_of[r] = base + idx
        for k in range(count):
            boards.append((r, edges[k], lengths[k]))
        d = np.full(N, 1e9)
        for e in edges[:count]:
            diff = np.abs(x - e)
            d = np.minimum(d, np.minimum(diff, N - diff))
        dj[r] = d
    ids = board_of[row_of[:, None], _XX]
    dd = np.minimum(dy[:, None] + 0.0 * _XX, dj[row_of])
    return ids, dd, row_of, boards, ys


def dormitory_field():
    ids, dd, row_of, boards, ys = plank_layout(601)
    rng = np.random.RandomState(602)
    nb = len(boards)
    bright = rng.uniform(0.76, 1.18, nb)[ids]
    warm = rng.uniform(-0.03, 0.03, nb)[ids]
    lift = rng.uniform(-0.25, 0.25, nb)[ids]
    oy = rng.randint(0, N, nb)[ids]
    ox = rng.randint(0, N, nb)[ids]
    ysh = (_YY + oy) % N
    xsh = (_XX + ox) % N
    grain_a = fbm(61, 2.2, 64.0, 3)
    grain_b = fbm(62, 7.0, 22.0, 2)
    grain = (grain_a[ysh, xsh] + 0.5 * grain_b[ysh, xsh]) / 1.12
    dd = dd + 1.1 * fbm(63, 12.0, 12.0, 2)
    col = PLANK_COL[None, None, :] * bright[..., None]
    col = col + warm[..., None] * np.array([0.5, 0.15, -0.3])[None, None, :]
    col = col * (1.0 + 0.11 * grain[..., None])
    streak = smoothstep(0.6, 1.8, grain)
    col = col * (1.0 - 0.34 * streak[..., None])
    hgt = 0.14 * grain + 0.07 * (bright - 1.0) + 0.3 * lift
    # knots
    kn = np.random.RandomState(603)
    for b in range(nb):
        if kn.uniform() < 0.38:
            r, start, length = boards[b]
            cy = (ys[r] + ys[r + 1]) / 2.0 + kn.uniform(-0.15, 0.15) * (ys[r + 1] - ys[r])
            cx = start + kn.uniform(0.2, 0.8) * length
            rc = min(kn.uniform(0.25, 0.4) * (ys[r + 1] - ys[r]), 15.0)
            ra = rc * kn.uniform(1.3, 1.9)
            ddx = (_XX - cx + N / 2.0) % N - N / 2.0
            ddy = _YY - cy
            rr = np.sqrt((ddx / ra) ** 2 + (ddy / rc) ** 2)
            ring = 0.5 + 0.5 * np.sin(rr * 9.0)
            area = 1.0 - smoothstep(0.75, 1.5, rr)
            col = col * (1.0 - area[..., None] * (0.25 + 0.3 * ring[..., None]))
            hgt = hgt - 0.3 * area
    # worn walkways (lighter, smoother) and dark stains
    wear = smoothstep(0.7, 1.5, fbm(64, 8.0, 8.0, 2))
    col = mix(col, np.clip(col * 1.16 + 0.008, 0, 1), wear * 0.35)
    stain = smoothstep(0.9, 1.9, fbm(65, 10.0, 10.0, 3))
    col = col * (1.0 - 0.22 * stain)[..., None]
    # splits along the grain
    split = 1.0 - smoothstep(0.0, 0.05, np.abs(fbm(66, 4.0, 60.0, 2)))
    split = split * smoothstep(1.0, 1.7, np.abs(fbm(67, 2.0, 2.0, 1))) * smoothstep(PLANK_GAP_W + 3.0, PLANK_GAP_W + 10.0, dd)
    col = col * (1.0 - 0.55 * split)[..., None]
    hgt = hgt - 0.7 * split
    # gaps and bevels between the boards
    t = smoothstep(PLANK_GAP_W - 1.0, PLANK_GAP_W + 1.2, dd)
    col = mix(np.broadcast_to(GAP, col.shape), col, t)
    col = col * (0.78 + 0.22 * smoothstep(PLANK_GAP_W, PLANK_GAP_W + 8.0, dd))[..., None]
    hgt = hgt * 0.6 + 1.8 * smoothstep(PLANK_GAP_W - 1.0, PLANK_GAP_W + 5.0, dd)
    # scattered straw
    sp = Sprites(wrap=True)
    scatter_straw(sp, 604, 1, 2, (0, 0, N, N), (0.56, 0.44, 0.21), bundle_len=(40, 66), loose_len=(36, 58), width=4.4,
                  blue=True)
    col, hgt = overlay(col, hgt, sp, 0.8)
    return col, hgt


def dormitory_band(col, hgt, side):
    d = depth_map(side)
    wob = 9.0 * nz(51, 10.0, 10.0, 2)
    dw = d + wob
    # dirty wall contact: darker, packed floor against the wall
    dark = 0.46 * (1.0 - smoothstep(2.0, 70.0, dw)) + 0.12 * (1.0 - smoothstep(0.0, 130.0, dw))
    col = col * (1.0 - dark)[..., None]
    dirt = (1.0 - smoothstep(10.0, 46.0, dw + 16.0 * nz(52, 14.0, 14.0, 2))) * 0.55
    col = mix(col, np.array([0.11, 0.08, 0.055])[None, None, :] * (1.0 + 0.3 * nz(53, 90.0, 90.0, 2)[..., None]), dirt)
    hgt = hgt + 0.4 * dirt
    # straw gathered at the wall
    sp = Sprites()
    scatter_straw(sp, 60 + ord(side), 8, 10, (MARGIN + 34, 40, N - MARGIN - 34, 96), (0.60, 0.47, 0.22),
                  side_mapper(side), bundle_len=(50, 78), loose_len=(40, 64), width=5.0, angle=0.0, spread=0.8)
    col, hgt = overlay(col, hgt, sp, 0.8)
    return col, hgt


# ---------------------------------------------------------------------------------------------------------------
# casino: worn dark stone slabs, ink stains (plain tiles, no edge pieces; was the library floor before the carpet)

STONE = np.array([0.262, 0.240, 0.208])
GW = 3


def slab_layout(seed, rows, h_range, w_range, warp_amp, warp_seed):
    """Wrapping rows of slabs of different height; every row is cut into slabs of random width with its own random
    offset, so there is no grid. Returns slab id, distance to the nearest joint (pixels), slab count."""
    rng = np.random.RandomState(seed)
    hs = rng.uniform(h_range[0], h_range[1], rows)
    hs = hs * N / hs.sum()
    starts = np.concatenate([[0.0], np.cumsum(hs)[:-1]])
    y0 = rng.uniform(0, N)
    xw = (_XX + warp_amp * fbm(warp_seed, 7.0, 7.0, 2)) % N
    yw = (_YY + warp_amp * fbm(warp_seed + 1, 7.0, 7.0, 2) - y0) % N
    row = np.searchsorted(starts, yw, 'right') - 1
    ry = yw - starts[row]
    dist = np.minimum(ry, hs[row] - ry)
    pid = np.zeros((N, N), dtype=int)
    count = 0
    for r in range(rows):
        widths = []
        total = 0.0
        while total < N:
            widths.append(rng.uniform(w_range[0], w_range[1]))
            total += widths[-1]
        widths = np.array(widths) * N / total
        cuts = (rng.uniform(0, N) + np.concatenate([[0.0], np.cumsum(widths)[:-1]])) % N
        cuts.sort()
        mask = row == r
        xs = xw[mask]
        idx = np.searchsorted(cuts, xs, 'right') - 1
        pid[mask] = count + idx % len(cuts)
        dx = np.min(wrap_dist(xs[:, None], cuts[None, :]), axis=1)
        dist[mask] = np.minimum(dist[mask], dx)
        count += len(cuts)
    return pid, dist, count


def stone_slab_field():
    pid, dd, count = slab_layout(201, 4, (70, 300), (90, 420), 7.0, 21)
    rng = np.random.RandomState(202)
    bright = rng.uniform(0.88, 1.10, count)[pid]
    tintb = rng.uniform(-0.02, 0.02, count)[pid]
    mott = nz(22, 14.0, 14.0, 3)
    grit = nz(23, 140.0, 140.0, 2)
    col = STONE[None, None, :] * bright[..., None]
    col = col + tintb[..., None] * np.array([0.6, 0.2, -0.6])[None, None, :]
    col = col * (1.0 + 0.07 * mott[..., None] + 0.05 * grit[..., None])
    hgt = 0.22 * mott + 0.08 * grit
    # scuffed, rubbed paths
    wear = smoothstep(0.6, 1.4, nz(24, 9.0, 9.0, 2))
    col = mix(col, np.clip(col * 1.16 + 0.01, 0, 1), wear * 0.3)
    hgt = hgt - 0.12 * wear
    # cracks and chips
    crack = 1.0 - smoothstep(0.0, 0.085, np.abs(nz(25, 9.0, 9.0, 3)))
    crack = crack * smoothstep(0.1, 0.6, np.abs(nz(26, 3.0, 3.0, 1)))
    col = col * (1.0 - 0.55 * crack)[..., None]
    hgt = hgt - 0.9 * crack
    # inky dust stains
    ink = smoothstep(0.9, 2.3, nz(27, 12.0, 12.0, 3))
    col = mix(col, np.array([0.105, 0.09, 0.08])[None, None, :] * (1.0 + 0.3 * grit[..., None]), ink * 0.4)
    # gaps and bevels
    t = smoothstep(GW - 1.0, GW + 1.5, dd)
    col = mix(np.broadcast_to(np.array([0.055, 0.048, 0.042]), col.shape), col, t)
    col = col * (0.7 + 0.3 * smoothstep(GW, GW + 10.0, dd))[..., None]
    hgt = hgt * 0.7 + 2.2 * smoothstep(GW - 1.0, GW + 6.0, dd)
    return col, hgt


# ---------------------------------------------------------------------------------------------------------------
# library: terracotta carpet with a fine dense small-scale pattern, a wide darker red-brown patterned border band and a
# narrow light grey stone frame along the shelves / room edge. Everything is truly periodic (motif cells of 32 px and
# 64 px, the border ornament 32 px), so the pieces line up across tiles.

CARPET = np.array([0.345, 0.168, 0.092])
CARPET_DEEP = np.array([0.150, 0.072, 0.050])
CARPET_RED = np.array([0.500, 0.205, 0.095])
CARPET_SAND = np.array([0.400, 0.255, 0.140])
BORDER = np.array([0.215, 0.085, 0.060])
BORDER_DARK = np.array([0.120, 0.050, 0.038])
BORDER_LIGHT = np.array([0.560, 0.340, 0.205])
FRAME = np.array([0.520, 0.500, 0.455])
FRAME_DARK = np.array([0.275, 0.260, 0.240])
BORDER_W = 200.0
FRAME_W = 16.0
_CARPET_FINISH = []


def carpet_finish():
    """Weave, dust and wear shared by the field and the border: (brightness factor, height)."""
    if not _CARPET_FINISH:
        weave = 0.5 * np.sin(_XX * (2.0 * np.pi / 4.0)) * np.sin(_YY * (2.0 * np.pi / 4.0)) + 0.5 * np.sin(_XX * (2.0 * np.pi / 8.0) + _YY * (2.0 * np.pi / 8.0))
        blot = fbm(71, 13.0, 13.0, 3)
        grit = fbm(72, 120.0, 120.0, 2)
        wear = smoothstep(0.7, 1.7, fbm(73, 9.0, 9.0, 2))          # rubbed, threadbare, slightly lighter and dustier
        dust = smoothstep(0.8, 2.0, fbm(74, 11.0, 11.0, 3))        # settled dust
        mult = 1.0 + 0.035 * weave + 0.050 * blot + 0.04 * grit + 0.10 * wear + 0.05 * dust
        hgt = 0.10 * weave + 0.12 * blot - 0.08 * wear
        _CARPET_FINISH.append((mult, hgt))
    return _CARPET_FINISH[0]


def motif_layer(cell, seed, radius, stagger):
    """Small motifs (dot, diamond or short bar, randomly chosen, sized, shifted and left out per cell) on a grid of
    `cell` px, every second row shifted by `stagger` cells. Returns the coverage of the dots, diamonds and bars."""
    n = N // cell
    rng = np.random.RandomState(seed)
    kind = rng.randint(0, 3, (n, n))
    size = rng.uniform(0.7, 1.25, (n, n))
    offx = rng.uniform(-0.30, 0.30, (n, n)) * cell
    offy = rng.uniform(-0.30, 0.30, (n, n)) * cell
    present = rng.uniform(0, 1, (n, n)) < 0.72
    cy = (_YY // cell).astype(int)
    shift = ((cy % 2) * stagger * cell).astype(int)
    cxs = ((_XX + shift) // cell).astype(int) % n
    fx = ((_XX + shift) % cell).astype(float) - cell / 2 - offx[cy, cxs]
    fy = (_YY % cell).astype(float) - cell / 2 - offy[cy, cxs]
    k = kind[cy, cxs]
    s = size[cy, cxs] * radius
    ok = present[cy, cxs]
    dot = 1.0 - smoothstep(s * 0.7, s * 0.7 + 1.6, np.sqrt(fx ** 2 + fy ** 2))
    dia = 1.0 - smoothstep(s * 0.9, s * 0.9 + 1.8, np.abs(fx) + np.abs(fy))
    bar = (1.0 - smoothstep(s * 1.15, s * 1.15 + 1.6, np.abs(fx))) * (1.0 - smoothstep(s * 0.38, s * 0.38 + 1.4, np.abs(fy)))
    return dot * (k == 0) * ok, dia * (k == 1) * ok, bar * (k == 2) * ok


def library_field():
    blot = fbm(82, 6.0, 6.0, 3)
    sandy = smoothstep(0.55, 0.95, np.clip(0.5 + 0.5 * blot, 0, 1)) * 0.25
    base = mix(np.broadcast_to(CARPET, (N, N, 3)).copy(), CARPET_SAND[None, None, :], sandy)
    # dense small pattern: a staggered grid of small motifs in red-orange, a second one in dark brown, sand flecks
    d1, m1, b1 = motif_layer(64, 83, 11.0, 0.5)
    d2, m2, b2 = motif_layer(64, 84, 9.0, 0.0)
    d3, m3, b3 = motif_layer(32, 85, 5.0, 0.5)
    red = np.clip(d1 + m1 * 0.9 + b1 * 0.8, 0, 1)
    deep = np.clip(d2 * 0.9 + m2 + b2 * 0.9, 0, 1)
    sand = np.clip(d3 + m3 * 0.8, 0, 1)
    # fine diagonal thread lines between the motifs so that the field reads as woven textile
    lat = smoothstep(6.0, 7.4, np.abs(((_XX + _YY) % 16).astype(float) - 8.0))
    col = mix(base, CARPET_DEEP[None, None, :], lat * 0.12)
    col = mix(col, CARPET_DEEP[None, None, :], deep * 0.85)
    col = mix(col, CARPET_RED[None, None, :], red * 0.9)
    col = mix(col, CARPET_SAND[None, None, :], sand * 0.45)
    mult, hgt = carpet_finish()
    col = col * mult[..., None]
    hgt = hgt + 0.30 * (red + sand) - 0.25 * deep
    return col, hgt


def library_band(col, hgt, sides):
    """Light grey stone frame at the wall, then a wide dark red-brown patterned border along every exposed side. The
    depth is the distance to the nearest exposed side, which mitres the corners. Drawn on top of the open field, so
    sides that are not exposed stay identical to it."""
    depth = np.full((N, N), 1e9)
    along = np.zeros((N, N))
    for side in sides:
        dmap = depth_map(side)
        amap = _XX if side in 'TB' else _YY
        closer = dmap < depth
        along = np.where(closer, amap, along)
        depth = np.minimum(depth, dmap)
    depth = depth + 2.0 * fbm(91, 25.0, 25.0, 2)
    inner = depth - FRAME_W                                          # distance into the carpet border
    fa = (along % 32.0) - 16.0
    fb = (along % 64.0) - 32.0

    def line(pos, hw):
        return 1.0 - smoothstep(hw, hw + 1.6, np.abs(inner - pos))

    # ornamental stripes parallel to the edge: two light lines, a wide dark stripe with a chain of large diamonds (64 px
    # period), a light line, a zig-zag line, and a dark line toward the field
    stripe1 = line(14.0, 4.0)
    stripe2 = line(30.0, 2.4)
    dark = 1.0 - smoothstep(0.8, 2.0, np.abs(inner - 66.0) - 20.0)
    chain = 1.0 - smoothstep(0.0, 2.0, (np.abs(fb) + np.abs(inner - 66.0) - 17.0) / 1.414)
    core = 1.0 - smoothstep(0.0, 2.0, (np.abs(fb) + np.abs(inner - 66.0) - 6.0) / 1.414)
    stripe3 = line(106.0, 3.4)
    zig = 1.0 - smoothstep(1.6, 3.4, np.abs((inner - 132.0) - (np.abs(fa) - 8.0)) / 1.414)
    bcol = np.broadcast_to(BORDER, (N, N, 3)).copy()
    bcol = mix(bcol, BORDER_DARK[None, None, :], dark * 0.8)
    bcol = mix(bcol, BORDER_LIGHT[None, None, :] * 0.92, chain * 0.9)
    bcol = mix(bcol, BORDER_DARK[None, None, :], core * 0.85)
    bcol = mix(bcol, BORDER_LIGHT[None, None, :], np.clip(stripe1 + stripe2 + stripe3, 0, 1) * 0.85)
    bcol = mix(bcol, BORDER_LIGHT[None, None, :] * 0.9, zig * 0.8)
    bcol = mix(bcol, BORDER_DARK[None, None, :], line(BORDER_W - FRAME_W - 14.0, 3.0) * 0.85)
    mult, bhgt = carpet_finish()
    bcol = bcol * mult[..., None]
    hb = bhgt + 0.30 * np.clip(chain + stripe1 + stripe2 + stripe3 + zig * 0.8, 0, 1) - 0.15 * dark
    # stone frame between the wall and the carpet
    fcol = mix(np.broadcast_to(FRAME, (N, N, 3)).copy(), FRAME_DARK[None, None, :], 0.5 * np.clip(0.5 + 0.5 * fbm(92, 30.0, 30.0, 2), 0, 1))
    fcol = fcol * (0.92 + 0.08 * np.clip(fbm(93, 140.0, 140.0, 2), -1, 1))[..., None]
    fcol = mix(fcol, FRAME_DARK[None, None, :] * 0.5, 1.0 - smoothstep(1.0, 4.0, depth))              # wall contact
    fcol = mix(fcol, FRAME_DARK[None, None, :] * 0.7, 1.0 - smoothstep(1.0, 2.6, np.abs(depth - FRAME_W + 1.0)))  # groove
    fhgt = 0.45 + 0.1 * fbm(94, 60.0, 60.0, 2)
    inframe = 1.0 - smoothstep(FRAME_W - 2.0, FRAME_W, depth)
    bcol = mix(bcol, fcol, inframe)
    hb = hb * (1.0 - inframe) + fhgt * inframe
    cover = 1.0 - smoothstep(BORDER_W - 3.0, BORDER_W + 1.5, depth)
    return mix(col, bcol, cover), hgt * (1.0 - cover) + hb * cover

# ---------------------------------------------------------------------------------------------------------------
# hatchery: trodden earth, mud, straw and feathers

EARTH = np.array([0.315, 0.235, 0.155])
MUD = np.array([0.165, 0.115, 0.08])


def feather(sp, cx, cy, ang, length, rgb):
    ca, sa = np.cos(ang), np.sin(ang)
    pts = []
    for t, w in ((0.0, 0.0), (0.2, 0.22), (0.5, 0.30), (0.8, 0.2), (1.0, 0.0)):
        pts.append((t, w))
    poly = []
    for t, w in pts:
        along, across = (t - 0.5) * length, w * length * 0.42
        poly.append((cx + ca * along - sa * across, cy + sa * along + ca * across))
    for t, w in reversed(pts[1:-1]):
        along, across = (t - 0.5) * length, -w * length * 0.42
        poly.append((cx + ca * along - sa * across, cy + sa * along + ca * across))
    sp.polygon(poly, rgb, 0.6)
    sp.line((cx - ca * length * 0.5, cy - sa * length * 0.5), (cx + ca * length * 0.5, cy + sa * length * 0.5),
            1.0, tuple(c * 0.75 for c in rgb), 0.8)


_HATCHERY_BASE = []


def hatchery_base():
    """Calm trodden earth shared by all variants: broad clods, soft mud patches, hardly any fine grain, flat in
    brightness. It wraps and is the same in all variants, so the tiles match at every border (the open piece needs
    rotation 0 for that)."""
    if _HATCHERY_BASE:
        return _HATCHERY_BASE[0]
    lump = fbm(31, 12.0, 12.0, 3)
    grit = fbm(32, 55.0, 55.0, 2)
    col = EARTH[None, None, :] * (1.0 + 0.045 * lump[..., None] + 0.012 * grit[..., None])
    hgt = 0.6 * lump + 0.1 * grit
    mud = smoothstep(0.35, 1.8, fbm(34, 11.0, 11.0, 3))
    mudcol = MUD[None, None, :] * (1.0 + 0.08 * lump[..., None])
    col = mix(col, mudcol, mud * 0.40)
    hgt = mix(hgt, hgt * 0.4 - 0.4, mud)
    dust = smoothstep(0.9, 1.8, fbm(35, 13.0, 13.0, 2))
    col = mix(col, np.clip(col * 1.12 + 0.012, 0, 1), dust * 0.35)
    col = flatten(col, 48.0, 0.8)
    _HATCHERY_BASE.append((col, hgt))
    return _HATCHERY_BASE[0]


def hatchery_field(variant=0):
    """Trodden earth with straw in a few loose groups of large clumps (and a few feathers). The straw stays inside the
    tile (at least 24 px from the border), so the four variants of the open piece (Farm, FarmB, FarmC, FarmD,
    picked at random per tile through [oneOf] in config/tilesets.cfg) match each other at every border and no
    straw pattern repeats from tile to tile."""
    col, hgt = hatchery_base()
    sp = Sprites()
    scatter_groups(sp, 301 + 17 * variant, 2 + variant % 2, (1, 2), 1, 56.0, (0.74, 0.51, 0.18), (84, 124), 5.2, 60.0,
                   452.0)
    fr = np.random.RandomState(302 + variant)
    for _ in range(2):
        feather(sp, fr.uniform(60, N - 60), fr.uniform(60, N - 60), fr.uniform(0, np.pi), fr.uniform(20, 28),
                (0.58, 0.54, 0.46))
    return overlay(col, hgt, sp, 0.9)


def hatchery_band(col, hgt, side):
    d = depth_map(side)
    fringe = 1.0 - smoothstep(18.0, 84.0, d + 22.0 * fbm(71, 13.0, 13.0, 3))
    dirt = np.array([0.205, 0.15, 0.10])[None, None, :] * (1.0 + 0.08 * fbm(72, 40.0, 40.0, 2)[..., None])
    col = mix(col, dirt, fringe * 0.62)
    col = col * (1.0 - 0.22 * (1.0 - smoothstep(0.0, 40.0, d)))[..., None]
    hgt = hgt - 0.45 * fringe
    sp = Sprites(wrap=True)
    # stalks must not cross the wall line (the sprites wrap around the tile): depth >= 50, spread and length limited
    rng = np.random.RandomState(80 + ord(side))
    mp = side_mapper(side)
    for k in range(6):
        cx = (k + 0.5) * N / 6.0 + rng.uniform(-30.0, 30.0)
        cy = rng.uniform(84.0, 96.0)
        if k % 2 == 0:
            straw_tuft(sp, rng, cx, cy, rng.uniform(-0.2, 0.2), rng.randint(9, 14), (40, 58), 4.4, (0.72, 0.50, 0.18), mp,
                       spread=0.4)
        else:
            straw_tuft(sp, rng, cx, cy, rng.uniform(-0.2, 0.2), rng.randint(2, 4), (34, 50), 4.0, (0.72, 0.50, 0.18), mp,
                       spread=0.25)
    col, hgt = overlay(col, hgt, sp, 0.9)
    return col, hgt


# ---------------------------------------------------------------------------------------------------------------
# dungeon temple and treasury: irregular slabs inside a gap along the tile border

BASALT = np.array([0.118, 0.110, 0.113])
EMBER = np.array([0.88, 0.15, 0.05])
EMBER_HOT = np.array([1.0, 0.50, 0.13])
EMBER_DEEP = np.array([0.26, 0.035, 0.03])
JOINT_COLD = np.array([0.030, 0.020, 0.022])


SLATE = np.array([0.272, 0.280, 0.294])
TREASURY_GAP = np.array([0.048, 0.050, 0.056])


def blue_noise_points(seed, count, min_dist):
    """Well spread random points on the wrapping tile (best candidate sampling)."""
    rng = np.random.RandomState(seed)
    pts = [rng.uniform(0, N, 2)]
    while len(pts) < count:
        best, best_d = None, -1.0
        for _ in range(40):
            c = rng.uniform(0, N, 2)
            d = min(np.hypot(*(wrap_dist(c, p))) for p in pts)
            if d > best_d:
                best, best_d = c, d
        pts.append(best)
    return np.array(pts)


def voronoi_slabs(seed, count, weight_amp, warp_amp, warp_seed):
    """Irregular crazy-paving slabs on the torus: cell id, distance to the nearest cell edge (pixels), position
    inside the cell relative to its centre (pixels). Cell sizes vary through additive weights."""
    pts = blue_noise_points(seed, count, 0)
    rng = np.random.RandomState(seed + 1)
    weights = rng.uniform(-weight_amp, weight_amp, count)
    xw = _XX + warp_amp * fbm(warp_seed, 6.0, 6.0, 2)
    yw = _YY + warp_amp * fbm(warp_seed + 1, 6.0, 6.0, 2)
    d1 = np.full((N, N), 1e9)
    d2 = np.full((N, N), 1e9)
    idx = np.zeros((N, N), dtype=int)
    off = np.zeros((N, N, 2))
    for i, p in enumerate(pts):
        for sx in (-N, 0, N):
            for sy in (-N, 0, N):
                dx = xw - (p[0] + sx)
                dy = yw - (p[1] + sy)
                d = np.sqrt(dx * dx + dy * dy) - weights[i]
                closer = d < d1
                second = (~closer) & (d < d2)
                d2 = np.where(closer, d1, np.where(second, d, d2))
                idx = np.where(closer, i, idx)
                off[..., 0] = np.where(closer, dx, off[..., 0])
                off[..., 1] = np.where(closer, dy, off[..., 1])
                d1 = np.where(closer, d, d1)
    return idx, (d2 - d1) * 0.5, off


def route_joints(dd, a, b, lo, hi):
    """Cheapest 8-connected path from a to b (x, y pixels) that prefers the slab joints (small distance `dd` to the
    nearest slab edge), confined to the square lo..hi (A* with a straight-line heuristic)."""
    x0, x1 =max(lo, int(min(a[0], b[0])) - 60), min(hi, int(max(a[0], b[0])) + 60)
    y0, y1 = max(lo, int(min(a[1], b[1])) - 60), min(hi, int(max(a[1], b[1])) + 60)
    cost = (1.0 + 0.9 * np.minimum(dd[y0:y1 + 1, x0:x1 + 1], 12.0)).tolist()
    w = x1 - x0 + 1
    start, goal = (int(a[0]) - x0, int(a[1]) - y0), (int(b[0]) - x0, int(b[1]) - y0)
    best = {start: 0.0}
    prev = {}
    heap = [(0.0, start)]
    steps = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.414), (-1, 1, 1.414), (1, -1, 1.414),
             (-1, -1, 1.414)]
    while heap:
        f, cur = heapq.heappop(heap)
        if cur == goal:
            break
        g = best[cur]
        if f - np.hypot(goal[0] - cur[0], goal[1] - cur[1]) > g + 1e-6:
            continue
        for sx, sy, sl in steps:
            nx, ny = cur[0] + sx, cur[1] + sy
            if nx < 0 or ny < 0 or nx >= w or ny > y1 - y0:
                continue
            ng = g + sl * cost[ny][nx]
            if ng < best.get((nx, ny), 1e18):
                best[(nx, ny)] = ng
                prev[(nx, ny)] = cur
                heapq.heappush(heap, (ng + np.hypot(goal[0] - nx, goal[1] - ny), (nx, ny)))
    path = [goal]
    while path[-1] != start:
        path.append(prev[path[-1]])
    path.reverse()
    pts = np.array([(x + x0, y + y0) for x, y in path], dtype=float)
    k = 4  # smooth the pixel staircase, keep the ends
    sm = pts.copy()
    for i in range(len(pts)):
        j0, j1 = max(0, i - k), min(len(pts), i + k + 1)
        sm[i] = pts[j0:j1].mean(axis=0)
    sm[0], sm[-1] = pts[0], pts[-1]
    return [tuple(p) for p in sm[::4]] + [tuple(sm[-1])]


def joint_cracks(dd, seed, plan, margin):
    """Long glowing cracks that follow the slab joints (masks core, halo, hot as wrapping arrays). `plan` is a list of
    (length, branches); every crack is routed along the joint net between two points about `length` px apart, so it
    meanders from slab edge to slab edge like a real crack, tapers out at both ends, has cold (dark) stretches and a
    few short branches. Nothing gets closer than `margin` px to the tile border, so all variants match at every
    border."""
    rng = np.random.RandomState(seed)
    core = Layer()
    halo = Layer()
    hot = Layer()
    lo, hi = margin, N - 1 - margin

    def snap(x, y):
        x, y = int(np.clip(x, lo, hi)), int(np.clip(y, lo, hi))
        x0, y0 = max(lo, x - 8), max(lo, y - 8)
        patch = dd[y0:min(hi, y + 8) + 1, x0:min(hi, x + 8) + 1]
        iy, ix = np.unravel_index(np.argmin(patch), patch.shape)
        return (x0 + ix, y0 + iy)

    def draw(pts, wmax, heat0):
        # jagged like a real fracture: small sideways jitter on every vertex (none at the ends)
        jit = np.convolve(rng.normal(0.0, 1.6, len(pts)), [0.25, 0.5, 0.25], mode='same')
        jit[0] = jit[-1] = 0.0
        jp = []
        for i, (x, y) in enumerate(pts):
            j0, j1 = pts[max(0, i - 1)], pts[min(len(pts) - 1, i + 1)]
            tg = np.arctan2(j1[1] - j0[1], j1[0] - j0[0])
            jp.append((x - np.sin(tg) * jit[i], y + np.cos(tg) * jit[i]))
        pts = jp
        n = len(pts) - 1
        phase = rng.uniform(0, 6.28)
        cold = rng.uniform(0, 6.28)
        wob = rng.uniform(0.75, 1.25, n)
        for i in range(n):
            u = (i + 0.5) / n
            taper = np.sin(np.pi * u) ** 0.5
            heat = heat0 * (0.72 + 0.28 * np.sin(phase + u * 6.0)) * (0.35 + 0.65 * taper)
            heat *= 1.0 - 0.9 * smoothstep(0.6, 0.95, np.sin(cold + u * 7.0))  # dims towards cold stretches
            if heat < 0.15:
                continue  # cold stretch: only the dark joint of the stone
            w = (2.4 + wmax * taper) * wob[i]
            core.line([pts[i], pts[i + 1]], w, int(255 * heat))
            hot.line([pts[i], pts[i + 1]], w * 0.45, int(255 * heat))
            halo.line([pts[i], pts[i + 1]], w + 13.0, int(255 * heat))

    for length, branches in plan:
        pts = None
        for _ in range(300):
            ang = rng.uniform(0, 2.0 * np.pi)
            sx, sy = rng.uniform(lo, hi, 2)
            ex, ey = sx + np.cos(ang) * length, sy + np.sin(ang) * length
            if lo <= ex <= hi and lo <= ey <= hi:
                pts = route_joints(dd, snap(sx, sy), snap(ex, ey), lo, hi)
                break
        if pts is None:
            continue
        draw(pts, 3.0, rng.uniform(0.85, 1.0))
        for _ in range(branches):
            k = rng.randint(len(pts) // 4, 3 * len(pts) // 4)
            tang = np.arctan2(pts[k + 1][1] - pts[k - 1][1], pts[k + 1][0] - pts[k - 1][0])
            for _ in range(40):
                ba = tang + rng.choice([-1.0, 1.0]) * rng.uniform(0.6, 1.2)
                bl = rng.uniform(70, 120)
                tx, ty = pts[k][0] + np.cos(ba) * bl, pts[k][1] + np.sin(ba) * bl
                if lo <= tx <= hi and lo <= ty <= hi:
                    bp = route_joints(dd, snap(*pts[k]), snap(tx, ty), lo, hi)
                    draw(bp, 1.6, rng.uniform(0.7, 0.85))
                    break
    return (np.clip(blur(core.result(), 1.0) * 1.5, 0.0, 1.0), np.clip(blur(halo.result(), 8.0) * 2.0, 0.0, 1.0),
            np.clip(blur(hot.result(), 0.9) * 1.5, 0.0, 1.0))


_TEMPLE_BASE = []


def temple_base():
    """The basalt floor shared by all variants of the temple floor: a few large slabs of very different size cut by
    thin, cold, broken joints (no honeycomb, no glow), chipped, pitted, flat in brightness. It wraps."""
    if _TEMPLE_BASE:
        return _TEMPLE_BASE[0]
    count = 6
    idx, dd, off = voronoi_slabs(431, count, 70.0, 6.0, 433)
    rng = np.random.RandomState(432)
    bright = rng.uniform(0.94, 1.08, count)[idx]
    tintb = rng.uniform(-0.006, 0.006, count)[idx]
    tilt = rng.uniform(-0.04, 0.04, (count, 2))[idx]
    chip = smoothstep(0.9, 1.8, fbm(423, 38.0, 38.0, 2))
    dd = np.maximum(dd - 4.0 * chip, 0.0)
    mott = fbm(42, 11.0, 11.0, 3)
    grit = fbm(43, 90.0, 90.0, 2)
    col = BASALT[None, None, :] * (bright * (1.0 + (tilt * off).sum(-1) / 90.0))[..., None]
    col = col + tintb[..., None] * np.array([0.5, 0.0, 0.6])[None, None, :]
    col = col * (1.0 + 0.08 * mott[..., None] + 0.04 * np.clip(grit, -2.0, 2.0)[..., None])
    hgt = 0.2 * mott + 0.1 * grit
    wear = smoothstep(0.6, 1.5, fbm(44, 8.0, 8.0, 2))
    col = mix(col, np.clip(col * 1.12 + 0.004, 0, 1), wear * 0.3)
    pit = smoothstep(2.3, 2.9, fbm(47, 70.0, 70.0, 1))
    col = col * (1.0 - 0.35 * pit)[..., None]
    hgt = hgt - 0.6 * pit
    # thin, cold, broken joints: dark, but only a little darker than the stone, and missing on parts of the rim
    hw = 1.5 + 1.2 * smoothstep(-0.6, 1.0, fbm(426, 9.0, 9.0, 2))
    present = smoothstep(-0.2, 0.6, fbm(427, 5.0, 5.0, 2))
    core = (1.0 - smoothstep(hw * 0.6, hw * 1.6, dd)) * present
    col = mix(col, np.broadcast_to(JOINT_COLD, col.shape), core * 0.6)
    col = flatten(col, 48.0, 0.5)
    hgt = hgt - 0.8 * core
    hgt = hgt * 0.7 + 1.6 * smoothstep(1.0, 8.0, dd * (1.0 - 0.6 * present))
    _TEMPLE_BASE.append((col, hgt, dd))
    return _TEMPLE_BASE[0]


# (length, branches) of the glowing cracks of every variant; variants 1 and 3 have none, so whole stretches of floor stay cold
TEMPLE_CRACKS = (((350, 1), (190, 0)), (), ((310, 2),), ())


def temple_field(variant=0):
    """Dark basalt with a few long glowing cracks that follow the slab joints (the glow is painted into the diffuse
    texture: the room shader has no emissive term): a thin hot core, an ember body and a soft red falloff, the
    rest of the joints stays dark. Per tile 0-2 cracks (variants 1 and 3 have none), so the heart stays the focal point and
    parts of the floor stay cold. Nothing glows
    near the tile border, so the four variants (`DungeonTempleFloor`, `...B`, `...C`, `...D`, picked at random per
    tile through [oneOf] in config/tilesets.cfg) differ only in these features and still match at every border;
    there is no tile-shaped motif. The whole texture wraps, so it needs the same rotation on every tile."""
    col, hgt, dd = temple_base()
    cm, hm, tm = joint_cracks(dd, 440 + variant, TEMPLE_CRACKS[variant], 70)
    col = col + hm[..., None] * np.array([0.11, 0.014, 0.006])[None, None, :]
    ramp = mix(np.broadcast_to(EMBER_DEEP * 1.6, col.shape), np.broadcast_to(EMBER, col.shape),
               smoothstep(0.2, 0.6, cm))
    ramp = mix(ramp, np.broadcast_to(EMBER_HOT, col.shape), smoothstep(0.3, 0.9, tm))
    col = mix(col, ramp, smoothstep(0.06, 0.35, cm))
    hgt = hgt - 0.8 * cm
    return col, hgt


def flagstone_field():
    """Cold grey flagstones as irregular crazy paving (no grid, no joints along the tile border; the whole
    texture wraps, so it needs the same rotation on every tile: see [arenaRoom] in config/tilesets.cfg). This was
    the treasury floor until the checkerboard came back; the arena uses it now, unchanged."""
    idx, edge, off = voronoi_slabs(411, 22, 30.0, 2.5, 413)
    rng = np.random.RandomState(412)
    count = 22
    bright = rng.uniform(0.84, 1.14, count)[idx]
    warm = rng.uniform(-1.0, 1.0, count)[idx]
    tilt = rng.uniform(-0.07, 0.07, (count, 2))[idx]
    mott = fbm(52, 6.0, 6.0, 3)
    grit = fbm(53, 140.0, 140.0, 2)
    col = SLATE[None, None, :] * (bright * (1.0 + (tilt * off).sum(-1) / 60.0))[..., None]
    col = col + warm[..., None] * np.array([0.020, 0.010, -0.004])[None, None, :]
    col = col * (1.0 + 0.09 * mott[..., None] + 0.05 * grit[..., None])
    hgt = 0.2 * mott + 0.08 * grit
    # chalky wear
    chalk = smoothstep(0.5, 1.6, fbm(54, 3.0, 3.0, 3))
    col = mix(col, np.array([0.38, 0.385, 0.40])[None, None, :] * (1.0 + 0.1 * grit[..., None]), chalk * 0.25)
    hgt = hgt - 0.12 * chalk
    # hairline cracks
    crack = 1.0 - smoothstep(0.0, 0.04, np.abs(fbm(55, 10.0, 10.0, 3)))
    crack = crack * smoothstep(0.3, 1.0, np.abs(fbm(56, 3.0, 3.0, 1)))
    col = col * (1.0 - 0.5 * crack)[..., None]
    hgt = hgt - 0.6 * crack
    # thin dark joints, bevels
    gw = 1.4
    t = smoothstep(gw - 0.8, gw + 1.2, edge)
    col = mix(np.broadcast_to(TREASURY_GAP, col.shape), col, t)
    col = col * (0.78 + 0.22 * smoothstep(gw, gw + 7.0, edge))[..., None]
    hgt = hgt * 0.7 + 2.0 * smoothstep(gw - 0.8, gw + 5.0, edge)
    return col, hgt


# ---------------------------------------------------------------------------------------------------------------
def git_source(path, commit):
    """Reads a file from git history (so the original does not have to be committed a second time)."""
    root =os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    data = subprocess.check_output(['git', 'show', '%s:%s' % (commit, path)], cwd=root)
    return Image.open(io.BytesIO(data)).convert('RGB')


# treasury: marble checkerboard (dark slate and light grey-white squares, dark grout), four variants

CHK = 4                      # squares per tile and axis (even, so the checker continues across tiles)
SQ = N // CHK
MARBLE_DARK = np.array([0.056, 0.060, 0.074])
MARBLE_LIGHT = np.array([0.282, 0.290, 0.312])
CHECK_GROUT = np.array([0.018, 0.018, 0.021])
GROUT_HALF = 3.4             # half width of the grout in px; the grout lines run along the tile border
_SQ_PX = ((_XX + 0.5) % SQ) - SQ / 2.0     # position inside the square (centre 0)
_SQ_PY = ((_YY + 0.5) % SQ) - SQ / 2.0
_SQ_I = ((_XX // SQ) % CHK).astype(int)
_SQ_J = ((_YY // SQ) % CHK).astype(int)


def square_sdf():
    """Signed distance (px, negative inside) to the rounded square of every checker cell."""
    half = SQ / 2.0 - GROUT_HALF
    radius = 7.0
    qx = np.abs(_SQ_PX) - (half - radius)
    qy = np.abs(_SQ_PY) - (half - radius)
    return np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0)) + np.minimum(np.maximum(qx, qy), 0.0) - radius


def shear(field, k):
    """Shears a periodic field by whole pixels per column (y + k * x), which keeps it periodic: horizontal streaks
    become diagonal ones."""
    return field[(_YY + k * _XX) % N, _XX]


def blob(layer, rng, cx, cy, radius, squash=0.7):
    layer.ellipse(cx, cy, radius, radius * squash, rng.uniform(0, np.pi), 255)


def treasury_field(variant=0):
    """Marble checkerboard: 4x4 squares per tile, even brightness per
    colour, dark grout. The grout and the square outlines are the same in all variants; veining, dirt, stains,
    scratches and chips are drawn inside the squares only, so a tile border is always just grout and the variants
    (picked at random per tile through [oneOf]) fit next to each other. Every tile has an even number of squares,
    so the checker continues across tile borders with the same phase."""
    sdf = square_sdf()
    inside = 1.0 - smoothstep(-1.0, 0.8, sdf)
    depth = np.clip(-sdf, 0.0, None)
    rng = np.random.RandomState(810 + variant * 37)
    seed = 820 + variant * 40
    dark_sq = ((_SQ_I + _SQ_J) % 2 == 0)
    dk = dark_sq[..., None]
    sq_tone = rng.uniform(-0.025, 0.025, (CHK, CHK))[_SQ_J, _SQ_I]
    sq_vein = rng.uniform(0.6, 1.2, (CHK, CHK))[_SQ_J, _SQ_I]
    base = np.where(dk, MARBLE_DARK[None, None, :], MARBLE_LIGHT[None, None, :])
    cloud = fbm(seed + 1, 4.0, 4.0, 3)
    grain = fbm(seed + 2, 150.0, 150.0, 2)
    fine = fbm(seed + 3, 60.0, 60.0, 2)
    col = base * (1.0 + sq_tone + np.where(dark_sq, 0.14, 0.07) * cloud + 0.035 * grain + 0.02 * fine)[..., None]
    hgt = 0.12 * cloud + 0.10 * grain
    # marble veins: warped ridge lines, grey on the light marble, pale on the dark marble
    wx = fbm(seed + 4, 2.5, 2.5, 2)
    v1 = shear(fbm(seed + 5, 2.6, 2.6, 3), 1) + 0.30 * wx
    v2 = shear(fbm(seed + 6, 4.0, 4.0, 3), -1) + 0.15 * wx
    v3 = fbm(seed + 7, 13.0, 13.0, 2)
    main = np.exp(-(v1 / 0.17) ** 2)
    side = np.exp(-(v2 / 0.12) ** 2) * 0.55
    hair = np.exp(-(v3 / 0.06) ** 2) * 0.25
    veins = np.clip(main + side + hair, 0.0, 1.0) * sq_vein
    vein_fade = smoothstep(3.0, 16.0, depth)
    vein_col = np.where(dk, np.array([0.17, 0.172, 0.185])[None, None, :], np.array([0.14, 0.142, 0.150])[None, None, :])
    vein_amt = np.where(dark_sq, 0.50, 0.55) * veins * vein_fade
    col = mix(col, vein_col, vein_amt)
    # soft light cloudiness in the light marble (toned down, no bright patches)
    pale = smoothstep(0.6, 1.9, fbm(seed + 8, 3.0, 3.0, 2)) * vein_fade
    col = col + (np.where(dark_sq, 0.0, 1.0) * pale * 0.05)[..., None]
    hgt = hgt - 0.35 * veins * vein_fade
    # dirt: grime gathers along the edges of the squares, irregular
    grime_n = smoothstep(-0.3, 1.4, fbm(seed + 9, 9.0, 9.0, 3))
    grime = (1.0 - smoothstep(0.0, 14.0, depth)) * grime_n
    col = col * (1.0 - 0.34 * grime)[..., None]
    col = col * (0.88 + 0.12 * smoothstep(0.0, 6.0, depth))[..., None]
    # broad dusty wear
    dust = smoothstep(0.5, 1.8, fbm(seed + 10, 5.0, 5.0, 3)) * 0.14
    col = mix(col, np.array([0.17, 0.165, 0.155])[None, None, :] * np.where(dk, 0.55, 1.0), dust)
    # stains: soft irregular blotches, only a few, never the same spot (random per variant)
    stain = Layer()
    tint = Layer()
    for _ in range(5):
        i, j = rng.randint(0, CHK, 2)
        blob(stain, rng, i * SQ + SQ / 2.0 + rng.uniform(-38, 38), j * SQ + SQ / 2.0 + rng.uniform(-38, 38),
             rng.uniform(9, 22))
    for _ in range(2):
        i, j = rng.randint(0, CHK, 2)
        blob(tint, rng, i * SQ + SQ / 2.0 + rng.uniform(-36, 36), j * SQ + SQ / 2.0 + rng.uniform(-36, 36),
             rng.uniform(5, 10), 0.8)
    sm = np.clip(blur(stain.result(), 5.0) * 2.2, 0.0, 1.0) * (0.55 + 0.45 * smoothstep(-0.6, 0.8, fbm(seed + 11, 10.0, 10.0, 2)))
    sm = sm * smoothstep(4.0, 12.0, depth)
    col = mix(col, np.where(dk, np.array([0.105, 0.098, 0.090])[None, None, :], np.array([0.185, 0.160, 0.130])[None, None, :]),
              sm * 0.42)
    tm = np.clip(blur(tint.result(), 3.0) * 2.0, 0.0, 1.0) * smoothstep(4.0, 12.0, depth)
    col = mix(col, np.array([0.205, 0.120, 0.070])[None, None, :] * np.where(dk, 0.6, 1.0), tm * 0.28)
    # scratches and scuffs: a few short pale lines
    scr = Layer()
    for _ in range(9):
        i, j = rng.randint(0, CHK, 2)
        x, y = i * SQ + SQ / 2.0 + rng.uniform(-30, 30), j * SQ + SQ / 2.0 + rng.uniform(-30, 30)
        ang = rng.uniform(0, 2 * np.pi)
        ln = rng.uniform(12, 34)
        scr.line([(x, y), (x + np.cos(ang) * ln, y + np.sin(ang) * ln)], rng.uniform(0.8, 1.4), 255)
    scm = np.clip(blur(scr.result(), 0.6) * 1.4, 0.0, 1.0) * smoothstep(5.0, 12.0, depth)
    col = mix(col, np.array([0.17, 0.17, 0.175])[None, None, :], scm * 0.35)
    hgt = hgt - 0.6 * scm
    # hairline cracks inside a square
    crk = Layer()
    for _ in range(2):
        i, j = rng.randint(0, CHK, 2)
        x, y = i * SQ + SQ / 2.0 + rng.uniform(-20, 20), j * SQ + SQ / 2.0 + rng.uniform(-20, 20)
        ang = rng.uniform(0, 2 * np.pi)
        pts = [(x, y)]
        for _ in range(7):
            ang += rng.uniform(-0.6, 0.6)
            x, y = x + np.cos(ang) * 9.0, y + np.sin(ang) * 9.0
            pts.append((x, y))
        crk.line(pts, 1.1, 255)
    cm = np.clip(blur(crk.result(), 0.6) * 1.3, 0.0, 1.0) * smoothstep(5.0, 12.0, depth)
    col = col * (1.0 - 0.5 * cm)[..., None]
    hgt = hgt - 0.8 * cm
    # chipped corners at the inner grout crossings
    chip = Layer()
    for _ in range(2):
        a, b = rng.randint(1, CHK, 2)
        sx, sy = rng.choice([-1, 1], 2)
        blob(chip, rng, a * SQ + sx * (GROUT_HALF + 2.0), b * SQ + sy * (GROUT_HALF + 2.0), rng.uniform(4, 7), 0.8)
    chm = np.clip(blur(chip.result(), 0.7) * 1.4, 0.0, 1.0)
    col = mix(col, CHECK_GROUT[None, None, :] * 1.5, chm * 0.85 * inside)
    hgt = hgt - 1.2 * chm
    # grout (shared by all variants) and the raised, slightly bevelled squares
    gn = fbm(880, 60.0, 60.0, 2)
    grout = CHECK_GROUT[None, None, :] * (1.0 + 0.25 * gn[..., None])
    col = mix(np.broadcast_to(grout, col.shape), col, inside)
    bevel = smoothstep(0.0, 6.0, depth)
    hgt = hgt * 0.6 + 2.2 * bevel - 1.0 * (1.0 - inside)
    return col, hgt


# crypt: the original cobbles, darkened, with moss and lichen in the joints and a few cracks

def blur(img, sigma):
    k = np.fft.fftfreq(N)
    filt = np.exp(-2.0 * (np.pi * sigma) ** 2 * (k[None, :] ** 2 + k[:, None] ** 2))
    return np.fft.ifft2(np.fft.fft2(img) * filt).real


def crypt_field():
    # the original Crypt.png (before F2 batch 2)
    col = np.asarray(git_source('materials/textures/Crypt.png', '2ac74a729^')).astype(float) / 255.0
    col = col * 0.85
    lum = col.mean(-1)
    low = blur(lum, 2.2)
    joint = 1.0 - smoothstep(0.12, 0.27, low)
    moss_n = pn(61, 14.0, 14.0, 3)
    grit = pn(62, 120.0, 120.0, 2)
    moss = smoothstep(-0.1, 1.0, moss_n) * joint
    mosscol = np.array([0.15, 0.19, 0.10])[None, None, :] * (0.75 + 0.45 * (grit[..., None] * 0.5 + 0.5))
    col = mix(col, mosscol, np.clip(moss * 0.9, 0, 0.85))
    # grey-green lichen speckles on the stones
    patch = smoothstep(0.7, 1.6, pn(63, 12.0, 12.0, 2))
    speck = smoothstep(1.4, 2.2, pn(64, 55.0, 55.0, 1)) * patch * (1.0 - joint)
    col = mix(col, np.array([0.36, 0.40, 0.32])[None, None, :] * (0.8 + 0.3 * grit[..., None]), speck * 0.55)
    # cracks
    crack = 1.0 - smoothstep(0.0, 0.05, np.abs(pn(65, 9.0, 9.0, 3)))
    crack = crack * smoothstep(0.3, 1.0, np.abs(pn(66, 3.0, 3.0, 1)))
    col = col * (1.0 - 0.6 * crack)[..., None]
    return col, None


# ---------------------------------------------------------------------------------------------------------------
# training hall, casino, prison (F2 batch 3): every layer is truly periodic (objects wrap around the tile edge)

class Layer(object):
    """Wrapping greyscale mask drawn supersampled (objects crossing a tile edge continue on the other side)."""

    def __init__(self):
        self.img = Image.new('L', (N * SUPER, N * SUPER), 0)
        self.draw = ImageDraw.Draw(self.img)

    def _each(self, pts):
        for ox in (-N, 0, N):
            for oy in (-N, 0, N):
                yield [((x + ox) * SUPER, (y + oy) * SUPER) for x, y in pts]

    def polygon(self, pts, val=255):
        for shifted in self._each(pts):
            self.draw.polygon(shifted, fill=val)

    def line(self, pts, width, val=255):
        for shifted in self._each(pts):
            self.draw.line(shifted, fill=val, width=max(1, int(width * SUPER)))

    def ellipse(self, cx, cy, rx, ry, ang, val=255):
        ca, sa = np.cos(ang), np.sin(ang)
        pts = []
        for t in np.linspace(0.0, 2.0 * np.pi, 28, endpoint=False):
            ex, ey = rx * np.cos(t), ry * np.sin(t)
            pts.append((cx + ca * ex - sa * ey, cy + sa * ex + ca * ey))
        self.polygon(pts, val)

    def result(self):
        return np.asarray(self.img.resize((N, N), Image.BOX)).astype(float) / 255.0


SAND = np.array([0.405, 0.335, 0.250])
SAND_DARK = np.array([0.240, 0.200, 0.145])


def training_hall_field():
    """Warm, fine-grained ochre sand: two grain scales, a few small pebbles with a little shadow and some
    drag marks. Everything is periodic (objects are drawn with all tile offsets)."""
    tone = fbm(401, 9.0, 9.0, 3)
    lump = fbm(402, 26.0, 26.0, 3)
    grain = fbm(403, 130.0, 130.0, 2)
    fine = fbm(408, 230.0, 230.0, 1)
    ripple = fbm(410, 7.0, 46.0, 2)
    col = SAND[None, None, :] * (1.0 + 0.03 * tone[..., None] + 0.03 * lump[..., None] + 0.075 * grain[..., None]
                                 + 0.05 * fine[..., None])
    hgt = 0.4 * lump + 0.26 * grain + 0.12 * fine + 0.08 * ripple + 0.12 * tone
    # slightly paler dry patches and a few darker compacted ones
    dry = smoothstep(0.6, 1.7, fbm(404, 5.0, 5.0, 2))
    col = mix(col, np.array([0.47, 0.39, 0.265])[None, None, :], dry * 0.25)
    rng = np.random.RandomState(406)
    # small pebbles, light, mid and dark, each with a short shadow to the lower right
    pebbles = [Layer(), Layer(), Layer()]
    shadow = Layer()
    for k in range(80):
        r = rng.uniform(2.6, 5.4)
        x, y = rng.uniform(0, N), rng.uniform(0, N)
        ry = r * rng.uniform(0.65, 1.0)
        a = rng.uniform(0, np.pi)
        shadow.ellipse(x + 1.6, y + 1.8, r * 1.05, ry * 1.05, a, 255)
        pebbles[(0, 0, 1, 1, 2)[k % 5]].ellipse(x, y, r, ry, a, 255)
    sh = np.clip(blur(shadow.result(), 1.1) * 1.3, 0.0, 1.0)
    col = col * (1.0 - 0.32 * sh)[..., None]
    for lyr, rgb in zip(pebbles, (np.array([0.55, 0.48, 0.37]), np.array([0.36, 0.31, 0.25]), np.array([0.27, 0.23, 0.18]))):
        m = np.clip(blur(lyr.result(), 0.55) * 1.4, 0.0, 1.0)
        col = mix(col, rgb[None, None, :] * (0.92 + 0.16 * grain[..., None]), m * 0.85)
        hgt = hgt + 2.0 * m
    hgt = hgt - 0.8 * sh
    # drag marks: short curved scuffs, soft
    scuff = Layer()
    for _ in range(16):
        x, y = rng.uniform(0, N), rng.uniform(0, N)
        ang = rng.uniform(0, 2 * np.pi)
        curve = rng.uniform(-0.05, 0.05)
        length = rng.uniform(40, 90)
        pts = []
        for t in np.linspace(0.0, 1.0, 10):
            a = ang + curve * t * length * 0.1
            pts.append((x + np.cos(a) * length * t, y + np.sin(a) * length * t))
        scuff.line(pts, rng.uniform(2.6, 4.6), 255)
    scuff_m = np.clip(blur(scuff.result(), 1.3) * 1.3, 0.0, 1.0)
    col = mix(col, SAND_DARK[None, None, :] * (0.92 + 0.16 * grain[..., None]), scuff_m * 0.38)
    hgt = hgt - 0.8 * scuff_m
    return col, hgt


WOOD = np.array([0.255, 0.170, 0.150])
WOOD_GAP = np.array([0.045, 0.03, 0.026])
HB_W = 32
HB_L = 4
HB_T = N // HB_W  # 16 cells per tile


def herringbone_ids():
    """Plank id, orientation (0 = along x, 1 = along y) and position inside the plank for every cell [x][y]."""
    cell_id = -np.ones((HB_T, HB_T), dtype=int)
    cell_dir = np.zeros((HB_T, HB_T), dtype=int)
    cell_pos = np.zeros((HB_T, HB_T), dtype=int)
    anchors = sorted({((i + 4 * j) % HB_T, (i - 4 * j) % HB_T) for i in range(-20, 21) for j in range(-20, 21)})
    for index, (ax, ay) in enumerate(anchors):
        for k in range(HB_L):
            for (x, y, d) in ((ax + k, ay, 0), (ax + 4, ay - 3 + k, 1)):
                cell_id[x % HB_T, y % HB_T] = index * 2 + d
                cell_dir[x % HB_T, y % HB_T] = d
                cell_pos[x % HB_T, y % HB_T] = k
    return cell_id, cell_dir, cell_pos


def casino_field():
    cell_id, cell_dir, cell_pos = herringbone_ids()
    cx = _XX // HB_W
    cy = _YY // HB_W
    pid = cell_id[cx, cy]
    direction = cell_dir[cx, cy]
    along_cells = cell_pos[cx, cy]
    lx = _XX % HB_W
    ly = _YY % HB_W
    u = np.where(direction == 0, along_cells * HB_W + lx, along_cells * HB_W + ly).astype(float)  # along plank
    v = np.where(direction == 0, ly, lx).astype(float)  # across plank
    length = HB_L * HB_W
    dist = np.minimum(np.minimum(u, length - 1 - u), np.minimum(v, HB_W - 1 - v))
    rng = np.random.RandomState(501)
    ptone = rng.uniform(0.86, 1.16, pid.max() + 1)
    phue = rng.uniform(-0.03, 0.03, pid.max() + 1)
    pt = ptone[pid]
    ph = phue[pid]
    grain_h = fbm(502, 5.0, 100.0, 3)
    grain = np.where(direction == 0, grain_h, grain_h.T)
    col = WOOD[None, None, :] * pt[..., None]
    col = col + np.stack([ph, ph * 0.3, -ph * 0.5], -1)
    col = col * (1.0 + 0.11 * grain)[..., None]
    col = col * (1.0 + 0.05 * fbm(503, 160.0, 160.0, 2))[..., None]
    hgt = 0.35 * grain + 0.3 * fbm(504, 40.0, 40.0, 2)
    # worn, sanded spots in front of the tables (lighter, smoother)
    worn = smoothstep(0.85, 1.9, fbm(505, 10.0, 10.0, 2))
    col = col * (1.0 + 0.12 * worn)[..., None]
    # spilled drinks: dark reddish blotches with a drying rim
    sn = fbm(506, 15.0, 15.0, 3)
    stain = smoothstep(1.25, 1.55, sn)
    rim = np.clip(1.0 - np.abs(sn - 1.25) / 0.09, 0.0, 1.0) * 0.6
    col = mix(col, col * np.array([0.60, 0.48, 0.44])[None, None, :], np.clip(stain * 0.7 + rim * 0.25, 0, 1))
    # dark joints and bevels
    t = smoothstep(0.6, 2.2, dist)
    col = mix(np.broadcast_to(WOOD_GAP, col.shape), col, t)
    col = col * (0.82 + 0.18 * smoothstep(1.0, 6.0, dist))[..., None]
    hgt = hgt * 0.6 + 2.2 * smoothstep(0.6, 4.5, dist) - 0.4 * stain
    return col, hgt


MUD_BROWN = np.array([0.175, 0.135, 0.098])
RUST = np.array([0.30, 0.17, 0.10])


def prison_field():
    # the original Prison.png (commit 6db9aa611): dark blue-grey cobbles with light mortar
    col = np.asarray(git_source('materials/textures/Prison.png', '6db9aa611')).astype(float) / 255.0
    lum = col.mean(-1)
    low = blur(lum, 1.6)
    # mortar = the light parts between the cobbles
    mortar = smoothstep(0.30, 0.42, low)
    grit = fbm(601, 130.0, 130.0, 2)
    mud = MUD_BROWN[None, None, :] * (0.85 + 0.25 * lum[..., None] / 0.3) * (1.0 + 0.08 * grit[..., None])
    col = mix(col, np.clip(mud, 0, 1), mortar * 0.92)
    # damp darkening on the cobbles
    damp = smoothstep(0.2, 1.4, fbm(602, 14.0, 14.0, 3))
    col = col * (1.0 - 0.10 * damp)[..., None]
    # rust stains
    rn = fbm(603, 13.0, 13.0, 3)
    rust = smoothstep(0.85, 1.6, rn) * (0.6 + 0.4 * smoothstep(-1.0, 1.0, fbm(604, 40.0, 40.0, 2)))
    col = mix(col, RUST[None, None, :] * (0.7 + 0.5 * lum[..., None] / 0.3), rust * 0.24)
    # straw wisps
    sp = Sprites(wrap=True)
    draw_straws(sp, straw_segments(605, 48, (0, 0, N, N), (20, 38)), (0.50, 0.40, 0.20))
    sc, sa, _ = sp.result()
    col = mix(col, sc, sa * 0.92)
    return col, None


# ---------------------------------------------------------------------------------------------------------------
# arena, torture, workshop (F2 batch 4): every layer is truly periodic

def wrap_dist(a, b):
    d = np.abs(a - b) % N
    return np.minimum(d, N - d)


def row_layout(seed, rows, min_w, max_w, warp_amp, warp_seed):
    """Wrapping rows of slabs / planks. Every row is cut into pieces of random width (min_w..max_w, wrapping
    around the tile), the coordinates are warped by periodic noise so the joints are not ruler straight.
    Returns piece id, distance to the nearest joint in pixels and the number of pieces."""
    rng = np.random.RandomState(seed)
    row_h = N // rows
    xw = (_XX + warp_amp * fbm(warp_seed, 7.0, 7.0, 2)) % N
    yw = (_YY + warp_amp * fbm(warp_seed + 1, 7.0, 7.0, 2)) % N
    row = np.minimum((yw // row_h).astype(int), rows - 1)
    ry = yw - row * row_h
    dist = np.minimum(ry, row_h - ry)
    pid = np.zeros((N, N), dtype=int)
    count = 0
    for r in range(rows):
        widths = []
        total = 0.0
        while total < N:
            widths.append(rng.uniform(min_w, max_w))
            total += widths[-1]
        widths = np.array(widths) * N / total
        cuts = (rng.uniform(0, N) + np.concatenate([[0.0], np.cumsum(widths)[:-1]])) % N
        cuts.sort()
        mask = row == r
        xs = xw[mask]
        idx = np.searchsorted(cuts, xs, 'right') - 1
        pid[mask] = count + idx % len(cuts)
        dx = np.min(wrap_dist(xs[:, None], cuts[None, :]), axis=1)
        dist[mask] = np.minimum(dist[mask], dx)
        count += len(cuts)
    return pid, dist, count


SLAB = np.array([0.238, 0.196, 0.186])


def torture_field():
    pid, dist, count = row_layout(801, 6, 90, 190, 4.0, 802)
    rng = np.random.RandomState(803)
    tone = rng.uniform(0.90, 1.10, count)[pid]
    hue = rng.uniform(-0.02, 0.025, count)[pid]
    mottle = fbm(804, 14.0, 14.0, 3)
    grit = fbm(805, 170.0, 170.0, 2)
    col = SLAB[None, None, :] * tone[..., None] + np.stack([-hue * 0.3, hue * 0.1, hue * 0.5], -1) * 0.5
    col = col * (1.0 + 0.10 * mottle + 0.07 * grit)[..., None]
    hgt = 0.5 * mottle + 0.4 * grit
    # wet patches: darker, a little bluer
    wet = smoothstep(0.1, 1.3, fbm(806, 12.0, 12.0, 3))
    col = col * (1.0 - 0.12 * wet)[..., None] * np.array([0.98, 1.0, 1.03])[None, None, :]
    hgt = hgt * (1.0 - 0.5 * wet)
    # dark stains
    sn = fbm(807, 14.0, 14.0, 3)
    stain = smoothstep(1.2, 1.6, sn)
    col = mix(col, np.array([0.085, 0.065, 0.06])[None, None, :] * (0.85 + 0.3 * grit[..., None]), stain * 0.6)
    # dark, wet joints with a bevel
    t = smoothstep(0.8, 3.2, dist)
    col = mix(np.broadcast_to(np.array([0.045, 0.047, 0.052]), col.shape), col, t)
    hgt = hgt * 0.7 + 2.6 * smoothstep(0.8, 5.0, dist)
    # drain grooves: short curved channels
    rng2 = np.random.RandomState(808)
    groove = Layer()
    for _ in range(9):
        x, y = rng2.uniform(0, N), rng2.uniform(0, N)
        ang = rng2.uniform(0, 2 * np.pi)
        curve = rng2.uniform(-0.08, 0.08)
        length = rng2.uniform(40, 90)
        pts = []
        for tt in np.linspace(0.0, 1.0, 12):
            a = ang + curve * tt * length * 0.1
            pts.append((x + np.cos(a) * length * tt, y + np.sin(a) * length * tt))
        groove.line(pts, rng2.uniform(3.0, 4.5), 255)
    gm = np.clip(blur(groove.result(), 0.9), 0.0, 1.0)
    col = mix(col, np.array([0.04, 0.042, 0.048])[None, None, :], gm * 0.6)
    hgt = hgt - 3.0 * gm
    # iron grates: dark recess with raised bars
    back = Layer()
    bars = Layer()
    for cx, cy in blue_noise_points(810, 8, 0):
        w, h = 26, 17
        back.polygon([(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2),
                      (cx - w / 2, cy + h / 2)], 255)
        for k in range(5):
            bx = cx - w / 2 + 3 + k * (w - 6) / 4.0
            bars.line([(bx, cy - h / 2 + 2), (bx, cy + h / 2 - 2)], 2.2, 255)
    bm = np.clip(back.result(), 0.0, 1.0)
    brm = np.clip(bars.result(), 0.0, 1.0)
    col = mix(col, np.array([0.03, 0.03, 0.034])[None, None, :], bm * 0.92)
    col = mix(col, np.array([0.16, 0.13, 0.11])[None, None, :] * (0.8 + 0.4 * grit[..., None]), brm * 0.9)
    hgt = hgt - 3.2 * bm + 3.6 * brm
    return col, hgt


WORKSHOP_PLANK = np.array([0.262, 0.205, 0.176])
IRON = np.array([0.135, 0.13, 0.13])
RUST_ORANGE = np.array([0.30, 0.15, 0.08])


def workshop_field():
    pid, dist, count = row_layout(901, 8, 160, 300, 3.0, 902)
    rng = np.random.RandomState(903)
    tone = rng.uniform(0.88, 1.12, count)[pid]
    hue = rng.uniform(-0.02, 0.02, count)[pid]
    grain = fbm(904, 4.0, 110.0, 3)
    grit = fbm(905, 150.0, 150.0, 2)
    col = WORKSHOP_PLANK[None, None, :] * tone[..., None] + np.stack([hue, hue * 0.4, -hue * 0.5], -1) * 0.5
    col = col * (1.0 + 0.12 * grain + 0.05 * grit)[..., None]
    hgt = 0.45 * grain + 0.25 * grit
    # soot: dark, slightly cool darkening in broad patches
    soot = smoothstep(-0.2, 1.4, fbm(906, 10.0, 10.0, 3))
    col = col * (1.0 - 0.16 * soot)[..., None] * np.array([1.0, 0.98, 0.96])[None, None, :]
    # dark joints
    t = smoothstep(0.6, 2.6, dist)
    col = mix(np.broadcast_to(np.array([0.03, 0.025, 0.024]), col.shape), col, t)
    col = col * (0.85 + 0.15 * smoothstep(1.0, 5.0, dist))[..., None]
    hgt = hgt + 2.0 * smoothstep(0.6, 4.0, dist)
    # oil: dark bluish-black blotches
    oil = smoothstep(1.35, 1.65, fbm(907, 16.0, 16.0, 3))
    col = mix(col, np.array([0.045, 0.045, 0.055])[None, None, :], oil * 0.6)
    hgt = hgt - 0.6 * oil
    # rust spots: small orange-brown flecks
    rust = smoothstep(1.5, 2.1, fbm(908, 26.0, 26.0, 2)) * smoothstep(0.0, 1.2, fbm(909, 4.0, 4.0, 2))
    col = mix(col, RUST_ORANGE[None, None, :] * (0.8 + 0.4 * grit[..., None]), rust * 0.6)
    # iron plates with rivets
    plates = Layer()
    rivets = Layer()
    prng = np.random.RandomState(912)
    for cx, cy in blue_noise_points(913, 10, 0):
        w, h = prng.uniform(34, 56), prng.uniform(26, 46)
        plates.polygon([(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2),
                        (cx - w / 2, cy + h / 2)], 255)
        for sx in (-1, 1):
            for sy in (-1, 1):
                rivets.ellipse(cx + sx * (w / 2 - 5), cy + sy * (h / 2 - 5), 2.4, 2.4, 0.0, 255)
    pm = np.clip(plates.result(), 0.0, 1.0)
    edge = np.clip(pm - blur(pm, 2.5), 0.0, 1.0)
    rv = np.clip(blur(rivets.result(), 0.6) * 1.3, 0.0, 1.0)
    plate_col = IRON[None, None, :] * (0.85 + 0.35 * (grit[..., None] * 0.5 + 0.5)) * (1.0 - 0.3 * soot)[..., None]
    col = mix(col, plate_col, pm)
    plate_rust = pm * smoothstep(1.1, 1.7, fbm(911, 14.0, 14.0, 3)) * 0.45
    col = mix(col, RUST_ORANGE[None, None, :], plate_rust)
    col = mix(col, col * 1.5, rv * 0.7)
    col = col * (1.0 - 0.6 * edge)[..., None]
    hgt = hgt + 3.0 * pm + 2.5 * rv - 1.2 * edge
    return col, hgt


# ---------------------------------------------------------------------------------------------------------------
# portal, portal wave (F2 batch 5): every layer is truly periodic; the wooden bridge atlas follows below

PORTAL_STONE = np.array([0.182, 0.172, 0.208])
WAVE_STONE = np.array([0.165, 0.212, 0.222])


def rune_layer(seed, count, glyph, width):
    """Hairline rune glyphs: a stem plus a few short straight strokes (horizontal, vertical, diagonal) inside a
    small box."""
    rng = np.random.RandomState(seed)
    layer = Layer()
    for _ in range(count):
        cx, cy = rng.uniform(0, N), rng.uniform(0, N)
        half = glyph * rng.uniform(0.7, 1.1)
        pts = [(rng.randint(0, 3) - 1, rng.randint(0, 3) - 1) for _ in range(rng.randint(3, 6))]
        if rng.randint(0, 2) == 0:
            layer.line([(cx, cy - half), (cx, cy + half)], width, 255)
        else:
            layer.line([(cx - half, cy), (cx + half, cy)], width, 255)
        for k in range(len(pts) - 1):
            a, b = pts[k], pts[k + 1]
            if a == b:
                continue
            layer.line([(cx + a[0] * half, cy + a[1] * half), (cx + b[0] * half, cy + b[1] * half)], width, 255)
    return layer.result()


def portal_field():
    pid, dist, count = row_layout(1001, 5, 110, 200, 4.0, 1002)
    rng = np.random.RandomState(1003)
    tone = rng.uniform(0.90, 1.10, count)[pid]
    hue = rng.uniform(-0.02, 0.02, count)[pid]
    mottle = fbm(1004, 12.0, 12.0, 3)
    grit = fbm(1005, 160.0, 160.0, 2)
    col = PORTAL_STONE[None, None, :] * tone[..., None] + np.stack([hue * 0.6, -hue * 0.3, hue * 0.8], -1) * 0.4
    col = col * (1.0 + 0.10 * mottle + 0.07 * grit)[..., None]
    hgt = 0.5 * mottle + 0.4 * grit
    # worn, smoother patches where the stone is walked
    worn = smoothstep(0.3, 1.5, fbm(1006, 10.0, 10.0, 2))
    col = col * (1.0 + 0.05 * worn)[..., None]
    hgt = hgt * (1.0 - 0.4 * worn)
    # dark grimy joints with a bevel
    t = smoothstep(0.8, 3.0, dist)
    col = mix(np.broadcast_to(np.array([0.045, 0.042, 0.058]), col.shape), col, t)
    col = col * (0.86 + 0.14 * smoothstep(1.0, 6.0, dist))[..., None]
    hgt = hgt * 0.7 + 2.4 * smoothstep(0.8, 5.0, dist)
    # faint violet bloom in some patches
    bloom = smoothstep(0.6, 1.8, fbm(1007, 13.0, 13.0, 3))
    col = mix(col, col * np.array([1.08, 0.96, 1.22])[None, None, :], bloom * 0.4)
    # hairline rune grooves: glyphs plus a few thin groove lines
    runes = np.clip(blur(rune_layer(1008, 40, 11, 1.4), 0.5) * 1.4, 0.0, 1.0)
    rng2 = np.random.RandomState(1009)
    lines = Layer()
    for _ in range(11):
        x, y = rng2.uniform(0, N), rng2.uniform(0, N)
        ang = rng2.choice([0.0, np.pi / 2, np.pi / 4, -np.pi / 4]) + rng2.uniform(-0.04, 0.04)
        length = rng2.uniform(40, 100)
        lines.line([(x, y), (x + np.cos(ang) * length, y + np.sin(ang) * length)], 1.2, 255)
    lm = np.clip(blur(lines.result(), 0.5) * 1.4, 0.0, 1.0)
    groove = np.clip(runes + lm * 0.8, 0.0, 1.0)
    col = mix(col, np.array([0.06, 0.045, 0.085])[None, None, :], groove * 0.8)
    col = mix(col, col * np.array([1.0, 0.95, 1.35])[None, None, :], blur(groove, 1.6) * 0.35)
    hgt = hgt - 2.6 * groove
    return col, hgt


def portal_wave_field():
    pid, dist, count = row_layout(1101, 7, 70, 150, 3.0, 1102)
    rng = np.random.RandomState(1103)
    tone = rng.uniform(0.88, 1.12, count)[pid]
    hue = rng.uniform(-0.02, 0.02, count)[pid]
    mottle = fbm(1104, 16.0, 16.0, 3)
    grit = fbm(1105, 190.0, 190.0, 2)
    col = WAVE_STONE[None, None, :] * tone[..., None] + np.stack([-hue * 0.5, hue * 0.2, hue * 0.6], -1) * 0.4
    col = col * (1.0 + 0.11 * mottle + 0.08 * grit)[..., None]
    hgt = 0.55 * mottle + 0.4 * grit
    # damp, cooler patches
    damp = smoothstep(0.2, 1.4, fbm(1106, 11.0, 11.0, 3))
    col = col * (1.0 - 0.08 * damp)[..., None] * np.array([0.97, 1.0, 1.05])[None, None, :]
    # dark joints
    t = smoothstep(0.7, 2.8, dist)
    col = mix(np.broadcast_to(np.array([0.035, 0.042, 0.055]), col.shape), col, t)
    col = col * (0.85 + 0.15 * smoothstep(1.0, 5.0, dist))[..., None]
    hgt = hgt * 0.7 + 2.2 * smoothstep(0.7, 4.5, dist)
    # ripple arcs: broken concentric rings around scattered points (distance on the torus, so the pattern is
    # periodic and has no ring centre at the tile centre)
    groove = np.zeros((N, N))
    prng = np.random.RandomState(1109)
    arcs = smoothstep(-0.3, 0.5, fbm(1108, 7.0, 7.0, 2))
    for cx, cy in blue_noise_points(1110, 15, 0):
        dx = wrap_dist(_XX + 0.0, cx)
        dy = wrap_dist(_YY + 0.0, cy)
        radius = np.sqrt(dx ** 2 + dy ** 2) + 2.5 * fbm(1107, 8.0, 8.0, 2)
        r0 = prng.uniform(14.0, 34.0)
        for k in range(prng.randint(1, 4)):
            groove = np.maximum(groove, (1.0 - smoothstep(1.0, 3.0, np.abs(radius - r0 - 17.0 * k))) * (1.0 - smoothstep(60.0, 80.0, radius)))
    groove = np.clip(groove * arcs, 0.0, 1.0)
    col = mix(col, np.array([0.05, 0.065, 0.095])[None, None, :], groove * 0.85)
    col = mix(col, col * np.array([0.95, 1.05, 1.30])[None, None, :], blur(groove, 2.0) * 0.26)
    hgt = hgt - 2.0 * groove
    return col, hgt


# wooden bridge atlas -------------------------------------------------------------------------------------------

def mesh_uv_triangles(mesh_path):
    """Texture coordinates and triangle list of models/WoodBridge.mesh (OGRE binary mesh v1.8, one sub mesh, 48
    byte vertices with the texture coordinates at offset 24, 16 bit indices)."""
    with open(mesh_path, 'rb') as f:
        data = f.read()
    found = {}

    def walk(pos, end):
        end = min(end, len(data))  # the length of the mesh chunk is a little too large in this file
        while pos + 6 <= end:
            cid, ln = struct.unpack_from('<HI', data, pos)
            body = pos + 6
            if cid == 0x3000:
                walk(body + 1, pos + ln)
            elif cid == 0x5000:
                found['count'], = struct.unpack_from('<I', data, body)
                walk(body + 4, pos + ln)
            elif cid == 0x5200:
                walk(body + 4, pos + ln)
            elif cid == 0x5210:
                found['verts'] = np.frombuffer(data[body:body + found['count'] * 48], dtype='<f4').reshape(-1, 12)
            elif cid == 0x4000:
                e = data.index(b'\n', body) + 1
                icount, = struct.unpack_from('<I', data, e + 1)
                found['idx'] = np.frombuffer(data[e + 6:e + 6 + icount * 2], dtype='<u2').reshape(-1, 3)
            pos += ln

    walk(24, len(data))
    return found['verts'][:, 6:8].astype(float), found['idx']


def id_map_grow(ids, steps):
    """Fills the pixels with id 0 around the painted ones with the neighbouring id (steps pixels wide)."""
    out = ids.copy()
    for _ in range(steps):
        empty = out == 0
        pad = np.pad(out, 1)
        best = np.zeros_like(out)
        for oy in (0, 1, 2):
            for ox in (0, 1, 2):
                nb = pad[oy:oy + N, ox:ox + N]
                best = np.where((best == 0) & (nb != 0), nb, best)
        out = np.where(empty, best, out)
    return out


def bridge_atlas(mesh_path):
    """Repaints the UV atlas of the wooden bridge. Every quad (two triangles) of the mesh is one plank; the
    planks are painted on the pixels of the mesh UVs plus a 14 px padding (no texture filtering bleed), the rest
    of the atlas is a neutral flat colour. Returns colour, height, painted zone and the pixels used by the UVs."""
    uv, tris = mesh_uv_triangles(mesh_path)
    ids_img = Image.new('I', (N, N), 0)
    draw = ImageDraw.Draw(ids_img)
    count = len(tris) // 2
    boxes = np.zeros((count + 1, 4))
    for i in range(count):
        pts = uv[np.concatenate([tris[2 * i], tris[2 * i + 1]])] * N
        boxes[i + 1] = (pts[:, 0].min(), pts[:, 1].min(), pts[:, 0].max(), pts[:, 1].max())
        for tri in (tris[2 * i], tris[2 * i + 1]):
            draw.polygon([(uv[k, 0] * N, uv[k, 1] * N) for k in tri], fill=i + 1)
    ids = np.asarray(ids_img).astype(int)
    covered = ids != 0
    ids = id_map_grow(ids, 14)
    zone = ids != 0
    rng = np.random.RandomState(1201)
    tone = rng.uniform(0.84, 1.16, count + 1)
    hue = rng.uniform(-0.03, 0.03, count + 1)
    off_y = rng.randint(0, N, count + 1)
    off_x = rng.randint(0, N, count + 1)
    yy = (_YY + off_y[ids]) % N
    xx = (_XX + off_x[ids]) % N
    ga = fbm(1202, 3.0, 70.0, 3)[yy, xx]
    gb = fbm(1203, 2.0, 22.0, 3)[yy, xx]
    grit = fbm(1204, 150.0, 150.0, 2)
    base = np.array([0.435, 0.348, 0.283])
    col = base[None, None, :] * tone[ids][..., None] + np.stack([hue[ids] * 0.6, hue[ids] * 0.2, -hue[ids] * 0.5], -1) * 0.5
    # weathering: silvery grey streaks along the grain
    silver = smoothstep(0.3, 1.6, gb)
    col = mix(col, np.array([0.40, 0.375, 0.345])[None, None, :] * tone[ids][..., None], silver * 0.45)
    col = col * (1.0 + 0.12 * ga + 0.05 * grit)[..., None]
    hgt = 0.6 * ga + 0.3 * gb + 0.25 * grit
    # dark joints at the plank border (bounding box of the quad)
    edge = np.minimum(np.minimum(_YY - boxes[ids, 1], boxes[ids, 3] - _YY),
                      np.minimum(_XX - boxes[ids, 0], boxes[ids, 2] - _XX))
    t = smoothstep(0.2, 2.2, edge)
    col = mix(np.broadcast_to(np.array([0.045, 0.036, 0.030]), col.shape), col, t)
    col = col * (0.78 + 0.22 * smoothstep(1.0, 7.0, edge))[..., None]
    hgt = hgt * 0.7 + 2.0 * smoothstep(0.2, 4.0, edge)
    # dirt: dark brown blotches, more along the joints
    dirt = smoothstep(0.2, 1.7, fbm(1205, 5.0, 5.0, 3)) * 0.5 + (1.0 - smoothstep(2.0, 10.0, edge)) * 0.35
    col = mix(col, np.array([0.135, 0.105, 0.075])[None, None, :], np.clip(dirt, 0.0, 0.8) * 0.6)
    # cracks along the grain
    rng2 = np.random.RandomState(1206)
    cracks = Layer()
    for _ in range(40):
        x, y = rng2.uniform(0, N), rng2.uniform(0, N)
        length = rng2.uniform(30, 90)
        cracks.line([(x, y), (x + length, y + rng2.uniform(-2, 2))], 1.2, 255)
    cm = np.clip(blur(cracks.result(), 0.5) * 1.4, 0.0, 1.0) * 0.8
    col = mix(col, np.array([0.08, 0.06, 0.045])[None, None, :], cm)
    hgt = hgt - 1.5 * cm
    # nail heads near both ends of the wide planks, with a rust streak running down
    nails = Layer()
    streak = Layer()
    for i in range(1, count + 1):
        w = boxes[i, 2] - boxes[i, 0]
        h = boxes[i, 3] - boxes[i, 1]
        if w < 60 or h < 14:
            continue
        sy = (boxes[i, 1] + boxes[i, 3]) * 0.5 + rng2.uniform(-h * 0.22, h * 0.22)
        for sx in (boxes[i, 0] + 11.0, boxes[i, 2] - 11.0):
            nails.ellipse(sx, sy, 2.6, 2.6, 0.0, 255)
            streak.line([(sx, sy), (sx + rng2.uniform(-0.5, 0.5), sy + rng2.uniform(5, 11))], 1.6, 255)
    nm = np.clip(blur(nails.result(), 0.5) * 1.4, 0.0, 1.0)
    sm = np.clip(blur(streak.result(), 0.8) * 1.4, 0.0, 1.0)
    rim = np.clip(blur(nm, 1.6) - nm, 0.0, 1.0)
    col = mix(col, np.array([0.30, 0.24, 0.19])[None, None, :], np.clip(rim * 1.2, 0.0, 0.5))
    col = mix(col, np.array([0.11, 0.07, 0.05])[None, None, :], sm * 0.45)
    col = mix(col, np.array([0.055, 0.050, 0.048])[None, None, :], nm * 0.95)
    hgt = hgt + 2.2 * nm - 0.6 * sm
    # outside the painted zone: neutral dark grey-brown, flat normal
    col = np.where(zone[..., None], col, np.array([0.20, 0.17, 0.15])[None, None, :])
    hgt = np.where(zone, hgt, 0.0)
    return col, hgt, zone, covered


# ---------------------------------------------------------------------------------------------------------------

ROOMS = {
    'dormitory': {
        # the planks run along x on every piece and every tile uses rotation 0, so the boards continue across
        # the pieces; one piece per neighbour mask (image sides that are exposed, see [dormitoryRoom] in tilesets.cfg)
        'field': dormitory_field, 'band': dormitory_band, 'strength': 1.4, 'rot_invariant': False,
        'pieces': {'Dormitory1111': ('', 'Dormitory1111Normal'), 'Dormitory': ('', 'DormitoryNormal'),
                   'Dormitory1011': ('B', 'Dormitory1011Normal'), 'Dormitory0111': ('R', 'Dormitory0111Normal'),
                   'Dormitory1101': ('L', 'Dormitory1101Normal'), 'Dormitory1110': ('T', 'Dormitory1110Normal'),
                   'Dormitory0110': ('RT', 'Dormitory0110Normal'), 'Dormitory1100': ('LT', 'Dormitory1100Normal'),
                   'Dormitory0011': ('RB', 'Dormitory0011Normal'), 'Dormitory1001': ('BL', 'Dormitory1001Normal'),
                   'Dormitory0000': ('RBLT', 'Dormitory0000Normal'), 'Dormitory0001': ('RBL', 'Dormitory0001Normal'),
                   'Dormitory0010': ('RBT', 'Dormitory0010Normal'), 'Dormitory0100': ('RLT', 'Dormitory0100Normal'),
                   'Dormitory1000': ('BLT', 'Dormitory1000Normal'), 'Dormitory0101': ('RL', 'Dormitory0101Normal'),
                   'Dormitory1010': ('BT', 'Dormitory1010Normal')},
    },
    'library': {
        # carpet with a border: like the dormitory, every tile uses rotation 0 and there is one piece
        # per neighbour mask (image sides that are exposed, see [libraryRoom] in tilesets.cfg)
        'field': library_field, 'band': library_band, 'band_all': True, 'strength': 0.7, 'rot_invariant': False,
        'pieces': dict(('Library' + m, (sides, 'Library' + m + 'Normal')) for m, sides in (
            ('1111', ''), ('1011', 'B'), ('0111', 'R'), ('1101', 'L'), ('1110', 'T'), ('0110', 'RT'), ('1100', 'LT'),
            ('0011', 'RB'), ('1001', 'BL'), ('0000', 'RBLT'), ('0001', 'RBL'), ('0010', 'RBT'), ('0100', 'RLT'),
            ('1000', 'BLT'), ('0101', 'RL'), ('1010', 'BT'))),
    },
    'hatchery': {
        # the open piece has four variants (straw in the interior only, flat calm earth along the tile border), picked
        # at random per tile through [oneOf] in config/tilesets.cfg; the wall pieces use variant 0 as their base
        'field': hatchery_field, 'band': hatchery_band, 'strength': 1.15, 'rot_invariant': False,
        'variant_of': {'FarmB': 1, 'FarmC': 2, 'FarmD': 3},
        'pieces': {'FarmB': ('', 'FarmBNormal'), 'FarmC': ('', 'FarmCNormal'), 'FarmD': ('', 'FarmDNormal'),
                   'Farm0000': ('TBLR', 'Farm0000Normal'), 'Farm1000': ('TBL', 'Farm1000Normal'),
                   'Farm1010': ('TB', 'Farm1010Normal'), 'Farm1011': ('T', 'Farm1011Normal'),
                   'Farm1100': ('TL', 'Farm1100Normal'), 'Farm': ('', 'FarmNormal')},
    },
    'dungeonTemple': {
        # four variants, picked at random per tile: same base, different ember cracks (see temple_field)
        'field': temple_field, 'band': None, 'strength': 1.3,
        'variant_of': {'DungeonTempleFloorB': 1, 'DungeonTempleFloorC': 2, 'DungeonTempleFloorD': 3},
        'pieces': {'DungeonTempleFloor': ('', 'DungeonTempleFloorNormal'),
                   'DungeonTempleFloorB': ('', 'DungeonTempleFloorBNormal'),
                   'DungeonTempleFloorC': ('', 'DungeonTempleFloorCNormal'),
                   'DungeonTempleFloorD': ('', 'DungeonTempleFloorDNormal')},
    },
    'treasury': {
        # marble checkerboard, four variants (same grout and squares, different veining, dirt and stains), picked at
        # random per tile through [oneOf] in config/tilesets.cfg; painted as is, no tone correction
        'field': treasury_field, 'band': None, 'strength': 1.3, 'tone': None,
        'variant_of': {'TreasuryFloorB': 1, 'TreasuryFloorC': 2, 'TreasuryFloorD': 3},
        'pieces': {'Treasury': ('', 'TreasuryNormal'), 'TreasuryFloorB': ('', 'TreasuryFloorBNormal'),
                   'TreasuryFloorC': ('', 'TreasuryFloorCNormal'), 'TreasuryFloorD': ('', 'TreasuryFloorDNormal')},
    },
    'crypt': {
        # the existing CryptNormal.png is kept
        'field': crypt_field, 'band': None, 'strength': 0.0,
        'pieces': {'Crypt': ('', None)},
    },
    'trainingHall': {
        'field': training_hall_field, 'band': None, 'strength': 1.2,
        'pieces': {'TrainingHallFloor': ('', 'TrainingHallFloorNormal')},
    },
    'casino': {
        'field': stone_slab_field, 'band': None, 'strength': 1.3,
        'pieces': {'CasinoFloor': ('', 'CasinoFloorNormal')},
    },
    'prison': {
        # the existing PrisonNormal.png is kept
        'field': prison_field, 'band': None, 'strength': 0.0,
        'pieces': {'Prison': ('', None)},
    },
    'arena': {
        # Arena.png is repainted in place: the pit meshes (ArenaLowered, ArenaFallOf) use the material Arena. It shows
        # the former treasury floor (irregular flagstones), painted with the treasury's tone correction
        'field': flagstone_field, 'band': None, 'strength': 1.3, 'tone': 'treasury',
        'pieces': {'Arena': ('', 'ArenaNormal')},
    },
    'torture': {
        'field': torture_field, 'band': None, 'strength': 1.3,
        'pieces': {'TortureFloor': ('', 'TortureFloorNormal')},
    },
    'workshop': {
        'field': workshop_field, 'band': None, 'strength': 1.3,
        'pieces': {'WorkshopFloor': ('', 'WorkshopFloorNormal')},
    },
    'portal': {
        # own file, Claimed.png / ClaimedMask.png (claimed ground, seat colour emblem mask) stay untouched
        'field': portal_field, 'band': None, 'strength': 1.3,
        'pieces': {'PortalFloor': ('', 'PortalFloorNormal')},
    },
    'portalWave': {
        'field': portal_wave_field, 'band': None, 'strength': 1.3,
        'pieces': {'PortalWaveFloor': ('', 'PortalWaveFloorNormal')},
    },
}


def build(room):
    spec = ROOMS[room]
    fields = {}
    result = {}
    for name, (sides, normal_name) in spec['pieces'].items():
        variant = spec.get('variant_of', {}).get(name, 0)  # variants of the open field (see temple_field)
        if variant not in fields:
            fields[variant] = spec['field'](variant) if 'variant_of' in spec else spec['field']()
            if room in FLATTEN:
                fields[variant] = (flatten(fields[variant][0], *FLATTEN[room]), fields[variant][1])
        field_col, field_hgt = fields[variant]
        col, hgt = field_col.copy(), (field_hgt.copy() if field_hgt is not None else None)
        if spec.get('band_all') and sides:
            col, hgt = spec['band'](col, hgt, sides)
        else:
            for side in sides:
                col, hgt = spec['band'](col, hgt, side)
        result[name] = (tone(col, spec.get('tone', room)), hgt, normal_name, sides)
    return result, spec['strength']


def border_check(result):
    """Field borders must be mirror and transpose symmetric; opposite open sides of a piece must agree."""
    worst = 0.0
    for name, (col, hgt, _, sides) in result.items():
        if not sides:
            top, left = col[0], col[:, 0]
            worst = max(worst, np.abs(top - top[::-1]).max(), np.abs(top - left).max(),
                        np.abs(top - col[N - 1]).max(), np.abs(left - col[:, N - 1]).max())
        else:
            for a, b, la, lb in (('T', 'B', col[0], col[N - 1]), ('L', 'R', col[:, 0], col[:, N - 1])):
                if a not in sides and b not in sides:
                    worst = max(worst, np.abs(la - lb).max())
    return worst


def wrap_check(result):
    """Periodicity: the jump across the tile wrap-around divided by the typical jump between neighbouring
    pixels (about 1 or less when there is no seam). Also: the sides of a piece that are not exposed must equal
    the base piece (far away from the exposed sides)."""
    worst = 0.0
    base = None
    for name, (col, hgt, _, sides) in result.items():
        if not sides:
            base = col
    for name, (col, hgt, _, sides) in result.items():
        for axis, ends in ((0, 'TB'), (1, 'LR')):
            if any(e in sides for e in ends):
                continue  # a wall fringe on an exposed side is meant to end there
            typical = np.abs(np.diff(col, axis=axis)).mean()
            jump = np.abs(np.take(col, 0, axis=axis) - np.take(col, N - 1, axis=axis)).mean()
            worst = max(worst, jump / (typical + 1e-9))
        if sides and base is not None:
            for side, sl in (('T', (slice(0, 3), slice(200, N - 200))), ('B', (slice(N - 3, N), slice(200, N - 200))),
                             ('L', (slice(200, N - 200), slice(0, 3))), ('R', (slice(200, N - 200), slice(N - 3, N)))):
                if side not in sides:
                    diff = np.abs(col[sl] - base[sl]).max()
                    if diff > 0.02:
                        print(name, 'side', side, 'differs from the base tile by', diff)
    return worst


def seam_image(result):
    """The open floor piece rolled by half a tile in x and y (the former borders cross the middle), tiled 2x2."""
    for col, _, _, sides in result.values():
        if not sides:
            return to_image(np.tile(np.roll(col, (N // 2, N // 2), (0, 1)), (2, 2, 1)))


# Rooms whose floors are periodic and get the wrap seam ratio (--check) and the rolled preview (--seamcheck).
SEAM_CHECK_ROOMS = ('hatchery', 'dormitory', 'dungeonTemple', 'treasury', 'trainingHall', 'casino', 'prison', 'arena',
                    'torture', 'workshop', 'portal', 'portalWave')


def main():
    args =[a for a in sys.argv[1:] if not a.startswith('--')]
    out = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'materials',
                                            'textures')
    seam_dir = None
    for a in sys.argv[1:]:
        if a.startswith('--seamcheck='):
            seam_dir = a.split('=', 1)[1]
    seam_rooms = SEAM_CHECK_ROOMS if seam_dir else ()
    for room in ROOMS:
        result, strength = build(room)
        if '--check' in sys.argv and ROOMS[room].get('rot_invariant', room in ('library',)):
            print(room, 'max asymmetry on open borders (0..1): %.3f' % border_check(result))
        if '--check' in sys.argv and room in SEAM_CHECK_ROOMS:
            print(room, 'wrap seam ratio (about 1 or less = no seam): %.2f' % wrap_check(result))
        if room in seam_rooms:
            seam_image(result).save(os.path.join(seam_dir, 'f2-%s-seamcheck.png' % room.lower()))
        for name, (col, hgt, normal_name, _) in result.items():
            to_image(col).save(os.path.join(out, name + '.png'), optimize=True)
            if normal_name is not None:
                to_image(normal_map(hgt, strength)).save(os.path.join(out, normal_name + '.png'), optimize=True)
            print(name, 'mean RGB', (col.reshape(-1, 3).mean(0) * 255).round().astype(int))
    # wooden bridge atlas (repainted in place, the UV layout comes from the mesh)
    col, hgt, zone, covered = bridge_atlas(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                                                        'models', 'WoodBridge.mesh'))
    if '--check' in sys.argv:
        print('WoodBridge painted zone %.1f %% of the atlas, mesh UV pixels %.1f %%' % (zone.mean() * 100, covered.mean() * 100))
    col = tone(col, 'bridgeWooden')
    to_image(col).convert('RGBA').save(os.path.join(out, 'WoodBridge.png'), optimize=True)
    to_image(normal_map(hgt, 1.3)).convert('RGBA').save(os.path.join(out, 'WoodBridgeNormal.png'), optimize=True)
    print('WoodBridge mean RGB of the UV pixels', (col[covered].mean(0) * 255).round().astype(int))


if __name__ == '__main__':
    main()
