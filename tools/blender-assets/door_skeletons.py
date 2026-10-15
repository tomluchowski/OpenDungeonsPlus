#!/usr/bin/env python3
"""Gives every door model its own skeleton with clips that fit the door (no Blender needed).

    python tools/blender-assets/door_skeletons.py <work folder>

The work folder holds the door meshes as XML (OgreXMLConverter models/<Name>.mesh, which gives
<Name>.mesh.xml). The script writes <Name>.skeleton.xml and rewrites <Name>.mesh.xml so that it links to the new
skeleton and uses its bones. Afterwards run OgreXMLConverter -o on both files and copy the .mesh and .skeleton
files to models/. WoodenDoor keeps the old shared skeleton.

All bones have a neutral rest orientation, so a clip key is a plain move, turn or scale in model space around the
bone position (the model stands on the z axis, the door is wide along x and thin along y).

Clips (every door type has Open, Close and Destroyed, the barricade has Collapse and Destroyed):
  Ironbound  heavy swing with a slow start, a sag and a shake of the frame when it hits home
  Steel      the leaves slide into the wall and stop with a thud
  Secret     the wall slabs grind aside and sink a little
  Rune       the runes flare and the leaves dissolve
  Barricade  every board and both posts fall into a heap of rubble
"""

import math
import os
import random
import sys
import xml.etree.ElementTree as ET

FPS = 24.0
LEFT = "LeftDoorBone"
RIGHT = "RightDoorBone"
ROOT = "RootBone"


# ------------------------------------------------------------------ small math helpers

def clamp(value, low=0.0, high=1.0):
    return max(low, min(high, value))


def smooth(p):
    p = clamp(p)
    return p * p * (3.0 - 2.0 * p)


def smoother(p):
    p = clamp(p)
    return p * p * p * (p * (p * 6.0 - 15.0) + 10.0)


def quat(axis, angle):
    length = math.sqrt(sum(a * a for a in axis))
    s = math.sin(angle / 2.0) / length
    return (math.cos(angle / 2.0), axis[0] * s, axis[1] * s, axis[2] * s)


def qmul(a, b):
    return (a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
            a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
            a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
            a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0])


IDENTITY = (1.0, 0.0, 0.0, 0.0)
X_AXIS = (1.0, 0.0, 0.0)
Y_AXIS = (0.0, 1.0, 0.0)
Z_AXIS = (0.0, 0.0, 1.0)


def compose(*turns):
    """Turns are (axis, angle) pairs, applied right to left like a matrix product."""
    result = IDENTITY
    for axis, angle in turns:
        result = qmul(result, quat(axis, angle))
    return result


def key(translate=(0.0, 0.0, 0.0), rotation=IDENTITY, scale=(1.0, 1.0, 1.0)):
    return {"t": translate, "q": rotation, "s": scale}


REST = key()


# ------------------------------------------------------------------ skeleton writer

