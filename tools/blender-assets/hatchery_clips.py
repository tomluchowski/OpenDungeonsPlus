# Builds the hatchery animal clips Peep, Run, Crow and Hatch for the chicken skeleton in Blender.
# Poses are authored with odp_fk (world axes: x sideways, y backwards, z up, the animal looks along -y;
# a positive rotation around x tips the head down), relative to the first frame of the hen Idle clip.
import math
import bpy
from mathutils import Quaternion, Vector
import odp_ogre_io as oo
import odp_fk as fk

X = (1, 0, 0)
Y = (0, 1, 0)
Z = (0, 0, 1)
ease = fk.ease
HIP = (0.0, 0.026, 0.092)
SHOULDER = (0.0, 0.03, 0.10)
NECK = (0.0, -0.06, 0.139)
HEADP = (0.0, -0.095, 0.139)


def bump(u, a, b):
    if u <= a or u >= b:
        return 0.0
    return math.sin(math.pi * (u - a) / (b - a))


def wings(spec, raise_deg):
    """Raise (positive) or fold (negative) both wings around the forward axis."""
    spec["Shoulder_L"] = {"rot": [(Y, -raise_deg, SHOULDER)]}
    spec["Shoulder_R"] = {"rot": [(Y, raise_deg, SHOULDER)]}


def peep_pose(t, length=1.2):
    u = t / length
    p1 = bump(u, 0.05, 0.25)
    p2 = bump(u, 0.32, 0.52)
    peep = max(p1, p2)
    spec = {
        "Wishbone": {"rot": [(X, -14.0 * peep, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, -10.0 * peep, NECK)]},
        "Head": {"rot": [(X, -24.0 * peep, HEADP)]},
        "Hip": {"move": (0, 0, 0.006 * peep)},
    }
    flutter = peep * (0.5 + 0.5 * math.sin(u * 60.0))
    wings(spec, 14.0 * flutter)
    return spec


def crow_pose(t, length=2.0):
    u = t / length
    up = ease(0.0, 0.22, u) - ease(0.82, 1.0, u)
    call = bump(u, 0.3, 0.78)
    beat = abs(math.sin(u * math.pi * 5.0)) * bump(u, 0.28, 0.8)
    spec = {
        "Hip": {"rot": [(X, -12.0 * up, HIP)], "move": (0, 0, 0.012 * up)},
        "Wishbone": {"rot": [(X, -26.0 * up - 6.0 * call, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, -20.0 * up - 6.0 * call, NECK)]},
        "Head": {"rot": [(X, -22.0 * up - 16.0 * call, HEADP)]},
    }
    wings(spec, 8.0 * up + 52.0 * beat)
    spec["TailFeather_L"] = {"rot": [(Y, -6.0 * up, HIP)]}
    spec["TailFeather_R"] = {"rot": [(Y, 6.0 * up, HIP)]}
    return spec


def hatch_pose(t, length=1.6):
    u = t / length
    curl = 1.0 - ease(0.42, 0.82, u)
    shake = math.sin(u * math.pi * 2.0 * 5.0) * curl * ease(0.0, 0.08, u)
    pop = bump(u, 0.66, 0.95)
    spec = {
        "Hip": {"rot": [(X, 8.0 * curl, HIP), (Y, 7.0 * shake, HIP)], "move": (0, 0, -0.006 * curl)},
        "Wishbone": {"rot": [(X, 14.0 * curl - 8.0 * pop, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, 24.0 * curl - 12.0 * pop, NECK)]},
        "Head": {"rot": [(X, 40.0 * curl - 22.0 * pop + 9.0 * shake, HEADP)]},
    }
    wings(spec, -22.0 * curl + 34.0 * pop * abs(math.sin(u * math.pi * 7.0)))
    return spec


def compress_walk_into_run(arm, rig, name, length):
    """Run = hen Walk played faster, leaning forward with the wings spread."""
    walk = bpy.data.actions["hen_Walk"]
    old = bpy.data.actions.get(name)
    if old is not None:
        bpy.data.actions.remove(old)
    act = oo.new_action(arm, name, length)
    factor = length / float(walk["ogre_length"])
    lean = rig.solve({
        "Hip": {"rot": [(X, 7.0, HIP)]},
        "Wishbone": {"rot": [(X, 6.0, (0.0, 0.0, 0.10))]},
        "Shoulder_L": {"rot": [(Y, -24.0, SHOULDER)]},
        "Shoulder_R": {"rot": [(Y, 24.0, SHOULDER)]},
    })
    for bone, keys in oo._action_tracks(arm, walk):
        last = None
        for (t, T, Q, _S) in keys:
            dT, dQ = lean.get(bone, (Vector((0, 0, 0)), Quaternion((1, 0, 0, 0))))
            Q2 = Q @ dQ
            if last is not None and last.dot(Q2) < 0:
                Q2 = Quaternion((-Q2.w, -Q2.x, -Q2.y, -Q2.z))
            last = Q2
            oo.add_pose_key(arm, act, bone, min(t * factor, length), loc=T + dT, rot=Q2)
    return act


def make(arm):
    rig = fk.Rig(arm)
    acts = [
        fk.build_clip(rig, "Peep", 1.2, peep_pose),
        fk.build_clip(rig, "Crow", 2.0, crow_pose),
        fk.build_clip(rig, "Hatch", 1.6, hatch_pose),
        compress_walk_into_run(arm, rig, "Run", 0.55),
    ]
    return rig, acts
