# Author an independent roof-flight clip on the shared chicken skeleton.
# Blender background invocation: --python this_file -- <private output directory> <unique blend path>
import math
import os
import sys
import bpy
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_fk as fk
import odp_ogre_io as oo
from hatchery_clips import X, Y, HIP, NECK, HEADP, wings, ease


def roof_flight_pose(t):
    u = t / 4.0
    crouch = ease(0.0, 0.07, u) - ease(0.07, 0.15, u)
    airborne = ease(0.10, 0.15, u) - ease(0.85, 0.95, u)
    landing = ease(0.83, 0.87, u) - ease(0.90, 1.0, u)
    # Eleven complete strokes during the entire travel interval, with smooth unfolding/folding.
    phase = max(0.0, min(1.0, (u - 0.12) / 0.73))
    flap = 0.5 - 0.5 * math.cos(phase * math.pi * 22.0)
    spec = {
        "Hip": {"move": (0, 0, -0.035 * crouch - 0.018 * landing),
                "rot": [(X, -12.0 * airborne, HIP)]},
        "Leg_L": {"move": (0, 0, 0.03 * crouch + 0.015 * landing),
                  "rot": [(X, 25.0 * airborne, (0, 0, 0.07))]},
        "Leg_R": {"move": (0, 0, 0.03 * crouch + 0.015 * landing),
                  "rot": [(X, 25.0 * airborne, (0, 0, 0.07))]},
        "NeckBone": {"rot": [(X, -8.0 * airborne, NECK)]},
        "Head": {"rot": [(X, -8.0 * airborne, HEADP)]},
        "TailFeather_L": {"rot": [(Y, -8.0 * airborne, HIP)]},
        "TailFeather_R": {"rot": [(Y, 8.0 * airborne, HIP)]},
    }
    wings(spec, airborne * (8.0 + 85.0 * flap))
    return spec


if __name__ == "__main__":
    output, blend = sys.argv[sys.argv.index("--") + 1:]
    arm = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"][0]
    rig = fk.Rig(arm, "Idle")
    clip = fk.build_clip(rig, "RoofFlight", 4.0, roof_flight_pose, step=1.0 / 48.0)
    clip.use_fake_user = True
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=blend)
    oo.export_skeleton_xml(arm, os.path.join(output, "roof-flight.clips.skeleton.xml"), only_actions=["RoofFlight"])
