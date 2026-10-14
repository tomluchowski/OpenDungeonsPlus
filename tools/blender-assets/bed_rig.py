# Gives a static dormitory bed mesh a two bone skeleton (Root, Cover) and the two clips the room ambience plays on it:
#   Sleep (2.4 s, loop)  the sleeping surface (blanket, pad, hide, nest) rises and falls like breathing
#   Wake  (1.6 s, once)  the surface is thrown back and settles; first and last frame are the rest pose
# Which vertices belong to the sleeping surface is chosen by material name (blanket beds) or by a radial mask around the
# middle of the bed (pads, nests, tents): the posts and frames at the border stay on Root, the weights fall off smoothly.
#
#   blender --background --python bed_rig.py -- <mesh name> <mesh folder> <work folder> [preview folder]
#
# <mesh folder> holds <mesh name>.mesh.xml (OgreXMLConverter output of models/<mesh name>.mesh). The result is written to
# <work folder>/<mesh name>.mesh.xml and <mesh name>.skeleton.xml and the scene is saved as <work folder>/blender<mesh name>.blend.
import math
import os
import sys
import traceback

import bpy
from mathutils import Quaternion

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo
import wake_preview as wp

# mats: materials that form the sleeping surface (None = every vertex); zmin: lowest z that may move (frames below stay put);
# style: "pad" = squash and bounce instead of throwing the surface back; flat: the sleeping surface is a flat sheet (its
# vertices are taken from the large horizontal faces); lift: scale of the throw; mode: "center" = weights fall off from the middle outwards, "rim" = the ring of posts moves (palisade bed)
CONFIG = {
    "Bed": {"mats": ["Blanket"]},
    "ImpBed": {"mats": ["ImpBedBlanket"]},
    "SpiderBed": {"mats": ["SpiderBedSilk"], "lift": 0.45},
    "AdventurerBed": {"style": "pad"},
    "GoblinBed": {"style": "pad"},
    "DragonBed": {"style": "pad"},
    "OrcBed": {"style": "pad"},
    "TentacleBed": {"style": "pad"},
    "RangerBed": {"zmin": 0.12, "lift": 0.7},
    "LizardmanBed": {"flat": True, "lift": 0.5},
    "TrollBed": {"zmin": 0.10, "mode": "rim"},
}

SKELETON = """<?xml version="1.0"?>
<skeleton blendmode="average">
\t<bones>
\t\t<bone id="0" name="Root">
\t\t\t<position x="0" y="0" z="0" />
\t\t\t<rotation angle="0"><axis x="1" y="0" z="0" /></rotation>
\t\t</bone>
\t\t<bone id="1" name="Cover">
\t\t\t<position x="%s" y="%s" z="%s" />
\t\t\t<rotation angle="0"><axis x="1" y="0" z="0" /></rotation>
\t\t</bone>
\t</bones>
\t<bonehierarchy>
\t\t<boneparent bone="Cover" parent="Root" />
\t</bonehierarchy>
\t<animations />
</skeleton>
"""


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def surface(me, cfg):
    """-> (weights {vertex: weight}, centre (x, y, z), half sizes (rx, ry), z range)"""
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    zmin = cfg.get("zmin", min(zs) - 1.0)
    mats = cfg.get("mats")
    allowed = set(range(len(me.vertices)))
    if mats:
        index = [i for i, m in enumerate(me.materials) if m.name in mats]
        allowed = {v for p in me.polygons if p.material_index in index for v in p.vertices}
        assert allowed, (mats, [m.name for m in me.materials])
    if cfg.get("flat"):
        allowed = {v for p in me.polygons if p.normal.z > 0.9 and p.area > 0.03 for v in p.vertices}
        assert allowed
    # the blanket beds are measured on the blanket alone, so that frame and posts do not stretch the ellipse
    px = [me.vertices[i].co.x for i in allowed]
    py = [me.vertices[i].co.y for i in allowed]
    pz = [me.vertices[i].co.z for i in allowed]
    cx, cy = (max(px) + min(px)) / 2, (max(py) + min(py)) / 2
    rx, ry = (max(px) - min(px)) / 2, (max(py) - min(py)) / 2
    rim = cfg.get("mode") == "rim"
    result = {}
    for i in allowed:
        co = me.vertices[i].co
        r = math.hypot((co.x - cx) / max(rx, 1e-6), (co.y - cy) / max(ry, 1e-6))
        if cfg.get("flat"):
            w = 1.0
        elif rim:
            w = smooth((r - 0.45) / 0.35)
        elif mats:
            w = smooth((1.15 - r) / 0.45)
        else:
            w = smooth((1.0 - r) / 0.55)
        w *= smooth((co.z - zmin) / 0.05)
        if w > 0.001:
            result[i] = w
    return result, (cx, cy, min(pz)), (rx, ry), (min(zs), max(zs))


