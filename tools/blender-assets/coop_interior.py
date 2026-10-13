# Adds the inside of the hatchery coop (ChickenCoopHouse): until now the doorway showed the black back sides of the
# walls. The cavity (floor at z 0.20, walls at x 0.04 / 0.56 and y +-0.30) gets a plank lining, a straw bed with
# nest rings, two open nest crates at the back wall and a perch across the room. Everything is weighted to Root, the
# skeleton and its clips (Door, Idle) stay as they are.
#
#   blender --background <blenderCoopHouse.blend> --python coop_interior.py -- <work folder> [preview folder]
#
# The scene must be the one saved by coop_house.py (armature CoopHouse, mesh CoopMesh, groups Root/Door/Lookout). The file
# is saved with the interior and <work folder>/ChickenCoopHouse.mesh.xml holds the mesh, ready for OgreXMLConverter.
# The doorway itself was closed by plug quads at x 0.572 and 0.563; they are removed. Running the script a second time does nothing (the group InteriorDone marks a built file).
import math
import os
import sys
import traceback

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo
import coop_house as ch

X0, X1 = 0.045, 0.555          # inner walls back to front (x), the front wall is at x 0.56
Y = 0.295                       # inner side walls
FLOOR = 0.200                   # top of the old floor
BOARD_H, GAP = 0.085, 0.006     # lining boards
LINING_TOP = 0.55                 # the roof comes down to about z 0.6 at the side walls


def board_wall(bm, x0, x1, y0, y1, z0, z1, inset):
    """A wall of stacked boards; every second board stands a little proud so the lining has relief."""
    z = z0
    k = 0
    while z + BOARD_H <= z1 + 1e-6:
        lift = inset if k % 2 == 0 else inset * 0.35
        if abs(x1 - x0) < abs(y1 - y0):      # wall across x (back wall): the boards grow towards +x
            ch.box(bm, (x0, y0, z), (x0 + lift, y1, z + BOARD_H - GAP))
        elif y1 < 0:                          # left wall (y < 0), boards grow towards +y
            ch.box(bm, (x0, y1 - lift, z), (x1, y1, z + BOARD_H - GAP))
        else:                                 # right wall, boards grow towards -y
            ch.box(bm, (x0, y0, z), (x1, y0 + lift, z + BOARD_H - GAP))
        z += BOARD_H
        k += 1


def crate(bm, cx, cy, size=0.13, height=0.095):
    """Open nest crate: floor and four low walls (the side towards the room is a little lower)."""
    t = 0.012
    h = size / 2
    ch.box(bm, (cx - h, cy - h, FLOOR), (cx + h, cy + h, FLOOR + t))
    ch.box(bm, (cx - h, cy - h, FLOOR), (cx - h + t, cy + h, FLOOR + height))
    ch.box(bm, (cx + h - t, cy - h, FLOOR), (cx + h, cy + h, FLOOR + height * 0.7))
    ch.box(bm, (cx - h, cy - h, FLOOR), (cx + h, cy - h + t, FLOOR + height))
    ch.box(bm, (cx - h, cy + h - t, FLOOR), (cx + h, cy + h, FLOOR + height))


