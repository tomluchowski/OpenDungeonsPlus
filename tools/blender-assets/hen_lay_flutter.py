# Builds the hen clips Lay (LAY_LENGTH: sits down, fluffs up, stands up and shows the egg) and Flutter (0.8 s: a short
# flap up) for the chicken skeleton in Blender. Same authoring as hatchery_clips.py, relative to the first frame of
# the Idle clip. The clips use bone rotations and moves plus a uniform scale on the Hip bone for the fluffing, so
# they work for the hen, the chick and the rooster (the three share the skeleton).
import math
import bpy
import odp_ogre_io as oo
import odp_fk as fk
from mathutils import Quaternion
from hatchery_clips import X, Y, Z, HIP, SHOULDER, NECK, HEADP, bump, wings, ease

# The server shows the Lay pose for HatcheryLayShowTurns turns (config/rooms.cfg, 2) and a turn takes
# 1 / ODApplication::turnsPerSecond (1.4) seconds; the pose length is pinned by the balance parity test, so the clip
# is made exactly as long as the pose and ends with the stand-up complete instead of being cut off
LAY_SHOW_TURNS = 2
TURNS_PER_SECOND = 1.4
LAY_LENGTH = LAY_SHOW_TURNS / TURNS_PER_SECOND

TAIL_L = "TailFeather_L"
TAIL_R = "TailFeather_R"


def lay_values(t, length=LAY_LENGTH):
    u = t / length
    sit = ease(0.0, 0.2, u) - ease(0.74, 0.92, u)
    fluff = ease(0.22, 0.36, u) - ease(0.62, 0.74, u)
    shiver = math.sin(u * math.pi * 2.0 * 9.0) * bump(u, 0.26, 0.66)
    proud = bump(u, 0.8, 1.0)
    return u, sit, fluff, shiver, proud


def lay_pose(t, length=LAY_LENGTH):
    u, sit, fluff, shiver, proud = lay_values(t, length)
    spec = {
        "Hip": {"rot": [(X, 5.0 * sit - 6.0 * proud, HIP), (Y, 4.0 * shiver, HIP)], "move": (0, 0, -0.05 * sit)},
        "Leg_L": {"move": (0, 0, 0.045 * sit)},
        "Leg_R": {"move": (0, 0, 0.045 * sit)},
        "Wishbone": {"rot": [(X, 8.0 * sit - 22.0 * proud, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, 10.0 * sit - 16.0 * proud, NECK)]},
        "Head": {"rot": [(X, 6.0 * sit - 12.0 * proud, HEADP), (Z, 5.0 * math.sin(u * 14.0) * sit, HEADP)]},
        TAIL_L: {"rot": [(Y, -10.0 * sit - 4.0 * shiver, HIP)]},
        TAIL_R: {"rot": [(Y, 10.0 * sit + 4.0 * shiver, HIP)]},
    }
    wings(spec, 16.0 * sit + 28.0 * fluff + 11.0 * shiver + 52.0 * proud * abs(math.sin(u * math.pi * 10.0)))
    return spec


def flutter_values(t, length=0.8):
    u = t / length
    env = ease(0.0, 0.12, u) - ease(0.82, 1.0, u)
    lift = math.sin(math.pi * min(1.0, u * 1.05)) ** 0.7
    flap = 0.5 + 0.5 * math.sin(u * math.pi * 2.0 * 5.0 - math.pi / 2.0)
    return u, env, lift, flap


def flutter_pose(t, length=0.8):
    u, env, lift, flap = flutter_values(t, length)
    spec = {
        "Root": {"move": (0, 0, 0.075 * lift)},
        "Hip": {"rot": [(X, -14.0 * env, HIP)]},
        "Wishbone": {"rot": [(X, -8.0 * env, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, -8.0 * env, NECK)]},
        "Head": {"rot": [(X, -8.0 * env + 4.0 * math.sin(u * 22.0) * env, HEADP)]},
        "Leg_L": {"rot": [(X, 28.0 * env, (0.0, 0.0, 0.07))]},
        "Leg_R": {"rot": [(X, 22.0 * env, (0.0, 0.0, 0.07))]},
        TAIL_L: {"rot": [(Y, -8.0 * env, HIP)]},
        TAIL_R: {"rot": [(Y, 8.0 * env, HIP)]},
    }
    wings(spec, env * (-8.0 + 78.0 * flap))
    return spec


def build_clip_scaled(rig, name, length, pose_fn, scale_fn, step=1.0 / 24.0):
    """Like fk.build_clip, plus a uniform scale on the Hip bone given by scale_fn(t)."""
    arm = rig.arm
    old = bpy.data.actions.get(name)
    if old is not None:
        bpy.data.actions.remove(old)
    act = oo.new_action(arm, name, length)
    times = []
    t = 0.0
    while t < length - 1e-6:
        times.append(t)
        t += step
    times.append(length)
    last = {}
    for t in times:
        res = rig.solve(pose_fn(t))
        for n in rig.order:
            T, Q = res[n]
            if n in last and last[n].dot(Q) < 0:
                Q = Quaternion((-Q.w, -Q.x, -Q.y, -Q.z))
            last[n] = Q
            s = scale_fn(t) if n == "Hip" else 1.0
            oo.add_pose_key(arm, act, n, t, loc=T, rot=Q, scale=(s, s, s))
    act.use_fake_user = True
    return act


def make(arm):
    rig = fk.Rig(arm)
    lay = build_clip_scaled(rig, "Lay", LAY_LENGTH, lay_pose, lambda t: 1.0 + 0.13 * lay_values(t)[2])
    flutter = build_clip_scaled(rig, "Flutter", 0.8, flutter_pose, lambda t: 1.0)
    return rig, [lay, flutter]
