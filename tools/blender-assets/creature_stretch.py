# Adds the clip Stretch (1.5 s, once) to a creature skeleton: the creature gets up, arches its back, tilts its head back and
# raises both arms (walkers) or rears up (animals), then relaxes. First and last frame are the rest pose. The pose is built
# with world axis rotations (odp_fk.Rig) from a small per creature table: the spine bones, the head and the arm roots.
#
#   blender --background <blender<Name>.blend> --python creature_stretch.py -- <Name> <skeleton xml> <work folder> [preview folder]
#
# <skeleton xml> is the OgreXMLConverter output of models/<Name>.skeleton (the game file). The blend must hold the same rig
# (bone positions are compared). The work folder gets <Name>.stretch.skeleton.xml with only the new clip; splice_anims.py puts
# it into the game skeleton. The blend is saved with the new action, Blender keeps the previous version as .blend1.
import math
import os
import sys
import traceback
import xml.etree.ElementTree as ET

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo
import odp_fk as fk
import wake_preview as wp

LENGTH = 1.5

# spine: bones from the hips upwards (each is bent), head: bones that tilt back, arms: arm roots that are raised,
# rear: animals only, the bones that rise (body or hips), arch: +1 / -1 flips the direction of the arch,
# squash: scale clip for the slime, depth: how far the arms and the spine go (1 = default)
TABLE = {
    "DarkElf": {"spine": ["LowerBack", "Spine", "Spine1"], "head": ["Neck", "Head"], "arms": ["LeftArm", "RightArm"]},
    "Defender": {"spine": ["LowerBack", "Spine", "Spine1"], "head": ["Neck", "Head"], "arms": ["LeftArm", "RightArm"]},
    "Elf": {"spine": ["LowerBack", "Spine", "Spine1"], "head": ["Neck", "Head"], "arms": ["LeftArm", "RightArm"]},
    "Goblin": {"spine": ["LowerBack", "Spine", "Spine1"], "head": ["Neck", "Head"], "arms": ["LeftArm", "RightArm"]},
    "Dwarf1": {"spine": ["spine"], "head": ["head"], "arms": ["arm_l", "arm_r"]},
    "Dwarf2": {"spine": ["spine"], "head": ["head"], "arms": ["arm_l", "arm_r"]},
    "Gnome": {"spine": ["spine"], "head": ["head"], "arms": ["arm_l", "arm_r"]},
    "Wizard": {"spine": ["spine"], "head": ["head"], "arms": ["arm_l", "arm_r"]},
    "Orc": {"spine": ["spine1", "spine2", "spine3"], "head": ["head"], "arms": ["arm_l", "arm_r"]},
    "Knight": {"spine": ["Spine_1", "Spine_2", "Spine_3"], "head": ["Head"], "arms": ["Arm_L", "Arm_R"]},
    "PitDemon": {"spine": ["Spine_1", "Spine_2", "Spine_3"], "head": ["Head"], "arms": ["Arm_L", "Arm_R"]},
    "Dragon": {"spine": ["Spine_1", "Spine_2", "Spine_3", "Spine_4"], "head": ["Head"], "arms": ["Arm_L", "Arm_R"]},
    "Wyvern": {"spine": ["Spine_1", "Spine_2", "Spine_3"], "head": ["Head"], "arms": ["Arm.L", "Arm.R"]},
    "Cultist": {"spine": ["spine", "chest"], "head": ["neck", "head"], "arms": ["upper_arm.L", "upper_arm.R"]},
    "Monk": {"spine": ["Spine", "Spine1"], "head": ["Neck", "Head"], "arms": ["Arm_L", "Arm_R"]},
    "NatureMonster": {"spine": ["Spine1", "Spine2", "Spine3"], "head": ["Neck", "Head"], "arms": ["Arm_L", "Arm_R"]},
    "RunelordDwarf": {"spine": ["spine.01", "spine.02", "spine.03", "spine.cr.01", "spine.cr.02", "spine.cr.03"],
                      "head": ["neck", "head", "neck.cr", "head.cr"],
                      "arms": ["upper_arm.L", "upper_arm.R", "upper_arm.cr.L", "upper_arm.cr.R"]},
    "lich": {"spine": ["spine1", "spine2", "spine3"], "head": ["neckBase", "crown"], "arms": ["shoulderLeft", "shoulderRight"]},
    "skeleton": {"spine": ["hip", "belly", "breast"], "head": ["neckbase", "crown"], "arms": ["shoulderLeft", "shoulderRight"]},
    "Kobold": {"spine": ["TorsoLower", "TorsoUpper"], "head": ["Neck", "Head"], "arms": ["ArmUpper.L", "ArmUpper.R"]},
    "LavaSpawn": {"spine": ["spine", "chest"], "head": ["neck", "head"], "arms": ["arm1.L", "arm1.R"]},
    "Lizardman": {"spine": ["Spine_0", "Spine_1", "Spine_2", "Spine_3"], "head": ["Spine_4", "Head"], "arms": ["Arm_L", "Arm_R"]},
    "Troll": {"spine": ["spine", "chest"], "head": ["head"], "arms": ["upperhand.L", "upperhand.R"]},
    "Adventurer": {"spine": ["C3", "C2", "C1"], "head": ["Neck", "Head"], "arms": ["Upperarm_L", "Upperarm_R"]},
    # animals: no arms, the front rises
    "Rat": {"spine": ["Backbone", "Backbone.001", "Backbone.002"], "head": ["Head"], "arms": []},
    "Roach": {"spine": ["SpineLow", "SpineHigh"], "head": ["Neck", "Head"], "arms": []},
    "Spider": {"spine": ["Body.002", "Body"], "head": [], "arms": []},
    "CaveHornet": {"spine": ["Body", "Body2"], "head": ["Head"], "arms": []},
    "Scarab": {"spine": ["Bone", "Bone.001"], "head": [], "arms": []},
    "Tentacle": {"spine": ["Body1", "Body2", "Body3"], "head": ["Head"], "arms": []},
    "Kreatur": {"spine": ["body", "body2"], "head": ["head"], "arms": []},
    "Slime": {"squash": True},
}