def plank_uvs(me, count=5):
    """UV points of the coop texture with a typical wall colour (median brightness of the mesh), for the lining boards."""
    image = bpy.data.images["ChickenCoop.png"]
    width, height = image.size
    pixels = image.pixels[:]
    uv = me.uv_layers.active
    found = []
    for p in list(me.polygons)[::7]:
        u = sum(uv.data[l].uv[0] for l in p.loop_indices) / len(p.loop_indices)
        v = sum(uv.data[l].uv[1] for l in p.loop_indices) / len(p.loop_indices)
        x, y = int(u * (width - 1)) % width, int(v * (height - 1)) % height
        o = (y * width + x) * 4
        lum = 0.3 * pixels[o] + 0.59 * pixels[o + 1] + 0.11 * pixels[o + 2]
        found.append((lum, u, v))
    found.sort()
    median = found[len(found) // 2][0]
    typical = [(u, v) for lum, u, v in found if 0.8 * median < lum < 1.2 * median]
    step = max(1, len(typical) // count)
    return typical[::step][:count] or [ch.wall_uv(me)]


def build():
    arm = bpy.data.objects["CoopHouse"]
    obj = bpy.data.objects["CoopMesh"]
    if "InteriorDone" in obj.vertex_groups:
        return arm, obj, False
    me = obj.data
    old_count = len(me.vertices)
    u0, v0 = ch.wall_uv(me)
    planks = plank_uvs(me)
    straw_slot = [m.name for m in me.materials].index("ChickenStraw")
    wood_slot = [m.name for m in me.materials].index("ChickenCoop")
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    # the doorway is closed by plug quads (the old black opening): one at x 0.572 and one at x 0.563 facing out; remove them
    plug = [f for f in bm.faces if all(min(abs(v.co.x - 0.572), abs(v.co.x - 0.563)) < 0.002 and abs(v.co.y) < 0.135
                                       and 0.21 < v.co.z < 0.51 for v in f.verts) and f.normal.x > 0.9]
    assert len(plug) >= 2, len(plug)   # the plugs are stored several times on top of each other
    bmesh.ops.delete(bm, geom=plug, context="FACES_ONLY")
    old_faces = set(bm.faces)

    # wood: lining, nest crates, perch
    board_wall(bm, X0, X0 + 0.02, -Y, Y, FLOOR, LINING_TOP, 0.02)                    # back wall
    board_wall(bm, X0, X1, -Y, -Y + 0.02, FLOOR, LINING_TOP, 0.02)                   # left wall (y < 0)
    board_wall(bm, X0, X1, Y - 0.02, Y, FLOOR, LINING_TOP, 0.02)                     # right wall
    crate(bm, 0.135, -0.17)
    crate(bm, 0.135, 0.17)
    ch.box(bm, (0.235, -Y + 0.02, 0.43), (0.26, Y - 0.02, 0.452))                    # perch across the room
    for sy in (-1, 1):
        ch.box(bm, (0.225, sy * (Y - 0.035) - 0.008, 0.36), (0.27, sy * (Y - 0.035) + 0.008, 0.452))
    wood = [f for f in bm.faces if f not in old_faces]
    for n, f in enumerate(wood):
        f.material_index = wood_slot
        pu, pv = planks[(n // 6) % len(planks)]      # a box has six faces: one colour per box
        for k, loop in enumerate(f.loops):
            loop[uv].uv = (pu + 0.01 * ((k & 1) - 0.5), pv + 0.01 * ((k >> 1) - 0.5))
    wood_set = set(wood)

    # straw: bed over the floor, nest rings, loose clumps
    ch.box(bm, (X0 + 0.03, -Y + 0.04, FLOOR - 0.004), (X1 + 0.012, Y - 0.04, FLOOR + 0.022))
    for cy in (-0.17, 0.17):
        ch.ring(bm, (0.135, cy, FLOOR), 0.045, 0.025, 0.02)
    for cx, cy, r in ((0.33, -0.1, 0.05), (0.40, 0.12, 0.06), (0.47, -0.05, 0.045), (0.30, 0.20, 0.04), (0.45, 0.22, 0.04)):
        ch.ring(bm, (cx, cy, FLOOR + 0.012), r, 0.018, 0.012)
    for f in bm.faces:
        if f not in old_faces and f not in wood_set:
            f.material_index = straw_slot
            for loop in f.loops:
                loop[uv].uv = (0.5, 0.5)
    bm.to_mesh(me)
    bm.free()
    me.update()

    root = obj.vertex_groups["Root"]
    root.add(list(range(old_count, len(me.vertices))), 1.0, "REPLACE")
    obj.vertex_groups.new(name="InteriorDone")
    return arm, obj, True


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    work = args[0]
    preview = args[1] if len(args) > 1 else None
    os.makedirs(work, exist_ok=True)
    for image in bpy.data.images:
        if image.name == "ChickenCoop.png":
            image.filepath = "//..\\..\\ODP-gold-tierc\\materials\\textures\\ChickenCoop.png"
            image.reload()
    arm, obj, built = build()
    if built:
        oo.export_mesh_xml(obj, arm, os.path.join(work, "ChickenCoopHouse.mesh.xml"), "ChickenCoopHouse.skeleton")
        bpy.ops.wm.save_mainfile()
    if preview:
        import wake_preview as wp
        os.makedirs(preview, exist_ok=True)
        wp.pose(arm, "Door", 0.55)
        for name, (e, a, d, t) in {"game": (50, 40, 2.4, (0.3, 0, 0.35)), "front": (20, 90, 2.4, (0.3, 0, 0.4)),
                                   "in": (35, 90, 1.2, (0.35, 0, 0.3)), "in2": (45, 75, 1.0, (0.35, 0, 0.3))}.items():
            wp.render(os.path.join(preview, "coop_%s.png" % name), e, a, d, t)


try:
    main()
    open(os.path.join(sys.argv[sys.argv.index("--") + 1], "coop_interior.ok"), "w").write("ok")
except Exception:
    open(os.path.join(sys.argv[sys.argv.index("--") + 1], "coop_interior.err"), "w").write(traceback.format_exc())
