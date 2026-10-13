# Note: the two straw rings next to the ramp that this script adds were cut from the shipped mesh afterwards (the
# nests are now the separate mesh ChickenNest); the shipped mesh no longer has them.
# Builds the hatchery coop mesh ChickenCoopHouse (skeleton with the clips Door and Idle) in Blender from the
# old coop mesh: the door leaf gets its own bone, a lookout platform on the roof ridge and two straw nests next
# to the ramp are added. Needs odp_ogre_io, the old mesh as XML (ChickenCoop.mesh.xml) and a skeleton XML with the
# bones Root, Door (hinge) and Lookout (perch point). Run with exec() inside Blender; returns the objects.
import math
import bpy
import bmesh
from mathutils import Quaternion, Vector
import odp_ogre_io as oo

DOOR_OPEN_DEGREES = -95.0
NEST_CENTERS = ((0.66, -0.272), (0.66, 0.272))
LOOKOUT = (0.3, 0.0, 0.975)


def islands(me):
    key = {}
    idx = []
    for v in me.vertices:
        idx.append(key.setdefault(tuple(round(c, 4) for c in v.co), len(key)))
    par = list(range(len(key)))

    def find(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a
    for p in me.polygons:
        vs = [idx[i] for i in p.vertices]
        for a in vs[1:]:
            par[find(a)] = find(vs[0])
    comp = {}
    for v in me.vertices:
        comp.setdefault(find(idx[v.index]), []).append(v.index)
    return list(comp.values())


def wall_uv(me):
    uv = me.uv_layers.active
    for p in me.polygons:
        if p.normal.y > 0.95 and me.vertices[p.vertices[0]].co.y > 0.2:
            u = sum(uv.data[l].uv[0] for l in p.loop_indices) / len(p.loop_indices)
            v = sum(uv.data[l].uv[1] for l in p.loop_indices) / len(p.loop_indices)
            return (u, v)
    return (0.5, 0.5)


def material(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    if name == "ChickenStraw":
        m.diffuse_color = (0.85, 0.68, 0.32, 1.0)
    return m


def box(bm, lo, hi):
    res = bmesh.ops.create_cube(bm, size=1.0)
    for v in res["verts"]:
        v.co = Vector(((hi[0] + lo[0]) / 2 + v.co.x * (hi[0] - lo[0]), (hi[1] + lo[1]) / 2 + v.co.y * (hi[1] - lo[1]),
                       (hi[2] + lo[2]) / 2 + v.co.z * (hi[2] - lo[2])))
    return res["verts"]


def ring(bm, center, major, minor, height, seg=16, tube=6):
    """Squashed torus of straw lying on the ground."""
    verts = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        row = []
        for j in range(tube):
            b = 2 * math.pi * j / tube
            r = major + minor * math.cos(b)
            row.append(bm.verts.new((center[0] + r * math.cos(a), center[1] + r * math.sin(a),
                                     center[2] + height + height * math.sin(b))))
        verts.append(row)
    for i in range(seg):
        for j in range(tube):
            bm.faces.new((verts[i][j], verts[(i + 1) % seg][j], verts[(i + 1) % seg][(j + 1) % tube], verts[i][(j + 1) % tube]))
    disc = [bm.verts.new((center[0] + (major - minor * 0.2) * math.cos(2 * math.pi * i / seg),
                          center[1] + (major - minor * 0.2) * math.sin(2 * math.pi * i / seg), center[2] + height * 0.7))
            for i in range(seg)]
    bm.faces.new(list(reversed(disc)))


def build(skel_xml, mesh_xml, texture):
    arm = oo.import_skeleton(skel_xml, "CoopArm", with_actions=False)
    obj = oo.import_mesh(mesh_xml, arm, "CoopMesh", texture)
    me = obj.data
    isl = islands(me)
    door = set()
    for c in isl:
        if len(c) < 1000 and 0.57 < min(me.vertices[i].co.x for i in c) < 0.62 and max(me.vertices[i].co.z for i in c) < 0.6:
            door = set(c)
    assert len(door) == 480, len(door)
    old_count = len(me.vertices)
    u0, v0 = wall_uv(me)
    # new geometry, new vertices after the old ones
    me.materials.append(material("ChickenStraw"))
    straw_slot = len(me.materials) - 1
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    old_faces = set(bm.faces)
    # lookout platform: plank on four short posts over the roof ridge
    lx, ly, lz = LOOKOUT
    box(bm, (lx - 0.17, ly - 0.07, lz - 0.02), (lx + 0.17, ly + 0.07, lz))
    for px in (lx - 0.14, lx + 0.14):
        for py in (-0.045, 0.045):
            box(bm, (px - 0.012, py - 0.012, lz - 0.14), (px + 0.012, py + 0.012, lz - 0.02))
    wood_faces = [f for f in bm.faces if f not in old_faces]
    for f in wood_faces:
        for k, loop in enumerate(f.loops):
            loop[uv].uv = (u0 + 0.02 * ((k & 1) - 0.5), v0 + 0.02 * ((k >> 1) - 0.5))
    wood_set = set(wood_faces)
    for c in NEST_CENTERS:
        ring(bm, (c[0], c[1], 0.0), 0.095, 0.03, 0.022)
    for f in bm.faces:
        if f not in old_faces and f not in wood_set:
            f.material_index = straw_slot
    bm.to_mesh(me)
    bm.free()
    me.update()
    # weights
    for v in me.vertices:
        if v.index < old_count and v.index in door:
            obj.vertex_groups["Door"].add([v.index], 1.0, "REPLACE")
        else:
            obj.vertex_groups["Root"].add([v.index], 1.0, "REPLACE")
    # clips
    zrot = lambda deg: Quaternion((0, 0, 1), math.radians(deg))
    act = oo.new_action(arm, "Door", 1.2)
    ramp = [(0.0, 0.0), (0.1, -12.0), (0.2, -50.0), (0.4, DOOR_OPEN_DEGREES), (0.7, DOOR_OPEN_DEGREES),
            (0.85, -60.0), (1.0, -15.0), (1.2, 0.0)]
    for t, deg in ramp:
        oo.add_pose_key(arm, act, "Door", t, rot=zrot(deg))
    idle = oo.new_action(arm, "Idle", 1.0)
    oo.add_pose_key(arm, idle, "Door", 0.0, rot=zrot(0))
    oo.add_pose_key(arm, idle, "Door", 1.0, rot=zrot(0))
    for a in (act, idle):
        a.use_fake_user = True
    return arm, obj
