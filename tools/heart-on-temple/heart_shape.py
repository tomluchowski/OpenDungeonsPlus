"""Shape of the dungeon heart, shared by generate_heart_geometry.py and tools/heart-textures.

The heart is described as an implicit surface (a smooth union of blobs with grooves cut into it) that is
star shaped around the origin, so every point of it is found by marching a ray from the origin. The same
description gives the mesh (see generate_heart_geometry.py) and, per texel, the surface point of the
texture (see tools/heart-textures/generate_heart_textures.py), so that veins, wounds and the geometry that
belongs to them sit at the same place.

Coordinates: x is right, y points to the back (the camera looks along +y), z is up, unit is one tile.
The heart's centre is the origin and its apex points down.
"""

import numpy as np

TIERS = ('Healthy', 'Damaged', 'Critical')
# Size of the whole heart relative to the shape below, about the apex, so that the apex stays on the pedestal
HEART_SIZE = 0.75
# The injured tiers shrink a little more
TIER_SCALE = {'Healthy': HEART_SIZE, 'Damaged': HEART_SIZE * 0.96, 'Critical': HEART_SIZE * 0.92}

# Texture layout (v, top to bottom): the body is mapped by a spherical projection around the x axis onto
# the first part of the image, the second part holds the walls of the vessel stubs and the sinew bands wrapped round them
BODY_V = 0.75
WALL_V = (0.77, 0.92)
SINEW_V = (0.93, 0.99)


