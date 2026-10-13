#!/usr/bin/env python3
"""Adds the clip WalkHurt (a limping, hobbling walk) to creature skeletons given as Ogre XML.

WalkHurt is derived from the Walk clip of the same skeleton: same length, every track is sampled at the key
times of Walk (their union), and every track ends on its own first key, so that the clip loops.
The changes are authored per body plan (see CONFIG at the bottom):

  biped    the injured left leg swings less, the body dips and leans over the injured side when that foot is
           on the ground, the upper body bends forward, the head is lowered, the left hand is pressed to the
           side (not for creatures that carry a weapon or shield in that hand), the other arm hangs
  quad     the injured front limb is favoured (little swing, held up), body and head low, tail low
  multi    insects and spiders: body lowered, one limb favoured, the next one swings less
  tentacle serpent and tentacle bodies: low, bent forward, one front limb favoured
  torso    body without legs: lowered, bent forward, arms hang

Usage (from the repository root, skeletons converted with OgreXMLConverter first):

    python tools/gen_walk_hurt.py <input dir with Name.skeleton.xml> <output dir> [Name ...]

The output files contain the whole skeleton with the new clip appended; nothing else in them is touched.
"""
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

CLIP = "WalkHurt"
EPS = 1e-9


# ---------------------------------------------------------------- small vector and quaternion maths

def vadd(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def vsub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def vscale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def vdot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def vcross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def vlen(a):
    return math.sqrt(vdot(a, a))


def vnorm(a):
    n = vlen(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > EPS else (0.0, 0.0, 1.0)


QI = (1.0, 0.0, 0.0, 0.0)


def qmul(a, b):
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    return (w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
            w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
            w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
            w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2)


def qinv(q):
    return (q[0], -q[1], -q[2], -q[3])


def qrot(q, v):
    r = qmul(qmul(q, (0.0, v[0], v[1], v[2])), qinv(q))
    return (r[1], r[2], r[3])


def qaxis(axis, angle):
    a = vnorm(axis)
    h = angle / 2.0
    s = math.sin(h)
    return (math.cos(h), a[0] * s, a[1] * s, a[2] * s)


def qnorm(q):
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q) if n > EPS else QI


def qdot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]


def qalign(q, ref):
    return q if qdot(q, ref) >= 0 else (-q[0], -q[1], -q[2], -q[3])


def qnlerp(a, b, u):
    b = qalign(b, a)
    return qnorm(tuple(a[i] * (1 - u) + b[i] * u for i in range(4)))


def qbetween(a, b):
    """Shortest rotation that turns the unit vector a onto the unit vector b."""
    d = vdot(a, b)
    if d > 1 - 1e-12:
        return QI
    if d < -1 + 1e-12:
        ax = vcross(a, (1.0, 0.0, 0.0))
        if vlen(ax) < 1e-6:
            ax = vcross(a, (0.0, 1.0, 0.0))
        return qaxis(ax, math.pi)
    c = vcross(a, b)
    return qnorm((1.0 + d, c[0], c[1], c[2]))


def smooth(u):
    u = max(0.0, min(1.0, u))
    return u * u * (3 - 2 * u)


# ---------------------------------------------------------------- skeleton