ARCH = -1.0      # rotation about +x by a negative angle tilts the upper body towards +y (the back of a creature that looks along -y)


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def amount(t):
    """0 -> 1 in 0.55 s, hold until 0.95 s, back to 0 at 1.5 s."""
    return smooth(t / 0.55) * (1.0 - smooth((t - 0.95) / 0.55))


def find_armature():
    return [o for o in bpy.data.objects if o.type == "ARMATURE"][0]


def check_same_rig(arm, skeleton_xml):
    bones, parents, _anims = oo.read_skeleton(skeleton_xml)
    names = {b["name"] for b in bones}
    missing = names - {b.name for b in arm.data.bones}
    return sorted(missing)


def arm_targets(arm, names):
    """For every arm root bone: (axis, degrees) that turns the bone from hanging (its rest direction) to raised outwards."""
    result = {}
    for name in names:
        bone = arm.data.bones[name]
        if not bone.children:
            continue
        head = bone.matrix_local.translation
        child = bone.children[0].matrix_local.translation
        rest = (child - head)
        if rest.length < 1e-6:
            continue
        side = 1.0 if head.x > 0.0 else -1.0
        up = Vector((side * 0.45, 0.0, 1.0)).normalized()
        axis = rest.normalized().cross(up)
        if axis.length < 1e-6:
            continue
        angle = math.degrees(rest.angle(up))
        result[name] = (tuple(axis.normalized()), angle * 0.85)
    return result


def build(name, skeleton_xml, work, preview):
    cfg = TABLE[name]
    os.makedirs(work, exist_ok=True)
    arm = find_armature()
    missing = check_same_rig(arm, skeleton_xml)
    if missing:
        raise RuntimeError("blend lacks bones of the game skeleton: %s" % missing[:8])
    if arm.animation_data and arm.animation_data.action:
        arm.animation_data.action = None
    # the rest pose comes from the bind pose, so neutralise the pose bones
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    old = bpy.data.actions.get("Stretch")
    if old is not None:
        bpy.data.actions.remove(old)
    if cfg.get("squash"):
        act = oo.new_action(arm, "Stretch", LENGTH)
        for i in range(0, 16):
            t = i * 0.1
            s = amount(min(t, LENGTH))
            for bone, k in (("slime_base", 1.0), ("slime_mid", 1.0), ("slime_head", 1.0)):
                oo.add_pose_key(arm, act, bone, t, loc=(0, 0, 0.06 * s * k),
                                scale=(1 - 0.08 * s, 1 - 0.08 * s, 1 + 0.2 * s))
        act.use_fake_user = True
    else:
        rig = fk.Rig(arm)
        spine = [b for b in cfg["spine"] if b in rig.rest]
        head = [b for b in cfg["head"] if b in rig.rest]
        targets = arm_targets(arm, [a for a in cfg["arms"] if a in rig.rest])
        count = max(1, len(spine))

        def pose(t):
            s = amount(t)
            spec = {}
            for b in spine:
                spec[b] = {"rot": [((1, 0, 0), ARCH * (28.0 / count) * s)]}
            for b in head:
                spec[b] = {"rot": [((1, 0, 0), ARCH * (22.0 / max(1, len(head))) * s)]}
            for b, (axis, degrees) in targets.items():
                spec[b] = {"rot": [(axis, degrees * s)]}
            return spec

        keyed = [b for b in rig.order]
        act = fk.build_clip(rig, "Stretch", LENGTH, pose, step=0.1, bones=keyed)
        act.use_fake_user = True
    oo.export_skeleton_xml(arm, os.path.join(work, name + ".stretch.skeleton.xml"), only_actions=["Stretch"])
    if preview:
        os.makedirs(preview, exist_ok=True)
        meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm]
        zs = [(o.matrix_world @ Vector(c)).z for o in meshes for c in o.bound_box]
        xs = [(o.matrix_world @ Vector(c)).x for o in meshes for c in o.bound_box]
        ys = [(o.matrix_world @ Vector(c)).y for o in meshes for c in o.bound_box]
        size = max(max(zs) - min(zs), max(xs) - min(xs), max(ys) - min(ys))
        target = ((max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, (max(zs) + min(zs)) / 2)
        for o in bpy.data.objects:
            if o.type in ("MESH", "ARMATURE"):
                o.hide_render = False
        for label, seconds in (("rest", 0.0), ("breath", 0.3), ("w1", 0.55), ("w2", 0.8), ("w3", 1.2)):
            wp.pose(arm, "Stretch", seconds)
            wp.render(os.path.join(preview, "%s_%s.png" % (name, label)), 40, 30, size * 2.2 + 0.3, target, lens=50)
    bpy.ops.wm.save_mainfile()


try:
    args = sys.argv[sys.argv.index("--") + 1:]
    build(args[0], args[1], args[2], args[3] if len(args) > 3 else None)
    open(os.path.join(args[2], args[0] + ".ok"), "w").write("ok")
except Exception:
    args = sys.argv[sys.argv.index("--") + 1:]
    os.makedirs(args[2], exist_ok=True)
    open(os.path.join(args[2], args[0] + ".err"), "w").write(traceback.format_exc())
