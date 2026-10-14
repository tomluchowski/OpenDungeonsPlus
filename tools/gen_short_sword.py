#!/usr/bin/env python3
"""Writes the short sword of the goblin: the mesh as Ogre XML (convert it to models/ShortSword.mesh with
OgreXMLConverter) and its small texture (materials/textures/ShortSwordSurface.png).

The model is made from primitive prisms, with the same axes as the other hand weapons: the blade runs along
+Z from the guard, the flat side of the blade faces +X/-X, the guard runs along Y, the grip end is at -Z.
The origin lies in the middle of the grip, where the hand holds it. Length about 0.30, guard width 0.07.
The 64x64 texture has steel for the blade (rows 0-31), brass for guard and pommel (rows 32-47) and
leather for the grip (rows 48-63). Run it from the repository root:

    python tools/gen_short_sword.py [output directory for the mesh XML]
"""
import os
import random
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (first row, number of rows) of the texture regions
STEEL = (0, 32)
BRASS = (32, 16)
LEATHER = (48, 16)


def uv(region, a, b):
    return (a, (region[0] + b * region[1]) / 64.0)


def sub(p, q):
    return (p[0] - q[0], p[1] - q[1], p[2] - q[2])


def cross(p, q):
    return (p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0])


def normalize(v):
    n = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) ** 0.5
    return (v[0] / n, v[1] / n, v[2] / n)


class Mesh(object):
    def __init__(self):
        self.vertices = []
        self.faces = []

    def triangle(self, p0, p1, p2, centre, region, uvs):
        normal = normalize(cross(sub(p1, p0), sub(p2, p0)))
        mid = ((p0[0] + p1[0] + p2[0]) / 3.0 - centre[0], (p0[1] + p1[1] + p2[1]) / 3.0 - centre[1],
               (p0[2] + p1[2] + p2[2]) / 3.0 - centre[2])
        if normal[0] * mid[0] + normal[1] * mid[1] + normal[2] * mid[2] < 0:
            p1, p2 = p2, p1
            uvs = (uvs[0], uvs[2], uvs[1])
            normal = (-normal[0], -normal[1], -normal[2])
        base = len(self.vertices)
        for p, t in zip((p0, p1, p2), uvs):
            self.vertices.append((p, normal, uv(region, t[0], t[1])))
        self.faces.append((base, base + 1, base + 2))

    def quad(self, p0, p1, p2, p3, centre, region):
        self.triangle(p0, p1, p2, centre, region, ((0, 0), (1, 0), (1, 1)))
        self.triangle(p0, p2, p3, centre, region, ((0, 0), (1, 1), (0, 1)))

    def box(self, x0, x1, y0, y1, z0, z1, region):
        centre = ((x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0)
        c = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
        # corners: index = x + 2 * y + 4 * z
        for quad in ((0, 1, 3, 2), (4, 5, 7, 6), (0, 1, 5, 4), (2, 3, 7, 6), (0, 2, 6, 4), (1, 3, 7, 5)):
            self.quad(c[quad[0]], c[quad[1]], c[quad[2]], c[quad[3]], centre, region)


# the origin lies in the middle of the grip (the grip spans -0.044 to -0.002 before the shift)
GRIP_SHIFT = 0.023
# the whole model is scaled to the size of the goblin (a hand weapon of this creature is about 0.3 long)
SCALE = 0.75


def build():
    mesh = Mesh()
    # pommel, grip, guard
    mesh.box(-0.014, 0.014, -0.014, 0.014, -0.066, -0.044, BRASS)
    mesh.box(-0.011, 0.011, -0.011, 0.011, -0.044, -0.002, LEATHER)
    mesh.box(-0.013, 0.013, -0.046, 0.046, -0.002, 0.012, BRASS)
    # blade: straight part and a pointed tip
    t = 0.006
    w = 0.026
    z0 = 0.012
    z1 = 0.27
    tip = 0.34
    centre = (0.0, 0.0, 0.17)
    mesh.box(-t, t, -w, w, z0, z1, STEEL)
    c = [(-t, -w, z1), (t, -w, z1), (t, w, z1), (-t, w, z1)]
    apex = (0.0, 0.0, tip)
    for i in range(4):
        mesh.triangle(c[i], c[(i + 1) % 4], apex, centre, STEEL, ((0, 0), (1, 0), (0.5, 1)))
    return mesh


def write_mesh(path):
    mesh = build()
    lines = ['<?xml version="1.0"?>', '<mesh>', '\t<submeshes>',
             '\t\t<submesh material="ShortSword" usesharedvertices="false" use32bitindexes="false" '
             'operationtype="triangle_list">', '\t\t\t<faces count="%d">' % len(mesh.faces)]
    for f in mesh.faces:
        lines.append('\t\t\t\t<face v1="%d" v2="%d" v3="%d" />' % f)
    lines.append('\t\t\t</faces>')
    lines.append('\t\t\t<geometry vertexcount="%d">' % len(mesh.vertices))
    lines.append('\t\t\t\t<vertexbuffer positions="true" normals="true" colours_diffuse="true" '
                 'texture_coord_dimensions_0="float2" texture_coords="1">')
    for p, n, t in mesh.vertices:
        lines.append('\t\t\t\t\t<vertex>')
        lines.append('\t\t\t\t\t\t<position x="%.6f" y="%.6f" z="%.6f" />' % (SCALE * p[0], SCALE * p[1], SCALE * (p[2] + GRIP_SHIFT)))
        lines.append('\t\t\t\t\t\t<normal x="%.6f" y="%.6f" z="%.6f" />' % n)
        lines.append('\t\t\t\t\t\t<colour_diffuse value="1 1 1 1" />')
        lines.append('\t\t\t\t\t\t<texcoord u="%.6f" v="%.6f" />' % t)
        lines.append('\t\t\t\t\t</vertex>')
    lines += ['\t\t\t\t</vertexbuffer>', '\t\t\t</geometry>', '\t\t</submesh>', '\t</submeshes>', '</mesh>', '']
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines))


def write_texture(path):
    random.seed(41)
    image = Image.new("RGB", (64, 64))
    for y in range(64):
        for x in range(64):
            noise = random.randint(-6, 6)
            if y < 32:
                grey = 150 + noise + int(30 * (1.0 - abs(x - 31.5) / 31.5))
                if 30 <= x <= 33:
                    grey -= 28
                image.putpixel((x, y), (grey, grey + 3, grey + 8))
            elif y < 48:
                base = 150 + noise
                image.putpixel((x, y), (base, int(base * 0.78), int(base * 0.30)))
            else:
                base = 92 + noise + (14 if (y // 3) % 2 == 0 else 0)
                image.putpixel((x, y), (base, int(base * 0.62), int(base * 0.38)))
    image.save(path)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "models")
    write_mesh(os.path.join(out, "ShortSword.mesh.xml"))
    write_texture(os.path.join(ROOT, "materials", "textures", "ShortSwordSurface.png"))


if __name__ == "__main__":
    main()