class Skeleton:
    def __init__(self):
        self.bones = []  # (name, pivot)
        self.clips = []  # (name, length, {bone: function(time) -> key})

    def add_bone(self, name, pivot):
        self.bones.append((name, pivot))
        return len(self.bones) - 1

    def add_clip(self, name, length, tracks):
        self.clips.append((name, length, tracks))

    def to_xml(self):
        out = ['<?xml version="1.0"?>', '<skeleton blendmode="average">', "\t<bones>"]
        for index, (name, pivot) in enumerate(self.bones):
            out.append('\t\t<bone id="%d" name="%s">' % (index, name))
            out.append('\t\t\t<position x="%.6g" y="%.6g" z="%.6g" />' % pivot)
            out.append('\t\t\t<rotation angle="0">')
            out.append('\t\t\t\t<axis x="1" y="0" z="0" />')
            out.append("\t\t\t</rotation>")
            out.append("\t\t</bone>")
        out.append("\t</bones>")
        out.append("\t<bonehierarchy>")
        for name, _ in self.bones[1:]:
            out.append('\t\t<boneparent bone="%s" parent="%s" />' % (name, ROOT))
        out.append("\t</bonehierarchy>")
        out.append("\t<animations>")
        for name, length, tracks in self.clips:
            out.append('\t\t<animation name="%s" length="%.6g">' % (name, length))
            out.append("\t\t\t<tracks>")
            frames = max(2, int(math.ceil(length * FPS)))
            times = [length * i / frames for i in range(frames + 1)]
            for bone, make in tracks.items():
                out.append('\t\t\t\t<track bone="%s">' % bone)
                out.append("\t\t\t\t\t<keyframes>")
                for time in times:
                    k = make(time)
                    q = k["q"]
                    w = clamp(q[0], -1.0, 1.0)
                    angle = 2.0 * math.acos(w)
                    sin_half = math.sqrt(max(0.0, 1.0 - w * w))
                    axis = (1.0, 0.0, 0.0) if sin_half < 1e-6 else (q[1] / sin_half, q[2] / sin_half, q[3] / sin_half)
                    if angle > math.pi:
                        angle -= 2.0 * math.pi
                    out.append('\t\t\t\t\t\t<keyframe time="%.6g">' % time)
                    out.append('\t\t\t\t\t\t\t<translate x="%.6g" y="%.6g" z="%.6g" />' % k["t"])
                    out.append('\t\t\t\t\t\t\t<rotate angle="%.6g">' % angle)
                    out.append('\t\t\t\t\t\t\t\t<axis x="%.6g" y="%.6g" z="%.6g" />' % axis)
                    out.append("\t\t\t\t\t\t\t</rotate>")
                    out.append('\t\t\t\t\t\t\t<scale x="%.6g" y="%.6g" z="%.6g" />' % k["s"])
                    out.append("\t\t\t\t\t\t</keyframe>")
                out.append("\t\t\t\t\t</keyframes>")
                out.append("\t\t\t\t</track>")
            out.append("\t\t\t</tracks>")
            out.append("\t\t</animation>")
        out.append("\t</animations>")
        out.append("</skeleton>")
        return "\n".join(out) + "\n"


def reversed_clip(make, length):
    return lambda time: make(length - time)


# ------------------------------------------------------------------ the five door types

def leaf_bones(skeleton, left_pivot, right_pivot):
    skeleton.add_bone(ROOT, (0.0, 0.0, 0.0))
    skeleton.add_bone(LEFT, left_pivot)
    skeleton.add_bone(RIGHT, right_pivot)


def thud(q, amount, turn):
    """The frame shakes for a moment after something heavy hit home (q runs 0..1 over the shake)."""
    if q <= 0.0 or q >= 1.0:
        return REST
    fade = 1.0 - q
    return key((0.0, 0.0, -amount * math.sin(math.pi * q)),
               compose((Y_AXIS, turn * math.sin(2.0 * math.pi * q) * fade),
                       (X_AXIS, turn * 0.5 * math.sin(3.0 * math.pi * q) * fade)))


def ironbound():
    skeleton = Skeleton()
    leaf_bones(skeleton, (-0.42, 0.0, 0.0), (0.42, 0.0, 0.0))
    swing = 1.75

    def open_angle(time, length=0.9):
        p = time / length
        main = swing * smoother(p / 0.8)
        if p > 0.8:
            main += 0.05 * math.sin(math.pi * (p - 0.8) / 0.2)
        return main

    def open_leaf(side):
        def make(time):
            p = time / 0.9
            sag = -0.012 * math.sin(math.pi * clamp(p / 0.8))
            return key((0.0, 0.0, sag), compose((Z_AXIS, side * open_angle(time))))
        return make

    def open_root(time):
        p = time / 0.9
        return thud((p - 0.8) / 0.2, 0.01, 0.012)

    def close_angle(time):
        p = time / 0.8
        if p < 0.85:
            return swing * (1.0 - clamp(p / 0.85) ** 2.2)
        return 0.07 * math.sin(math.pi * (p - 0.85) / 0.15)

    def close_leaf(side):
        return lambda time: key((0.0, 0.0, 0.0), compose((Z_AXIS, side * close_angle(time))))

    def close_root(time):
        p = time / 0.8
        return thud((p - 0.85) / 0.15, 0.03, 0.02)

    def destroyed_leaf(side):
        def make(time):
            p = time / 0.5
            fall = p * p
            return key((0.0, 0.0, -0.04 * fall), compose((X_AXIS, -1.3 * fall)))
        return make

    skeleton.add_clip("Open", 0.9, {ROOT: open_root, LEFT: open_leaf(1.0), RIGHT: open_leaf(-1.0)})
    skeleton.add_clip("Close", 0.8, {ROOT: close_root, LEFT: close_leaf(1.0), RIGHT: close_leaf(-1.0)})
    skeleton.add_clip("Destroyed", 0.5, {LEFT: destroyed_leaf(1.0), RIGHT: destroyed_leaf(-1.0)})
    return skeleton


