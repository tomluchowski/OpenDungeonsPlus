#!/usr/bin/env python3
"""Adds the clip "Droop" to models/WarBanner.skeleton: the cloth of the watch banner hangs down.

The watch banner plays its Loop clip while it is ready (the flag flies). After it has called the guards it reloads
and the flag hangs, see config/roomAmbienceFixEffects.cfg (WatchBannerDroop, When Reloading).

The clip is a still pose of two keyframes. The cloth of each side starts with the bone below the arm (Bone.002 and
Bone.008, children of RigthArm and LeftArm). Their bind direction (local y axis) is turned to point straight down, the
bones further out keep their bind bend, so the chain hangs as one piece. The rotation of a key is applied in the frame
of the bone, after the bind rotation:   q_key = W^-1 * delta * W,   W = world bind orientation of the bone,
delta = the world rotation that turns the bone direction to -z.

Usage (OgreXMLConverter has to be on the PATH, or give --converter <path>):
    banner_droop_clip.py models/WarBanner.skeleton              adds the clip in place
    banner_droop_clip.py models/WarBanner.skeleton --check      only prints the angles, writes nothing

Plain Python 3, no numpy.
"""
import math
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

CLIP = "Droop"
LENGTH = 0.5
#: first cloth bone of each side
CLOTH = ("Bone.002", "Bone.008")


def q_from_aa(angle, axis):
    n = math.sqrt(sum(a * a for a in axis))
    if n < 1e-12 or abs(angle) < 1e-12:
        return (1.0, 0.0, 0.0, 0.0)
    s = math.sin(angle / 2) / n
    return (math.cos(angle / 2), axis[0] * s, axis[1] * s, axis[2] * s)


def q_mul(a, b):
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    return (w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2, w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
            w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2)


def q_conj(q):
    return (q[0], -q[1], -q[2], -q[3])


def q_norm(q):
    n = math.sqrt(sum(x * x for x in q))
    return tuple(x / n for x in q)


def q_rot(q, v):
    r = q_mul(q_mul(q, (0.0, v[0], v[1], v[2])), q_conj(q))
    return (r[1], r[2], r[3])


def q_between(a, b):
    """Shortest rotation that turns the unit vector a to the unit vector b."""
    dot = sum(x * y for x, y in zip(a, b))
    cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    if dot < -0.999999:
        # opposite: turn half a circle around any axis that is not along a
        axis = (1.0, 0.0, 0.0) if abs(a[0]) < 0.9 else (0.0, 1.0, 0.0)
        return q_from_aa(math.pi, axis)
    return q_norm((1.0 + dot, cross[0], cross[1], cross[2]))


def aa_from_q(q):
    q = q_norm(q)
    if q[0] < 0:
        q = tuple(-x for x in q)
    s = math.sqrt(max(0.0, 1.0 - q[0] * q[0]))
    angle = 2.0 * math.acos(max(-1.0, min(1.0, q[0])))
    if s < 1e-9:
        return 0.0, (1.0, 0.0, 0.0)
    return angle, (q[1] / s, q[2] / s, q[3] / s)


def read_bones(root):
    bones = {}
    for b in root.find("bones").findall("bone"):
        r = b.find("rotation")
        ax = r.find("axis")
        q = q_norm(q_from_aa(float(r.get("angle")), [float(ax.get(c)) for c in "xyz"]))
        bones[b.get("name")] = q
    parents = {h.get("bone"): h.get("parent") for h in root.find("bonehierarchy").findall("boneparent")}
    return bones, parents


def world_orientation(name, bones, parents):
    q = bones[name]
    while name in parents:
        name = parents[name]
        q = q_mul(bones[name], q)
    return q


def droop_keys(root):
    bones, parents = read_bones(root)
    keys = {}
    for name in CLOTH:
        world = world_orientation(name, bones, parents)
        direction = q_rot(world, (0.0, 1.0, 0.0))
        delta = q_between(direction, (0.0, 0.0, -1.0))
        keys[name] = q_norm(q_mul(q_mul(q_conj(world), delta), world))
    return keys


def add_clip(root, keys):
    anims = root.find("animations")
    for a in anims.findall("animation"):
        if a.get("name") == CLIP:
            raise SystemExit("the clip %s exists already" % CLIP)
    anim = ET.SubElement(anims, "animation", {"name": CLIP, "length": "%.6g" % LENGTH})
    tracks = ET.SubElement(anim, "tracks")
    for name, q in keys.items():
        angle, axis = aa_from_q(q)
        track = ET.SubElement(tracks, "track", {"bone": name})
        keyframes = ET.SubElement(track, "keyframes")
        for time in (0.0, LENGTH):
            key = ET.SubElement(keyframes, "keyframe", {"time": "%.6g" % time})
            ET.SubElement(key, "translate", {"x": "0", "y": "0", "z": "0"})
            rotate = ET.SubElement(key, "rotate", {"angle": "%.7g" % angle})
            ET.SubElement(rotate, "axis", {"x": "%.7g" % axis[0], "y": "%.7g" % axis[1], "z": "%.7g" % axis[2]})
            ET.SubElement(key, "scale", {"x": "1", "y": "1", "z": "1"})


def run_converter(converter, args):
    subprocess.check_call([converter, "-q"] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check_only = "--check" in sys.argv
    converter = "OgreXMLConverter"
    if "--converter" in sys.argv:
        converter = sys.argv[sys.argv.index("--converter") + 1]
        args = [a for a in args if a != converter]
    if not args:
        print(__doc__)
        return 2
    skeleton = args[0]
    with tempfile.TemporaryDirectory() as tmp:
        src_xml = os.path.join(tmp, "in.xml")
        run_converter(converter, [skeleton, src_xml])
        root = ET.parse(src_xml).getroot()
        keys = droop_keys(root)
        # The bone direction after the key must point down
        bones, parents = read_bones(root)
        for name, q in keys.items():
            world = q_mul(world_orientation(name, bones, parents), q)
            direction = q_rot(world, (0.0, 1.0, 0.0))
            print("%s: key %.1f deg, bone direction after the key %.3f %.3f %.3f" % (
                name, math.degrees(aa_from_q(q)[0]), direction[0], direction[1], direction[2]))
            assert direction[2] < -0.999, direction
        if check_only:
            return 0

        before = sorted(a.get("name") for a in root.find("animations").findall("animation"))
        add_clip(root, keys)
        ET.indent(root, space="\t")
        out_xml = os.path.join(tmp, "out.xml")
        with open(out_xml, "w", newline="\n") as f:
            f.write('<?xml version="1.0"?>\n' + ET.tostring(root, encoding="unicode") + "\n")
        out_skel = os.path.join(tmp, "out.skeleton")
        run_converter(converter, ["-o", out_xml, out_skel])

        # Round trip of the binary file: the other clips are still there, and the new one
        back_xml = os.path.join(tmp, "back.xml")
        run_converter(converter, [out_skel, back_xml])
        back = ET.parse(back_xml).getroot()
        after = sorted(a.get("name") for a in back.find("animations").findall("animation"))
        assert after == sorted(before + [CLIP]), after
        with open(out_skel, "rb") as f:
            data = f.read()
        with open(skeleton, "wb") as f:
            f.write(data)
    print("added %s to %s" % (CLIP, skeleton))
    return 0


if __name__ == "__main__":
    sys.exit(main())
