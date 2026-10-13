# Brings a door .blend file in line with the exported door skeleton and mesh, then checks the round trip.
#
#   blender --background <DoorX.blend> --python door_blend_sync.py -- <work folder> <DoorX> <log file>
#
# The work folder holds <DoorX>.skeleton.xml and <DoorX>.mesh.xml (OgreXMLConverter output of models/<DoorX>.*).
# The script replaces the armature and the mesh object of the file with fresh imports of those two files (the
# materials of the old mesh are kept), saves the file (Blender keeps the previous version as .blend1), exports
# skeleton and mesh again and compares bones, hierarchy, clips and weights with the source files.
import os
import sys

import bpy
from mathutils import Quaternion

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo

work, name, logpath = sys.argv[sys.argv.index("--") + 1:][:3]
log = open(logpath, "w")


def say(*parts):
    log.write(" ".join(str(p) for p in parts) + "\n")
    log.flush()


def main():
    global problems
    skeleton_xml = os.path.join(work, name + ".skeleton.xml")
    mesh_xml = os.path.join(work, name + ".mesh.xml")
    arm_name = name + "Arm"

    old_arm = bpy.data.objects.get(arm_name)
    old_mesh = bpy.data.objects.get(name)
    say("old", arm_name, len(old_arm.data.bones) if old_arm else None, name, len(old_mesh.data.vertices) if old_mesh else None)
    old_materials = [slot.material for slot in old_mesh.material_slots] if old_mesh else []
    for obj in (old_arm, old_mesh):
        if obj is not None:
            obj.name = obj.name + "_old"
            obj.data.name = obj.data.name + "_old"

    arm = oo.import_skeleton(skeleton_xml, arm_name)
    for act in bpy.data.actions:
        if act.get("ogre_length") is not None:
            act.use_fake_user = True
    mesh = oo.import_mesh(mesh_xml, arm, name)
    for index, material in enumerate(old_materials):
        if index < len(mesh.data.materials) and material is not None and mesh.data.materials[index].name != material.name:
            mesh.data.materials[index] = material
        elif index >= len(mesh.data.materials) and material is not None:
            mesh.data.materials.append(material)
    mesh.parent = arm

    for obj in (old_arm, old_mesh):
        if obj is not None:
            data = obj.data
            is_mesh = obj.type == "MESH"
            bpy.data.objects.remove(obj, do_unlink=True)
            if is_mesh:
                bpy.data.meshes.remove(data)
            else:
                bpy.data.armatures.remove(data)
    for act in list(bpy.data.actions):
        if act.get("ogre_length") is None and act.users == 0:
            bpy.data.actions.remove(act)
    bpy.ops.wm.save_mainfile()
    say("saved", bpy.data.filepath)

    # ---- round trip: export again and compare with the source files
    out_skel = os.path.join(work, "rt_" + name + ".skeleton.xml")
    out_mesh = os.path.join(work, "rt_" + name + ".mesh.xml")
    oo.export_skeleton_xml(arm, out_skel)
    stats = oo.export_mesh_xml(mesh, arm, out_mesh, name + ".skeleton")
    bones0, parents0, anims0 = oo.read_skeleton(skeleton_xml)
    bones1, parents1, anims1 = oo.read_skeleton(out_skel)
    problems = []
    if [b["name"] for b in bones0] != [b["name"] for b in bones1]:
        problems.append("bone names differ")
    if parents0 != parents1:
        problems.append("hierarchy differs")
    worst = 0.0
    for b0, b1 in zip(bones0, bones1):
        worst = max(worst, (b0["pos"] - b1["pos"]).length)
        q0, q1 = b0["rot"], b1["rot"]
        worst = max(worst, min((q0 - q1).magnitude, (q0 + q1).magnitude))
    say("bones", len(bones1), "max bind deviation %.2e" % worst)
    if worst > 1e-4:
        problems.append("bind pose deviation %.2e" % worst)
    by0 = dict((a[0], a) for a in anims0)
    by1 = dict((a[0], a) for a in anims1)
    if sorted(by0) != sorted(by1):
        problems.append("clips differ %s %s" % (sorted(by0), sorted(by1)))
    clip_worst = 0.0
    for cname in sorted(by0):
        if cname not in by1:
            continue
        t0 = dict((t[0], t[1]) for t in by0[cname][2])
        t1 = dict((t[0], t[1]) for t in by1[cname][2])
        if abs(by0[cname][1] - by1[cname][1]) > 1e-5:
            problems.append("length of %s differs" % cname)
        for bone, keys0 in t0.items():
            keys1 = t1.get(bone)
            if keys1 is None or len(keys1) != len(keys0):
                problems.append("%s/%s key count %d vs %s" % (cname, bone, len(keys0), None if keys1 is None else len(keys1)))
                continue
            for k0, k1 in zip(keys0, keys1):
                d = (k0[1] - k1[1]).length
                d = max(d, min((k0[2] - k1[2]).magnitude, (k0[2] + k1[2]).magnitude))
                d = max(d, (k0[3] - k1[3]).length)
                clip_worst = max(clip_worst, d)
        say("clip", cname, by0[cname][1], "tracks", len(t0), "->", len(t1))
    say("clips max deviation %.2e" % clip_worst)
    if clip_worst > 1e-4:
        problems.append("clip deviation %.2e" % clip_worst)
    import xml.etree.ElementTree as ET
    src_tris = len(ET.parse(mesh_xml).getroot().findall(".//face"))
    say("mesh", stats, "source triangles", src_tris)
    if stats["triangles"] != src_tris or stats["unweighted"]:
        problems.append("mesh triangles %d vs %d, unweighted %d" % (stats["triangles"], src_tris, stats["unweighted"]))
    say("RESULT", "OK" if not problems else "PROBLEMS " + "; ".join(problems))
    log.close()



try:
    main()
except Exception:
    import traceback
    say(traceback.format_exc())
    log.close()
