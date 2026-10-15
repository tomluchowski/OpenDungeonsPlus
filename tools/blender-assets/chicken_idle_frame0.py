# Removes the one frame glitch at the start of the hen Idle clip: frame 0 held a different pose (hip sunk, head,
# neck, wishbone, tail feathers and legs off), frames 1 to the end form the clean loop. Every key of Idle at frame 0
# gets the value of frame 1; the other clips are not touched.
#
#   blender --background <blenderHuhn.blend> --python chicken_idle_frame0.py -- <work folder> <name>
#
# Saves the file (Blender keeps the previous version as .blend1) and writes <work folder>/<name>.idle.skeleton.xml
# with the Idle clip only, ready for chicken_clips_splice.py. Run it on blenderHuhn.blend and blenderHahn.blend
# (the rooster file keeps a copy of Idle as base for its mating clips).
import os
import sys
import traceback

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo

work, name = sys.argv[sys.argv.index("--") + 1:][:2]
log = open(os.path.join(work, name + ".idle.log"), "w")

try:
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    idle = bpy.data.actions["Idle"]
    changed = 0
    for fc in oo._channelbag(idle).fcurves:
        first = [k for k in fc.keyframe_points if abs(k.co.x) < 1e-6]
        if not first:
            continue
        value = fc.evaluate(1.0)
        for key in first:
            if abs(key.co.y - value) > 1e-9:
                changed += 1
            key.co.y = value
            key.handle_left.y = value
            key.handle_right.y = value
        fc.update()
    log.write("changed %d channel keys\n" % changed)
    bpy.ops.wm.save_mainfile()
    log.write("saved %s\n" % bpy.data.filepath)
    oo.export_skeleton_xml(arm, os.path.join(work, name + ".idle.skeleton.xml"), only_actions=["Idle"])
    log.write("exported\n")
except Exception:
    log.write(traceback.format_exc())
log.close()