def steel():
    skeleton = Skeleton()
    leaf_bones(skeleton, (-0.21, 0.0, 0.0), (0.21, 0.0, 0.0))
    slide = 0.78

    def open_leaf(side):
        def make(time):
            p = time / 0.7
            moved = slide * smooth(p / 0.9)
            q = (p - 0.9) / 0.1
            jitter = 0.01 * math.sin(math.pi * q) if 0.0 < q < 1.0 else 0.0
            return key((side * moved, jitter, 0.0))
        return make

    def open_root(time):
        return thud((time / 0.7 - 0.9) / 0.1, 0.03, 0.012)

    def close_leaf(side):
        def make(time):
            p = time / 0.6
            moved = slide * (1.0 - clamp(p / 0.85) ** 2.0)
            q = (p - 0.85) / 0.15
            jitter = -0.012 * math.sin(math.pi * q) if 0.0 < q < 1.0 else 0.0
            return key((side * moved, jitter, 0.0))
        return make

    def close_root(time):
        return thud((time / 0.6 - 0.85) / 0.15, 0.04, 0.02)

    def destroyed_leaf(side):
        def make(time):
            p = time / 0.5
            fall = p * p
            return key((0.0, 0.0, -0.04 * fall), compose((X_AXIS, side * 1.25 * fall)))
        return make

    skeleton.add_clip("Open", 0.7, {ROOT: open_root, LEFT: open_leaf(-1.0), RIGHT: open_leaf(1.0)})
    skeleton.add_clip("Close", 0.6, {ROOT: close_root, LEFT: close_leaf(-1.0), RIGHT: close_leaf(1.0)})
    skeleton.add_clip("Destroyed", 0.5, {LEFT: destroyed_leaf(1.0), RIGHT: destroyed_leaf(-1.0)})
    return skeleton


def secret():
    skeleton = Skeleton()
    leaf_bones(skeleton, (-0.21, 0.0, 0.0), (0.21, 0.0, 0.0))
    slide = 0.8

    def open_leaf(side, length=0.9):
        def make(time):
            p = time / length
            moved = slide * smoother(p)
            jitter = 0.008 * math.sin(60.0 * p) * math.sin(math.pi * p)
            return key((side * moved, jitter, -0.05 * math.sin(math.pi * p)))
        return make

    def close_leaf(side, length=0.8):
        def make(time):
            p = time / length
            moved = slide * (1.0 - smoother(p / 0.88))
            q = (p - 0.88) / 0.12
            bump = -0.02 * math.sin(math.pi * q) if 0.0 < q < 1.0 else 0.0
            jitter = 0.008 * math.sin(60.0 * p) * math.sin(math.pi * clamp(p / 0.88))
            return key((side * moved, jitter, bump))
        return make

    def destroyed_leaf(side):
        def make(time):
            p = time / 0.8
            shrink = 1.0 - 0.35 * p
            return key((side * 0.1 * p, 0.0, -1.2 * p * p), compose((Y_AXIS, side * 0.5 * p ** 1.5)), (shrink, shrink, shrink))
        return make

    skeleton.add_clip("Open", 0.9, {LEFT: open_leaf(-1.0), RIGHT: open_leaf(1.0)})
    skeleton.add_clip("Close", 0.8, {LEFT: close_leaf(-1.0), RIGHT: close_leaf(1.0)})
    skeleton.add_clip("Destroyed", 0.8, {LEFT: destroyed_leaf(-1.0), RIGHT: destroyed_leaf(1.0)})
    return skeleton


