# Gives the static crypt objects (statues and coffins) a two bone skeleton (Root, Top) and the clips the room ambience plays:
#   Sleep (3.0 s, loop)  the figure or lid settles and trembles a little, as if something breathes inside
#   Wake  (1.8 s, once)  the statue turns its head, the stone knight stirs, the lid slides open and back; first and last
#                        frame are the rest pose
# The upper part (everything above the cut height: statue figure, effigy on the tomb, coffin lid) is weighted to Top, the
# plinth and the base stay on Root.
#
#   blender --background --python crypt_rig.py -- <mesh name> <mesh folder> <work folder> [preview folder]
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

# cut: height above which the vertices move (plinth top, tomb top, lid bottom); kind: how the wake clip looks
CONFIG = {
    "KnightStatue": {"cut": 0.70, "kind": "turn"},
    "KnightStatue2": {"cut": 0.55, "kind": "turn"},
    "KnightCoffin": {"cut": 0.60, "kind": "stir"},
    "StoneCoffin": {"cut": 0.40, "kind": "lid"},
}

SKELETON = """<?xml version="1.0"?>
<skeleton blendmode="average">
\t<bones>
\t\t<bone id="0" name="Root">
\t\t\t<position x="0" y="0" z="0" />
\t\t\t<rotation angle="0"><axis x="1" y="0" z="0" /></rotation>
\t\t</bone>
\t\t<bone id="1" name="Top">
\t\t\t<position x="0" y="0" z="%s" />
\t\t\t<rotation angle="0"><axis x="1" y="0" z="0" /></rotation>
\t\t</bone>
\t</bones>
\t<bonehierarchy>
\t\t<boneparent bone="Top" parent="Root" />
\t</bonehierarchy>
\t<animations />
</skeleton>
"""


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def rot(axis, degrees):
    return Quaternion(axis, math.radians(degrees))


def build(name, mesh_dir, work, preview):
    cfg = CONFIG[name]
    cut = cfg["cut"]
    os.makedirs(work, exist_ok=True)
    wp.clear_scene()
    skeleton_xml = os.path.join(work, name + ".base.skeleton.xml")
    with open(skeleton_xml, "w") as f:
        f.write(SKELETON % cut)
    arm = oo.import_skeleton(skeleton_xml, name + "Arm", with_actions=False)
    obj = oo.import_mesh(os.path.join(mesh_dir, name + ".mesh.xml"), arm, name + "Mesh")
    me = obj.data
    for v in me.vertices:
        w = smooth((v.co.z - cut) / 0.03)
        if w > 0.0:
            obj.vertex_groups["Top"].add([v.index], w, "REPLACE")
        if w < 0.999:
            obj.vertex_groups["Root"].add([v.index], 1.0 - w, "REPLACE")
    zmax = max(v.co.z for v in me.vertices)
    h = zmax - cut

    sleep = oo.new_action(arm, "Sleep", 3.0)
    for i in range(11):
        t = i * 0.3
        v = math.sin(2 * math.pi * t / 3.0)
        if cfg["kind"] == "lid":
            oo.add_pose_key(arm, sleep, "Top", t, loc=(0.004 * v, 0, 0.003 * abs(v)), rot=rot((0, 0, 1), 0.8 * v))
        else:
            b = 0.5 * (1.0 - math.cos(2 * math.pi * t / 3.0))
            oo.add_pose_key(arm, sleep, "Top", t, loc=(0, 0, 0.012 * h * b), rot=rot((0, 0, 1), 1.2 * v),
                            scale=(1 + 0.012 * b, 1 + 0.012 * b, 1 + 0.02 * b))
    wake = oo.new_action(arm, "Wake", 1.8)
    if cfg["kind"] == "turn":
        # the head of the statue turns: a shudder, a slow turn with a small lift, and back
        keys = ((0.0, 0, 0), (0.2, 0, 3), (0.35, 0, -3), (0.7, 0.03, 38), (1.2, 0.03, 32), (1.5, 0.01, 8), (1.8, 0, 0))
        for t, dz, yaw in keys:
            oo.add_pose_key(arm, wake, "Top", t, loc=(0, 0, dz * h), rot=rot((0, 0, 1), yaw))
    elif cfg["kind"] == "stir":
        # the stone knight on the tomb lifts and rolls as if it wanted to sit up, then lies down again
        keys = ((0.0, 0, 0, 0), (0.25, 0.08, 6, -4), (0.6, 0.28, 20, -8), (1.0, 0.4, 26, 7), (1.4, 0.12, 8, 0),
                (1.8, 0, 0, 0))
        for t, dz, pitch, roll in keys:
            oo.add_pose_key(arm, wake, "Top", t, loc=(0, 0, dz * h), rot=rot((1, 0, 0), pitch) @ rot((0, 1, 0), roll))
    else:
        # lid: rattles, slides open sideways and back
        keys = ((0.0, 0, 0, 0), (0.2, 0.01, 0.0, 0.8), (0.45, 0.015, 0.1, -0.8), (0.8, 0.06, 0.28, 10),
                (1.2, 0.06, 0.28, 10), (1.5, 0.02, 0.06, 2), (1.8, 0, 0, 0))
        for t, dz, dy, yaw in keys:
            oo.add_pose_key(arm, wake, "Top", t, loc=(0, dy, dz), rot=rot((0, 0, 1), yaw))
    for a in (sleep, wake):
        a.use_fake_user = True

    oo.export_skeleton_xml(arm, os.path.join(work, name + ".skeleton.xml"))
    oo.export_mesh_xml(obj, arm, os.path.join(work, name + ".mesh.xml"), name + ".skeleton", smooth=False)
    if preview:
        os.makedirs(preview, exist_ok=True)
        xs = [v.co.x for v in me.vertices]
        ys = [v.co.y for v in me.vertices]
        d = max(max(xs) - min(xs), max(ys) - min(ys), zmax)
        target = (0, 0, zmax * 0.45)
        for label, action, seconds in (("rest", "Sleep", 0.0), ("breath", "Sleep", 0.75), ("w1", "Wake", 0.35),
                                       ("w2", "Wake", 0.8), ("w3", "Wake", 1.2)):
            wp.pose(arm, action, seconds)
            wp.render(os.path.join(preview, "%s_%s.png" % (name, label)), 40, 30, d * 2.2 + 0.4, target, lens=50)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, "blender%s.blend" % name))


try:
    args = sys.argv[sys.argv.index("--") + 1:]
    build(args[0], args[1], args[2], args[3] if len(args) > 3 else None)
    open(os.path.join(args[2], args[0] + ".ok"), "w").write("ok")
except Exception:
    args = sys.argv[sys.argv.index("--") + 1:]
    os.makedirs(args[2], exist_ok=True)
    open(os.path.join(args[2], args[0] + ".err"), "w").write(traceback.format_exc())
