#!/usr/bin/env python3
"""Checks config/roomAmbience.cfg (and the files it includes) without starting the game.

    python tools/check_room_ambience.py

Checks that the tags are balanced, that keys and values can be read, that every particle system used
exists in particles/*.particle with a material that has its texture, and that the mesh names and tile
visual names that are matched exist (names of rooms that only some builds have are accepted). Exits
with 1 and prints every problem if something is wrong.
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SETTINGS = ("ScanInterval", "MaxParticles", "MaxParticlesReduced", "MaxMotions", "MaxOneShots", "OccupiedRadius",
            "ReducedDistanceFactor")
EFFECT_KEYS = ("Name", "Target", "Match", "When", "Event", "Kind", "System", "Motion", "After", "Offset", "Axis",
               "Amount", "Speed", "Flicker", "Duration", "Chance", "Spacing", "MaxDistance", "Priority", "Reduced",
               "NeedWall", "Clips", "Every")
TARGETS = ("Object", "Tile", "Event")
WHENS = ("Always", "Occupied", "Empty")
KINDS = ("Particle", "Motion", "Clip")
MOTIONS = ("Sway", "Wobble", "Spin", "Bob", "Pulse", "Flicker")
# Room tile visuals that only some builds have
OPTIONAL_VISUALS = ("guardRoom", "templeRoom")
# Tile visuals a bridge can lie over
BRIDGE_VISUALS = ("lavaGround", "waterGround")


def read_lines(path):
    lines = []
    with open(path, encoding="utf-8") as handle:
        for raw in handle:
            words = raw.split("#", 1)[0].split()
            if words:
                lines.append(words)
    return lines


def is_number(text):
    try:
        float(text)
        return True
    except ValueError:
        return False


def tile_visuals():
    names = set(OPTIONAL_VISUALS)
    with open(os.path.join(ROOT, "source", "entities", "Tile.cpp"), encoding="utf-8") as handle:
        for match in re.finditer(r'return "([A-Za-z]+)";', handle.read()):
            names.add(match.group(1))
    return names


def particle_systems():
    systems = {}
    for path in glob.glob(os.path.join(ROOT, "particles", "*.particle")):
        with open(path, encoding="utf-8") as handle:
            text = handle.read()
        for match in re.finditer(r"particle_system\s+(\S+)\s*\{(.*?)\n\}", text, re.S):
            material = re.search(r"^\s*material\s+(\S+)", match.group(2), re.M)
            systems[match.group(1)] = material.group(1) if material else None
    return systems


def materials():
    result = {}
    for path in glob.glob(os.path.join(ROOT, "materials", "scripts", "*.material")):
        with open(path, encoding="utf-8") as handle:
            text = handle.read()
        for match in re.finditer(r"^material\s+(\S+)", text, re.M):
            start = match.end()
            nxt = re.search(r"^material\s", text[start:], re.M)
            body = text[start:start + nxt.start()] if nxt else text[start:]
            textures = re.findall(r"^\s*texture\s+(\S+)", body, re.M)
            result[match.group(1)] = textures
    return result


SYSTEM_KEYS = ("material", "point_rendering", "particle_width", "particle_height", "cull_each", "quota",
               "billboard_type", "common_direction", "common_up_vector", "billboard_origin", "sorted")
EMITTER_KEYS = ("angle", "colour", "colour_range_start", "colour_range_end", "direction", "emission_rate", "position",
                "velocity", "velocity_min", "velocity_max", "time_to_live", "time_to_live_min", "time_to_live_max",
                "duration", "duration_min", "duration_max", "repeat_delay", "repeat_delay_min", "repeat_delay_max",
                "width", "height", "depth", "inner_width", "inner_height", "inner_depth")
EMITTERS = ("Point", "Box", "Ring")
AFFECTOR_KEYS = {
    "ColourFader": ("red", "green", "blue", "alpha"),
    "ColourInterpolator": tuple("time%d" % i for i in range(6)) + tuple("colour%d" % i for i in range(6)),
    "Scaler": ("rate",),
    "Rotator": ("rotation_speed_range_start", "rotation_speed_range_end", "rotation_range_start", "rotation_range_end"),
    "LinearForce": ("force_vector", "force_application"),
    "DirectionRandomiser": ("randomness", "scope", "keep_velocity"),
}
BILLBOARD_TYPES = ("point", "oriented_common", "oriented_self", "perpendicular_common", "perpendicular_self")


def check_particle_files(problems):
    """Looks for misspelled keys in the particle files of the room ambience (OGRE only logs them when it loads them)."""
    for path in sorted(glob.glob(os.path.join(ROOT, "particles", "RoomAmbience*.particle"))):
        base = os.path.basename(path)
        context = []
        pending = None
        with open(path, encoding="utf-8") as handle:
            for number, raw in enumerate(handle, 1):
                line = raw.split("//", 1)[0].strip()
                if not line:
                    continue
                where = "%s:%d" % (base, number)
                if line == "{":
                    context.append(pending)
                    pending = None
                    continue
                if line == "}":
                    context.pop()
                    continue
                words = line.split()
                if words[0] == "particle_system":
                    pending = ("system",)
                elif words[0] == "emitter":
                    pending = ("emitter",)
                    if len(words) < 2 or words[1] not in EMITTERS:
                        problems.append("%s: unknown emitter %s" % (where, line))
                elif words[0] == "affector":
                    kind = words[1] if len(words) > 1 else "?"
                    pending = ("affector", kind)
                    if kind not in AFFECTOR_KEYS:
                        problems.append("%s: unknown affector %s" % (where, kind))
                elif context:
                    scope = context[-1]
                    if scope == ("system",):
                        if words[0] not in SYSTEM_KEYS:
                            problems.append("%s: unknown system key %s" % (where, words[0]))
                        elif words[0] == "billboard_type" and words[1] not in BILLBOARD_TYPES:
                            problems.append("%s: unknown billboard type %s" % (where, words[1]))
                    elif scope == ("emitter",):
                        if words[0] not in EMITTER_KEYS:
                            problems.append("%s: unknown emitter key %s" % (where, words[0]))
                    elif scope and scope[0] == "affector":
                        if words[0] not in AFFECTOR_KEYS.get(scope[1], ()):
                            problems.append("%s: unknown key %s for affector %s" % (where, words[0], scope[1]))


def mesh_exists(name):
    return os.path.exists(os.path.join(ROOT, "models", name + ".mesh"))


def check_effect(effect, where, problems, visuals, systems, mats, counts):
    for key in effect:
        if key not in EFFECT_KEYS:
            problems.append("%s: unknown key %s" % (where, key))
    name = effect.get("Name", ["?"])[0]
    where = "%s (%s)" % (where, name)
    target = effect.get("Target", ["?"])[0]
    if target not in TARGETS:
        problems.append("%s: bad Target %s" % (where, target))
        return
    kind = effect.get("Kind", ["?"])[0]
    if kind not in KINDS:
        problems.append("%s: bad Kind %s" % (where, kind))
    when = effect.get("When", ["Always"])[0]
    if when not in WHENS:
        problems.append("%s: bad When %s" % (where, when))
    if target == "Event":
        counts["events"] += 1
        if "Event" not in effect:
            problems.append("%s: event effect without Event" % where)
        if kind != "Particle":
            problems.append("%s: event effects must be particles" % where)
    else:
        if "Match" not in effect:
            problems.append("%s: no Match" % where)
    if kind == "Particle":
        system = effect.get("System", [None])[0]
        if system is None:
            problems.append("%s: particle without System" % where)
        elif system not in systems:
            problems.append("%s: unknown particle system %s" % (where, system))
        else:
            material = systems[system]
            if material not in mats:
                problems.append("%s: particle system %s uses unknown material %s" % (where, system, material))
            else:
                for texture in mats[material]:
                    if not os.path.exists(os.path.join(ROOT, "materials", "textures", texture)):
                        problems.append("%s: material %s uses missing texture %s" % (where, material, texture))
    elif kind == "Motion":
        bridges = target == "Tile" and all(m.startswith("bridge:") for m in effect.get("Match", []))
        if target != "Object" and not bridges:
            problems.append("%s: motions only work on objects and bridges" % where)
        motion = effect.get("Motion", [None])[0]
        if motion not in MOTIONS:
            problems.append("%s: bad Motion %s" % (where, motion))
    elif kind == "Clip":
        if target != "Object":
            problems.append("%s: clips only work on objects" % where)
        if not effect.get("Clips"):
            problems.append("%s: clip effect without Clips" % where)
    for key in ("After", "Amount", "Speed", "Flicker", "Duration", "Every", "Chance", "Spacing", "MaxDistance", "Priority"):
        if key in effect and not is_number(effect[key][0]):
            problems.append("%s: %s is not a number" % (where, key))
    for key in ("Offset", "Axis"):
        if key in effect and (len(effect[key]) != 3 or not all(is_number(v) for v in effect[key])):
            problems.append("%s: %s needs three numbers" % (where, key))
    for key in ("Reduced", "NeedWall"):
        if key in effect and effect[key][0] not in ("yes", "no", "true", "false", "1", "0"):
            problems.append("%s: %s needs yes or no" % (where, key))
    for match in effect.get("Match", []):
        if target == "Object" and match.startswith("trap:"):
            if not re.match(r"^[A-Za-z*]+$", match[len("trap:"):]):
                problems.append("%s: bad trap type %s" % (where, match))
        elif target == "Object":
            if "*" not in match and not mesh_exists(match):
                problems.append("%s: no mesh %s" % (where, match))
            elif "*" in match:
                pattern = match.replace("*", "")
                if not any(f.startswith(pattern) or f.endswith(pattern + ".mesh") for f in os.listdir(os.path.join(ROOT, "models"))):
                    problems.append("%s: wildcard %s matches no mesh" % (where, match))
        elif match.startswith("bridge:"):
            name = match[len("bridge:"):]
            if name not in BRIDGE_VISUALS and not mesh_exists(name):
                problems.append("%s: unknown bridge mesh or visual %s" % (where, name))
        elif match not in visuals:
            problems.append("%s: unknown tile visual %s" % (where, match))
    counts["effects"] += 1


def check_file(path, problems, visuals, systems, mats, counts):
    lines = read_lines(path)
    base = os.path.basename(path)
    if not lines or lines[0] != ["[RoomAmbience]"]:
        problems.append("%s: must start with [RoomAmbience]" % base)
        return
    i = 1
    closed = False
    while i < len(lines):
        words = lines[i]
        i += 1
        if words == ["[/RoomAmbience]"]:
            closed = True
            break
        if words == ["[Settings]"]:
            while i < len(lines) and lines[i] != ["[/Settings]"]:
                if lines[i][0] not in SETTINGS or len(lines[i]) < 2 or not is_number(lines[i][1]):
                    problems.append("%s: bad setting %s" % (base, " ".join(lines[i])))
                i += 1
            i += 1
        elif words == ["[Effects]"]:
            while i < len(lines) and lines[i] != ["[/Effects]"]:
                if lines[i] != ["[Effect]"]:
                    problems.append("%s: expected [Effect], got %s" % (base, " ".join(lines[i])))
                    i += 1
                    continue
                i += 1
                effect = {}
                while i < len(lines) and lines[i] != ["[/Effect]"]:
                    if len(lines[i]) < 2:
                        problems.append("%s: key without value %s" % (base, lines[i][0]))
                    else:
                        effect[lines[i][0]] = lines[i][1:]
                    i += 1
                i += 1
                check_effect(effect, base, problems, visuals, systems, mats, counts)
            i += 1
        elif words[0] == "Include" and len(words) == 2:
            included = os.path.join(os.path.dirname(path), words[1])
            if os.path.exists(included):
                check_file(included, problems, visuals, systems, mats, counts)
        else:
            problems.append("%s: unexpected %s" % (base, " ".join(words)))
    if not closed:
        problems.append("%s: missing [/RoomAmbience]" % base)


def main():
    problems = []
    counts = {"effects": 0, "events": 0}
    check_file(os.path.join(ROOT, "config", "roomAmbience.cfg"), problems, tile_visuals(), particle_systems(),
               materials(), counts)
    check_particle_files(problems)
    if problems:
        for problem in problems:
            print("PROBLEM:", problem)
        return 1
    print("fine (%d effects, %d of them events)" % (counts["effects"], counts["events"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