class Skeleton(object):
    def __init__(self, path):
        self.path = path
        self.text = open(path, encoding="utf-8").read()
        root = ET.fromstring(self.text)
        self.order = []
        self.pos = {}
        self.q = {}
        self.scale = {}
        self.parent = {}
        for b in root.find("bones").findall("bone"):
            n = b.get("name")
            p = b.find("position")
            self.pos[n] = (float(p.get("x")), float(p.get("y")), float(p.get("z")))
            r = b.find("rotation")
            q = QI
            if r is not None and abs(float(r.get("angle"))) > 1e-9:
                ax = r.find("axis")
                q = qaxis((float(ax.get("x")), float(ax.get("y")), float(ax.get("z"))), float(r.get("angle")))
            self.q[n] = q
            sc = b.find("scale")
            self.scale[n] = float(sc.get("x")) if sc is not None else 1.0
            self.order.append(n)
            self.parent[n] = None
        for bp in root.find("bonehierarchy").findall("boneparent"):
            self.parent[bp.get("bone")] = bp.get("parent")
        self.children = dict((n, []) for n in self.order)
        for n in self.order:
            if self.parent[n]:
                self.children[self.parent[n]].append(n)
        self.WQ = {}
        self.WP = {}
        self.SC = {}
        for n in self.order:
            self._world(n)
        self.topo = sorted(self.order, key=lambda n: len(self.ancestors(n)))
        self.clips = {}
        for a in root.find("animations").findall("animation"):
            tracks = {}
            for t in a.find("tracks").findall("track"):
                keys = []
                for k in t.find("keyframes").findall("keyframe"):
                    tt = k.find("translate")
                    ro = k.find("rotate")
                    ax = ro.find("axis")
                    ang = float(ro.get("angle"))
                    q = QI
                    if abs(ang) > 1e-9:
                        q = qaxis((float(ax.get("x")), float(ax.get("y")), float(ax.get("z"))), ang)
                    keys.append((float(k.get("time")),
                                 (float(tt.get("x")), float(tt.get("y")), float(tt.get("z"))), q))
                tracks[t.get("bone")] = keys
            self.clips[a.get("name")] = (float(a.get("length")), tracks)

    def _world(self, n):
        if n in self.WQ:
            return
        p = self.parent[n]
        if p:
            self._world(p)
            self.WP[n] = vadd(self.WP[p], qrot(self.WQ[p], vscale(self.pos[n], self.SC[p])))
            self.WQ[n] = qmul(self.WQ[p], self.q[n])
            self.SC[n] = self.SC[p] * self.scale[n]
        else:
            self.WP[n] = self.pos[n]
            self.WQ[n] = self.q[n]
            self.SC[n] = self.scale[n]

    def ancestors(self, n):
        out = []
        while self.parent[n]:
            n = self.parent[n]
            out.append(n)
        return out

    def descendants(self, n):
        out = [n]
        for c in self.children[n]:
            out += self.descendants(c)
        return out

    def path_between(self, top, bottom):
        """Bones strictly between top (an ancestor) and bottom, from the top down."""
        chain = []
        n = self.parent[bottom]
        while n and n != top:
            chain.append(n)
            n = self.parent[n]
        return list(reversed(chain))

    def common_ancestor(self, a, b):
        la = [a] + self.ancestors(a)
        lb = set([b] + self.ancestors(b))
        for n in la:
            if n in lb:
                return n
        return None

    def rest_dir(self, a, b):
        return vnorm(vsub(self.WP[b], self.WP[a]))

    def local_rot(self, n, ep, ei):
        """Key rotation of bone n so that its world orientation is changed by ei when the parent's is changed by ep."""
        r = self.WQ[n]
        return qmul(qmul(qinv(r), qmul(qinv(ep), ei)), r)

    def frame_rot(self, n, rworld):
        """The key rotation that applies the world rotation rworld (about the bone's own pivot) to bone n."""
        r = self.WQ[n]
        return qmul(qmul(qinv(r), rworld), r)

    def local_trans(self, n, vworld):
        """Key translation that moves bone n by the world vector vworld."""
        p = self.parent[n]
        if not p:
            return vworld
        return vscale(qrot(qinv(self.WQ[p]), vworld), 1.0 / self.SC[p])


# ---------------------------------------------------------------- sampling the Walk clip

def interp(keys, t):
    if t <= keys[0][0]:
        return keys[0][1], keys[0][2]
    if t >= keys[-1][0]:
        return keys[-1][1], keys[-1][2]
    for k0, k1 in zip(keys, keys[1:]):
        if k0[0] <= t <= k1[0]:
            d = k1[0] - k0[0]
            u = 0.0 if d < EPS else (t - k0[0]) / d
            return (vadd(vscale(k0[1], 1 - u), vscale(k1[1], u)), qnlerp(k0[2], k1[2], u))
    return keys[-1][1], keys[-1][2]


def time_grid(tracks, length):
    ts = set([0.0, round(length, 5)])
    for keys in tracks.values():
        for k in keys:
            if k[0] <= length + 1e-6:
                ts.add(round(k[0], 5))
    out = []
    for t in sorted(ts):
        if not out or t - out[-1] > 2e-4:
            out.append(t)
    return out


def mean_q(qs):
    acc = (0.0, 0.0, 0.0, 0.0)
    ref = qs[0]
    for q in qs:
        q = qalign(q, ref)
        acc = tuple(acc[i] + q[i] for i in range(4))
    return qnorm(acc)


# ---------------------------------------------------------------- the pose being built

