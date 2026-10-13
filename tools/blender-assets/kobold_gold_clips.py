# Builds the Kobold worker clips ClimbGold and PourGold in Blender (poses are authored with
# odp_fk, relative to the first frame of the Idle clip).
import math
import bpy
from mathutils import Vector
import odp_ogre_io as oo
import odp_fk as fk

X = (1, 0, 0)
Y = (0, 1, 0)
HIP = (0, 0.03, 0.16)
ease = fk.ease


def bump(u, a, b):
    if u <= a or u >= b:
        return 0.0
    return math.sin(math.pi * (u - a) / (b - a))


def upper_body(lean, sack_extra=0.0):
    """Lean the upper body (and the sack on the back) forward around the hip."""
    spec = {
        "TorsoUpper": {"rot": [(X, lean, HIP)]},
        "Sack": {"rot": [(X, sack_extra, None), (X, lean, HIP)]},
    }
    return spec


def climb_pose(t, length=0.8):
    u = t / length
    spec = upper_body(16.0 * ease(0.0, 0.3, u) + 4.0 * bump(u, 0.0, 1.0))
    spec["Head"] = {"rot": [(X, -9.0 * ease(0.0, 0.3, u))]}
    lift_l = bump(u, 0.05, 0.5)
    lift_r = bump(u, 0.45, 0.95)
    for side, lift in (("L", lift_l), ("R", lift_r)):
        spec["LegUpper." + side] = {"rot": [(X, -60.0 * lift)]}
        spec["LegLower." + side] = {"rot": [(X, 70.0 * lift)]}
        spec["Foot." + side] = {"rot": [(X, -18.0 * lift)]}
    reach_r = bump(u, 0.0, 0.55)
    reach_l = bump(u, 0.4, 0.95)
    for side, reach in (("R", reach_r), ("L", reach_l)):
        spec["ArmUpper." + side] = {"rot": [(X, -65.0 * reach - 12.0 * ease(0.0, 0.3, u))]}
        spec["ArmLower." + side] = {"rot": [(X, -25.0 * reach)]}
    spec["TorsoLower"] = {"move": (0, 0, 0.012 * (bump(u, 0.05, 0.5) + bump(u, 0.45, 0.95)))}
    return spec


def pour_pose(t, length=1.2):
    u = t / length
    shake = math.sin(u * 2 * math.pi * 3.0) * ease(0.42, 0.52, u) * (1.0 - ease(0.78, 0.88, u))
    lean = 16.0 + 40.0 * ease(0.12, 0.4, u) - 40.0 * ease(0.82, 1.0, u) + 6.0 * shake
    tilt = 62.0 * ease(0.22, 0.46, u) - 62.0 * ease(0.8, 1.0, u) + 10.0 * shake
    spec = upper_body(lean, tilt)
    spec["Head"] = {"rot": [(X, -0.55 * lean)]}
    arms_back = -12.0 + 92.0 * ease(0.1, 0.32, u) - 92.0 * ease(0.82, 1.0, u)
    for side in ("L", "R"):
        spec["ArmUpper." + side] = {"rot": [(X, arms_back + 6.0 * shake)]}
        spec["ArmLower." + side] = {"rot": [(X, -30.0 * ease(0.1, 0.32, u) + 30.0 * ease(0.82, 1.0, u))]}
        spec["LegUpper." + side] = {"rot": [(X, -22.0 * ease(0.1, 0.4, u) + 22.0 * ease(0.82, 1.0, u))]}
        spec["LegLower." + side] = {"rot": [(X, 35.0 * ease(0.1, 0.4, u) - 35.0 * ease(0.82, 1.0, u))]}
    spec["TorsoLower"] = {"move": (0, -0.012 * ease(0.1, 0.4, u) + 0.012 * ease(0.82, 1.0, u), -0.02 * (ease(0.1, 0.4, u) - ease(0.82, 1.0, u)) + 0.006 * shake)}
    return spec


def make(arm):
    rig = fk.Rig(arm, base_action="Idle", base_time=0.0)
    a = fk.build_clip(rig, "ClimbGold", 0.8, climb_pose)
    b = fk.build_clip(rig, "PourGold", 1.2, pour_pose)
    return rig, a, b
