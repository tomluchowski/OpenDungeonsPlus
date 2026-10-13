#!/usr/bin/env python3
"""Moves the keyframe translations of the hen clips Lay and Flutter from the bone frame back to the parent frame.

Background: Ogre applies the <translate> of a keyframe in the PARENT frame of the bone
(NodeAnimationTrack::applyToNode calls Node::translate with TS_PARENT). An earlier fix script turned the
translations of these two clips into the bone frame (T_bone = q_bind^-1 * T_parent), which is wrong for Ogre
and made the hip sink sideways instead of down. The correction for exactly these clips is

    T_new = q_bind * T_old

where q_bind is the bind rotation of the bone (the <rotation> in <bones>). Rotations, scales and all other clips
(Idle, Die, Walk, Pick, Paw, Sleep, Peep, Crow, Hatch, Run) are left alone.

WARNING: ogre_fix.py must NOT be applied to these clips. It converts to the bone frame, which is what this
script undoes.

Usage (OgreXMLConverter has to be on the PATH, or give --converter):
    chicken_frame_fix.py models/Chicken.skeleton              fixes the file in place
    chicken_frame_fix.py models/Chicken.skeleton --check      only prints the numbers, writes nothing

Plain Python 3, no numpy.
"""
import math
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

CLIPS = ("Lay", "Flutter")
CLIP_NAMES = ["Crow", "Die", "Flutter", "Hatch", "Idle", "Lay", "Paw", "Peep", "Pick", "Run", "Sleep", "Walk"]
BONE_COUNT = 22


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


def q_rot(q, v):
    r = q_mul(q_mul(q, (0.0, v[0], v[1], v[2])), q_conj(q))
    return (r[1], r[2], r[3])


def bind_rotations(root):
    out = {}
    for b in root.find("bones").findall("bone"):
        r = b.find("rotation")
        ax = r.find("axis")
        q = q_from_aa(float(r.get("angle")), [float(ax.get(c)) for c in "xyz"])
        n = math.sqrt(sum(x * x for x in q))
        out[b.get("name")] = tuple(x / n for x in q)
    return out


def fix(root):
    rots = bind_rotations(root)
    for a in root.find("animations").findall("animation"):
        if a.get("name") not in CLIPS:
            continue
        for t in a.find("tracks").findall("track"):
            q = rots[t.get("bone")]
            for k in t.find("keyframes").findall("keyframe"):
                tr = k.find("translate")
                if tr is None:
                    continue
                v = q_rot(q, [float(tr.get(c)) for c in "xyz"])
                for c, val in zip("xyz", v):
                    s = "%.7g" % val
                    tr.set(c, "0" if s in ("-0", "0") else s)


def track_values(root, clip, bone, axis):
    for a in root.find("animations").findall("animation"):
        if a.get("name") != clip:
            continue
        for t in a.find("tracks").findall("track"):
            if t.get("bone") == bone:
                return [float(k.find("translate").get(axis)) for k in t.find("keyframes").findall("keyframe")]
    return []


def all_translations(root, clip):
    out = []
    for a in root.find("animations").findall("animation"):
        if a.get("name") != clip:
            continue
        # The converter may write the tracks in another order, so compare them sorted by bone
        for t in sorted(a.find("tracks").findall("track"), key=lambda e: e.get("bone")):
            for k in t.find("keyframes").findall("keyframe"):
                tr = k.find("translate")
                if tr is not None:
                    out.extend(float(tr.get(c)) for c in "xyz")
                out.append(float(k.get("time")))
    return out


def structure(root):
    bones = [(b.get("id"), b.get("name")) for b in root.find("bones").findall("bone")]
    hier = [(h.get("bone"), h.get("parent")) for h in root.find("bonehierarchy").findall("boneparent")]
    clips = sorted(a.get("name") for a in root.find("animations").findall("animation"))
    return bones, hier, clips


def run_converter(converter, args):
    subprocess.check_call([converter, "-q"] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check_only = "--check" in sys.argv
    converter = "OgreXMLConverter"
    if not args:
        print(__doc__)
        return 2
    skeleton = args[0]
    with tempfile.TemporaryDirectory() as tmp:
        src_xml = os.path.join(tmp, "in.xml")
        run_converter(converter, [skeleton, src_xml])
        old = ET.parse(src_xml).getroot()
        new = ET.parse(src_xml).getroot()
        if min(track_values(old, "Lay", "Hip", "z")) < -0.03:
            print("Lay Hip already sinks along z: the file is fixed, nothing to do")
            return 0
        fix(new)

        # Self test: structure unchanged, untouched clips identical
        assert structure(old) == structure(new)
        bones, hier, clips = structure(new)
        assert len(bones) == BONE_COUNT, len(bones)
        assert clips == sorted(CLIP_NAMES), clips
        for clip in CLIP_NAMES:
            if clip in CLIPS:
                continue
            a = all_translations(old, clip)
            b = all_translations(new, clip)
            assert max(abs(x - y) for x, y in zip(a, b)) < 1e-6, clip
        hip = track_values(new, "Lay", "Hip", "z")
        root_z = track_values(new, "Flutter", "Root", "z")
        print("Lay Hip z min %.3f, Flutter Root z max %.3f" % (min(hip), max(root_z)))
        if check_only:
            return 0

        ET.indent(new, space="\t")
        out_xml = os.path.join(tmp, "out.xml")
        with open(out_xml, "w", newline="\n") as f:
            f.write('<?xml version="1.0"?>\n' + ET.tostring(new, encoding="unicode") + "\n")
        out_skel = os.path.join(tmp, "out.skeleton")
        # -o keeps all keyframes (the converter would drop "redundant" ones otherwise)
        run_converter(converter, ["-o", out_xml, out_skel])

        # Round trip of the binary file
        back_xml = os.path.join(tmp, "back.xml")
        run_converter(converter, [out_skel, back_xml])
        back = ET.parse(back_xml).getroot()
        assert structure(back) == structure(old)
        for clip in CLIP_NAMES:
            a = all_translations(new if clip in CLIPS else old, clip)
            b = all_translations(back, clip)
            assert max(abs(x - y) for x, y in zip(a, b)) < 1e-5, clip
        with open(out_skel, "rb") as f:
            data = f.read()
        with open(skeleton, "wb") as f:
            f.write(data)
    print("fixed", skeleton)
    return 0


if __name__ == "__main__":
    sys.exit(main())
