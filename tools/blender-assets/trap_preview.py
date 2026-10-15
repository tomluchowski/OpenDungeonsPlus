# Renders a preview of one trap model from the game camera angle (and a side view) in a background Blender.
#
#   blender --background <blenderTrapX.blend> --python trap_preview.py -- <out folder> <prefix>
#
# Writes <out folder>/<prefix>_game.png and <prefix>_side.png (Workbench, flat colours of the materials).
import math
import os
import sys

import bpy
from mathutils import Vector

out_dir, prefix = sys.argv[sys.argv.index("--") + 1:][:2]
os.makedirs(out_dir, exist_ok=True)

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.render.resolution_x = scene.render.resolution_y = 360
if scene.world is None:
    scene.world = bpy.data.worlds.new("W")
scene.world.color = (0.32, 0.30, 0.27)
data = bpy.data.cameras.new("PreviewCam")
data.lens = 70
camera = bpy.data.objects.new("PreviewCam", data)
scene.collection.objects.link(camera)
scene.camera = camera
target = Vector((0.0, 0.0, 0.12))
for view, (elevation, azimuth, distance) in {"game": (55.0, 35.0, 3.0), "side": (14.0, 90.0, 3.0)}.items():
    e, a = math.radians(elevation), math.radians(azimuth)
    camera.location = target + Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e))) * distance
    camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = os.path.join(out_dir, "%s_%s.png" % (prefix, view))
    bpy.ops.render.render(write_still=True)
