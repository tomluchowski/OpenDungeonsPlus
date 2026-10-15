#!/usr/bin/env python3
"""Static check of the door skeletons (no compiler needed).

    python source/tests/check_door_final.py

Checks that every door model links to its own skeleton with the right clips and that the skeletons are in CREDITS.
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts, binary=False):
    """Returns the content of a file below the repository root, as bytes when binary is true."""
    with open(os.path.join(ROOT, *parts), "rb" if binary else "r", **({} if binary else {"encoding": "utf-8"})) as handle:
        return handle.read()


problems = []
credits = read("CREDITS")

DOORS = {
    "DoorIronbound": (b"Open", b"Close", b"Destroyed"),
    "DoorSteel": (b"Open", b"Close", b"Destroyed"),
    "DoorSecret": (b"Open", b"Close", b"Destroyed"),
    "DoorRune": (b"Open", b"Close", b"Destroyed"),
    "DoorBarricade": (b"Collapse", b"Destroyed"),
}
for name, clips in DOORS.items():
    mesh = read("models", name + ".mesh", binary=True)
    if (name + ".skeleton").encode() not in mesh:
        problems.append("%s.mesh does not link to its own skeleton" % name)
    if b"WoodenDoor.skeleton" in mesh:
        problems.append("%s.mesh still uses the shared wooden door skeleton" % name)
    skeleton_path = os.path.join(ROOT, "models", name + ".skeleton")
    if not os.path.exists(skeleton_path):
        problems.append("models/%s.skeleton is missing" % name)
        continue
    skeleton = read("models", name + ".skeleton", binary=True)
    for clip in clips:
        if clip not in skeleton:
            problems.append("%s.skeleton lacks the clip %s" % (name, clip.decode()))
    if "models/%s.skeleton" % name not in credits:
        problems.append("models/%s.skeleton is not in CREDITS" % name)
if "tools/blender-assets/door_skeletons.py" not in credits:
    problems.append("door_skeletons.py is not in CREDITS")
if b"Collapse" not in read("models", "WoodenDoor.skeleton", binary=True):
    problems.append("WoodenDoor.skeleton lost its clips")

if problems:
    for problem in problems:
        print("PROBLEM: " + problem)
    sys.exit(1)
print("fine")