def unit(v):
    return v / np.linalg.norm(v, axis=-1, keepdims=True)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b * (1.0 - h) + a * h - k * h * (1.0 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


def ellipsoid(p, centre, radii):
    q = (p - np.array(centre)) / np.array(radii)
    return (np.linalg.norm(q, axis=-1) - 1.0) * min(radii)


def dist_polyline(p, points):
    """Distance from every point of p to the polyline, and the parameter (0..1 along its length) of the nearest point."""
    best = np.full(len(p), 1.0e9)
    where = np.zeros(len(p))
    lengths = np.linalg.norm(np.diff(points, axis=0), axis=1)
    total = lengths.sum()
    done = 0.0
    for i in range(len(points) - 1):
        a = points[i]
        ab = points[i + 1] - a
        t = np.clip(((p - a) @ ab) / (ab @ ab), 0.0, 1.0)
        d = np.linalg.norm(p - (a + t[:, None] * ab), axis=1)
        closer = d < best
        best = np.where(closer, d, best)
        where = np.where(closer, (done + t * lengths[i]) / total, where)
        done += lengths[i]
    return best, where


BLOBS = (
    ((0.12, 0.00, -0.12), (0.78, 0.68, 0.98)),   # left ventricle, the big mass
    ((-0.36, -0.22, 0.07), (0.52, 0.46, 0.68)),  # right ventricle, in front
    ((0.20, -0.02, -0.80), (0.36, 0.34, 0.42)),  # apex
    ((-0.56, 0.05, 0.78), (0.33, 0.32, 0.30)),   # right atrium
    ((0.40, -0.12, 0.85), (0.30, 0.26, 0.28)),   # left auricle
    ((0.00, 0.32, 0.30), (0.55, 0.36, 0.55)),    # back
)


def base_sdf(p):
    f = ellipsoid(p, *BLOBS[0])
    for blob in BLOBS[1:]:
        f = smin(f, ellipsoid(p, *blob), 0.28)
    return f


def surface(directions, f, t_max=2.6, iterations=30):
    """Point on the surface of f along each direction from the origin (f must be negative at the origin)."""
    directions = unit(directions)
    lo = np.zeros(len(directions))
    hi = np.full(len(directions), t_max)
    for _ in range(iterations):
        mid = 0.5 * (lo + hi)
        inside = f(directions * mid[:, None]) < 0.0
        lo = np.where(inside, mid, lo)
        hi = np.where(inside, hi, mid)
    return directions * (0.5 * (lo + hi))[:, None]


def surface_point(points, f):
    return surface(np.array(points, dtype=float), f)


def gradient(f, p, eps=0.004):
    g = np.zeros_like(p)
    for axis in range(3):
        step = np.zeros(3)
        step[axis] = eps
        g[:, axis] = f(p + step) - f(p - step)
    return unit(g)


def path_points(f, control, samples):
    """Smooth (Catmull-Rom) path through the control points projected onto the surface of f."""
    control = np.array(control, dtype=float)
    return surface_point(catmull_rom(control, samples), f)


def catmull_rom(control, samples):
    padded = np.vstack([2.0 * control[0] - control[1], control, 2.0 * control[-1] - control[-2]])
    out = []
    per = max(1, samples // (len(control) - 1))
    for i in range(len(control) - 1):
        p0, p1, p2, p3 = padded[i], padded[i + 1], padded[i + 2], padded[i + 3]
        for k in range(per):
            t = k / per
            out.append(0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t
                              + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t ** 3))
    out.append(control[-1])
    return np.array(out)


# Grooves between the chambers (control points are only directions; they are projected onto the surface)
GROOVES = (
    # between the ventricles, front
    (0.085, ((-0.05, -0.70, 0.72), (0.02, -0.75, 0.30), (0.10, -0.72, -0.10), (0.16, -0.62, -0.55), (0.22, -0.45, -0.95))),
    # between atria and ventricles, all the way round the front
    (0.07, ((-0.78, -0.25, 0.62), (-0.52, -0.58, 0.58), (-0.16, -0.72, 0.66), (0.20, -0.68, 0.70),
            (0.55, -0.48, 0.72), (0.82, -0.18, 0.66))),
    # back, between the ventricles
    (0.07, ((0.02, 0.68, 0.72), (0.10, 0.70, 0.20), (0.18, 0.58, -0.30), (0.24, 0.34, -0.85))),
)

# Raised veins: (radius at the root, radius at the end, control points)
VEINS = (
    (0.055, 0.030, ((-0.03, -0.72, 0.85), (0.02, -0.77, 0.30), (0.10, -0.74, -0.10), (0.16, -0.63, -0.55), (0.22, -0.46, -0.98))),
    (0.040, 0.020, ((0.02, -0.77, 0.30), (-0.25, -0.76, 0.20), (-0.50, -0.62, 0.08), (-0.72, -0.44, -0.10))),
    (0.036, 0.018, ((0.10, -0.74, -0.10), (-0.16, -0.72, -0.25), (-0.38, -0.62, -0.42))),
    (0.040, 0.020, ((0.05, -0.78, 0.25), (0.35, -0.70, 0.18), (0.62, -0.52, 0.08), (0.86, -0.22, -0.05))),
    (0.034, 0.018, ((0.14, -0.68, -0.40), (0.42, -0.60, -0.48), (0.62, -0.40, -0.62))),
    (0.048, 0.030, ((-0.80, -0.22, 0.66), (-0.52, -0.60, 0.62), (-0.16, -0.74, 0.70), (0.20, -0.70, 0.74),
                    (0.55, -0.50, 0.78), (0.84, -0.18, 0.70))),
    (0.036, 0.018, ((0.36, -0.30, 0.98), (0.58, -0.46, 0.62), (0.80, -0.38, 0.22))),
    (0.032, 0.016, ((-0.30, -0.42, 0.90), (-0.48, -0.56, 0.62))),
    # back
    (0.055, 0.028, ((0.04, 0.72, 0.85), (0.10, 0.74, 0.30), (0.16, 0.62, -0.20), (0.22, 0.38, -0.80))),
    (0.040, 0.020, ((0.10, 0.74, 0.30), (0.42, 0.62, 0.20), (0.75, 0.32, 0.10))),
    (0.038, 0.020, ((0.10, 0.74, 0.30), (-0.20, 0.66, 0.22), (-0.55, 0.42, 0.12), (-0.82, 0.10, 0.0))),
    (0.034, 0.018, ((0.16, 0.62, -0.20), (0.45, 0.50, -0.34), (0.68, 0.22, -0.45))),
    # sides
    (0.040, 0.020, ((0.85, 0.08, 0.66), (0.92, 0.05, 0.10), (0.80, 0.0, -0.40), (0.55, -0.02, -0.80))),
    (0.040, 0.020, ((-0.88, 0.10, 0.60), (-0.94, 0.05, 0.07), (-0.70, -0.02, -0.45))),
)

# Vessel stubs: (radius at the root, radius at the end, control points from inside the body to the rounded end)
VESSELS = (
    (0.21, 0.17, ((0.10, -0.02, 0.55), (0.16, 0.00, 0.95), (0.30, 0.02, 1.30), (0.52, 0.02, 1.52))),
    (0.18, 0.15, ((-0.15, -0.20, 0.60), (-0.20, -0.18, 1.00), (-0.30, -0.12, 1.40), (-0.36, -0.10, 1.56))),
    (0.15, 0.125, ((-0.55, 0.05, 0.70), (-0.62, 0.06, 1.05), (-0.66, 0.08, 1.38))),
    (0.12, 0.10, ((0.25, 0.25, 0.50), (0.35, 0.50, 0.75), (0.42, 0.80, 0.86))),
    (0.11, 0.09, ((0.00, 0.30, 0.60), (0.05, 0.60, 0.85), (0.05, 0.88, 0.96))),
)

# Injuries per tier. Each is drawn into the geometry (dents and slits are cut into the surface) and into the
# textures (bruises, scars, trickles). Points are directions from the centre, projected onto the surface.
INJURIES = {
    'Healthy': {'dents': (), 'slits': (), 'bruises': (), 'scars': (), 'wounds': ()},
    'Damaged': {
        'dents': (((0.50, -0.60, 0.45), 0.18, 0.075), ((-0.50, -0.62, 0.20), 0.16, 0.07), ((0.15, -0.45, 0.85), 0.15, 0.06)),
        'slits': (((-0.38, -0.62, 0.62), (-0.14, -0.68, 0.34)), ((0.62, -0.50, 0.05), (0.48, -0.58, -0.18))),
        'bruises': (((0.45, -0.62, 0.35), 0.40), ((-0.55, -0.58, 0.45), 0.36), ((0.05, -0.50, 0.85), 0.32)),
        'scars': (((-0.85, -0.25, 0.50), (-0.68, -0.48, 0.48), (-0.42, -0.65, 0.42), (-0.18, -0.72, 0.32)),),
        'wounds': ((-0.26, -0.66, 0.50), (0.56, -0.54, -0.04)),
    },
    'Critical': {
        'dents': (((0.50, -0.60, 0.45), 0.20, 0.09), ((-0.50, -0.62, 0.20), 0.18, 0.085), ((0.15, -0.45, 0.85), 0.17, 0.08),
                  ((0.05, -0.72, 0.10), 0.16, 0.08), ((-0.82, -0.10, 0.45), 0.15, 0.07), ((0.85, -0.05, 0.30), 0.16, 0.07)),
        'slits': (((-0.38, -0.62, 0.62), (-0.12, -0.68, 0.30)), ((0.62, -0.50, 0.05), (0.44, -0.58, -0.28)),
                  ((0.12, -0.55, 0.78), (0.32, -0.45, 0.60)), ((-0.72, -0.42, 0.28), (-0.80, -0.30, 0.06))),
        'bruises': (((0.45, -0.62, 0.35), 0.46), ((-0.55, -0.58, 0.45), 0.42), ((0.05, -0.50, 0.85), 0.38),
                    ((0.05, -0.72, -0.12), 0.34), ((0.80, -0.20, 0.50), 0.32)),
        'scars': (((-0.85, -0.25, 0.50), (-0.68, -0.48, 0.48), (-0.42, -0.65, 0.42), (-0.18, -0.72, 0.32)),
                  ((0.30, -0.70, 0.55), (0.48, -0.62, 0.38), (0.60, -0.50, 0.24)),
                  ((-0.30, -0.45, 0.85), (-0.10, -0.55, 0.78), (0.05, -0.60, 0.68))),
        'wounds': ((-0.26, -0.66, 0.50), (0.50, -0.56, -0.08), (0.20, -0.52, 0.70), (-0.74, -0.38, 0.24), (0.05, -0.72, 0.08)),
    },
}


class Heart:
    """The implicit surface of one tier: base blobs, grooves and the tier's dents and slits."""

    def __init__(self, tier):
        self.tier = tier
        self.grooves = []
        for radius, control in GROOVES:
            self.grooves.append((radius, path_points(base_sdf, control, 40)))
        self.dent_points = []
        self.slit_points = []
        injuries = INJURIES[tier]
        grooved = self._grooved
        for centre, radius, depth in injuries['dents']:
            self.dent_points.append((surface_point([centre], grooved)[0], radius, depth))
        for a, b in injuries['slits']:
            self.slit_points.append(surface_point([a, b], grooved))
        self.f = self._final

    def _grooved(self, p):
        f = base_sdf(p)
        for radius, points in self.grooves:
            d, _ = dist_polyline(p, points)
            f = smax(f, radius - d, 0.06)
        return f

    def _final(self, p):
        f = self._grooved(p)
        for centre, radius, depth in self.dent_points:
            # a sphere whose centre lies (radius - depth) above the surface leaves a dent of that depth
            n = np.linalg.norm(centre)
            outward = centre / n
            sphere = np.linalg.norm(p - (centre + outward * (radius - depth)), axis=1) - radius
            f = smax(f, -sphere, 0.03)
        for points in self.slit_points:
            d, _ = dist_polyline(p, points)
            f = smax(f, 0.045 - d, 0.015)
        return f

    def wound_points(self):
        return surface_point(INJURIES[self.tier]['wounds'], self._final) if INJURIES[self.tier]['wounds'] else np.zeros((0, 3))


def uv_from_direction(d):
    d = unit(d)
    theta = np.arccos(np.clip(d[..., 0], -1.0, 1.0))
    phi = np.arctan2(d[..., 1], d[..., 2])
    return phi / (2.0 * np.pi) + 0.5, theta / np.pi * BODY_V


def direction_from_uv(u, v):
    theta = v / BODY_V * np.pi
    phi = (u - 0.5) * 2.0 * np.pi
    return np.stack([np.cos(theta), np.sin(theta) * np.sin(phi), np.sin(theta) * np.cos(phi)], axis=-1)
