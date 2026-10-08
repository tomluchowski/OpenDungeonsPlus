#!/usr/bin/env python3
"""Generates the darker, muted wall textures of the dungeon palette (materials/textures/).

  - DirtWall.png / DirtWallNormal.png: warm dark brown, rough natural earth (no regular cells) for the earth wall (dirtFull) tiles and their fog of war
    mesh. 256x256, truly periodic (FFT noise and a wrapping cell pattern), so it stays seam free with the world
    space UV of DirtTile.vert (one repeat per tile on upward facing surfaces).
  - DungeonClaimedWall*.png: the existing atlases of the reinforced wall are regraded in place. Bricks and stone caps
    keep their painted structure (mortar, cracks, relief) but become darker and a cool blue-grey (clearly different from the warm earth wall), and the flat brown earth
    path on top is replaced by a mottled slate surface. The normal maps and the ownership mask are not touched.

Original work of the project, licence CC0. Everything is procedural and seeded; running the script again gives
byte-identical files. Needs numpy and Pillow and, for the wall atlases, git (the unchanged atlases are read from
the history, commit ORIGINAL_COMMIT, so the script can be run repeatedly).

Usage: python generate_wall_textures.py <materials/textures folder> [output folder]
"""

import io
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ORIGINAL_COMMIT = os.environ.get('WALL_ATLAS_COMMIT', '21320e9d6')  # a commit before the palette change, with the unregraded wall atlases

LUMA = np.array([0.299, 0.587, 0.114])


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def fbm(n, seed, fx, octaves=3):
    """Periodic noise on an n x n grid, zero mean, unit variance; fx = feature count per tile."""
    rng = np.random.RandomState(seed)
    k = np.fft.fftfreq(n) * n
    kx = k[None, :]
    ky = k[:, None]
    total = np.zeros((n, n))
    for octave in range(octaves):
        white = rng.standard_normal((n, n))
        scale = 2.0 ** octave
        filt = np.exp(-(kx / (fx * scale)) ** 2 - (ky / (fx * scale)) ** 2)
        layer = np.fft.ifft2(np.fft.fft2(white) * filt).real
        layer /= layer.std() + 1e-9
        total += layer * 0.55 ** octave
    return total / (total.std() + 1e-9)