def rune():
    skeleton = Skeleton()
    leaf_bones(skeleton, (-0.21, 0.0, 0.62), (0.21, 0.0, 0.62))

    def dissolve(p):
        """1 -> a short flare -> almost nothing."""
        if p < 0.25:
            return 1.0 + 0.1 * smooth(p / 0.25)
        return 1.1 - 1.08 * clamp((p - 0.25) / 0.75) ** 1.6

    def open_leaf(side, length=0.9):
        def make(time):
            p = time / length
            s = dissolve(p)
            return key((0.0, 0.0, 0.12 * p * p), compose((Z_AXIS, side * 0.9 * p * p)), (s, s, s))
        return make

    def destroyed_leaf(side):
        def make(time):
            p = time / 0.6
            s = 1.1 * smooth(p / 0.15) if p < 0.15 else 1.1 - 1.09 * clamp((p - 0.15) / 0.85)
            return key((0.0, side * 0.3 * p, 0.2 * p), compose((Z_AXIS, side * 2.0 * p)), (s, s, s))
        return make

    skeleton.add_clip("Open", 0.9, {LEFT: open_leaf(1.0), RIGHT: open_leaf(-1.0)})
    skeleton.add_clip("Close", 0.9, {LEFT: reversed_clip(open_leaf(1.0), 0.9), RIGHT: reversed_clip(open_leaf(-1.0), 0.9)})
    skeleton.add_clip("Destroyed", 0.6, {LEFT: destroyed_leaf(1.0), RIGHT: destroyed_leaf(-1.0)})
    return skeleton


# ------------------------------------------------------------------ barricade (a bone for every board)