class Pose(object):
    """Rotation Q and translation T of every bone at every grid time (key semantics of the Ogre clip)."""

    def __init__(self, S, tracks, grid):
        self.S = S
        self.grid = grid
        self.n = len(grid)
        self.Q = {}
        self.T = {}
        self.base_tracked = set(tracks)
        for b in S.order:
            if b in tracks:
                vals = [interp(tracks[b], t) for t in grid]
                self.T[b] = [v[0] for v in vals]
                self.Q[b] = [v[1] for v in vals]
            else:
                self.T[b] = [(0.0, 0.0, 0.0)] * self.n
                self.Q[b] = [QI] * self.n
        self.touched = set(tracks)

    def fk(self, i):
        """World positions and rotations of all bones at grid index i."""
        S = self.S
        pos, rot, sc = {}, {}, {}
        for n in S.topo:
            p = S.parent[n]
            local = vadd(S.pos[n], self.T[n][i])
            lq = qmul(S.q[n], self.Q[n][i])
            if p:
                pos[n] = vadd(pos[p], qrot(rot[p], vscale(local, sc[p])))
                rot[n] = qmul(rot[p], lq)
                sc[n] = sc[p] * S.scale[n]
            else:
                pos[n], rot[n], sc[n] = local, lq, S.scale[n]
        return pos, rot

    def rotate_world(self, bone, axis, angle, i):
        if abs(angle) < 1e-12:
            return
        d = self.S.frame_rot(bone, qaxis(axis, angle))
        self.Q[bone][i] = qmul(d, self.Q[bone][i])
        self.touched.add(bone)

    def move_world(self, bone, v, i):
        d = self.S.local_trans(bone, v)
        self.T[bone][i] = vadd(self.T[bone][i], d)
        self.touched.add(bone)

    def damp(self, bones, k):
        """Pull the tracks of the bones towards their own mean: k = 1 keeps the swing, k = 0 holds the mean pose."""
        for b in bones:
            if b not in self.base_tracked:
                continue
            m = max(1, self.n - 1)
            qm = mean_q(self.Q[b][:m])
            tm = tuple(sum(self.T[b][i][a] for i in range(m)) / m for a in range(3))
            for i in range(self.n):
                self.Q[b][i] = qnlerp(qm, self.Q[b][i], k)
                self.T[b][i] = vadd(tm, vscale(vsub(self.T[b][i], tm), k))
            self.touched.add(b)


X_AXIS = (1.0, 0.0, 0.0)
Y_AXIS = (0.0, 1.0, 0.0)
Z_AXIS = (0.0, 0.0, 1.0)


def planted_signal(P, ref):
    """1 when the reference bone (a foot) is at its lowest, 0 at its highest; smoothed."""
    hs = []
    for i in range(P.n):
        pos, _rot = P.fk(i)
        hs.append(pos[ref][2])
    lo, hi = min(hs), max(hs)
    span = hi - lo
    if span < 1e-9:
        return [0.5] * P.n
    return [smooth(1.0 - (h - lo) / span) for h in hs]


# ---------------------------------------------------------------- the authored changes

def spine_lean(P, bones, total, i):
    if not bones:
        return
    for b in bones:
        P.rotate_world(b, X_AXIS, math.radians(total) / len(bones), i)


def spine_roll(P, bones, total, i):
    if not bones:
        return
    for b in bones:
        P.rotate_world(b, Y_AXIS, math.radians(total) / len(bones), i)


def lift_limb(P, chain, angles, i):
    """Raise the end of a limb: about the horizontal axis across the limb, or the X axis for a hanging limb."""
    S = P.S
    for b, a in zip(chain, angles):
        nxt = chain[chain.index(b) + 1] if chain.index(b) + 1 < len(chain) else None
        if nxt is None or abs(a) < 1e-9:
            continue
        d = S.rest_dir(b, nxt)
        if abs(d[2]) > 0.75:
            axis = X_AXIS
        else:
            axis = vnorm(vcross(d, Z_AXIS))
        P.rotate_world(b, axis, math.radians(a), i)


def side_sign(S, chain):
    return 1.0 if S.WP[chain[1]][0] > 0 else -1.0


def to_world(v, s):
    """(out, forward, up) of the creature to a model direction (forward is -Y)."""
    return vnorm((s * v[0], -v[1], v[2]))


