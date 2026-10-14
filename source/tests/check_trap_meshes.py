#!/usr/bin/env python3
"""Static check that every trap type has its own mesh (no compiler needed).

    python source/tests/check_trap_meshes.py

Reads the mesh name of every trap in source/traps/Trap*.cpp (and of every door type in TrapDoor.cpp), checks that no
two types share a mesh, that the .mesh file exists, that all its materials are defined in a material script and that
the own meshes are listed in CREDITS.
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
problems = []

TRAPS = {
    "Cannon": "Cannon",
    "Spike": "Spiketrap",
    "Boulder": "Boulder",
    "Alarm": "AlarmTrap",
    "Fear": "FearTrap",
    "Gas": "GasTrap",
    "Lightning": "LightningTrap",
    "Fireburst": "FireburstTrap",
    "Freeze": "FrostTrap",
    "Trigger": "TriggerTrap",
}
# The watch banner is added to the list of traps by the change that gives it its own mesh
DOORS = ("WoodenDoor", "DoorIronbound", "DoorSteel", "DoorBarricade", "DoorSecret", "DoorRune")


def read(*parts, binary=False):
    with open(os.path.join(ROOT, *parts), "rb" if binary else "r", **({} if binary else {"encoding": "utf-8"})) as handle:
        return handle.read()


credits = read("CREDITS")
material_text = "".join(read("materials", "scripts", os.path.basename(p))
                        for p in glob.glob(os.path.join(ROOT, "materials", "scripts", "*.material")))
defined = set(re.findall(r"^\s*material\s+(\S+)", material_text, re.M))

seen = {}
for trap, expected in TRAPS.items():
    source = read("source", "traps", "Trap%s.cpp" % trap)
    match = re.search(r'getMeshName\(\) const override\s*\{(?:\s*//[^\n]*)*\s*static const std::string \w+ = "([^"]+)";', source)
    if not match:
        problems.append("Trap%s.cpp: no mesh name found" % trap)
        continue
    mesh = match.group(1)
    if mesh != expected:
        problems.append("Trap%s uses mesh %s, expected %s" % (trap, mesh, expected))
    if mesh in seen:
        problems.append("Trap%s shares the mesh %s with Trap%s" % (trap, mesh, seen[mesh]))
    seen[mesh] = trap
    path = os.path.join(ROOT, "models", mesh + ".mesh")
    if not os.path.exists(path):
        problems.append("models/%s.mesh is missing" % mesh)
        continue
    data = read("models", mesh + ".mesh", binary=True)
    # own static meshes (the ones made by trap_meshes.py) use materials of TrapTypes.material
    if mesh.endswith("Trap") and mesh != "Spiketrap":
        used = [m for m in defined if m.startswith("Trap") and m.encode() in data]
        if not used:
            problems.append("models/%s.mesh uses none of the TrapTypes materials" % mesh)
        if b".skeleton" in data:
            problems.append("models/%s.mesh links a skeleton, it should be static" % mesh)
        if ("models/%s.mesh" % mesh) not in credits:
            problems.append("models/%s.mesh has no CREDITS entry" % mesh)

door_source = read("source", "traps", "TrapDoor.cpp")
door_meshes = re.findall(r'static const std::string mesh\w+ = "([^"]+)";', door_source)
if sorted(door_meshes) != sorted(DOORS):
    problems.append("TrapDoor.cpp mesh names %s differ from %s" % (sorted(door_meshes), sorted(DOORS)))
for mesh in DOORS:
    if not os.path.exists(os.path.join(ROOT, "models", mesh + ".mesh")):
        problems.append("models/%s.mesh is missing" % mesh)

for name in ("AlarmTrap", "FearTrap", "GasTrap", "LightningTrap", "FireburstTrap", "FrostTrap", "TriggerTrap"):
    if not os.path.exists(os.path.join(ROOT, "models", name + ".mesh")):
        problems.append("models/%s.mesh is missing" % name)

if problems:
    print("check_trap_meshes: %d problem(s)" % len(problems))
    for line in problems:
        print("  " + line)
    sys.exit(1)
print("check_trap_meshes: ok (%d trap types and %d door types, every type has its own mesh)" % (len(TRAPS), len(DOORS)))

