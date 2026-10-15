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
        "Wishbone": {"rot": [(X, -22.0 * peep, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, -16.0 * peep, NECK)]},
        "Head": {"rot": [(X, -38.0 * peep, HEADP)]},
        "Hip": {"move": (0, 0, 0.014 * peep)},
    }
    flutter = peep * (0.5 + 0.5 * math.sin(u * 60.0))
    wings(spec, 30.0 * flutter)
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
    wings(spec, 14.0 * up + 80.0 * beat)
    spec["TailFeather_L"] = {"rot": [(Y, -6.0 * up, HIP)]}
    spec["TailFeather_R"] = {"rot": [(Y, 6.0 * up, HIP)]}
    return spec


def hatch_pose(t, length=1.6):
    u = t / length
    curl = 1.0 - ease(0.42, 0.82, u)
    shake = math.sin(u * math.pi * 2.0 * 5.0) * curl * ease(0.0, 0.08, u)
    pop = bump(u, 0.66, 0.95)
    spec = {
        "Hip": {"rot": [(X, 10.0 * curl, HIP), (Y, 11.0 * shake, HIP)], "move": (0, 0, -0.008 * curl + 0.012 * pop)},
        "Wishbone": {"rot": [(X, 18.0 * curl - 12.0 * pop, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, 30.0 * curl - 18.0 * pop, NECK)]},
        "Head": {"rot": [(X, 48.0 * curl - 30.0 * pop + 12.0 * shake, HEADP)]},
    }
    wings(spec, -30.0 * curl + 74.0 * pop * abs(math.sin(u * math.pi * 7.0)))
    return spec