def press_arm(P, chain, press, i, wob):
    """Replace the arm: aim upper arm and forearm at the given directions (out, fwd, up); the hand follows."""
    S = P.S
    pos, rot = P.fk(i)
    s = side_sign(S, chain)
    up, fo, ha = chain
    par = S.parent[up]
    ep = QI
    if par:
        ep = qmul(rot[par], qinv(S.WQ[par]))
    d0, d1 = press
    out = {}
    for bone, child, tgt, epv in ((up, fo, to_world(d0, s), ep), (fo, ha, to_world(d1, s), None)):
        if epv is None:
            epv = out[up]
        cur = qrot(epv, S.rest_dir(bone, child))
        e = qmul(qbetween(cur, tgt), epv)
        out[bone] = e
        P.Q[bone][i] = S.local_rot(bone, epv, e)
        P.touched.add(bone)
    # the hand follows the forearm
    P.Q[ha][i] = QI
    P.touched.add(ha)
    for dsc in S.descendants(up):
        if dsc not in chain:
            P.Q[dsc][i] = QI
            P.T[dsc][i] = (0.0, 0.0, 0.0)


def build(S, rigs, cfg, grid=None, skip=()):
    """Authors the limp on the Walk clip. grid: key times to sample (default: the union of the Walk key times);
    skip: bones of limbs that are planted feet solved elsewhere (fix_walk_hurt_feet.py): the swing damping and the
    lift of these limbs are left out, the body, spine, head, arm and tail changes stay. Returns the length, the grid,
    the pose and the unchanged Walk pose."""
    length, tracks = S.clips["Walk"]
    if grid is None:
        grid = time_grid(tracks, length)
    skip = set(skip)
    P = Pose(S, tracks, grid)
    base_pose = Pose(S, tracks, grid)
    for rig in rigs:
        for key, val in rig.items():
            names = []
            if key in ("body", "spine", "wings"):
                names = val
            elif key in ("hurt", "second", "hang", "tail"):
                names = [b for chain in val for b in chain]
            elif key in ("head", "phase", "press") and val:
                names = [val] if isinstance(val, str) else list(val)
            for b in names:
                assert b in S.pos, "unknown bone %s in %s" % (b, S.path)
    arm_bones = set()
    for rig in rigs:
        if rig.get("press"):
            arm_bones.update(S.descendants(rig["press"][0]))
    # pass 1: everything but the pressed arm
    for rig in rigs:
        kind = rig["kind"]
        ref = rig.get("phase") or rig["hurt"][0][-1]
        sig = planted_signal(base_pose, ref)
        c = dict(KIND_DEFAULTS[kind])
        c.update(rig.get("p", {}))
        leg_len = vlen(vsub(S.WP[rig["hurt"][0][0]], S.WP[rig["hurt"][0][-1]])) if rig.get("hurt") else 0.0
        body = rig["body"]
        spine = rig.get("spine", [])
        head = rig.get("head")
        # swing of the limbs
        for chain in rig.get("hurt", []):
            if not skip.intersection(chain):
                P.damp(chain, c["k_hurt"])
        for chain in rig.get("second", []):
            if not skip.intersection(chain):
                P.damp(chain, c["k_second"])
        for chain in rig.get("hang", []):
            P.damp(chain, c["k_arm"])
        for chain in rig.get("tail", []):
            P.damp(chain, 0.6)
        for chain in rig.get("wings", []):
            P.damp(chain, 0.55)
        for i in range(P.n):
            s = sig[i]
            for chain in rig.get("hurt", []):
                if c["lift"] and not skip.intersection(chain):
                    lift_limb(P, chain, c["lift"], i)
            for b in body:
                if skip and set(S.ancestors(b)).intersection(body):
                    continue    # a body bone below another one follows it
                P.move_world(b, (0.0, 0.0, -c["dip"] * leg_len * (1.0 - c.get("bob", 0.6) + c.get("bob", 0.6) * s)), i)
            spine_lean(P, spine, c["lean"] + 2.0 * s, i)
            spine_roll(P, spine, c["roll"] * (0.35 + 0.65 * s), i)
            if head:
                P.rotate_world(head, X_AXIS, math.radians(c["head"] + 4.0 * s), i)
                P.rotate_world(head, Y_AXIS, -math.radians(c["roll"] * 0.4 * (0.35 + 0.65 * s)), i)
            for chain in rig.get("tail", []):
                for b in chain:
                    P.rotate_world(b, X_AXIS, -math.radians(c["tail"]) / len(chain), i)
            for b in rig.get("wings", []):
                sg = 1.0 if S.WP[b][0] > 0 else -1.0
                P.rotate_world(b, Y_AXIS, sg * math.radians(c["wing"]), i)
    # pass 2: pressed arms use the posed parent
    for rig in rigs:
        if not rig.get("press"):
            continue
        c = dict(KIND_DEFAULTS[rig["kind"]])
        c.update(rig.get("p", {}))
        sig = planted_signal(base_pose, rig.get("phase") or rig["hurt"][0][-1])
        chain = rig["press"]
        for i in range(P.n):
            d0, d1 = c["press"]
            wob = sig[i]
            press_arm(P, chain, ((d0[0] + 0.06 * wob, d0[1], d0[2]), (d1[0], d1[1] + 0.05 * wob, d1[2])), i, wob)
    return length, grid, P, base_pose