def normal_map(height, strength):
    gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    gx *= strength / (gx.std() + 1e-9) * 0.30
    gy *= strength / (gy.std() + 1e-9) * 0.30
    nrm = np.stack([-gx, gy, np.ones_like(gx)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    return nrm * 0.5 + 0.5


def save_rgb(arr, path):
    Image.fromarray((np.clip(arr, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), 'RGB').save(path, optimize=True)


def cells_on_torus(n, seed, count, warp_amp, warp_seed):
    """Irregular angular cells on the torus: cell id, distance to the nearest cell edge (px), offset from the cell
    centre (px). Cell centres come from a jittered grid so the sizes stay even."""
    rng = np.random.RandomState(seed)
    side = int(round(np.sqrt(count)))
    step = n / side
    pts = []
    for i in range(side):
        for j in range(side):
            pts.append(((i + 0.5 + rng.uniform(-0.38, 0.38)) * step, (j + 0.5 + rng.uniform(-0.38, 0.38)) * step))
    yy, xx = np.mgrid[0:n, 0:n].astype(float)
    xw = xx + warp_amp * fbm(n, warp_seed, 5.0, 2)
    yw = yy + warp_amp * fbm(n, warp_seed + 1, 5.0, 2)
    d1 = np.full((n, n), 1e9)
    d2 = np.full((n, n), 1e9)
    idx = np.zeros((n, n), dtype=int)
    off = np.zeros((n, n, 2))
    for i, p in enumerate(pts):
        for sx in (-n, 0, n):
            for sy in (-n, 0, n):
                dx = xw - (p[0] + sx)
                dy = yw - (p[1] + sy)
                d = np.sqrt(dx * dx + dy * dy)
                closer = d < d1
                second = (~closer) & (d < d2)
                d2 = np.where(closer, d1, np.where(second, d, d2))
                idx = np.where(closer, i, idx)
                off[..., 0] = np.where(closer, dx, off[..., 0])
                off[..., 1] = np.where(closer, dy, off[..., 1])
                d1 = np.where(closer, d, d1)
    return idx, (d2 - d1) * 0.5, off


# Screen values are about 1.2x the texture luminance and 1.25x the texture saturation (room shader gain and
# saturation boost, cursor light); the means below follow the project's dungeon palette and were calibrated
# with the overview render.
EARTH_MEAN = np.array([46.0, 36.0, 27.0]) / 255.0   # warm dark brown, hue about 28 degrees


def ridged(n, seed, fx, octaves=3):
    """Periodic ridged noise in 0..1 (bright thin ridges), used for the rough natural rock of the earth wall."""
    return 1.0 - np.abs(np.tanh(0.9 * fbm(n, seed, fx, octaves)))


def earth_wall():
    n = 256
    # natural rock and packed earth: no regular cells, only warped broad patches, ridged crevices and grit
    patch = fbm(n, 103, 3.5, 3)
    patch2 = fbm(n, 106, 9.0, 3)
    ridge = ridged(n, 107, 8.0, 3)
    ridge2 = ridged(n, 108, 20.0, 2)
    grain = fbm(n, 104, 60.0, 2)
    pits = smoothstep(1.5, 2.3, fbm(n, 109, 14.0, 2))
    crev = smoothstep(0.80, 0.97, ridge)
    crev2 = smoothstep(0.88, 0.99, ridge2)
    lum = 1.0 + 0.20 * patch + 0.12 * patch2 + 0.09 * grain - 0.38 * crev - 0.20 * crev2 - 0.30 * pits
    warm = fbm(n, 110, 4.0, 2)
    col = EARTH_MEAN[None, None, :] * lum[..., None]
    col *= (1.0 + 0.10 * warm[..., None] * np.array([1.0, 0.0, -1.0])[None, None, :])
    height = 0.50 * ridge + 0.20 * ridge2 + 0.25 * patch + 0.12 * patch2 + 0.08 * grain - 0.35 * pits
    return col, normal_map(height, 3.2)


def rgb_to_sat(a):
    mx = a.max(-1)
    mn = a.min(-1)
    return np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0.0)


# Reinforced wall regrade. Regions are separated by saturation and position in the atlas: high saturation above
# row 300 is the brown earth path on top, saturated pixels below it are brick, the rest is the stone of the caps.
BRICK = dict(mean=np.array([44.0, 48.0, 60.0]) / 255.0, keep=0.04, contrast=1.7)
STONE = dict(mean=np.array([50.0, 53.0, 62.0]) / 255.0, keep=0.04, contrast=1.3)
TOP_MEAN = np.array([40.0, 43.0, 52.0]) / 255.0


def regrade(old, weight, p):
    lum = (old * LUMA).sum(-1)
    m = (lum * weight).sum() / max(weight.sum(), 1.0)
    chroma = old - lum[..., None]
    target = (p['mean'] * LUMA).sum()
    newlum = target * (1.0 + (lum - m) / max(m, 1e-3) * p['contrast'])
    return p['mean'][None, None, :] / target * newlum[..., None] + chroma * p['keep']


def slate_top(w, h):
    """Mottled dark slate, periodic over the 256 px atlas width so neighbouring tiles continue each other."""
    n = 256
    lum = 1.0 + 0.10 * fbm(n, 201, 4.0, 3) + 0.07 * fbm(n, 202, 14.0, 2) + 0.06 * fbm(n, 203, 70.0, 2)
    crack = fbm(n, 204, 16.0, 1)
    lum *= 1.0 - 0.14 * smoothstep(1.4, 1.95, np.abs(crack))
    field = TOP_MEAN[None, None, :] * lum[..., None]
    field *= (1.0 + 0.04 * fbm(n, 205, 6.0, 1)[..., None] * np.array([1.0, 0.0, -1.0])[None, None, :])
    out = np.zeros((h, w, 3))
    for y in range(0, h, n):
        for x in range(0, w, n):
            out[y:y + n, x:x + n] = field[:min(n, h - y), :min(n, w - x)]
    return out


def original_atlas(texdir, name):
    path = os.path.abspath(os.path.join(texdir, name))
    try:
        data = subprocess.check_output(['git', 'show', '%s:materials/textures/%s' % (ORIGINAL_COMMIT, name)],
                                       cwd=os.path.dirname(path), stderr=subprocess.DEVNULL)
        return Image.open(io.BytesIO(data))
    except (subprocess.CalledProcessError, OSError):
        # not a git checkout or commit unknown: fall back to the file as it is
        return Image.open(path)


def wall_atlas(img):
    rgba = np.array(img.convert('RGBA')).astype(float) / 255.0
    old = rgba[..., :3]
    h, w = old.shape[:2]
    sat = rgb_to_sat(old)
    rows = np.arange(h)[:, None] * np.ones((1, w))
    earth = smoothstep(0.38, 0.52, sat) * (rows < 300)
    brick = smoothstep(0.24, 0.33, sat) * (rows >= 300)
    # the grey dot under the ownership mask (radius about 14 px around the centre of the path) is painted like the path
    cols = np.arange(w)[None, :] * np.ones((h, 1))
    dot = (((cols - 128.0) ** 2 + (rows - 128.0) ** 2) < 26.0 ** 2)
    earth = np.where(dot, 1.0, earth)
    stone = np.clip(1.0 - earth - brick, 0.0, 1.0)
    new = (earth[..., None] * slate_top(w, h) + brick[..., None] * regrade(old, brick, BRICK)
           + stone[..., None] * regrade(old, stone, STONE))
    out = np.concatenate([np.clip(new, 0, 1), rgba[..., 3:]], axis=-1)
    return Image.fromarray((out * 255.0 + 0.5).astype(np.uint8), 'RGBA')


def main():
    texdir = sys.argv[1]
    outdir = sys.argv[2] if len(sys.argv) > 2 else texdir
    os.makedirs(outdir, exist_ok=True)
    col, nrm = earth_wall()
    save_rgb(col, os.path.join(outdir, 'DirtWall.png'))
    save_rgb(nrm, os.path.join(outdir, 'DirtWallNormal.png'))
    for code in ('0000', '0100', '0101', '0110', '1110', '1111'):
        name = 'DungeonClaimedWall%s.png' % code
        wall_atlas(original_atlas(texdir, name)).save(os.path.join(outdir, name), optimize=True)


if __name__ == '__main__':
    main()
