"""Small height-field renderer for the forged emblems (icons) of the game interface.

An emblem is built from layers. Every layer is a signed distance shape with a material and a
height profile (rounded bevel, flat top). The renderer keeps an albedo, a height, a specular
and an emission map, lights them from the top left with a bump-mapped Blinn-Phong model, adds
cavity darkening and soft drop shadows, and reduces the result with a box filter (supersampling).
All coordinates are design units: the visible square is -1..1 on both axes, y grows downwards.
"""
import numpy as np

LIGHT = np.array([-0.52, -0.62, 0.60], dtype=np.float32)
LIGHT = LIGHT / np.linalg.norm(LIGHT)
HALF = LIGHT + np.array([0.0, 0.0, 1.0], dtype=np.float32)
HALF = HALF / np.linalg.norm(HALF)


def blur(a, sigma):
    """Separable gaussian blur of a 2D or 3D (h, w, c) float array; sigma in pixels."""
    if sigma < 0.3:
        return a
    radius = int(np.ceil(sigma * 3))
    k = np.exp(-0.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    k = (k / k.sum()).astype(np.float32)
    out = a
    for axis in (0, 1):
        pad = [(0, 0)] * out.ndim
        pad[axis] = (radius, radius)
        padded = np.pad(out, pad, mode="edge")
        acc = np.zeros_like(out)
        length = out.shape[axis]
        for i, w in enumerate(k):
            sl = [slice(None)] * out.ndim
            sl[axis] = slice(i, i + length)
            acc += padded[tuple(sl)] * w
        out = acc
    return out


def shift(a, dx, dy):
    """Shifts an array by whole pixels, filling with zero."""
    out = np.zeros_like(a)
    h, w = a.shape[:2]
    ys, yd = (slice(max(dy, 0), h + min(dy, 0)), slice(max(-dy, 0), h + min(-dy, 0)))
    xs, xd = (slice(max(dx, 0), w + min(dx, 0)), slice(max(-dx, 0), w + min(-dx, 0)))
    out[ys, xs] = a[yd, xd]
    return out


class Mat:
    """A surface material: colour (optionally a top to bottom gradient), spec, shininess, metal."""

    def __init__(self, base, bottom=None, spec=0.4, shin=24.0, metal=0.0, grain=0.05, mottle=0.06,
                 brushed=0.0, emit=None, emit_noise=0.0, stripes=0.0, stripe_scale=0.06):
        self.base = np.array(base, dtype=np.float32)
        self.bottom = np.array(bottom if bottom is not None else base, dtype=np.float32)
        self.spec, self.shin, self.metal = spec, shin, metal
        self.grain, self.mottle, self.brushed = grain, mottle, brushed
        self.emit = None if emit is None else np.array(emit, dtype=np.float32)
        self.emit_noise = emit_noise
        self.stripes, self.stripe_scale = stripes, stripe_scale


class Canvas:
    def __init__(self, cells=128, ss=4, extent=(1.0, 1.0), seed=1):
        n = cells * ss
        self.cells, self.ss, self.n = cells, ss, n
        ex, ey = extent
        xs = ((np.arange(n, dtype=np.float32) + 0.5) / n * 2 - 1) * ex
        ys = ((np.arange(n, dtype=np.float32) + 0.5) / n * 2 - 1) * ey
        self.X, self.Y = np.meshgrid(xs, ys)
        self.X0, self.Y0 = self.X.copy(), self.Y.copy()
        self.dx0, self.dy0 = 2.0 * ex / n, 2.0 * ey / n
        self.dx, self.dy = self.dx0, self.dy0
        self.px = min(self.dx, self.dy)
        self.glow_r = None
        self.clip_box = None
        self.alb = np.zeros((n, n, 3), dtype=np.float32)
        self.h = np.zeros((n, n), dtype=np.float32)
        self.a = np.zeros((n, n), dtype=np.float32)
        self.spec = np.zeros((n, n), dtype=np.float32)
        self.shin = np.full((n, n), 20.0, dtype=np.float32)
        self.metal = np.zeros((n, n), dtype=np.float32)
        self.emit = np.zeros((n, n, 3), dtype=np.float32)
        self.seed = seed
        self._noise = {}

    def set_zoom(self, k):
        """Draws the following layers k times larger around the centre (the frame keeps its size)."""
        self.X, self.Y = self.X0 / k, self.Y0 / k
        self.dx, self.dy = self.dx0 / k, self.dy0 / k
        self.px = min(self.dx, self.dy)

    def snapshot(self):
        return [a.copy() for a in (self.alb, self.h, self.a, self.spec, self.shin, self.metal, self.emit)]

    def restore_outside(self, snap, r):
        """Undoes everything drawn since the snapshot outside the circle of radius r, or outside the
        rounded square set with clip_box (soft edge)."""
        keep = np.clip(self._outside(r) / self.dx0 + 0.5, 0, 1)
        for cur, old in zip((self.alb, self.h, self.a, self.spec, self.shin, self.metal, self.emit), snap):
            k = keep[..., None] if cur.ndim == 3 else keep
            cur[...] = cur * (1 - k) + old * k

    # ---- noise ------------------------------------------------------------------------------
    def noise(self, cell, seed=0):
        """Smooth value noise in -1..1 with a feature size of `cell` design units."""
        key = (round(cell, 4), seed)
        if key not in self._noise:
            g = max(int(2.0 / cell) + 3, 4)
            grid_ = np.random.RandomState(self.seed * 131 + seed).uniform(-1, 1, (g, g)).astype(np.float32)
            t = np.linspace(0, g - 3, self.n, dtype=np.float32)
            i0 = t.astype(int)
            f = t - i0
            f = f * f * (3 - 2 * f)
            rows = grid_[i0] * (1 - f)[:, None] + grid_[i0 + 1] * f[:, None]
            cols = rows[:, i0] * (1 - f)[None, :] + rows[:, i0 + 1] * f[None, :]
            self._noise[key] = cols
        return self._noise[key]

    def fine(self, seed=0):
        return np.random.RandomState(self.seed * 977 + seed).uniform(-1, 1, (self.n, self.n)).astype(np.float32)

    # ---- distance fields -----------------------------------------------------------------------
    def circle(self, cx, cy, r):
        return np.hypot(self.X - cx, self.Y - cy) - r

    def ellipse(self, cx, cy, rx, ry, rot=0.0):
        x, y = self.X - cx, self.Y - cy
        if rot:
            c, s = np.cos(rot), np.sin(rot)
            x, y = x * c + y * s, -x * s + y * c
        return (np.hypot(x / rx, y / ry) - 1.0) * min(rx, ry)

    def box(self, cx, cy, hw, hh, rad=0.0, rot=0.0):
        x, y = self.X - cx, self.Y - cy
        if rot:
            c, s = np.cos(rot), np.sin(rot)
            x, y = x * c + y * s, -x * s + y * c
        qx, qy = np.abs(x) - (hw - rad), np.abs(y) - (hh - rad)
        return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rad

    def seg(self, ax, ay, bx, by, r, r2=None):
        """Capsule; with r2 a tapered capsule (radius r at a, r2 at b)."""
        dx, dy = bx - ax, by - ay
        ln = max(dx * dx + dy * dy, 1e-9)
        t = np.clip(((self.X - ax) * dx + (self.Y - ay) * dy) / ln, 0, 1)
        d = np.hypot(self.X - (ax + t * dx), self.Y - (ay + t * dy))
        return d - (r if r2 is None else r + (r2 - r) * t)

    def poly(self, pts):
        """Polygon: signed distance by edge distance and even-odd winding."""
        d = np.full(self.X.shape, 1e9, dtype=np.float32)
        inside = np.zeros(self.X.shape, dtype=bool)
        m = len(pts)
        for i in range(m):
            ax, ay = pts[i]
            bx, by = pts[(i + 1) % m]
            ex, ey = bx - ax, by - ay
            ln = max(ex * ex + ey * ey, 1e-9)
            t = np.clip(((self.X - ax) * ex + (self.Y - ay) * ey) / ln, 0, 1)
            d = np.minimum(d, np.hypot(self.X - (ax + t * ex), self.Y - (ay + t * ey)))
            cond = ((ay > self.Y) != (by > self.Y)) & (self.X < (bx - ax) * (self.Y - ay) / (by - ay + 1e-12) + ax)
            inside ^= cond
        return np.where(inside, -d, d)

    def ring_box(self, cx, cy, half, rad, w):
        """Outline of a rounded square (half width `half`, corner radius `rad`), w wide on both sides."""
        return np.abs(self.box(cx, cy, half, half, rad)) - w

    def ring(self, cx, cy, r, w):
        return np.abs(np.hypot(self.X - cx, self.Y - cy) - r) - w

    def arc(self, cx, cy, r, w, a0, a1):
        """Arc band between angles a0..a1 (radians, y down), rounded ends."""
        ang = np.arctan2(self.Y - cy, self.X - cx)
        rel = np.mod(ang - a0, 2 * np.pi)
        band = np.abs(np.hypot(self.X - cx, self.Y - cy) - r) - w
        ends = np.minimum(np.hypot(self.X - (cx + r * np.cos(a0)), self.Y - (cy + r * np.sin(a0))),
                          np.hypot(self.X - (cx + r * np.cos(a1)), self.Y - (cy + r * np.sin(a1)))) - w
        return np.where(rel <= (a1 - a0), band, ends)

    @staticmethod
    def union(*a):
        out = a[0]
        for s in a[1:]:
            out = np.minimum(out, s)
        return out

    @staticmethod
    def smooth_union(a, b, k):
        h = np.clip(0.5 + 0.5 * (b - a) / k, 0, 1)
        return b * (1 - h) + a * h - k * h * (1 - h)

    @staticmethod
    def subtract(a, b):
        return np.maximum(a, -b)

    @staticmethod
    def intersect(a, b):
        return np.maximum(a, b)

    # ---- layers ------------------------------------------------------------------------------------------
    def coverage(self, sdf):
        return np.clip(0.5 - sdf / self.px, 0.0, 1.0)

    def shadow(self, sdf, strength=0.5, dx=0.05, dy=0.07, soft=0.04):
        """Darkens what lies below a shape (soft cast shadow toward the lower right)."""
        cov = self.coverage(sdf)
        moved = shift(cov, int(round(dx / self.dx)), int(round(dy / self.dy)))
        sh = blur(moved, soft / self.px)
        self.alb *= (1.0 - strength * sh * self.a)[..., None]

    def add(self, sdf, mat, z=0.08, bevel=0.04, base=0.0, shadow=0.0, sh_dx=0.05, sh_dy=0.07,
            sh_soft=0.04, tint=None, emit=None):
        """Places a shape. z is the height of the flat top over `base`; bevel the width of the round edge."""
        cov = self.coverage(sdf)
        if not cov.any():
            return
        if shadow > 0:
            self.shadow(sdf, shadow, sh_dx, sh_dy, sh_soft)
        depth = np.clip(-sdf / max(bevel, 1e-6), 0, 1)
        prof = np.sqrt(np.clip(1 - (1 - depth) ** 2, 0, 1))
        top = base + z * prof
        alb, spec, shin, metal, em = self._material(mat, cov, tint)
        c3 = cov[..., None]
        self.alb = self.alb * (1 - c3) + alb * c3
        self.h = self.h * (1 - cov) + top * cov
        self.spec = self.spec * (1 - cov) + spec * cov
        self.shin = self.shin * (1 - cov) + shin * cov
        self.metal = self.metal * (1 - cov) + metal * cov
        self.emit = self.emit * (1 - c3) + (em * c3 if em is not None else 0)
        if emit is not None:
            self.emit += np.array(emit, dtype=np.float32)[None, None, :] * c3
        self.a = self.a + cov * (1 - self.a)

    def _material(self, mat, cov, tint):
        ys, xs = np.nonzero(cov > 0.5)
        if len(ys):
            y0, y1 = self.Y[ys.min(), 0], self.Y[ys.max(), 0]
        else:
            y0, y1 = -1.0, 1.0
        t = np.clip((self.Y - y0) / max(y1 - y0, 1e-6), 0, 1)[..., None]
        base = mat.base[None, None, :] * (1 - t) + mat.bottom[None, None, :] * t
        var = 1.0 + mat.mottle * self.noise(0.12, 5)[..., None] + mat.grain * self.fine(9)[..., None]
        if mat.brushed:
            var = var + mat.brushed * self.noise(0.9, 3)[..., None] * self.noise(0.05, 4)[..., None]
        if mat.stripes:
            wobble = self.noise(0.5, 6) * 0.05
            var = var + mat.stripes * np.sin((self.Y + wobble) / mat.stripe_scale * 3.14159)[..., None]
        alb = base * var
        if tint is not None:
            alb = alb * np.array(tint, dtype=np.float32)[None, None, :]
        em = None
        if mat.emit is not None:
            k = 1.0 + mat.emit_noise * self.noise(0.1, 8)
            em = mat.emit[None, None, :] * k[..., None]
        return (alb, mat.spec, mat.shin, mat.metal, em)

    def paint(self, sdf, colour, opacity=1.0):
        """Colours the existing surface (inlays, engraving colour) without changing height."""
        cov = self.coverage(sdf) * opacity * (self.a > 0)
        c = np.array(colour, dtype=np.float32)[None, None, :]
        self.alb = self.alb * (1 - cov[..., None]) + c * cov[..., None]

    def carve(self, sdf, depth=0.04, bevel=0.02, colour=None):
        """Cuts a groove into the surface below a shape."""
        cov = self.coverage(sdf) * (self.a > 0)
        d = np.clip(-sdf / max(bevel, 1e-6), 0, 1)
        self.h = self.h - depth * np.sqrt(np.clip(1 - (1 - d) ** 2, 0, 1)) * cov
        if colour is not None:
            k = cov[..., None] * 0.9
            self.alb = self.alb * (1 - k) + np.array(colour, dtype=np.float32)[None, None, :] * k

    def glow(self, cx, cy, r, colour, strength=1.0, power=2.0):
        d = np.hypot(self.X - cx, self.Y - cy) / r
        f = np.exp(-(d ** power)) * strength * self._glow_mask()
        self.emit += np.array(colour, dtype=np.float32)[None, None, :] * f[..., None] * (self.a > 0)[..., None]

    def _outside(self, r):
        """Signed distance out of the drawing area: a circle of radius r, or the rounded square clip_box."""
        if self.clip_box is not None:
            half, rad = self.clip_box
            qx, qy = np.abs(self.X0) - (half - rad), np.abs(self.Y0) - (half - rad)
            return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rad
        return np.hypot(self.X0, self.Y0) - r

    def _glow_mask(self):
        if self.glow_r is None:
            return 1.0
        return np.clip(-self._outside(self.glow_r) / 0.06, 0, 1)

    def glow_sdf(self, sdf, colour, reach=0.1, strength=1.0):
        f = np.exp(-np.maximum(sdf, 0) / reach) * strength * self._glow_mask()
        self.emit += np.array(colour, dtype=np.float32)[None, None, :] * f[..., None] * (self.a > 0)[..., None]

    def fill_well(self, sdf, centre, edge, cx=0.0, cy=0.0, r=1.0, z=-0.02):
        """A recessed radial gradient, used for the dark well of a medallion."""
        cov = self.coverage(sdf)
        d = np.clip(np.hypot(self.X - cx, self.Y - cy) / r, 0, 1)[..., None]
        lit = np.clip(1 - np.hypot(self.X - cx + 0.25, self.Y - cy + 0.3) / (r * 1.4), 0, 1)[..., None]
        col = np.array(edge, dtype=np.float32) * d + np.array(centre, dtype=np.float32) * (1 - d)
        col = col * (0.8 + 0.4 * lit)
        col = col * (1.0 + 0.06 * self.noise(0.1, 11)[..., None] + 0.03 * self.fine(12)[..., None])
        c3 = cov[..., None]
        self.alb = self.alb * (1 - c3) + col * c3
        self.h = self.h * (1 - cov) + z * cov
        self.spec = self.spec * (1 - cov) + 0.05 * cov
        self.metal = self.metal * (1 - cov)
        self.emit = self.emit * (1 - c3)
        self.a = self.a + cov * (1 - self.a)

    # ---- lighting ------------------------------------------------------------------------------------------------
    def render(self, bump=1.0, ambient=0.34, drop=None, ao=1.4):
        s = self.ss
        hb = blur(self.h, 0.55 * s)
        gy, gx = np.gradient(hb, self.dy, self.dx)
        nx, ny = -gx * bump, -gy * bump
        norm = np.sqrt(nx * nx + ny * ny + 1.0)
        nx, ny, nz = nx / norm, ny / norm, 1.0 / norm
        diff = np.clip(nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2], 0, 1)
        # cavity: how far the surface lies below its wider neighbourhood
        cav = np.clip(blur(self.h, 5.0 * s) - self.h, 0, 0.2) * 5.0 * ao
        occl = 1.0 / (1.0 + 1.6 * cav)
        shade = ambient * occl + (1 - ambient) * (diff / LIGHT[2]) * 0.92 * (0.55 + 0.45 * occl)
        rgb = self.alb * shade[..., None]
        sp = np.clip(nx * HALF[0] + ny * HALF[1] + nz * HALF[2], 0, 1) ** self.shin * self.spec * occl
        spec_col = 255.0 * (1 - self.metal[..., None]) + self.alb * self.metal[..., None] * 1.6
        rgb = rgb + spec_col * sp[..., None] * 0.9
        rgb = rgb + self.emit
        rgb = np.clip(rgb, 0, 255)
        a = np.clip(self.a, 0, 1)
        out = np.concatenate([rgb, a[..., None]], axis=2)
        if drop is not None:
            out = self._drop(out, *drop)
        return self._reduce(out)

    def _drop(self, out, dx=0.05, dy=0.07, soft=0.05, strength=0.65):
        a = out[..., 3]
        moved = shift(a, int(round(dx / self.dx)), int(round(dy / self.dy)))
        sh = np.clip(blur(moved, soft / self.px), 0, 1) * strength
        sh_a = sh * (1 - a)
        new_a = a + sh_a
        rgb = (out[..., :3] * a[..., None] + np.array([6, 3, 2], dtype=np.float32) * sh_a[..., None]) / np.maximum(new_a, 1e-4)[..., None]
        return np.concatenate([rgb, new_a[..., None]], axis=2)

    def _reduce(self, out):
        s = self.ss
        n = self.n
        a = out[..., 3:4]
        pre = np.concatenate([out[..., :3] * a, a], axis=2)
        pre = pre.reshape(n // s, s, n // s, s, 4).mean(axis=(1, 3))
        alpha = pre[..., 3:4]
        rgb = pre[..., :3] / np.maximum(alpha, 1e-4)
        return np.clip(np.concatenate([rgb, alpha * 255.0], axis=2), 0, 255)
