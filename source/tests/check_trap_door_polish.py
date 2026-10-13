#!/usr/bin/env python3
"""Static check of the own meshes of the door types (no compiler needed).

    python source/tests/check_trap_door_polish.py

Checks that every door type has its own mesh file, that TrapDoor.cpp uses it, that its materials are defined and that
every texture exists and has a CREDITS line.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


problems = []
credits = read("CREDITS")

# Own meshes of the door types: file, assignment in TrapDoor.cpp, materials and textures
trap_door = read("source", "traps", "TrapDoor.cpp")
material_text = read("materials", "scripts", "DoorTypes.material")
for mesh in ("DoorIronbound", "DoorSteel", "DoorBarricade", "DoorSecret", "DoorRune"):
    if not os.path.exists(os.path.join(ROOT, "models", mesh + ".mesh")):
        problems.append("models/%s.mesh is missing" % mesh)
    if ('std::string mesh%s = "%s"' % (mesh[4:], mesh)) not in trap_door:
        problems.append("TrapDoor.cpp does not use the mesh %s" % mesh)
for material in ("DoorIronboundWood", "DoorMetal", "DoorSteel", "DoorBarricade", "DoorSecret", "DoorRune", "DoorRuneGlow"):
    if "material %s" % material not in material_text:
        problems.append("material %s is not defined" % material)
    if "textures/%s.png" % material not in credits:
        problems.append("texture %s has no CREDITS line" % material)
for texture in re.findall(r"^\s*texture\s+(\S+)", material_text, re.M):
    if not os.path.exists(os.path.join(ROOT, "materials", "textures", texture)):
        problems.append("texture %s is missing" % texture)

if problems:
    for problem in problems:
        print("PROBLEM: " + problem)
    sys.exit(1)
print("fine")
