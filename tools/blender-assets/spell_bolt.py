# Lightning bolt mesh for the spell effects, built in Blender: two crossed ribbons with a zigzag centre line
# (so the bolt shows from every side), a narrower pair of ribbons inside (the hot core) and a short side fork.
# The bolt runs along +y from 0 to 1 and is about 0.32 wide; the game stretches it from the sky to the target.
# One material, SpellBolt (materials/scripts/RoomAmbienceSpells.material, texture tools/gen_spell_bolt.py).
# Run inside Blender; works in a scene of its own and writes the Ogre mesh XML to OUT_XML.
import math
import random
import bpy
import bmesh

SEGMENTS = 14
HALF_WIDTH = 0.16
OUT_XML = None  # set by the caller


def centre_line(seed, count, amplitude, y0=0.0, y1=1.0, dx0=0.0, dz0=0.0, lean=0.0):
    rng = random.Random(seed)
    pts = []
    for i in range(count + 1):
        t = i / count
        taper = math.sin(math.pi * t) ** 0.5 if 0 < i < count else 0.0
        side = 1.0 if i % 2 == 0 else -1.0
        x = dx0 + lean * t + side * amplitude * taper * (0.55 + 0.45 * rng.random())
        z = dz0 + rng.uniform(-1.0, 1.0) * amplitude * 0.8 * taper
        pts.append((x, y0 + (y1 - y0) * t, z))
    return pts


def ribbon(bm, pts, half, axis):
    """A strip along the points, widening in the x direction (axis 0) or the z direction (axis 1)."""
    left, right = [], []
    for i, (x, y, z) in enumerate(pts):
        w = half * (0.25 + 0.75 * math.sin(math.pi * min(1.0, max(0.0, i / (len(pts) - 1)))) ** 0.4)
        if axis == 0:
            left.append(bm.verts.new((x - w, y, z)))
            right.append(bm.verts.new((x + w, y, z)))
        else:
            left.append(bm.verts.new((x, y, z - w)))
            right.append(bm.verts.new((x, y, z + w)))
    uv_layer = bm.loops.layers.uv.verify()
    for i in range(len(pts) - 1):
        face = bm.faces.new((left[i], right[i], right[i + 1], left[i + 1]))
        v0 = pts[i][1]
        v1 = pts[i + 1][1]
        for loop, (u, v) in zip(face.loops, ((0.0, v0), (1.0, v0), (1.0, v1), (0.0, v1))):
            loop[uv_layer].uv = (u, v)


def build():
    scene = bpy.data.scenes.new("SpellBoltWork")
    bpy.context.window.scene = scene
    mesh = bpy.data.meshes.new("SpellBolt")
    obj = bpy.data.objects.new("SpellBolt", mesh)
    scene.collection.objects.link(obj)
    material = bpy.data.materials.get("SpellBolt") or bpy.data.materials.new("SpellBolt")
    mesh.materials.append(material)
    bm = bmesh.new()
    main = centre_line(7, SEGMENTS, 0.07)
    # Outer glow ribbons, inner core ribbons (same line, narrower)
    ribbon(bm, main, HALF_WIDTH, 0)
    ribbon(bm, main, HALF_WIDTH, 1)
    ribbon(bm, main, HALF_WIDTH * 0.4, 0)
    ribbon(bm, main, HALF_WIDTH * 0.4, 1)
    # A short fork leaving the bolt at about 40 % of its length
    start = main[6]
    fork = centre_line(11, 5, 0.04, y0=start[1], y1=0.72, dx0=start[0], dz0=start[2], lean=0.22)
    ribbon(bm, fork, HALF_WIDTH * 0.5, 0)
    ribbon(bm, fork, HALF_WIDTH * 0.5, 1)
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    return scene, obj, mesh, material


def bounds(obj):
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))
