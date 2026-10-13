#!/usr/bin/env python3
"""Checks config/creatureReactions.cfg without starting the game.

    python tools/check_creature_reactions.py

Checks that the tags are balanced, that the values can be read, that the creatures of the groups and
variants exist in config/creatures.cfg and that every emote and effect used has a material, a texture
and a particle system. Exits with 1 and prints every problem if something is wrong.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRIORITIES = ("death", "combat", "held", "event", "work", "mood", "ambient")
JOBS = ("Fighter", "Worker")
MOTIONS = ("hop", "shake", "squash", "spin", "turn")
SETTINGS = ("MaxSimultaneous", "MaxCameraDistance", "GroupStaggerMin", "GroupStaggerMax", "DefaultGroup")
EVENT_KEYS = ("Name", "Priority", "Cooldown", "Probability", "GroupMax")
VARIANT_KEYS = ("Name", "Weight", "Clip", "Fallback", "Emote", "Effect", "Motion", "Cooldown", "Probability",
                "Creatures", "Groups", "Jobs", "RequiresSleepNeed")


def read_lines(path):
    """Returns the lines of the file as lists of words, without comments and empty lines."""
    lines = []
    with open(path, encoding="utf-8") as handle:
        for raw in handle:
            words = raw.split("#", 1)[0].split()
            if words:
                lines.append(words)
    return lines


def is_number(text):
    """True if the text can be read as a number."""
    try:
        float(text)
        return True
    except ValueError:
        return False


def creature_names():
    """Returns the names of all creatures of config/creatures.cfg."""
    names = set()
    with open(os.path.join(ROOT, "config", "creatures.cfg"), encoding="utf-8") as handle:
        for raw in handle:
            words = raw.split("#", 1)[0].split()
            if len(words) >= 2 and words[0] == "Name":
                names.add(words[1])
    return names


def collect(pattern, directory, extension):
    """Returns what the regular expression finds in all files of the directory with this extension."""
    found = set()
    for name in os.listdir(os.path.join(ROOT, directory)):
        if not name.endswith(extension):
            continue
        with open(os.path.join(ROOT, directory, name), encoding="utf-8") as handle:
            found.update(re.findall(pattern, handle.read(), re.MULTILINE))
    return found


def main():
    """Checks the whole file, prints every problem and returns the exit code."""
    errors = []
    lines = read_lines(os.path.join(ROOT, "config", "creatureReactions.cfg"))
    creatures = creature_names()
    materials = collect(r"^\s*material\s+(\S+)", os.path.join("materials", "scripts"), ".material")
    particles = collect(r"^\s*particle_system\s+(\S+)", "particles", ".particle")

    def error(text):
        """Remembers one problem."""
        errors.append(text)

    if not lines or lines[0] != ["[CreatureReactions]"] or lines[-1] != ["[/CreatureReactions]"]:
        error("the file must start with [CreatureReactions] and end with [/CreatureReactions]")
        return finish(errors)

    groups = {}
    events = {}
    index = 1
    block = None
    event = None
    variant = None
    group = None
    while index < len(lines) - 1:
        words = lines[index]
        index += 1
        key = words[0]
        if key in ("[Settings]", "[Groups]", "[Events]"):
            if block is not None:
                error("%s inside %s" % (key, block))
            block = key
            continue
        if key in ("[/Settings]", "[/Groups]", "[/Events]"):
            if block != key.replace("/", ""):
                error("%s without %s" % (key, key.replace("/", "")))
            block = None
            continue

        if block == "[Settings]":
            if key not in SETTINGS:
                error("unknown setting %s" % key)
            elif len(words) < 2 or (key != "DefaultGroup" and not is_number(words[1])):
                error("setting %s needs a value" % key)
        elif block == "[Groups]":
            if key == "[Group]":
                group = {"Name": None, "Creatures": []}
            elif key == "[/Group]":
                if not group or not group["Name"]:
                    error("group without Name")
                else:
                    groups[group["Name"]] = group["Creatures"]
                group = None
            elif group is None:
                error("%s outside [Group]" % key)
            elif key == "Name" and len(words) >= 2:
                group["Name"] = words[1]
            elif key == "Creatures":
                group["Creatures"] = words[1:]
                for name in words[1:]:
                    if name not in creatures:
                        error("group %s: unknown creature %s" % (group["Name"], name))
            else:
                error("unknown group key %s" % key)
        elif block == "[Events]":
            if key == "[Event]":
                event = {"Name": None, "variants": []}
            elif key == "[/Event]":
                if not event or not event["Name"]:
                    error("event without Name")
                elif event["Name"] in events:
                    error("event %s defined twice" % event["Name"])
                elif not event["variants"]:
                    error("event %s has no variant" % event["Name"])
                else:
                    events[event["Name"]] = event
                event = None
            elif key == "[Variant]":
                variant = {}
            elif key == "[/Variant]":
                if variant is None or event is None:
                    error("[/Variant] outside an event")
                else:
                    event["variants"].append(variant)
                variant = None
            elif variant is not None:
                check_variant(key, words, variant, event, creatures, groups, materials, particles, error)
            elif event is not None:
                if key not in EVENT_KEYS:
                    error("unknown event key %s" % key)
                elif len(words) < 2:
                    error("event key %s needs a value" % key)
                elif key == "Name":
                    event["Name"] = words[1]
                elif key == "Priority" and words[1] not in PRIORITIES:
                    error("event %s: unknown priority %s" % (event["Name"], words[1]))
                elif key in ("Cooldown", "Probability", "GroupMax") and not is_number(words[1]):
                    error("event %s: %s must be a number" % (event["Name"], key))
                elif key == "Probability" and not 0.0 <= float(words[1]) <= 1.0:
                    error("event %s: Probability must be between 0 and 1" % event["Name"])
            else:
                error("%s outside [Event]" % key)
        else:
            error("%s outside a block" % key)

    if block is not None or event is not None or variant is not None or group is not None:
        error("a block, event, variant or group is not closed")

    check_material_lookups(error)

    default_group = None
    for words in lines:
        if words[0] == "DefaultGroup" and len(words) >= 2:
            default_group = words[1]
    if default_group is not None and default_group not in groups:
        error("DefaultGroup %s is not a defined group" % default_group)

    return finish(errors, len(events))


def check_variant(key, words, variant, event, creatures, groups, materials, particles, error):
    """Checks one line of a variant: known key, value types and that icons, effects, creatures, groups and jobs exist."""
    where = "event %s" % (event["Name"] if event else "?")
    if key not in VARIANT_KEYS:
        error("%s: unknown variant key %s" % (where, key))
        return
    if len(words) < 2:
        error("%s: variant key %s needs a value" % (where, key))
        return
    variant[key] = words[1:]
    if key in ("Weight", "Cooldown", "Probability") and not is_number(words[1]):
        error("%s: %s must be a number" % (where, key))
    if key == "Weight" and is_number(words[1]) and float(words[1]) <= 0:
        error("%s: Weight must be above 0" % where)
    if key == "Probability" and is_number(words[1]) and not 0.0 <= float(words[1]) <= 1.0:
        error("%s: Probability must be between 0 and 1" % where)
    if key in ("Clip", "Fallback") and not all(is_number(w) for w in words[2:]):
        error("%s: %s speed and range must be numbers" % (where, key))
    if key == "Fallback" and len(words) in (4,):
        error("%s: Fallback needs both start and end" % where)
    if key == "Emote":
        if "CreatureEmote_" + words[1] not in materials:
            error("%s: no material CreatureEmote_%s" % (where, words[1]))
        elif not os.path.exists(os.path.join(ROOT, "materials", "textures", "CreatureEmote%s.png" % words[1])):
            error("%s: no texture for emote %s" % (where, words[1]))
        if len(words) >= 3 and not is_number(words[2]):
            error("%s: Emote time must be a number" % where)
    if key == "Effect":
        if words[1] not in particles:
            error("%s: no particle system %s" % (where, words[1]))
        if len(words) >= 3 and not is_number(words[2]):
            error("%s: Effect time must be a number" % where)
    if key == "Motion" and (words[1] not in MOTIONS or len(words) < 5 or not all(is_number(w) for w in words[2:5])):
        error("%s: Motion must be '<%s> <count> <amount> <seconds>'" % (where, "|".join(MOTIONS)))
    if key == "Creatures":
        for name in words[1:]:
            if name not in creatures:
                error("%s: unknown creature %s" % (where, name))
    if key == "Groups":
        for name in words[1:]:
            if name not in groups:
                error("%s: unknown group %s" % (where, name))
    if key == "Jobs":
        for name in words[1:]:
            if name not in JOBS:
                error("%s: unknown job %s" % (where, name))
    if key == "RequiresSleepNeed" and words[1] not in ("yes", "no"):
        error("%s: RequiresSleepNeed must be yes or no" % where)


def check_material_lookups(error):
    """The materials live in the resource group Graphics; a lookup without the group never finds them."""
    path = os.path.join(ROOT, "source", "render", "CreatureReactions.cpp")
    with open(path, encoding="utf-8") as source:
        text = source.read()
    for found in re.finditer(r"MaterialManager::getSingleton\(\)\.resourceExists\(([^;]*?)\)\)", text):
        if "," not in found.group(1):
            error("CreatureReactions.cpp: MaterialManager resourceExists(%s) needs the group \"Graphics\"" % found.group(1))


def finish(errors, nb_events=0):
    """Prints the problems (or the summary) and returns the exit code."""
    if errors:
        for text in errors:
            print("ERROR: " + text)
        return 1
    print("creatureReactions.cfg is fine (%d events)" % nb_events)
    return 0


if __name__ == "__main__":
    sys.exit(main())