KIND_DEFAULTS = {
    "biped": dict(k_hurt=0.55, k_second=1.0, k_arm=0.5, lift=None, dip=0.075, lean=9.0, roll=6.0, head=13.0,
                  tail=14.0, wing=14.0, press=((0.22, -0.10, -0.95), (-0.45, 0.62, -0.35))),
    "quad": dict(k_hurt=0.3, k_second=0.75, k_arm=1.0, lift=(18.0, 26.0, 16.0, 0.0), dip=0.12, lean=7.0, roll=5.0,
                 head=12.0, tail=16.0, wing=0.0),
    "multi": dict(k_hurt=0.3, k_second=0.7, k_arm=1.0, lift=(16.0, 18.0, 10.0, 0.0), dip=0.11, lean=5.0, roll=5.0,
                  head=9.0, tail=10.0, wing=12.0),
    "tentacle": dict(k_hurt=0.35, k_second=0.7, k_arm=1.0, lift=(14.0, 16.0, 8.0, 0.0), dip=0.20, lean=6.0, roll=5.0,
                     head=6.0, tail=10.0, wing=0.0),
    "torso": dict(k_hurt=0.5, k_second=1.0, k_arm=0.5, lift=None, dip=0.0, lean=12.0, roll=7.0, head=14.0,
                  tail=14.0, wing=0.0),
}


# ---------------------------------------------------------------- the clip as XML

def fmt(v):
    s = "%.7g" % v
    return "0" if s in ("-0", "0") else s


def closed_tracks(P, length, grid, S):
    """Every touched track on the grid, ending on its first key (the last quarter blends into it)."""
    out = []
    fade = 0.25 * length
    for b in S.order:
        if b not in P.touched:
            continue
        qs = list(P.Q[b])
        ts = list(P.T[b])
        for i, t in enumerate(grid):
            if t > length - fade:
                w = smooth((t - (length - fade)) / fade)
                qs[i] = qnlerp(qs[i], qs[0], w)
                ts[i] = vadd(vscale(ts[i], 1 - w), vscale(ts[0], w))
        qs[-1] = qs[0]
        ts[-1] = ts[0]
        out.append((b, [(grid[i], ts[i], qs[i]) for i in range(len(grid))]))
    return out


def anim_xml(name, length, tracks):
    out = ['\t\t<animation name="%s" length="%s">\n\t\t\t<tracks>\n' % (name, fmt(length))]
    for bone, keys in tracks:
        out.append('\t\t\t\t<track bone="%s">\n\t\t\t\t\t<keyframes>\n' % bone)
        prev = None
        for (t, tr, q) in keys:
            q = qnorm(q)
            if prev is not None and qdot(prev, q) < 0:
                q = (-q[0], -q[1], -q[2], -q[3])
            prev = q
            ang = 2.0 * math.acos(max(-1.0, min(1.0, q[0])))
            sn = math.sqrt(max(0.0, 1.0 - q[0] * q[0]))
            ax = (1.0, 0.0, 0.0) if sn < 1e-9 else (q[1] / sn, q[2] / sn, q[3] / sn)
            if ang < 1e-7:
                ang, ax = 0.0, (1.0, 0.0, 0.0)
            out.append('\t\t\t\t\t\t<keyframe time="%s">\n' % fmt(t))
            out.append('\t\t\t\t\t\t\t<translate x="%s" y="%s" z="%s" />\n' % (fmt(tr[0]), fmt(tr[1]), fmt(tr[2])))
            out.append('\t\t\t\t\t\t\t<rotate angle="%s">\n\t\t\t\t\t\t\t\t<axis x="%s" y="%s" z="%s" />\n\t\t\t\t\t\t\t</rotate>\n'
                       % (fmt(ang), fmt(ax[0]), fmt(ax[1]), fmt(ax[2])))
            out.append("\t\t\t\t\t\t</keyframe>\n")
        out.append("\t\t\t\t\t</keyframes>\n\t\t\t\t</track>\n")
    out.append("\t\t\t</tracks>\n\t\t</animation>\n")
    return "".join(out)


