# Renders contact-sheet frames and motion numbers of the hatchery animal clips in a background Blender.
#
#   blender --background <blenderHuhn.blend> --python chicken_clip_preview.py -- <out folder> <prefix> <clips> <frames>
#
# <clips> is a comma separated list of action names, <frames> the number of frames per clip. For every clip the
# script renders the frames from a camera that looks down at an angle like the game camera ("game") and from the
# side ("side"), as <out folder>/<prefix>_<clip>_<view>_<n>.png, and writes <out folder>/<prefix>_motion.txt with the
# largest travel of the body parts away from the first frame of the clip (in model units; the animal is about 0.3 long and 0.18 high).
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo

out_dir, prefix, clip_list, frame_count = sys.argv[sys.argv.index("--") + 1:][:4]
frame_count = int(frame_count)
os.makedirs(out_dir, exist_ok=True)

def main():
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    mesh = [o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm][0]
    scene = bpy.context.scene
    for shown in (arm, mesh):
        shown.hide_render = False  # the rooster file keeps its objects hidden for rendering
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.display.shading.show_cavity = False
    scene.render.resolution_x = scene.render.resolution_y = 360
    scene.render.film_transparent = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new("W")
    scene.world.color = (0.32, 0.30, 0.27)
    for other in bpy.data.objects:
        if other.type == "CAMERA":
            other.hide_render = True

    camera_data = bpy.data.cameras.new("PreviewCam")
    camera_data.lens = 70
    camera = bpy.data.objects.new("PreviewCam", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    TARGET = Vector((0.0, 0.0, 0.10))

    VIEWS = {
        # elevation / azimuth in degrees, distance: seen from the front right, high up, like the game camera
        "game": (55.0, 35.0, 0.75),
        "side": (12.0, 90.0, 0.75),
    }


    def aim(view):
        elevation, azimuth, distance = VIEWS[view]
        e, a = math.radians(elevation), math.radians(azimuth)
        # the animal looks along -y, azimuth 0 is in front of it
        camera.location = TARGET + Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e))) * distance
        camera.rotation_euler = (TARGET - camera.location).to_track_quat("-Z", "Y").to_euler()


    def evaluated_points():
        graph = bpy.context.evaluated_depsgraph_get()
        graph.update()
        evaluated = mesh.evaluated_get(graph)
        data = evaluated.to_mesh()
        points = [v.co.copy() for v in data.vertices]
        evaluated.to_mesh_clear()
        return points


    def region(point):
        if point.y < -0.09 and point.z > 0.10:
            return "head"
        if abs(point.x) > 0.045 and point.z > 0.07:
            return "wings"
        if point.z < 0.065:
            return "legs"
        return "body"


    def frame_of(clip, u):
        return u * float(clip["ogre_length"]) * oo.FPS


    def pose_at(clip, frame):
        whole = int(math.floor(frame))
        scene.frame_set(whole, subframe=frame - whole)


    REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    for image in bpy.data.images:
        if image.source == "FILE" and not os.path.exists(bpy.path.abspath(image.filepath)):
            candidate = os.path.join(REPO, "materials", "textures", os.path.basename(image.filepath))
            if os.path.exists(candidate):
                image.filepath = candidate
                image.reload()

    rest_action = bpy.data.actions.get("Idle")
    oo.assign_action(arm, rest_action)
    pose_at(rest_action, 1.0)  # frame 0 of the Idle clip is a one frame glitch (hip sunk), so the reference is frame 1
    base = evaluated_points()
    counts = {}
    for start in base:
        counts[region(start)] = counts.get(region(start), 0) + 1
    motion = open(os.path.join(out_dir, prefix + "_motion.txt"), "w")
    motion.write("vertices per region %s, bounds %s %s\n" % (counts, tuple(min(p[i] for p in base) for i in range(3)), tuple(max(p[i] for p in base) for i in range(3))))

    for name in clip_list.split(","):
        clip = bpy.data.actions[name]
        oo.assign_action(arm, clip)
        travel = {"head": 0.0, "wings": 0.0, "legs": 0.0, "body": 0.0}
        steps = int(round(float(clip["ogre_length"]) * oo.FPS))
        pose_at(clip, 0.0)
        first = evaluated_points()
        for step in range(steps + 1):
            pose_at(clip, float(step))
            for point, start, own in zip(evaluated_points(), base, first):
                kind = region(start)
                travel[kind] = max(travel[kind], (point - own).length)
        motion.write("%s length %.2f travel head %.3f wings %.3f legs %.3f body %.3f\n" % (
            name, float(clip["ogre_length"]), travel["head"], travel["wings"], travel["legs"], travel["body"]))
        motion.flush()
        for view in VIEWS:
            aim(view)
            for n in range(frame_count):
                u = n / float(frame_count)
                pose_at(clip, frame_of(clip, u))
                scene.render.filepath = os.path.join(out_dir, "%s_%s_%s_%02d.png" % (prefix, name, view, n))
                bpy.ops.render.render(write_still=True)
    motion.close()


try:
    main()
except Exception:
    import traceback
    with open(os.path.join(out_dir, prefix + "_error.txt"), "w") as handle:
        handle.write(traceback.format_exc())
