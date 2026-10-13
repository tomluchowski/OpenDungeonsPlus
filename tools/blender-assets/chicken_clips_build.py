# Builds the hatchery animal clips in one of the chicken Blender files and exports the changed clips as skeleton XML.
#
#   blender --background <blenderHuhn.blend> --python chicken_clips_build.py -- <work folder> hen
#   blender --background <blenderHahn.blend> --python chicken_clips_build.py -- <work folder> rooster <blenderHuhn.blend>
#   blender --background <blenderKueken.blend> --python chicken_clips_build.py -- <work folder> chick
#
# hen:     Peep, Crow, Hatch, Run (hatchery_clips.py), Lay, Flutter (hen_lay_flutter.py), Duck, Mount, Tread, Dismount,
#          MountCycle (hatchery_clips.py, mating clips)
# rooster: Crow, Mount, Tread, Dismount, MountCycle (the base clips Idle, Walk, Pick and Run come from the hen file)
# chick:   Peep, Hatch
# The file is saved (Blender keeps the previous version as .blend1) and <work folder>/<mode>.clips.skeleton.xml
# holds the built clips, ready to be spliced into models/Chicken.skeleton.
import os
import sys
import traceback

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo
import odp_fk as fk
import hatchery_clips as hc
import hen_lay_flutter as hl

args = sys.argv[sys.argv.index("--") + 1:]
work, mode = args[0], args[1]
log = open(os.path.join(work, mode + ".build.log"), "w")


def say(*parts):
    log.write(" ".join(str(p) for p in parts) + "\n")
    log.flush()


try:
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    if mode == "rooster":
        source = args[2]
        with bpy.data.libraries.load(source, link=False) as (src, dst):
            dst.actions = [n for n in ("Idle", "Walk", "Pick", "Run") if n in src.actions and n not in bpy.data.actions]
        say("copied base clips", [a.name for a in bpy.data.actions])

    built = []
    if mode == "hen":
        rig, acts = hc.make(arm)
        built += acts
        rig, acts = hl.make(arm)
        built += acts
        rig, acts = hc.make_mating(arm)
        built += acts
    elif mode == "rooster":
        rig = fk.Rig(arm)
        built.append(fk.build_clip(rig, "Crow", 2.0, hc.crow_pose))
        rig, acts = hc.make_mating(arm, rooster_only=True)
        built += acts
    elif mode == "chick":
        rig = fk.Rig(arm)
        built.append(fk.build_clip(rig, "Peep", 1.2, hc.peep_pose))
        built.append(fk.build_clip(rig, "Hatch", 1.6, hc.hatch_pose))
    for act in bpy.data.actions:
        if act.get("ogre_length") is not None:
            act.use_fake_user = True
    names = [a.name for a in built]
    say("built", names)
    bpy.ops.wm.save_mainfile()
    say("saved", bpy.data.filepath)
    oo.export_skeleton_xml(arm, os.path.join(work, mode + ".clips.skeleton.xml"), only_actions=names)
    say("exported", names)
except Exception:
    say(traceback.format_exc())
log.close()