def build(name, mesh_dir, work, preview):
    cfg = CONFIG[name]
    os.makedirs(work, exist_ok=True)
    wp.clear_scene()
    mesh_xml = os.path.join(mesh_dir, name + ".mesh.xml")
    # first read the geometry alone to find the middle of the sleeping surface, then build the skeleton around it
    probe = oo.import_mesh(mesh_xml, None, name + "Probe")
    w, (cx, cy, cz), (rx, ry), (zlow, zhigh) = surface(probe.data, cfg)
    probe_mesh = probe.data
    bpy.data.objects.remove(probe)
    bpy.data.meshes.remove(probe_mesh)
    skeleton_xml = os.path.join(work, name + ".base.skeleton.xml")
    with open(skeleton_xml, "w") as f:
        f.write(SKELETON % (cx, cy, cz))
    arm = oo.import_skeleton(skeleton_xml, name + "Arm", with_actions=False)
    obj = oo.import_mesh(mesh_xml, arm, name + "Mesh")
    count = len(obj.data.vertices)
    for index in range(count):
        weight = w.get(index, 0.0)
        if weight > 0:
            obj.vertex_groups["Cover"].add([index], weight, "REPLACE")
        obj.vertex_groups["Root"].add([index], 1.0 - weight, "REPLACE") if weight < 0.999 else None

    size = 0.5 * (rx + ry)
    amp = 0.05 * size + 0.006                      # breathing height
    rim = cfg.get("mode") == "rim"
    sleep = oo.new_action(arm, "Sleep", 2.4)
    for i in range(9):
        t = i * 0.3
        v = 0.5 * (1.0 - math.cos(2 * math.pi * t / 2.4))
        if rim:
            oo.add_pose_key(arm, sleep, "Cover", t, scale=(1 + 0.012 * v, 1 + 0.012 * v, 1.0))
        else:
            oo.add_pose_key(arm, sleep, "Cover", t, loc=(0, 0, amp * v), scale=(1 + 0.02 * v, 1 + 0.02 * v, 1 + 0.05 * v))
    wake = oo.new_action(arm, "Wake", 1.6)
    r = (0.5 * min(rx, ry) + 0.05) * cfg.get("lift", 1.0)
    if rim:
        for t, s in ((0, 1.0), (0.4, 1.18), (0.9, 0.96), (1.3, 1.03), (1.6, 1.0)):
            oo.add_pose_key(arm, wake, "Cover", t, loc=(0, 0, 0.12 * (s - 1.0)), scale=(s, s, 1.0))
    elif cfg.get("style") == "pad":
        # squash, spring up with a wobble, settle (no big lift: the weights fall off towards the border)
        for t, dz, pitch, roll, s, sz in ((0.0, 0, 0, 0, 1.0, 1.0), (0.25, 0, 0, 0, 1.06, 0.8),
                                          (0.55, 0.12, -12, 8, 0.96, 1.18), (0.9, 0.04, 6, -4, 1.02, 0.95),
                                          (1.25, 0.0, -2, 1, 1.0, 1.02), (1.6, 0, 0, 0, 1.0, 1.0)):
            q = Quaternion((1, 0, 0), math.radians(pitch)) @ Quaternion((0, 1, 0), math.radians(roll))
            oo.add_pose_key(arm, wake, "Cover", t, loc=(0, 0, dz * r), rot=q, scale=(s, s, sz))
    else:
        for t, dz, dy, pitch in ((0.0, 0.0, 0.0, 0), (0.25, 0.7, -0.15, -20), (0.55, 1.5, -0.55, -70),
                                 (1.0, 0.6, -0.4, -35), (1.35, 0.1, -0.05, -6), (1.6, 0.0, 0.0, 0)):
            oo.add_pose_key(arm, wake, "Cover", t, loc=(0, dy * r, dz * r),
                            rot=Quaternion((1, 0, 0), math.radians(pitch)))
    for a in (sleep, wake):
        a.use_fake_user = True

    oo.export_skeleton_xml(arm, os.path.join(work, name + ".skeleton.xml"))
    oo.export_mesh_xml(obj, arm, os.path.join(work, name + ".mesh.xml"), name + ".skeleton", smooth=False)
    if preview:
        os.makedirs(preview, exist_ok=True)
        d = max(rx, ry) * 2
        target = (cx, cy, zlow + (zhigh - zlow) * 0.4)
        for label, action, seconds in (("rest", "Sleep", 0.0), ("breath", "Sleep", 1.2), ("w1", "Wake", 0.25),
                                       ("w2", "Wake", 0.55), ("w3", "Wake", 1.0)):
            wp.pose(arm, action, seconds)
            wp.render(os.path.join(preview, "%s_%s.png" % (name, label)), 45, 30, d * 2.4 + 0.5, target, lens=50)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, "blender%s.blend" % name))


try:
    args = sys.argv[sys.argv.index("--") + 1:]
    build(args[0], args[1], args[2], args[3] if len(args) > 3 else None)
    open(os.path.join(args[2], args[0] + ".ok"), "w").write("ok")
except Exception:
    args = sys.argv[sys.argv.index("--") + 1:]
    os.makedirs(args[2], exist_ok=True)
    open(os.path.join(args[2], args[0] + ".err"), "w").write(traceback.format_exc())
