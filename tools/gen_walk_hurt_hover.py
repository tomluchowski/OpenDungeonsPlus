#!/usr/bin/env python3
"""Adds the clip WalkHurt to the hovering creatures (CaveHornet, Wyvern), whose Walk is a hover clip without leg
motion, so tools/gen_walk_hurt.py skips them.

WalkHurt is derived from the Walk clip of the same skeleton: same length, closed loop (every track ends on its own
first key). A hurt hover is made of:

  - the body hangs lower than in Walk and sags a little with every wing beat
  - the body is tilted: nose down, rolled towards the injured (left, +X) side, tail drooping
  - the head is lowered
  - the wing beat is weaker (the injured side much weaker), uneven (the phase of each wing runs ahead and behind its
    Walk phase within the beat, the injured wing lags) and sits low: the wings droop
  - limbs that hang (legs, arms) swing less and the injured side's legs hang limp

No foot is planted, so nothing can slide. The floor clearance is checked on the skinned mesh in Blender.

Usage:

    python tools/gen_walk_hurt_hover.py <input dir with Name.skeleton.xml> <output dir> [CaveHornet] [Wyvern]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_walk_hurt as G


def hover_config(name, S):
    """Rig description of one hovering skeleton."""
    if name == "CaveHornet":
        return dict(
            body=["Root"], dip=0.035, bob=0.012,
            tilt=["Body"], pitch=9.0, roll=11.0, roll_bob=3.0,
            head="Head", head_down=12.0,
            tail=["Tail1", "Tail2"], tail_down=16.0,
            wings_l=["Wing1_L", "Wing2_L"], wings_r=["Wing1_R", "Wing2_R"],
            wing_root_l=["Wing1_L", "Wing2_L"], wing_root_r=["Wing1_R", "Wing2_R"],
            k_l=0.45, k_r=0.7, droop_l=26.0, droop_r=14.0, warp_l=0.17, warp_r=0.08, lag_l=0.05,
            hang_l=[["Leg1up_L", "Leg1mid_L", "Leg1low_L"], ["Leg2up_L", "Leg2mid_L", "Leg2low_L"],
                    ["Leg3up_L", "Leg3mid_L", "Leg3low_L"]],
            hang_r=[["Leg1up_R", "Leg1mid_R", "Leg1low_R"], ["Leg2up_R", "Leg2mid_R", "Leg2low_R"],
                    ["Leg3up_R", "Leg3mid_R", "Leg3low_R"]],
            k_hang_l=0.4, k_hang_r=0.7)
    if name == "Wyvern":
        return dict(
            body=["COG"], dip=0.03, bob=0.015,
            tilt=["Spine_1", "Spine_2", "Spine_3"], pitch=10.0, roll=9.0, roll_bob=3.0,
            head="Head", head_down=14.0,
            tail=["Tail_1", "Tail_2", "Tail_3", "Tail_4", "Tail_5", "Tail_6"], tail_down=14.0,
            wings_l=["Wing_1.L", "Wing_2.L", "Wing_3.L", "Wing_4.L", "Wing_5.L"],
            wings_r=["Wing_1.R", "Wing_2.R", "Wing_3.R", "Wing_4.R", "Wing_5.R"],
            wing_root_l=["Wing_1.L"], wing_root_r=["Wing_1.R"],
            k_l=0.45, k_r=0.7, droop_l=24.0, droop_r=12.0, warp_l=0.17, warp_r=0.08, lag_l=0.05,
            hang_l=[["Shoulder.L", "Arm.L", "Forearm.L", "Hand.L"]],
            hang_r=[["Shoulder.R", "Arm.R", "Forearm.R", "Hand.R"]],
            k_hang_l=0.4, k_hang_r=0.7)
    return None


def phase_warp(t, length, amount, lag):
    """Walk time that is shown at clip time t: the wing runs ahead and behind its Walk phase within the beat (periodic,
    so the loop stays closed); lag shifts the injured wing behind the healthy one by a fraction of the beat."""
    w = 2.0 * math.pi * t / length
    k = length / (2.0 * math.pi)
    return (t + amount * k * math.sin(w + 0.6) + 0.5 * amount * k * math.sin(2.0 * w + 1.9) - lag * length) % length


def refine_grid(grid, max_step):
    out = [grid[0]]
    for t in grid[1:]:
        n = int(math.ceil((t - out[-1]) / max_step - 1e-9))
        for j in range(1, n + 1):
            out.append(round(out[-1] + (t - out[-1]) / (n - j + 1), 5) if j < n else t)
    return out


def build_hover(S, cfg):
    length, tracks = S.clips["Walk"]
    grid = refine_grid(G.time_grid(tracks, length), 0.0375)
    P = G.Pose(S, tracks, grid)
    # the injured wing runs behind the healthy one: the warp is applied to the Walk tracks, then the beat is damped
    for side, wings, warp, lag, k in (("l", cfg["wings_l"], cfg["warp_l"], cfg["lag_l"], cfg["k_l"]),
                                      ("r", cfg["wings_r"], cfg["warp_r"], 0.0, cfg["k_r"])):
        for b in wings:
            if b not in tracks:
                continue
            for i, t in enumerate(grid):
                if i == len(grid) - 1:
                    tt = 0.0
                else:
                    tt = phase_warp(t, length, warp, lag)
                T, Q = G.interp(tracks[b], tt)
                P.T[b][i], P.Q[b][i] = T, Q
            P.touched.add(b)
        P.damp(wings, k)
    for chains, k in ((cfg["hang_l"], cfg["k_hang_l"]), (cfg["hang_r"], cfg["k_hang_r"])):
        for chain in chains:
            P.damp(chain, k)
    n = P.n
    for i, t in enumerate(grid):
        w = 2.0 * math.pi * t / length
        s = 0.5 + 0.5 * math.sin(w + 0.9)          # 1 = body sagged between two beats, 0 = lifted by the beat
        for b in cfg["body"]:
            P.move_world(b, (0.0, 0.0, -(cfg["dip"] + cfg["bob"] * s)), i)
        for b in cfg["tilt"]:
            P.rotate_world(b, G.X_AXIS, math.radians(cfg["pitch"] + 1.5 * s) / len(cfg["tilt"]), i)
            P.rotate_world(b, G.Y_AXIS, math.radians(cfg["roll"] + cfg["roll_bob"] * s) / len(cfg["tilt"]), i)
        P.rotate_world(cfg["head"], G.X_AXIS, math.radians(cfg["head_down"] + 3.0 * s), i)
        for b in cfg["tail"]:
            P.rotate_world(b, G.X_AXIS, -math.radians(cfg["tail_down"] + 3.0 * s) / len(cfg["tail"]), i)
        for b in cfg["wing_root_l"]:
            P.rotate_world(b, G.Y_AXIS, math.radians(cfg["droop_l"] * (0.8 + 0.2 * s)) / len(cfg["wing_root_l"]), i)
        for b in cfg["wing_root_r"]:
            P.rotate_world(b, G.Y_AXIS, -math.radians(cfg["droop_r"] * (0.8 + 0.2 * s)) / len(cfg["wing_root_r"]), i)
    # closed loop: every touched track ends on its own first key
    out = []
    for b in S.order:
        if b not in P.touched:
            continue
        keys = []
        for i, t in enumerate(grid):
            if i == n - 1:
                keys.append((t, P.T[b][0], P.Q[b][0]))
            else:
                keys.append((t, P.T[b][i], P.Q[b][i]))
        out.append((b, keys))
    return length, out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    src, dst = argv[1], argv[2]
    names = argv[3:] or ["CaveHornet", "Wyvern"]
    os.makedirs(dst, exist_ok=True)
    for name in names:
        S = G.Skeleton(os.path.join(src, name + ".skeleton.xml"))
        if G.CLIP in S.clips:
            print("SKIP", name, "already has", G.CLIP)
            continue
        cfg = hover_config(name, S)
        if cfg is None:
            print("SKIP", name, "no hover configuration")
            continue
        length, tracks = build_hover(S, cfg)
        text = G.append_clip(S.text, G.anim_xml(G.CLIP, length, tracks))
        with open(os.path.join(dst, name + ".skeleton.xml"), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("WROTE", name, "length", length, "keys", len(tracks[0][1]), "tracks", len(tracks))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
