# Builds the hatchery straw nest ChickenNest (static mesh, no skeleton, no eggs) and its small straw texture.
# A shallow bowl of rough straw stalks: ragged rim, a slight hollow in the middle, footprint about 0.30 x 0.30 tiles,
# height at most 0.06. The stalks are thin twisted ribbons laid around the bowl and across its floor.
#
#   blender --background --python chicken_nest.py -- <work folder> <texture png> [preview folder]
#
# <work folder>/ChickenNest.mesh.xml is ready for OgreXMLConverter. The texture (64x64, dark straw brown, plain
# streaks, made here with a fixed random seed) is written to <texture png>.
import math
import os
import random
import struct
import sys
import zlib

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo

RADIUS = 0.145          # bowl radius at the rim (footprint 0.30 x 0.30 tiles including the loose ends)
HEIGHT = 0.055          # rim height of the bowl
STRIPS = 16             # the texture is 16 horizontal strips of 4 pixels, every stalk uses one strip


def write_png(path, width, height, rows):
    raw = b"".join(b"\x00" + bytes(row) for row in rows)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def make_texture(path):
    rnd = random.Random(7)
    strips = []
    for k in range(STRIPS):
        base = 0.62 + 0.38 * rnd.random()
        strips.append((base, 0.9 + 0.2 * rnd.random()))
    rows = []
    for y in range(64):
        base, warm = strips[y // 4]
        row = []
        for x in range(64):
            streak = 0.9 + 0.1 * math.sin(x * 0.7 + y * 0.3) + 0.08 * (rnd.random() - 0.5)
            if y % 4 == 0 or y % 4 == 3:
                streak *= 0.82        # darker edge of the stalk
            v = base * streak
            row.extend((min(255, int(168 * v * warm)), min(255, int(128 * v)), min(255, int(74 * v / warm))))
        rows.append(row)
    write_png(path, 64, 64, rows)


def stalk(bm, uv_layer, centre, direction, length, width, roll, bend, strip):
    """One ribbon along direction (xy) around centre, bent upwards at its ends by bend, twisted by roll."""
    side = Vector((-direction.y, direction.x, 0.0))
    up = Vector((0.0, 0.0, 1.0))
    wdir = (side * math.cos(roll) + up * math.sin(roll)).normalized()
    steps = 4
    left, right = [], []
    for i in range(steps + 1):
        t = i / steps - 0.5
        p = centre + Vector((direction.x, direction.y, 0.0)) * (t * length)
        p.z += bend * (2 * t) ** 2
        w = width * (1.0 - 0.6 * abs(2 * t) ** 3)
        left.append(bm.verts.new(p - wdir * w * 0.5))
        right.append(bm.verts.new(p + wdir * w * 0.5))
    v0 = (strip * 4 + 0.5) / 64.0
    v1 = (strip * 4 + 3.5) / 64.0
    for i in range(steps):
        f = bm.faces.new((left[i], left[i + 1], right[i + 1], right[i]))
        loops = f.loops
        loops[0][uv_layer].uv = (i / steps, v0)
        loops[1][uv_layer].uv = ((i + 1) / steps, v0)
        loops[2][uv_layer].uv = ((i + 1) / steps, v1)
        loops[3][uv_layer].uv = (i / steps, v1)


def bowl_z(r):
    q = min(1.0, r / RADIUS)
    return 0.006 + (HEIGHT - 0.016) * q * q


def build():
    rnd = random.Random(11)
    me = bpy.data.meshes.new("ChickenNestMesh")
    obj = bpy.data.objects.new("ChickenNestMesh", me)
    bpy.context.scene.collection.objects.link(obj)
    mat = bpy.data.materials.get("ChickenNest") or bpy.data.materials.new("ChickenNest")
    me.materials.append(mat)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()
    # closed bed under the stalks, so no gaps show the ground
    rings = [(0.0, 0.0)] + [(RADIUS * 0.97 * j / 6, bowl_z(RADIUS * 0.97 * j / 6) - 0.002) for j in range(1, 7)]
    seg = 20
    grid = [[bm.verts.new((0, 0, rings[0][1]))]]
    for r, z in rings[1:]:
        grid.append([bm.verts.new((r * math.cos(2 * math.pi * i / seg), r * math.sin(2 * math.pi * i / seg), z)) for i in range(seg)])
    for i in range(seg):
        f = bm.faces.new((grid[0][0], grid[1][i], grid[1][(i + 1) % seg]))
        for loop in f.loops:
            loop[uv_layer].uv = (0.5, 0.5)
    for j in range(1, 6):
        for i in range(seg):
            f = bm.faces.new((grid[j][i], grid[j + 1][i], grid[j + 1][(i + 1) % seg], grid[j][(i + 1) % seg]))
            for loop in f.loops:
                loop[uv_layer].uv = (0.5, 0.5)
    # floor of the bowl: stalks crossing at random angles
    for _ in range(260):
        r = RADIUS * 0.8 * math.sqrt(rnd.random())
        a = rnd.random() * 2 * math.pi
        c = Vector((r * math.cos(a), r * math.sin(a), bowl_z(r) + 0.004 * rnd.random()))
        d = rnd.random() * math.pi
        stalk(bm, uv_layer, c, Vector((math.cos(d), math.sin(d), 0)), 0.09 + 0.05 * rnd.random(), 0.013,
              (rnd.random() - 0.5) * 1.6, 0.003, rnd.randrange(STRIPS))
    # walls: stalks laid along the circumference, in three rough layers up the slope
    for layer, (r0, count) in enumerate(((0.04, 20), (0.07, 36), (0.095, 50), (0.115, 62), (0.13, 70), (0.14, 74))):
        for k in range(count):
            a = 2 * math.pi * (k + rnd.random() * 0.9) / count
            r = r0 + (rnd.random() - 0.4) * 0.03
            c = Vector((r * math.cos(a), r * math.sin(a), bowl_z(r) + 0.004 * layer * rnd.random() + 0.003))
            tangent = a + math.pi / 2 + (rnd.random() - 0.5) * 0.7
            stalk(bm, uv_layer, c, Vector((math.cos(tangent), math.sin(tangent), 0)), 0.07 + 0.06 * rnd.random(),
                  0.012 + 0.004 * rnd.random(), (rnd.random() - 0.5) * 1.8, 0.004, rnd.randrange(STRIPS))
    # ragged rim: a few stalks that stick out or stand up a little
    for k in range(40):
        a = 2 * math.pi * (k + rnd.random()) / 40
        r = RADIUS + (rnd.random() - 0.5) * 0.02
        c = Vector((r * math.cos(a), r * math.sin(a), HEIGHT - 0.012 + 0.012 * rnd.random()))
        tangent = a + (rnd.random() - 0.5) * 1.6
        stalk(bm, uv_layer, c, Vector((math.cos(tangent), math.sin(tangent), 0)), 0.05 + 0.05 * rnd.random(), 0.011,
              (rnd.random() - 0.5) * 2.0, 0.006, rnd.randrange(STRIPS))
    # keep everything inside the footprint and below the height limit
    for v in bm.verts:
        v.co.x = max(-0.15, min(0.15, v.co.x))
        v.co.y = max(-0.15, min(0.15, v.co.y))
        v.co.z = max(0.0, min(0.06, v.co.z))
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    return obj


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    work, texture = args[0], args[1]
    preview = args[2] if len(args) > 2 else None
    os.makedirs(work, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    make_texture(texture)
    obj = build()
    mat = obj.data.materials[0]
    mat.use_nodes = True
    node = mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = bpy.data.images.load(texture)
    mat.node_tree.links.new(node.outputs["Color"], mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    info = oo.export_mesh_xml(obj, None, os.path.join(work, "ChickenNest.mesh.xml"), "")
    lo = [min(v.co[i] for v in obj.data.vertices) for i in range(3)]
    hi = [max(v.co[i] for v in obj.data.vertices) for i in range(3)]
    print("NEST", info, "min", lo, "max", hi)
    if preview:
        import wake_preview as wp
        os.makedirs(preview, exist_ok=True)
        wp.render(os.path.join(preview, "nest_top.png"), 60, 30, 0.75, (0, 0, 0.02), 50)
        wp.render(os.path.join(preview, "nest_low.png"), 25, 20, 0.7, (0, 0, 0.02), 50)


try:
    main()
except Exception:
    import traceback
    traceback.print_exc()
    sys.exit(1)