def append_clip(text, clip_xml):
    assert ('<animation name="%s"' % CLIP) not in text, "skeleton already has " + CLIP
    i = text.rindex("</animations>")
    return text[:i] + clip_xml + text[i:]


# ---------------------------------------------------------------- per skeleton configuration
# kind: body plan. body: bones that are moved down (the common parent of legs and spine); spine: bones that
# bend forward and to the injured side; head; hurt: limb chains of the injured (left, +X) side that swing
# less; second: limb chains that swing a little less; hang: arm chains that hang; press: the arm that is
# pressed to the side (None for creatures that hold a weapon or shield in that hand); phase: bone whose height
# tells when the injured side is on the ground; tail / wings: bones that droop.

def biped(S, body, head, leg, arm_l, arm_r, press=True, spine=None, phase=None, tail=None, wings=None, extra=None):
    if spine is None:
        top = S.common_ancestor(leg[0], head)
        spine = S.path_between(top, head)
    rig = dict(kind="biped", body=body, spine=spine, head=head, hurt=[leg], phase=phase, hang=[arm_r])
    if press:
        rig["press"] = arm_l
    else:
        rig["hang"] = [arm_l, arm_r]
    if tail:
        rig["tail"] = [tail]
    if wings:
        rig["wings"] = wings
    if extra:
        rig.update(extra)
    return rig


def lcabody(S, a, b):
    return [S.common_ancestor(a, b)]


