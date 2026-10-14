# Builds the dwarf worker clips ClimbGold and PourGold in Blender, the same two clips the Kobold worker has
# (kobold_gold_clips.py). Poses are authored with odp_fk, relative to the first frame of the Idle clip.
# The dwarf rig: cog (hips and legs), spine (body, arms, head), arm_*/forearm_*, leg_*/lowleg_*/foot_*, head.
import math
import bpy
import odp_fk as fk

X = (1, 0, 0)
HIP = (0.0, 0.0, 0.295)
ease = fk.ease


def bump(u, a, b):
    if u <= a or u >= b:
        return 0.0
    return math.sin(math.pi * (u - a) / (b - a))


def upper_body(lean):
    """Lean the upper body forward around the hip."""
    return {"spine": {"rot": [(X, lean, HIP)]}}


def climb_pose(t, length=0.8):
    u = t / length
    spec = upper_body(16.0 * ease(0.0, 0.3, u) + 4.0 * bump(u, 0.0, 1.0))
    spec["head"] = {"rot": [(X, -9.0 * ease(0.0, 0.3, u))]}
    for side, lift in (("l", bump(u, 0.05, 0.5)), ("r", bump(u, 0.45, 0.95))):
        spec["leg_" + side] = {"rot": [(X, -60.0 * lift)]}
        spec["lowleg_" + side] = {"rot": [(X, 70.0 * lift)]}
        spec["foot_" + side] = {"rot": [(X, -18.0 * lift)]}
    for side, reach in (("r", bump(u, 0.0, 0.55)), ("l", bump(u, 0.4, 0.95))):
        spec["arm_" + side] = {"rot": [(X, -65.0 * reach - 12.0 * ease(0.0, 0.3, u))]}
        spec["forearm_" + side] = {"rot": [(X, -25.0 * reach)]}
    spec["cog"] = {"move": (0, 0, 0.02 * (bump(u, 0.05, 0.5) + bump(u, 0.45, 0.95)))}
    return spec


def pour_pose(t, length=1.2):
    u = t / length
    shake = math.sin(u * 2 * math.pi * 3.0) * ease(0.42, 0.52, u) * (1.0 - ease(0.78, 0.88, u))
    lean = 16.0 + 40.0 * ease(0.12, 0.4, u) - 40.0 * ease(0.82, 1.0, u) + 6.0 * shake
    spec = upper_body(lean)
    spec["head"] = {"rot": [(X, -0.55 * lean)]}
    arms_back = -12.0 + 92.0 * ease(0.1, 0.32, u) - 92.0 * ease(0.82, 1.0, u)
    for side in ("l", "r"):
        spec["arm_" + side] = {"rot": [(X, arms_back + 6.0 * shake)]}
        spec["forearm_" + side] = {"rot": [(X, -30.0 * ease(0.1, 0.32, u) + 30.0 * ease(0.82, 1.0, u))]}
        spec["leg_" + side] = {"rot": [(X, -22.0 * ease(0.1, 0.4, u) + 22.0 * ease(0.82, 1.0, u))]}
        spec["lowleg_" + side] = {"rot": [(X, 35.0 * ease(0.1, 0.4, u) - 35.0 * ease(0.82, 1.0, u))]}
    spec["cog"] = {"move": (0, -0.02 * ease(0.1, 0.4, u) + 0.02 * ease(0.82, 1.0, u),
                            -0.035 * (ease(0.1, 0.4, u) - ease(0.82, 1.0, u)) + 0.01 * shake)}
    return spec


def make(arm):
    rig = fk.Rig(arm, base_action="Idle", base_time=0.0)
    a = fk.build_clip(rig, "ClimbGold", 0.8, climb_pose)
    b = fk.build_clip(rig, "PourGold", 1.2, pour_pose)
    return rig, a, b