def compress_walk_into_run(arm, rig, name, length):
    """Run = hen Walk played faster, leaning forward with the wings spread."""
    walk = bpy.data.actions.get("hen_Walk") or bpy.data.actions["Walk"]
    old = bpy.data.actions.get(name)
    if old is not None:
        bpy.data.actions.remove(old)
    act = oo.new_action(arm, name, length)
    factor = length / float(walk["ogre_length"])
    lean = rig.solve({
        "Hip": {"rot": [(X, 11.0, HIP)]},
        "Wishbone": {"rot": [(X, 9.0, (0.0, 0.0, 0.10))]},
        "Shoulder_L": {"rot": [(Y, -42.0, SHOULDER)]},
        "Shoulder_R": {"rot": [(Y, 42.0, SHOULDER)]},
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


# ------------------------------------------------------------------ mating clips (hen Duck, rooster Mount, Tread,
# Dismount and the whole sequence MountCycle). Authored relative to the first frame of the Idle clip, so they start
# and end in the posture of the standing animal. The client moves the rooster over the hen (lift, shift, pitch), the
# clips only add the bones: ducking, climbing, treading with the legs, beating the wings and climbing down.
LEG_L = (0.024, 0.026, 0.067)
LEG_R = (-0.024, 0.026, 0.067)
TAIL_L = "TailFeather_L"
TAIL_R = "TailFeather_R"
MOUNT_SECONDS = 0.4
TREAD_SECONDS = 0.55
DISMOUNT_SECONDS = 0.4
DUCK_SECONDS = 1.9
TREAD_CYCLES = 2
# The posture the rooster holds on the hen (the mean of Tread), where Mount ends and Dismount starts
HOLD_LEG = -34.0
HOLD_WING = 48.0
HOLD_HEAD = 12.0


def legs(spec, left, right):
    """Rotate the legs around the hip joint: negative reaches forward (the animal looks along -y)."""
    spec["Leg_L"] = {"rot": [(X, left, LEG_L)]}
    spec["Leg_R"] = {"rot": [(X, right, LEG_R)]}


def duck_pose(t, length=DUCK_SECONDS):
    """Hen under the rooster: sinks down with the neck stretched forward, the wings hang out and the body quivers."""
    u = t / length
    env = ease(0.0, 0.21, u) - ease(0.79, 1.0, u)
    quiver = math.sin(u * math.pi * 2.0 * 11.0) * bump(u, 0.12, 0.88)
    spec = {
        "Hip": {"rot": [(X, 9.0 * env, HIP), (Y, 6.0 * quiver, HIP)], "move": (0, 0, -0.05 * env + 0.004 * quiver)},
        "Leg_L": {"move": (0, 0, 0.044 * env)},
        "Leg_R": {"move": (0, 0, 0.044 * env)},
        "Wishbone": {"rot": [(X, 16.0 * env, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, 22.0 * env, NECK)]},
        "Head": {"rot": [(X, 14.0 * env + 5.0 * quiver, HEADP), (Z, 7.0 * quiver, HEADP)]},
        TAIL_L: {"rot": [(Y, -22.0 * env - 9.0 * quiver, HIP)]},
        TAIL_R: {"rot": [(Y, 22.0 * env + 9.0 * quiver, HIP)]},
    }
    wings(spec, 26.0 * env + 12.0 * quiver)
    return spec


def mount_pose(t, length=MOUNT_SECONDS):
    """Rooster: crouches, springs up with the wings raised and lands with the legs reaching forward."""
    u = t / length
    crouch = ease(0.0, 0.25, u) - ease(0.25, 0.5, u)
    air = bump(u, 0.3, 1.0)
    reach = ease(0.35, 0.85, u)
    spec = {
        "Hip": {"rot": [(X, 14.0 * crouch - 16.0 * air + 4.0 * reach, HIP)], "move": (0, 0, -0.035 * crouch + 0.03 * air)},
        "Wishbone": {"rot": [(X, 8.0 * crouch - 14.0 * air + 6.0 * reach, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, 6.0 * crouch - 12.0 * air + 6.0 * reach, NECK)]},
        "Head": {"rot": [(X, 6.0 * crouch - 16.0 * air + HOLD_HEAD * reach, HEADP)]},
        TAIL_L: {"rot": [(Y, -24.0 * air, HIP)]},
        TAIL_R: {"rot": [(Y, 24.0 * air, HIP)]},
    }
    legs(spec, HOLD_LEG * reach - 12.0 * crouch, HOLD_LEG * reach - 12.0 * crouch)
    spec["Leg_L"]["move"] = (0, 0, 0.03 * crouch)
    spec["Leg_R"]["move"] = (0, 0, 0.03 * crouch)
    wings(spec, HOLD_WING * ease(0.25, 0.6, u) + 40.0 * bump(u, 0.3, 0.8))
    return spec


def tread_pose(t, length=TREAD_SECONDS):
    """Rooster on the hen: the legs tread in turn, the wings beat twice in every step cycle, the head pecks."""
    u = t / length
    step = math.sin(u * math.pi * 2.0)
    beat = math.sin(u * math.pi * 2.0 * 2.0)
    spec = {
        "Hip": {"rot": [(X, 4.0, HIP), (Y, 5.0 * step, HIP)], "move": (0, 0, 0.007 * beat)},
        "Wishbone": {"rot": [(X, 6.0 + 3.0 * beat, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, 6.0 + 5.0 * beat, NECK)]},
        "Head": {"rot": [(X, HOLD_HEAD + 12.0 * beat, HEADP)]},
        TAIL_L: {"rot": [(Y, -10.0 + 6.0 * step, HIP)]},
        TAIL_R: {"rot": [(Y, 10.0 + 6.0 * step, HIP)]},
    }
    legs(spec, HOLD_LEG + 30.0 * step, HOLD_LEG - 30.0 * step)
    wings(spec, HOLD_WING + 46.0 * beat)
    return spec


def dismount_pose(t, length=DISMOUNT_SECONDS):
    """Rooster: one last flap, pushes off and lands in the standing posture."""
    u = t / length
    hold = 1.0 - ease(0.0, 0.7, u)
    push = bump(u, 0.05, 0.6)
    flap = bump(u, 0.0, 0.7)
    settle = bump(u, 0.7, 1.0)
    spec = {
        "Hip": {"rot": [(X, 4.0 * hold - 6.0 * push + 5.0 * settle, HIP)], "move": (0, 0, 0.03 * push - 0.02 * settle)},
        "Wishbone": {"rot": [(X, 6.0 * hold - 8.0 * push, (0.0, 0.0, 0.10))]},
        "NeckBone": {"rot": [(X, 6.0 * hold - 8.0 * push, NECK)]},
        "Head": {"rot": [(X, HOLD_HEAD * hold - 16.0 * push, HEADP)]},
        TAIL_L: {"rot": [(Y, -10.0 * hold - 14.0 * push, HIP)]},
        TAIL_R: {"rot": [(Y, 10.0 * hold + 14.0 * push, HIP)]},
    }
    legs(spec, HOLD_LEG * hold + 10.0 * settle, HOLD_LEG * hold + 10.0 * settle)
    spec["Leg_L"]["move"] = (0, 0, 0.025 * settle)
    spec["Leg_R"]["move"] = (0, 0, 0.025 * settle)
    wings(spec, HOLD_WING * hold + 44.0 * flap)
    return spec


def mount_cycle_pose(t, length=MOUNT_SECONDS + TREAD_SECONDS * TREAD_CYCLES + DISMOUNT_SECONDS):
    """The whole sequence in one clip: Mount, Tread (two cycles), Dismount (1.9 s with the default client timing)."""
    if t < MOUNT_SECONDS:
        return mount_pose(t)
    t -= MOUNT_SECONDS
    if t < TREAD_SECONDS * TREAD_CYCLES - 1e-6:
        return tread_pose(t % TREAD_SECONDS)
    return dismount_pose(min(t - TREAD_SECONDS * TREAD_CYCLES, DISMOUNT_SECONDS))


def make_mating(arm, rooster_only=False):
    """Builds the mating clips on top of the first frame of Idle (the arm needs the Idle action)."""
    rig = fk.Rig(arm, base_action="Idle", base_time=1.0 / 24.0)
    step = 1.0 / 24.0
    acts = []
    if not rooster_only:
        acts.append(fk.build_clip(rig, "Duck", DUCK_SECONDS, duck_pose, step))
    acts.append(fk.build_clip(rig, "Mount", MOUNT_SECONDS, mount_pose, step))
    acts.append(fk.build_clip(rig, "Tread", TREAD_SECONDS, tread_pose, step))
    acts.append(fk.build_clip(rig, "Dismount", DISMOUNT_SECONDS, dismount_pose, step))
    acts.append(fk.build_clip(rig, "MountCycle", MOUNT_SECONDS + TREAD_SECONDS * TREAD_CYCLES + DISMOUNT_SECONDS,
                              mount_cycle_pose, step))
    for act in acts:
        act.use_fake_user = True
    return rig, acts


def make(arm):
    rig = fk.Rig(arm)
    acts = [
        fk.build_clip(rig, "Peep", 1.2, peep_pose),
        fk.build_clip(rig, "Crow", 2.0, crow_pose),
        fk.build_clip(rig, "Hatch", 1.6, hatch_pose),
        compress_walk_into_run(arm, rig, "Run", 0.55),
    ]
    return rig, acts