def config(name, S):
    """Returns the list of rigs for the skeleton, or a string with the reason why it is skipped."""
    def L(a):
        return [a]

    def mir(chain):
        return [b.replace("_L", "_R").replace(".L", ".R").replace("_l", "_r").replace("Left", "Right") for b in chain]

    def hum(leg, head, armL, press=True, **kw):
        return [biped(S, lcabody(S, leg[0], head), head, leg, armL, mir(armL), press=press, **kw)]

    if name == "Adventurer":
        return hum(["Upperleg_L", "Lowerleg_L", "Feet_L"], "Head", ["Upperarm_L", "Forearm_L", "Hand_L"])
    if name == "Cultist":
        return hum(["thigh.L", "shin.L", "foot.L"], "head", ["upper_arm.L", "forearm.L", "hand.L"])
    if name == "DarkElf":
        return hum(["LeftUpLeg", "LeftLeg", "LeftFoot"], "Head", ["LeftArm", "LeftForeArm", "LeftHand"])
    if name == "Elf":
        return hum(["LeftUpLeg", "LeftLeg", "LeftFoot"], "Head", ["LeftArm", "LeftForeArm", "LeftHand"], press=False)
    if name == "Defender":
        return hum(["LeftUpLeg", "LeftLeg", "LeftFoot"], "Head", ["LeftArm", "LeftForeArm", "LeftHand"], press=False)
    if name == "Goblin":
        return hum(["LeftUpLeg", "LeftLeg", "LeftFoot"], "Head", ["LeftArm", "LeftForeArm", "LeftHand"])
    if name in ("Dwarf1", "Dwarf2"):
        return hum(["leg_l", "lowleg_l", "foot_l"], "head", ["arm_l", "forearm_l", "hand_l"])
    if name == "Gnome":
        return hum(["leg_l", "lowleg_l", "foot_l"], "head", ["arm_l", "forearm_l", "hand_l"])
    if name == "Wizard":
        return hum(["leg_l", "lowleg_l", "foot_l"], "head", ["arm_l", "forearm_l", "hand_l"])
    if name == "Orc":
        return hum(["leg_l", "lowleg_l", "foot_l"], "head", ["arm_l", "forearm_l", "hand_l"])
    if name == "Knight":
        return hum(["Leg_L", "Shin_L", "Foot_L"], "Head", ["Arm_L", "Forearm_L", "Hand_L"], press=False)
    if name == "Monk":
        return hum(["UpLeg_L", "Leg_L", "Foot_L"], "Head", ["Arm_L", "ForeArm_L", "Hand_L"], press=False)
    if name == "NatureMonster":
        return hum(["Thigh_L", "Leg_L"], "Head", ["Arm_L", "Forearm_L", "Hand_L"], press=False, phase="IK-Foot_L")
    if name == "RunelordDwarf":
        a = hum(["thigh.L", "shin.L", "tarsal.L"], "head", ["upper_arm.L", "forearm.L", "hand.L"], press=False)
        b = hum(["thigh.cr.L", "shin.cr.L", "tarsal.cr.L"], "head.cr", ["upper_arm.cr.L", "forearm.cr.L", "hand.cr.L"],
                press=False)
        return a + b
    if name == "Troll":
        return hum(["hip.L", "leg1.L", "leg2.L"], "head", ["shoulder.L", "upperhand.L", "arm.L"], press=False)
    if name == "Kobold":
        r = biped(S, ["TorsoLower", "TorsoUpper", "Sack"], "Head", ["LegUpper.L", "LegLower.L", "Foot.L"],
                  ["ArmUpper.L", "ArmLower.L", "Hand.L"], ["ArmUpper.R", "ArmLower.R", "Hand.R"],
                  spine=["TorsoUpper", "Neck"])
        return [r]
    if name == "Lizardman":
        r = biped(S, ["Pelvis", "Spine_0"], "Head", ["Leg_L", "Shin_L", "Shin_L.001", "Foot_L"],
                  ["Arm_L", "Forearm_L", "Hand_L"], ["Arm_R", "Forearm_R", "Hand_R"],
                  spine=["Spine_0", "Spine_1", "Spine_2", "Spine_3", "Spine_4"],
                  tail=["Tail_0", "Tail_1", "Tail_2", "Tail_3"])
        return [r]
    if name == "PitDemon":
        top = S.common_ancestor("Pelvis_L", "Head")
        r = biped(S, [top], "Head", ["Pelvis_L", "Leg_1_L", "Leg_2_L", "Shin_L", "Foot_L"],
                  ["Arm_L", "Forearm_L", "Hand_L"], ["Arm_R", "Forearm_R", "Hand_R"], press=False,
                  tail=["Tail_1", "Tail_2", "Tail_3", "Tail_4", "Tail_5", "Tail_6"],
                  wings=["Wing_1_L", "Wing_1_R"])
        return [r]
    if name == "Dragon":
        r = biped(S, ["COG_ROT"], "Head", ["Pelvis_L", "Leg_L", "Shin_L", "Foot_L"],
                  ["Arm_L", "Forearm_L", "Hand_L"], ["Arm_R", "Forearm_R", "Hand_R"], press=False,
                  tail=["Tail_1", "Tail_2", "Tail_3", "Tail_4", "Tail_5"], wings=["Wing_1_L", "Wing_1_R"])
        return [r]
    if name == "lich":
        r = biped(S, ["root"], "crown", ["hipLeft", "kneeLeft", "ankleLeft"],
                  ["shoulderLeft", "ellbowLeft", "wristLeft"], ["shoulderRight", "ellbowRight", "wristRight"],
                  spine=["pelvis", "spine1", "spine2", "spine3", "neckBase"])
        return [r]
    if name == "skeleton":
        r = biped(S, ["root"], "neck", ["hipLeft", "kneeLeft", "ankleLeft"],
                  ["shoulderJointLeft", "ellbowLeft", "wristLeft"], ["shoulderJointRight", "ellbowRight", "handJointRight"],
                  spine=["pelvis", "hip", "belly", "breast"])
        return [r]
    if name == "LavaSpawn":
        return [dict(kind="torso", body=["master"], spine=["spine", "chest"], head="head",
                     hurt=[["arm1.L", "arm2.L", "finger1.L"]], phase="head", hang=[["arm1.R", "arm2.R", "finger1.R"]],
                     tail=[["tail1", "tail2"]], p=dict(dip=0.06))]
    if name == "Kreatur":
        return [dict(kind="quad", body=["body", "body2"], spine=["body"], head="head",
                     hurt=[["foreleg1_l", "foreleg2_l", "foreleg3_l", "foreleg4_l"]],
                     second=[["hindleg1_r", "hindleg2_r", "hindleg3_r", "hindleg4_r"],
                             ["tentafr1_l", "tentafr2_l", "tentafr3_l", "tentafr4_l"]],
                     tail=[["tail1", "tail2"]])]
    if name == "Rat":
        return [dict(kind="quad", body=["Hip"], spine=["Backbone", "Backbone.001", "Backbone.002"], head="Head",
                     hurt=[["FrontLeg_L", "FrontLeg_L.001", "FrontLeg_L.002", "FrontLeg_L.003"]],
                     second=[["BackLeg_R", "BackLeg_R.001", "BackLeg_R.002", "BackLeg_R.003"]],
                     tail=[["Tail", "Tail.001", "Tail.002", "Tail.003"]])]
    if name == "Spider":
        return [dict(kind="multi", body=["Body.002"], spine=[], head=None,
                     hurt=[["v_B1_L", "v_B2_L", "v_B3_L"]], second=[["v2_B1_L", "v2_B2_L", "v2_B3_L"],
                                                                     ["v_B1_R", "v_B2_R", "v_B3_R"]],
                     phase="v_B3_L")]
    if name == "SmallSpider":
        return [dict(kind="multi", body=["body"], spine=[], head=None,
                     hurt=[["leg1-1_l", "leg1-2_l", "leg1-3_l"]], second=[["leg2-1_l", "leg2-2_l", "leg2-3_l"]])]
    if name == "Roach":
        return [dict(kind="multi", body=["MasterBone"], spine=["SpineHigh"], head="Head",
                     hurt=[["ArmL", "ForearmFrontL", "ClawFrontL"]],
                     second=[["LegL", "ForeLegL", "ClawBackL"], ["ArmR", "ForearmFrontR", "ClawFrontR"]],
                     tail=[["Tail", "Tail2", "Tail3", "TailEnd"]])]
    if name == "Scarab":
        return [dict(kind="multi", body=["Bone"], spine=[], head=None,
                     hurt=[["Bone_L", "Bone_L.001", "Bone_L.002", "Bone_L.003"]],
                     second=[["Bone_R", "Bone_R.001", "Bone_R.002", "Bone_R.003"]])]
    if name == "Tentacle":
        return [dict(kind="tentacle", body=["Root"], spine=["Body1", "Body2", "Body3"], head="Head",
                     hurt=[["ForeTentacle1_L", "ForeTentacle2_L", "ForeTentacle3_L"]],
                     second=[["HindTentacle1_L", "HindTentacle2_L", "HindTentacle3_L"]],
                     tail=[["Tail1", "Tail2", "Tail3"]], phase="ForeTentacle3_L")]
    if name == "Slime":
        return [dict(kind="tentacle", body=["slime_root"], spine=["slime_base", "slime_mid"], head="slime_head",
                     hurt=[["slime_base", "slime_mid", "slime_head"]], phase="slime_head",
                     p=dict(dip=0.2, k_hurt=0.8, lift=None, lean=12.0, roll=7.0, head=10.0))]
    if name == "Wyvern":
        return "Walk is a hover clip (the skeleton has no leg bones, the wings carry the motion)"
    if name == "CaveHornet":
        return "Walk is a hover clip (legs hang with 3 to 6 degrees of motion, the wings beat with 45 degrees)"
    return "no configuration"


ALL = ("Adventurer CaveHornet Cultist DarkElf Defender Dragon Dwarf1 Dwarf2 Elf Gnome Goblin Knight Kobold Kreatur "
       "LavaSpawn Lizardman Monk NatureMonster Orc PitDemon Rat Roach RunelordDwarf Scarab Slime SmallSpider Spider "
       "Tentacle Troll Wizard Wyvern lich skeleton").split()


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    src, dst = argv[1], argv[2]
    names = argv[3:] or ALL
    os.makedirs(dst, exist_ok=True)
    for name in names:
        S = Skeleton(os.path.join(src, name + ".skeleton.xml"))
        if "Walk" not in S.clips:
            print("SKIP", name, "no Walk clip")
            continue
        if CLIP in S.clips:
            print("SKIP", name, "already has", CLIP)
            continue
        rigs = config(name, S)
        if isinstance(rigs, str):
            print("SKIP", name, rigs)
            continue
        length, grid, P, _base = build(S, rigs, None)
        tracks = closed_tracks(P, length, grid, S)
        text = append_clip(S.text, anim_xml(CLIP, length, tracks))
        with open(os.path.join(dst, name + ".skeleton.xml"), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("WROTE", name, "length", length, "keys", len(grid), "tracks", len(tracks))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
