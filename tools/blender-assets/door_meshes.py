# Builds the five door meshes in Blender from the wooden door mesh (same skeleton, so the Open, Close and
# Destroyed clips keep working): iron-bound, steel, barricade, secret and rune door.
# Needs the armature "DoorArm" and the mesh "DoorWood" imported with odp_ogre_io (WoodenDoor.mesh/.skeleton as XML)
# and the textures made by tools/gen_door_textures.py. Run with exec() inside Blender; it fills the collection
# "Door_Assets" and returns the names of the variants.
import math
import random

import bpy
import bmesh
from mathutils import Matrix, Vector

TEXTURES = r"C:\Users\mario\GitHub\ODP-trap-polish\materials\textures"
LEFT = "LeftDoorBone"
RIGHT = "RigthDoorBone"
ROOT_BONE = "RootBone"


def material(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    return m


def uv_for(vert_co, center, size, normal):
    p = [(vert_co[i] - center[i]) / size[i] + 0.5 for i in range(3)]
    axis = max(range(3), key=lambda i: abs(normal[i]))
    a, b = [i for i in range(3) if i != axis]
    return (p[a], p[b])


def box(bm, center, size, rotate_z=0.0):
    res = bmesh.ops.create_cube(bm, size=1.0)
    verts = res["verts"]
    faces = set(f for v in verts for f in v.link_faces)
    for v in verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    uv = bm.loops.layers.uv.active
    if uv is not None:
        for f in faces:
            for loop in f.loops:
                u, w = uv_for(loop.vert.co, (0, 0, 0), size, f.normal)
                loop[uv].uv = (u, w)
    if rotate_y_needed(rotate_z):
        bmesh.ops.rotate(bm, verts=verts, cent=(0, 0, 0), matrix=Matrix.Rotation(rotate_z, 3, "Y"))
    for v in verts:
        v.co += Vector(center)
    return verts


def rotate_y_needed(angle):
    return abs(angle) > 1e-6


def new_variant(src, name, first_material, collection):
    obj = src.copy()
    obj.data = src.data.copy()
    obj.data.name = name
    obj.name = name
    collection.objects.link(obj)
    obj.data.materials[0] = material(first_material)
    return obj


def add_part(obj, fn, material_name, weights):
    me = obj.data
    names = [m.name for m in me.materials]
    if material_name in names:
        slot = names.index(material_name)
    else:
        me.materials.append(material(material_name))
        slot = len(me.materials) - 1
    bm = bmesh.new()
    bm.from_mesh(me)
    before = len(bm.verts)
    old = set(bm.faces)
    fn(bm)
    for f in bm.faces:
        if f not in old:
            f.material_index = slot
    bm.to_mesh(me)
    bm.free()
    me.update()
    for v in me.vertices:
        if v.index >= before:
            for bone, wt in weights(v).items():
                obj.vertex_groups[bone].add([v.index], wt, "REPLACE")


def leaf_weights(v):
    return {LEFT: 1.0} if v.co.x < 0 else {RIGHT: 1.0}


def delete_groups(obj, groups):
    me = obj.data
    names = {g.index: g.name for g in obj.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(me)
    dl = bm.verts.layers.deform.verify()
    doomed = []
    for v in bm.verts:
        if v[dl]:
            top = max(v[dl].items(), key=lambda kv: kv[1])[0]
            if names[top] in groups:
                doomed.append(v)
    bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(me)
    bm.free()
    me.update()


def build(src, collection):
    out = {}

    # Iron-bound: darkened planks with forged iron bands
    o = new_variant(src, "DoorIronbound", "DoorIronboundWood", collection)

    def bands(bm):
        for side in (-1, 1):
            for z in (0.2, 0.62, 1.04):
                box(bm, (side * 0.21, 0, z), (0.40, 0.272, 0.09))
            box(bm, (side * 0.4, 0, 0.62), (0.04, 0.28, 1.05))

    add_part(o, bands, "DoorMetal", leaf_weights)
    out["DoorIronbound"] = o

    # Steel: plates with heavy trim
    o = new_variant(src, "DoorSteel", "DoorSteel", collection)

    def trim(bm):
        for side in (-1, 1):
            for z in (0.04, 0.62, 1.2):
                box(bm, (side * 0.21, 0, z), (0.42, 0.285, 0.07))
            box(bm, (side * 0.04, 0, 0.62), (0.05, 0.285, 1.2))
            for z in (0.33, 0.9):
                box(bm, (side * 0.21, 0, z), (0.07, 0.29, 0.07))

    add_part(o, trim, "DoorMetal", leaf_weights)
    out["DoorSteel"] = o

    # Barricade: no leaves, a stack of rough planks and two braces
    o = new_variant(src, "DoorBarricade", "DoorBarricade", collection)
    delete_groups(o, (LEFT, RIGHT))
    rng = random.Random(21)

    def planks(bm):
        for i in range(7):
            side = LEFT if i % 2 == 0 else RIGHT
            x = rng.uniform(-0.03, 0.03)
            box(bm, (x, rng.uniform(-0.05, 0.05), 0.1 + i * 0.17), (1.0 + rng.uniform(-0.08, 0.0), 0.1, 0.14),
                rotate_z=rng.uniform(-0.05, 0.05))
        box(bm, (0.0, 0.1, 0.62), (1.35, 0.07, 0.1), rotate_z=math.radians(42))
        box(bm, (0.0, -0.1, 0.62), (1.35, 0.07, 0.1), rotate_z=math.radians(-42))

    def plank_weights(v):
        return {LEFT: 1.0} if int(v.co.z * 100) % 2 == 0 else {RIGHT: 1.0}

    add_part(o, planks, "DoorBarricade", plank_weights)
    out["DoorBarricade"] = o

    # Secret: the frame is gone, two stone slabs that look like the wall
    o = new_variant(src, "DoorSecret", "DoorSecret", collection)
    delete_groups(o, (ROOT_BONE,))
    out["DoorSecret"] = o

    # Rune: dark stone with glowing rune plates
    o = new_variant(src, "DoorRune", "DoorRune", collection)

    def plates(bm):
        for side in (-1, 1):
            box(bm, (side * 0.21, 0, 0.62), (0.32, 0.27, 0.32))

    add_part(o, plates, "DoorRuneGlow", leaf_weights)
    out["DoorRune"] = o

    # Place them next to each other for a look
    for i, name in enumerate(("DoorWood", "DoorIronbound", "DoorSteel", "DoorBarricade", "DoorSecret", "DoorRune")):
        obj = bpy.data.objects[name]
        obj.location.x = i * 1.5
    return list(out.keys())
