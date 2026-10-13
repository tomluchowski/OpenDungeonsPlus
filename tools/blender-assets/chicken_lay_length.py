# Rebuilds only the hen clip Lay with the length of the Lay pose (hen_lay_flutter.LAY_LENGTH) and exports it.
#
#   blender --background <blenderHuhn.blend> --python chicken_lay_length.py -- <work folder>
#
# The file is saved (Blender keeps the previous version as .blend1) and <work folder>/lay.clips.skeleton.xml holds
# the Lay clip only, ready for chicken_clips_splice.py. All other clips are not touched.
import os
import sys
import traceback

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo
import odp_fk as fk
import hen_lay_flutter as hl

work = sys.argv[sys.argv.index("--") + 1]
log = open(os.path.join(work, "lay.build.log"), "w")

try:
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    rig = fk.Rig(arm)
    lay = hl.build_clip_scaled(rig, "Lay", hl.LAY_LENGTH, hl.lay_pose, lambda t: 1.0 + 0.13 * hl.lay_values(t)[2])
    lay.use_fake_user = True
    log.write("built Lay %s s\n" % hl.LAY_LENGTH)
    bpy.ops.wm.save_mainfile()
    log.write("saved %s\n" % bpy.data.filepath)
    oo.export_skeleton_xml(arm, os.path.join(work, "lay.clips.skeleton.xml"), only_actions=["Lay"])
    log.write("exported\n")
except Exception:
    log.write(traceback.format_exc())
log.close()