def weld_components(positions, faces):
    """Groups the vertices of the mesh into connected pieces (the vertices are not shared between faces)."""
    ids = {}
    welded = []
    for p in positions:
        k = tuple(round(c, 3) for c in p)
        welded.append(ids.setdefault(k, len(ids)))
    parent = list(range(len(ids)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for a, b, c in faces:
        parent[find(welded[a])] = find(welded[b])
        parent[find(welded[b])] = find(welded[c])
    pieces = {}
    for index in range(len(positions)):
        pieces.setdefault(find(welded[index]), []).append(index)
    return list(pieces.values())


def principal_angle(points):
    """Angle in the x/z plane of the long axis of a board."""
    mx = sum(p[0] for p in points) / len(points)
    mz = sum(p[2] for p in points) / len(points)
    sxx = sum((p[0] - mx) ** 2 for p in points)
    szz = sum((p[2] - mz) ** 2 for p in points)
    sxz = sum((p[0] - mx) * (p[2] - mz) for p in points)
    theta = 0.5 * math.atan2(2.0 * sxz, sxx - szz)
    return theta


def fall_curve(p, delay, span):
    """Progress of a falling part: still, then speeding up until it hits the ground, then settled."""
    f = clamp((p - delay) / span)
    return f * f, f


def barricade(mesh_root):
    shared = mesh_root.find("sharedgeometry").find("vertexbuffer").findall("vertex")
    positions = [tuple(float(v.find("position").get(a)) for a in "xyz") for v in shared]
    faces = []
    for submesh in mesh_root.find("submeshes"):
        for face in submesh.find("faces"):
            faces.append((int(face.get("v1")), int(face.get("v2")), int(face.get("v3"))))
    old_bone = {}
    for assignment in mesh_root.find("boneassignments"):
        old_bone[int(assignment.get("vertexindex"))] = int(assignment.get("boneindex"))

    boards = [piece for piece in weld_components(positions, faces)
              if sum(1 for i in piece if old_bone.get(i, 0) in (2, 4)) > len(piece) // 2]
    boards.sort(key=lambda piece: (sum(positions[i][0] for i in piece) / len(piece), sum(positions[i][2] for i in piece) / len(piece)))

    skeleton = Skeleton()
    skeleton.add_bone(ROOT, (0.0, 0.0, 0.0))
    skeleton.add_bone("PostLeft", (-0.5, 0.0, 0.0))
    skeleton.add_bone("PostRight", (0.5, 0.0, 0.0))
    assignment_of = {}
    plank_info = []
    for number, piece in enumerate(boards, 1):
        points = [positions[i] for i in piece]
        center = tuple(sum(p[a] for p in points) / len(points) for a in range(3))
        name = "Plank%02d" % number
        index = skeleton.add_bone(name, center)
        for i in piece:
            assignment_of[i] = index
        plank_info.append((name, center, principal_angle(points)))
    for i in range(len(positions)):
        if i not in assignment_of:
            assignment_of[i] = 1 if positions[i][0] < 0.0 else 2

    def collapse_tracks(seed, spread, length):
        rng = random.Random(seed)
        tracks = {}
        for name, center, theta in plank_info:
            delay = 0.04 + 0.3 * rng.random()
            span = 0.55
            ground = 0.03 + 0.045 * rng.randrange(0, 4)
            dx = rng.uniform(-0.12, 0.12) * spread
            dy = rng.uniform(-0.3, 0.3) * spread
            yaw = rng.uniform(-0.7, 0.7)
            tilt = rng.uniform(-0.15, 0.15)
            if theta > math.pi / 2.0:
                theta -= math.pi
            flat = theta

            def make(time, center=center, delay=delay, span=span, ground=ground, dx=dx, dy=dy, yaw=yaw, tilt=tilt, flat=flat):
                p = time / length
                drop, f = fall_curve(p, delay, span)
                settle = clamp((p - (delay + span)) / 0.1)
                bounce = 0.03 * math.sin(math.pi * settle) if 0.0 < settle < 1.0 else 0.0
                ease = smooth(f)
                z = -(center[2] - ground) * drop + bounce
                return key((dx * ease, dy * ease, z),
                           compose((Z_AXIS, yaw * ease), (Y_AXIS, flat * ease), (X_AXIS, tilt * ease)))
            tracks[name] = make

        for name, side in (("PostLeft", -1.0), ("PostRight", 1.0)):
            def make(time, side=side):
                p = time / length
                drop, f = fall_curve(p, 0.12, 0.6)
                settle = clamp((p - 0.72) / 0.12)
                bounce = 0.02 * math.sin(math.pi * settle) if 0.0 < settle < 1.0 else 0.0
                # The posts break and lean over, so the heap stays within about half a tile
                return key((0.0, 0.0, bounce), compose((X_AXIS, -side * 1.2 * drop)), (1.0, 1.0, 1.0 - 0.5 * drop))
            tracks[name] = make
        return tracks

    skeleton.add_clip("Collapse", 0.9, collapse_tracks(7, 1.0, 0.9))
    skeleton.add_clip("Destroyed", 0.5, collapse_tracks(11, 1.4, 0.5))
    return skeleton, assignment_of


# ------------------------------------------------------------------ mesh rewrite

def rewrite_mesh(path, skeleton_name, bone_map):
    tree = ET.parse(path)
    root = tree.getroot()
    root.find("skeletonlink").set("name", skeleton_name)
    assignments = root.find("boneassignments")
    for assignment in assignments:
        index = int(assignment.get("vertexindex"))
        if callable(bone_map):
            assignment.set("boneindex", str(bone_map(index)))
        else:
            assignment.set("boneindex", str(bone_map[int(assignment.get("boneindex"))]))
    tree.write(path, encoding="utf-8", xml_declaration=True)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    folder = sys.argv[1]
    # old bone 0 is the frame, 2 the left leaf and 4 the right leaf
    leaf_map = {0: 0, 2: 1, 4: 2}
    for name, build in (("DoorIronbound", ironbound), ("DoorSteel", steel), ("DoorSecret", secret), ("DoorRune", rune)):
        skeleton = build()
        with open(os.path.join(folder, name + ".skeleton.xml"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(skeleton.to_xml())
        rewrite_mesh(os.path.join(folder, name + ".mesh.xml"), name + ".skeleton", leaf_map)
        print("wrote", name, len(skeleton.bones), "bones", [clip[0] for clip in skeleton.clips])

    mesh_path = os.path.join(folder, "DoorBarricade.mesh.xml")
    skeleton, assignment_of = barricade(ET.parse(mesh_path).getroot())
    with open(os.path.join(folder, "DoorBarricade.skeleton.xml"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write(skeleton.to_xml())
    rewrite_mesh(mesh_path, "DoorBarricade.skeleton", lambda index: assignment_of[index])
    print("wrote DoorBarricade", len(skeleton.bones), "bones", [clip[0] for clip in skeleton.clips])
    return 0


if __name__ == "__main__":
    sys.exit(main())
