#!/usr/bin/env python3
"""Generate the worker and spider beds (Ogre XML meshes + atlas textures) and the
restyled standard bed textures.  Original work, CC0.

Usage: generate_beds.py OUT_DIR
Writes ImpBed.xml, SpiderBed.xml (convert with OgreXMLConverter), ImpBed.png,
SpiderBed.png, BedWood.png, BedBlanket.png, BedLinen.png into OUT_DIR.
Meshes keep the 1x1 footprint (x, y within +-0.42) of the old meshes; head end at +y.
"""
import math
import os
import sys
import numpy as np
from PIL import Image, ImageDraw


def noise(n, scale, seed):
    r = np.random.RandomState(seed)
    small = r.rand(max(2, n // scale), max(2, n // scale))
    img = Image.fromarray((small * 255).astype(np.uint8)).resize((n, n), Image.BICUBIC)
    return np.asarray(img).astype(float) / 255.0


def save(arr, path):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(path)


def fbm(n, seed):
    total = 0.0
    for s, w in ((4, .5), (8, .3), (16, .2), (64, .2)):
        total = total + noise(n, s, seed + s) * w
    return total / 1.2


def planks(n, base, seed):
    """Dark coarse wooden planks."""
    f = fbm(n, seed)
    img = np.zeros((n, n, 3))
    pw = n // 5
    for i in range(5):
        tone = 0.8 + 0.4 * np.random.RandomState(seed + i).rand()
        img[:, i * pw:(i + 1) * pw] = np.array(base) * tone
    grain = np.tile(noise(n, 2, seed + 3)[:1, :], (n, 1)) * 0.5 + noise(n, 3, seed + 4) * 0.5
    img = img * (0.75 + 0.5 * f[..., None]) * (0.85 + 0.3 * grain[..., None])
    for i in range(1, 5):
        img[:, i * pw - 2:i * pw + 1] *= 0.4
    return img


def straw(n, seed, base=(118, 98, 60)):
    f = noise(n, 2, seed) * 0.5 + noise(n, 6, seed + 1) * 0.5
    lines = np.asarray(Image.fromarray((noise(n, 1, seed + 5) * 255).astype(np.uint8))
                       .resize((n, n // 8), Image.NEAREST).resize((n, n), Image.BICUBIC)).astype(float) / 255
    return np.array(base) * (0.65 + 0.7 * (0.5 * f + 0.5 * lines))[..., None]


def cloth(n, base, seed, patches=True):
    f = fbm(n, seed)
    w = np.sin(np.arange(n) * 1.6)
    weave = 0.93 + 0.07 * w[None, :] * w[:, None]
    img = np.array(base) * (0.75 + 0.5 * f[..., None]) * weave[..., None]
    if patches:
        p = noise(n, 24, seed + 9) > 0.72
        img[p] *= 0.78
    return img


def iron(n, seed):
    f = fbm(n, seed)
    img = np.array((72, 68, 66)) * (0.7 + 0.6 * f[..., None])
    rust = (noise(n, 10, seed + 1) > 0.78) & (noise(n, 3, seed + 2) > 0.5)
    img[rust] = img[rust] * np.array((1.25, 0.9, 0.65))
    return img


def atlas(bands):
    """bands: list of (fraction, array); returns the texture and the (v0, v1) range per band."""
    n = 512
    out = np.zeros((n, n, 3))
    y = 0
    ranges = []
    acc = 0.0
    for frac, arr in bands:
        hh = int(n * frac)
        out[y:y + hh] = arr[:hh]
        ranges.append((acc, acc + frac))
        acc += frac
        y += hh
    return out, ranges


class Mesh:
    def __init__(self):
        self.v = []
        self.f = []

    def quad(self, p, n, band):
        b0, b1 = band
        base = len(self.v)
        uv = [(0, b1 - 0.004), (1, b1 - 0.004), (1, b0 + 0.004), (0, b0 + 0.004)]
        for pt, t in zip(p, uv):
            self.v.append((pt, n, t))
        self.f += [(base, base + 1, base + 2), (base, base + 2, base + 3)]

    def box(self, c, s, band, taper=1.0):
        """Axis aligned box; taper scales the top face."""
        cx, cy, cz = c
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        t = taper
        B = [(cx - hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz - hz), (cx + hx, cy + hy, cz - hz), (cx - hx, cy + hy, cz - hz)]
        T = [(cx - hx * t, cy - hy * t, cz + hz), (cx + hx * t, cy - hy * t, cz + hz), (cx + hx * t, cy + hy * t, cz + hz), (cx - hx * t, cy + hy * t, cz + hz)]
        self.quad([B[3], B[2], B[1], B[0]], (0, 0, -1), band)
        self.quad(T, (0, 0, 1), band)
        self.quad([B[0], B[1], T[1], T[0]], (0, -1, 0), band)
        self.quad([B[1], B[2], T[2], T[1]], (1, 0, 0), band)
        self.quad([B[2], B[3], T[3], T[2]], (0, 1, 0), band)
        self.quad([B[3], B[0], T[0], T[3]], (-1, 0, 0), band)

    def cocoon(self, c, rx, ry, hz, sides, band):
        """Half-round elongated lump along y (upper half of a rounded prism)."""
        cx, cy, cz = c
        segs = 7
        rings = []
        for i in range(segs + 1):
            t = i / segs
            k = math.sqrt(max(0.0, 1 - (2 * t - 1) ** 2)) ** 0.7 * 0.9 + 0.1
            rings.append((cy - ry + 2 * ry * t, k))

        def pt(y, k, ang):
            return (cx + math.cos(ang) * rx * k, y, cz + math.sin(ang) * hz * k)

        for a in range(sides):
            a0, a1 = math.pi * a / sides, math.pi * (a + 1) / sides
            for i in range(segs):
                (y0, k0), (y1, k1) = rings[i], rings[i + 1]
                p = [pt(y0, k0, a0), pt(y0, k0, a1), pt(y1, k1, a1), pt(y1, k1, a0)]
                u = [p[1][j] - p[0][j] for j in range(3)]
                v = [p[3][j] - p[0][j] for j in range(3)]
                n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
                ln = math.sqrt(sum(x * x for x in n)) or 1
                n = tuple(x / ln for x in n)
                mid = [(p[0][j] + p[2][j]) / 2 for j in range(3)]
                out = (mid[0] - cx, 0.0, mid[2] - cz)
                if sum(n[j] * out[j] for j in range(3)) < 0:
                    p = [p[1], p[0], p[3], p[2]]
                    n = tuple(-x for x in n)
                self.quad(p, n, band)


def write_xml(meshes, path):
    """meshes: list of (material, Mesh)."""
    L = ['<?xml version="1.0"?>', '<mesh>', '\t<submeshes>']
    for mat, m in meshes:
        L.append('\t\t<submesh material="%s" usesharedvertices="false" use32bitindexes="false" operationtype="triangle_list">' % mat)
        L.append('\t\t\t<faces count="%d">' % len(m.f))
        for a, b, c in m.f:
            L.append('\t\t\t\t<face v1="%d" v2="%d" v3="%d" />' % (a, b, c))
        L.append('\t\t\t</faces>')
        L.append('\t\t\t<geometry vertexcount="%d">' % len(m.v))
        L.append('\t\t\t\t<vertexbuffer positions="true" normals="true" texture_coord_dimensions_0="float2" texture_coords="1">')
        for (pt, n, t) in m.v:
            L.append('\t\t\t\t\t<vertex>')
            L.append('\t\t\t\t\t\t<position x="%.5f" y="%.5f" z="%.5f" />' % pt)
            L.append('\t\t\t\t\t\t<normal x="%.5f" y="%.5f" z="%.5f" />' % tuple(n))
            L.append('\t\t\t\t\t\t<texcoord u="%.5f" v="%.5f" />' % t)
            L.append('\t\t\t\t\t</vertex>')
        L.append('\t\t\t\t</vertexbuffer>')
        L.append('\t\t\t</geometry>')
        L.append('\t\t</submesh>')
    L += ['\t</submeshes>', '\t<submeshnames>']
    for i, (mat, m) in enumerate(meshes):
        L.append('\t\t<submeshname name="%s" index="%d" />' % (mat, i))
    L += ['\t</submeshnames>', '</mesh>']
    with open(path, 'w') as xml_file:
        xml_file.write("\n".join(L) + "\n")


def worker_bed(out):
    n = 512
    tex, bands = atlas([(0.34, iron(n, 1)), (0.33, straw(n, 2, (112, 92, 58))), (0.33, cloth(n, (92, 46, 38), 3))])
    y0 = int(bands[2][0] * n)
    for x in range(0, n, 32):
        tex[y0 + 4:y0 + 8, x:x + 16] = (150, 70, 40)
    save(tex, os.path.join(out, "ImpBed.png"))
    m = Mesh()
    for sx in (-0.26, 0.26):
        for sy in (-0.38, 0.38):
            m.box((sx, sy, 0.07), (0.08, 0.08, 0.14), bands[0])
    m.box((-0.26, 0, 0.17), (0.06, 0.84, 0.06), bands[0])
    m.box((0.26, 0, 0.17), (0.06, 0.84, 0.06), bands[0])
    m.box((0, -0.40, 0.17), (0.56, 0.05, 0.06), bands[0])
    m.box((0, 0.40, 0.30), (0.56, 0.06, 0.32), bands[0])
    for sx in (-0.24, 0.24):
        m.box((sx, 0.40, 0.58), (0.09, 0.09, 0.26), bands[0], taper=0.25)
    mat = Mesh()
    mat.box((0, 0, 0.23), (0.50, 0.76, 0.12), bands[1])
    bl = Mesh()
    bl.box((0, -0.12, 0.31), (0.54, 0.50, 0.06), bands[2])
    bl.box((0, 0.27, 0.31), (0.36, 0.16, 0.06), bands[1])
    write_xml([("ImpBedFrame", m), ("ImpBedStraw", mat), ("ImpBedBlanket", bl)], os.path.join(out, "ImpBed.xml"))


def spider_bed(out):
    n = 512
    silk = cloth(n, (150, 146, 138), 21, patches=False)
    img = Image.fromarray(np.zeros((n, n), np.uint8))
    d = ImageDraw.Draw(img)
    for cx, cy in ((100, 70), (330, 120), (200, 130), (440, 60)):
        for k in range(10):
            a = k * math.pi / 5
            d.line([cx, cy, cx + 90 * math.cos(a), cy + 90 * math.sin(a)], fill=200, width=2)
        for rr in range(14, 90, 14):
            pts = [(cx + rr * math.cos(k * math.pi / 5), cy + rr * math.sin(k * math.pi / 5)) for k in range(11)]
            d.line(pts, fill=170, width=1)
    web = np.asarray(img).astype(float) / 255
    bone = planks(n, (74, 68, 62), 31)
    silkweb = silk * (0.85 + 0.25 * web[..., None])
    tex, bands = atlas([(0.30, bone), (0.35, cloth(n, (84, 72, 64), 33)), (0.35, silkweb)])
    save(tex, os.path.join(out, "SpiderBed.png"))
    m = Mesh()
    for sx in (-0.30, 0.30):
        for sy in (-0.38, 0.38):
            m.box((sx, sy, 0.10), (0.10, 0.10, 0.20), bands[0], taper=0.6)
    m.box((-0.30, 0, 0.22), (0.07, 0.86, 0.08), bands[0])
    m.box((0.30, 0, 0.22), (0.07, 0.86, 0.08), bands[0])
    m.box((0, -0.40, 0.22), (0.60, 0.07, 0.08), bands[0])
    m.box((0, 0.40, 0.22), (0.60, 0.07, 0.08), bands[0])
    m.box((0, 0.40, 0.42), (0.60, 0.05, 0.34), bands[0], taper=0.85)
    pad = Mesh()
    pad.box((0, 0, 0.27), (0.54, 0.78, 0.10), bands[1])
    co = Mesh()
    co.cocoon((0, -0.04, 0.32), 0.26, 0.36, 0.22, 8, bands[2])
    write_xml([("SpiderBedFrame", m), ("SpiderBedPad", pad), ("SpiderBedSilk", co)], os.path.join(out, "SpiderBed.xml"))


def standard_bed(out):
    save(planks(256, (84, 62, 44), 41), os.path.join(out, "BedWood.png"))
    save(cloth(256, (84, 64, 60), 43), os.path.join(out, "BedBlanket.png"))
    save(straw(256, 47, (120, 104, 76)) * 0.85 + 10, os.path.join(out, "BedLinen.png"))


if __name__ == "__main__":
    o = sys.argv[1]
    os.makedirs(o, exist_ok=True)
    worker_bed(o)
    spider_bed(o)
    standard_bed(o)
