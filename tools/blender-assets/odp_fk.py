# Forward kinematics helper for authoring Ogre clips in Blender with world-axis pose specs.
import math
import bpy
from mathutils import Matrix, Quaternion, Vector
import odp_ogre_io as oo


class Rig:
    def __init__(self, arm, base_action=None, base_time=0.0):
        self.arm = arm
        self.order = list(arm["ogre_bone_order"])
        self.rest = {}
        self.parent = {}
        for n in self.order:
            b = arm.data.bones[n]
            self.parent[n] = b.parent.name if b.parent else None
            loc = (b.parent.matrix_local.inverted() @ b.matrix_local) if b.parent else b.matrix_local
            self.rest[n] = (loc.to_translation(), loc.to_quaternion())
        self.base = {n: (Vector((0, 0, 0)), Quaternion((1, 0, 0, 0))) for n in self.order}
        if base_action is not None:
            for bname, keys in oo._action_tracks(arm, bpy.data.actions[base_action]):
                best = min(keys, key=lambda k: abs(k[0] - base_time))
                self.base[bname] = (best[1], best[2])
        # topological order
        self.topo = []
        seen = set()

        def visit(n):
            if n in seen:
                return
            p = self.parent[n]
            if p:
                visit(p)
            seen.add(n)
            self.topo.append(n)
        for n in self.order:
            visit(n)

    def solve(self, spec):
        """spec: bone -> {"rot": [(axis, degrees, pivot or None)], "move": (x, y, z)}.
        Returns bone -> (keyT, keyQ) (Ogre keyframe values)."""
        W = {}
        out = {}
        for n in self.topo:
            p = self.parent[n]
            Wp = W[p] if p else Matrix.Identity(4)
            pos, q = self.rest[n]
            bT, bQ = self.base[n]
            base_world = Wp @ (Matrix.Translation(pos + bT) @ (q @ bQ).to_matrix().to_4x4())
            des = base_world
            s = spec.get(n)
            if s:
                for item in s.get("rot", []):
                    axis, deg = item[0], item[1]
                    pivot = item[2] if len(item) > 2 else None
                    pv = Vector(pivot) if pivot is not None else base_world.translation
                    R = Matrix.Rotation(math.radians(deg), 4, Vector(axis).normalized())
                    des = Matrix.Translation(pv) @ R @ Matrix.Translation(-pv) @ des
                if "move" in s:
                    des = Matrix.Translation(Vector(s["move"])) @ des
            W[n] = des
            local = Wp.inverted() @ des
            keyT = local.to_translation() - pos
            keyQ = q.inverted() @ local.to_quaternion()
            out[n] = (keyT, keyQ)
        return out


def build_clip(rig, name, length, pose_fn, step=1.0 / 12.0, bones=None):
    """pose_fn(t) -> spec. Samples at multiples of step (and at length). Returns the action."""
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
    keyed = bones if bones is not None else rig.order
    last = {}
    for t in times:
        res = rig.solve(pose_fn(t))
        for n in keyed:
            T, Q = res[n]
            if n in last and last[n].dot(Q) < 0:
                Q = Quaternion((-Q.w, -Q.x, -Q.y, -Q.z))
            last[n] = Q
            oo.add_pose_key(arm, act, n, t, loc=T, rot=Q)
    return act


def ease(a, b, t):
    if t <= a:
        return 0.0
    if t >= b:
        return 1.0
    x = (t - a) / (b - a)
    return x * x * (3 - 2 * x)
