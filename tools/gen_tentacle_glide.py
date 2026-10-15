#!/usr/bin/env python3
"""Replaces the clips Walk and WalkHurt of the Tentacle skeleton (TentacleAlbine, TentacleGreen) by a gliding motion.

The tentacle creatures have no planted foot: the four tentacles reach only about a third of the body height, so
they glide over the floor like a slime and play at WalkClipRate 1 (the key is not set, see config/creatures.cfg).

Walk     the body glides on without footfalls: the front tentacles undulate in a travelling wave (the two sides
         half a period apart), the hind tentacles trail the same wave later and smaller, the tail follows, the
         body bobs a little and sways. Two wave periods in the clip, every motion is a sum of whole harmonics of
         the clip length, so the loop is closed exactly.
WalkHurt the limp of a glider: one wave period only (slower), smaller and uneven (an extra slower and a faster
         wave component), the body low, bent forward, the head hanging, the left front tentacle favoured.

Both clips keep the length of the old Walk (1.52 s) and the key spacing of 0.04 s; all other clips and the
skeleton are copied untouched.

Usage:  python tools/gen_tentacle_glide.py <Tentacle.skeleton.xml> <output.xml> [lift]

lift raises the whole body (metres of the skeleton) on top of the rest offset, so that no tentacle goes below the floor.
"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_walk_hurt as G

LENGTH = 1.52
STEP = 0.04
ROOT_Z = -0.026          # offset of the Root bone that every clip of the skeleton keeps
AX = (1.0, 0.0, 0.0)     # world X: swings a hanging tentacle forward and back
AY = (0.0, 1.0, 0.0)     # world Y: swings it in and out
FORE = {1: ["ForeTentacle1_L", "ForeTentacle2_L", "ForeTentacle3_L"],
        -1: ["ForeTentacle1_R", "ForeTentacle2_R", "ForeTentacle3_R"]}
HIND = {1: ["HindTentacle1_L", "HindTentacle2_L", "HindTentacle3_L"],
        -1: ["HindTentacle1_R", "HindTentacle2_R", "HindTentacle3_R"]}
TAIL = ["Tail1", "Tail2", "Tail3"]
SPINE = ["Body1", "Body2", "Body3"]
DEG = math.pi / 180.0


BASE = {}      # bone -> key rotation of the held pose (mean of the old Walk clip)
BASEW = {}     # bone -> world orientation of the bone in the held pose


def hold_pose(S):
    """The tentacles are not held in their rest pose by the clips of this skeleton: the waves are laid on the mean pose of
    the old Walk clip (taken from the skeleton before it is replaced)."""
    length, tracks = S.clips["Walk"]
    for n in S.order:
        keys = tracks.get(n)
        q = G.QI
        if keys:
            acc = [0.0, 0.0, 0.0, 0.0]
            for (_, _, kq) in keys[:-1]:
                kq = G.qalign(kq, keys[0][2])
                acc = [acc[i] + kq[i] for i in range(4)]
            q = G.qnorm(tuple(acc))
        BASE[n] = q
    for n in S.topo:
        par = S.parent[n]
        BASEW[n] = G.qmul(G.qmul(BASEW[par] if par else G.QI, S.q[n]), BASE[n])


def rot(S, bone, axis, ang):
    """Key rotation that turns bone (in its held pose) by ang about the world axis."""
    r = BASEW[bone]
    return G.qmul(G.qmul(G.qinv(r), G.qaxis(axis, ang)), r)


def compose(bone, *qs):
    out = BASE[bone]
    for q in qs:
        out = G.qmul(out, q)
    return out


def motion(S, p, u):
    """Pose of all animated bones at clip phase u (0..1). Returns {bone: (translation, rotation)}."""
    two = 2.0 * math.pi
    out = {}
    w = two * u * p["cycles"]                      # wave phase
    slow = two * u * 1.0                           # one period of the clip
    # irregularity of the limp: extra whole-harmonic components
    irr = p["irr"]
    def wv(ph):
        return math.sin(w + ph) + irr * (math.sin(0.5 * w + 1.3 + ph) * 0.8 + math.sin(1.5 * w + 0.4 + ph) * 0.5)
    def wl(ph):
        return 0.5 * (1.0 + wv(ph))          # 0..1 (the wave only lifts a tip, it never presses it into the floor)
    # front tentacles: travelling wave root to tip, the sides half a period apart
    for side in (1, -1):
        amp = p["fore_amp"] * (p["fav"] if side == 1 else 1.0)
        for i, b in enumerate(FORE[side]):
            lag = -i * p["lag"] + (0.0 if side == 1 else math.pi)
            pitch = -amp * p["fore_pitch"][i] * DEG * wl(lag)
            swing = amp * p["fore_swing"][i] * DEG * side * wv(lag + 0.5 * math.pi)
            droop = p["droop"][i] * DEG
            lift = (p["fav_lift"][i] * DEG if side == 1 else 0.0)
            out[b] = ((0.0, 0.0, 0.0), compose(b, rot(S, b, AX, pitch + droop - lift), rot(S, b, AY, swing)))
    # hind tentacles: trail the same wave, later and smaller
    for side in (1, -1):
        for i, b in enumerate(HIND[side]):
            lag = -p["hind_delay"] - i * p["lag"] + (0.0 if side == 1 else math.pi)
            pitch = p["hind_amp"] * p["hind_pitch"][i] * DEG * wl(lag)
            swing = p["hind_amp"] * p["hind_swing"][i] * DEG * side * wv(lag + 0.5 * math.pi)
            out[b] = ((0.0, 0.0, 0.0), compose(b, rot(S, b, AX, pitch + p["hind_droop"][i] * DEG), rot(S, b, AY, swing)))
    # tail: follows the wave
    for i, b in enumerate(TAIL):
        lag = -p["tail_delay"] - i * p["lag"]
        a = p["tail_amp"] * p["tail_pitch"][i] * DEG
        out[b] = ((0.0, 0.0, 0.0), compose(b, rot(S, b, AX, a * wv(lag + 0.5 * math.pi) + p["tail_droop"][i] * DEG),
                                          rot(S, b, AY, a * 0.7 * wv(lag))))
    # spine: sways from side to side with the wave and leans a little; head counters it
    tot_sw = 0.0
    tot_pi = 0.0
    for i, b in enumerate(SPINE):
        sw = p["body_sway"][i] * DEG * math.sin(slow * p["cycles"] * 0.5 + 0.6 - i * 0.5)
        pi_ = p["body_nod"][i] * DEG * math.sin(w + 0.9 - i * 0.6) + p["body_bend"][i] * DEG
        tot_sw += sw
        tot_pi += pi_
        out[b] = ((0.0, 0.0, 0.0), compose(b, rot(S, b, AX, pi_), rot(S, b, AY, sw)))
    out["Head"] = ((0.0, 0.0, 0.0), compose("Head", rot(S, "Head", AX, -0.6 * tot_pi + p["head_down"] * DEG + 2.5 * DEG * math.sin(w - 1.4)),
                                           rot(S, "Head", AY, -0.6 * tot_sw)))
    # root: small bob (up only, twice per wave period of one side) and a little sway
    bob = p["bob"] * (0.5 + 0.5 * math.sin(2.0 * w - 0.4))
    sway = p["root_sway"] * math.sin(w + 0.3)
    out["Root"] = ((sway, 0.0, ROOT_Z + p["lift"] + bob + p["root_drop"]), G.QI)
    return out


WALK = dict(cycles=2, irr=0.0, fore_amp=1.0, fav=1.0, lag=0.85,
            fore_pitch=(16.0, 24.0, 28.0), fore_swing=(5.0, 9.0, 11.0), droop=(0.0, 0.0, 0.0), fav_lift=(0.0, 0.0, 0.0),
            hind_amp=1.0, hind_delay=1.3, hind_pitch=(5.0, 8.0, 11.0), hind_swing=(2.5, 4.0, 6.0), hind_droop=(0.0, 0.0, 0.0),
            tail_amp=1.0, tail_delay=1.9, tail_pitch=(4.0, 7.0, 10.0), tail_droop=(0.0, 0.0, 0.0),
            body_sway=(2.0, 2.5, 2.5), body_nod=(1.2, 1.6, 1.6), body_bend=(0.0, 0.0, 0.0), head_down=0.0,
            bob=0.010, root_sway=0.004, root_drop=0.0, lift=0.0)

HURT = dict(cycles=1, irr=0.45, fore_amp=0.6, fav=0.35, lag=0.7,
            fore_pitch=(11.0, 17.0, 20.0), fore_swing=(5.0, 9.0, 11.0), droop=(0.0, 0.0, 0.0), fav_lift=(6.0, 10.0, 12.0),
            hind_amp=0.7, hind_delay=1.0, hind_pitch=(3.0, 5.0, 7.0), hind_swing=(2.5, 4.0, 6.0), hind_droop=(2.0, 4.0, 6.0),
            tail_amp=0.6, tail_delay=1.6, tail_pitch=(4.0, 7.0, 10.0), tail_droop=(2.0, 5.0, 8.0),
            body_sway=(2.0, 2.5, 2.5), body_nod=(1.0, 1.2, 1.2), body_bend=(3.0, 5.0, 6.0), head_down=10.0,
            bob=0.006, root_sway=0.003, root_drop=-0.006, lift=0.0)


def build_clip(S, p):
    n = int(round(LENGTH / STEP))
    grid = [i * STEP for i in range(n + 1)]
    tracks = {}
    for i, t in enumerate(grid):
        u = (t / LENGTH) if i < n else 0.0           # last key = first key: the loop is closed exactly
        for b, (tr, q) in motion(S, p, u).items():
            tracks.setdefault(b, []).append((t, tr, q))
    order = [b for b in S.order if b in tracks]
    return [(b, tracks[b]) for b in order]


def replace_clip(text, name, xml):
    m = re.search(r'[ \t]*<animation name="%s" length="[^"]+">' % re.escape(name), text)
    if not m:
        raise SystemExit("clip %s not found" % name)
    e = text.index("</animation>", m.start()) + len("</animation>")
    return text[:m.start()] + xml.rstrip(chr(10)) + text[e:]


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    src, dst = argv[1], argv[2]
    lift = float(argv[3]) if len(argv) > 3 else 0.0
    S = G.Skeleton(src)
    text = S.text
    hold_pose(S)
    for name, base in (("Walk", WALK), ("WalkHurt", HURT)):
        p = dict(base)
        p["lift"] = lift
        text = replace_clip(text, name, G.anim_xml(name, LENGTH, build_clip(S, p)))
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("WROTE", dst)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
