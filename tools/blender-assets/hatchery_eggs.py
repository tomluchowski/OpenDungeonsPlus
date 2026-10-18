# Egg meshes for the hatchery, built in Blender: an egg in straw and a cracked egg in straw with
# the top shell tilted open and loose shell chips. Material names match materials/scripts/ChickenHatchery.material.
import math
import random
import bpy
import bmesh
from mathutils import Vector, Matrix

R = 0.03
H = 0.08
BASE = 0.012


def material(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def egg_ring(u, seg, wobble=0.0, jag=None):
    z = BASE + H * (0.5 - 0.5 * math.cos(u))
    rad = R * math.sin(u) * (1.0 + 0.12 * math.cos(u))
    pts = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        zz = z
        if jag is not None:
            zz = jag(a)
        pts.append((rad * math.cos(a), rad * math.sin(a), zz))
    return pts


def add_shell(bm, u0, u1, rings, seg, jag_bottom=None, jag_top=None, cap=False):
    """Egg surface between egg parameters u0 and u1 (0 = bottom pole, pi = top pole)."""
    rows = []
    for r in range(rings + 1):
        u = u0 + (u1 - u0) * r / rings
        if u <= 1e-4 or u >= math.pi - 1e-4:
            ring = [bm.verts.new((0, 0, BASE + (0.0 if u <= 1e-4 else H)))] * 1
            rows.append(ring)
            continue
        pts = egg_ring(u, seg)
        if r == 0 and jag_bottom is not None:
            pts = [(x, y, jag_bottom(math.atan2(y, x))) for x, y, _ in pts]
        if r == rings and jag_top is not None:
            pts = [(x, y, jag_top(math.atan2(y, x))) for x, y, _ in pts]
        rows.append([bm.verts.new(p) for p in pts])
    faces = []
    for a, b in zip(rows, rows[1:]):
        for i in range(seg):
            j = (i + 1) % seg
            if len(a) == 1:
                f = bm.faces.new((a[0], b[j], b[i]))
            elif len(b) == 1:
                f = bm.faces.new((a[i], a[j], b[0]))
            else:
                f = bm.faces.new((a[i], a[j], b[j], b[i]))
            faces.append(f)
    return rows, faces


def add_straw(bm, count, inner, outer, seed):
    rnd = random.Random(seed)
    faces = []
    for _ in range(count):
        ang = 2 * math.pi * rnd.random()
        start = inner + (outer - inner) * 0.5 * rnd.random()
        length = (outer - start) * (0.6 + 0.4 * rnd.random())
        turn = ang + 0.7 * (rnd.random() * 2 - 1)
        frm = Vector((start * math.cos(ang), start * math.sin(ang), 0.006 + 0.014 * rnd.random()))
        d = Vector((math.cos(turn), math.sin(turn), -0.15 * rnd.random()))
        to = frm + d * length
        side = Vector((-d.y, d.x, 0)) * 0.0045
        faces.append(bm.faces.new((bm.verts.new(frm - side), bm.verts.new(frm + side),
                                   bm.verts.new(to + side), bm.verts.new(to - side))))
    return faces


def finish(name, parts):
    """parts: list of (material name, bmesh builder returning faces)"""
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    slots = []
    for mat, build in parts:
        if mat not in slots:
            slots.append(mat)
        faces = build(bm)
        for f in faces:
            f.material_index = slots.index(mat)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(me)
    bm.free()
    for mat in slots:
        me.materials.append(material(mat))
    for poly in me.polygons:
        poly.use_smooth = True
    obj = bpy.data.objects.new(name, me)
    bpy.data.collections["Hatchery_Assets"].objects.link(obj)
    return obj


def build_egg():
    def shell(bm):
        return add_shell(bm, 0.0, math.pi, 10, 14)[1]
    return finish("EggMesh", [("ChickenEgg", shell), ("ChickenStraw", lambda bm: add_straw(bm, 26, 0.02, 0.095, 11))])


def build_cracked():
    seg = 14
    cut = 0.056

    def jag(a):
        k = int(round(a / (2 * math.pi) * seg)) % 2
        return cut + (0.011 if k else -0.004)

    def lower(bm):
        # shell below the crack, jagged upper edge, seen from outside and inside
        u_cut = math.acos(1.0 - 2.0 * (cut - BASE) / H)
        rows, faces = add_shell(bm, 0.0, u_cut, 6, seg, jag_top=jag)
        return faces

    def cap(bm):
        u_cut = math.acos(1.0 - 2.0 * (cut - BASE) / H)
        n0 = len(bm.faces)
        rows, faces = add_shell(bm, u_cut, math.pi, 4, seg, jag_bottom=lambda a: jag(a) + 0.002)
        # tilt the cap open around the edge on the +x side
        pivot = Vector((R * 0.9, 0, cut))
        rot = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(34), 4, "Y") @ Matrix.Translation(-pivot)
        move = Matrix.Translation((0.0, 0.0, 0.012))
        verts = {v for f in faces for v in f.verts}
        for v in verts:
            v.co = (move @ rot) @ v.co
        return faces

    def inside(bm):
        # dark inner disc under the open cap
        ring = [bm.verts.new((0.8 * R * math.cos(2 * math.pi * i / seg), 0.8 * R * math.sin(2 * math.pi * i / seg), cut - 0.004))
                for i in range(seg)]
        return [bm.faces.new(ring)]

    def chips(bm):
        rnd = random.Random(5)
        faces = []
        for k in range(4):
            ang = 2 * math.pi * rnd.random()
            dist = 0.04 + 0.03 * rnd.random()
            c = Vector((dist * math.cos(ang), dist * math.sin(ang), 0.014))
            tilt = rnd.random() * 0.4
            s = 0.008 + 0.006 * rnd.random()
            pts = [Vector((-s, -s * 0.7, 0)), Vector((s, -s * 0.5, tilt * s)), Vector((s * 0.6, s, 0)), Vector((-s * 0.8, s * 0.6, tilt * s))]
            faces.append(bm.faces.new([bm.verts.new(c + p) for p in pts]))
        return faces

    return finish("EggCrackedMesh", [("ChickenEgg", lower), ("ChickenEgg", cap), ("ChickenEggInside", inside),
                                     ("ChickenStraw", lambda bm: add_straw(bm, 26, 0.02, 0.095, 11)), ("ChickenEgg", chips)])


def build_all():
    for n in ("EggMesh", "EggCrackedMesh"):
        o = bpy.data.objects.get(n)
        if o is not None:
            m = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            bpy.data.meshes.remove(m)
    return build_egg(), build_cracked()
