# Helper for the wake clip work: loads an Ogre mesh + skeleton (XML) into a background Blender, poses it at a clip
# time and renders Workbench pictures (game-like view from above, front and side).
#
#   blender --background --python <script that imports this module> -- ...
#
# Usage inside a script:
#   import wake_preview as wp
#   arm, obj = wp.load(mesh_xml, skeleton_xml, texture)         (skeleton_xml may be None for a static mesh)
#   wp.pose(arm, "Door", 0.5)                                    (action name, time in seconds)
#   wp.render(out_png, elevation, azimuth, distance, target)
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo


def clear_scene():
    """Removes the default cube, light and camera of a fresh background Blender."""
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load(mesh_xml, skeleton_xml, texture, name="Preview"):
    clear_scene()
    arm = None
    if skeleton_xml:
        arm = oo.import_skeleton(skeleton_xml, name + "Arm", with_actions=True)
    obj = oo.import_mesh(mesh_xml, arm, name + "Mesh", texture)
    return arm, obj


def pose(arm, action, seconds):
    act = bpy.data.actions[action]
    oo.assign_action(arm, act)
    bpy.context.scene.frame_set(int(round(seconds * oo.FPS)))
    bpy.context.view_layer.update()


def _setup(scene):
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.display.shading.show_cavity = False
    scene.render.resolution_x = scene.render.resolution_y = 480
    scene.render.film_transparent = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new("W")
    scene.world.color = (0.32, 0.30, 0.27)
    for other in bpy.data.objects:
        if other.type == "CAMERA" and other.name != "WakeCam":
            other.hide_render = True


def render(out_png, elevation=55.0, azimuth=35.0, distance=3.0, target=(0.0, 0.0, 0.3), lens=60):
    """azimuth 0 looks from -y at the model, 90 from +x; elevation in degrees above the floor."""
    scene = bpy.context.scene
    _setup(scene)
    camera = bpy.data.objects.get("WakeCam")
    if camera is None:
        camera_data = bpy.data.cameras.new("WakeCam")
        camera = bpy.data.objects.new("WakeCam", camera_data)
        scene.collection.objects.link(camera)
    camera.data.lens = lens
    scene.camera = camera
    t = Vector(target)
    e, a = math.radians(elevation), math.radians(azimuth)
    camera.location = t + Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e))) * distance
    camera.rotation_euler = (t - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
