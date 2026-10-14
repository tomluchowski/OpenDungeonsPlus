#!/usr/bin/env python3
"""Static check of the sleep and wake clips of beds, crypt objects and creatures (no compiler needed).

    python source/tests/check_wake_clips.py

Checks that config/roomAmbienceWakeClips.cfg is included and has the effects with the agreed clip names (beds and
crypt objects: Sleep as a loop and Wake once; creatures: Stretch once), that the events are the existing ones
(CreatureWoke, CryptRaised) and that the code plays a clip only if the skeleton has it, never changes a position, a
path or an animation state on the server and sends nothing over the network. Then runs the same checks on texts
with one thing taken away each (a counter test): every one of them must be found, otherwise the check itself is
wrong and fails.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


def effects(text):
    """Returns the [Effect] blocks of a room ambience file as a dictionary name -> {key: words}."""
    result = {}
    current = None
    for raw in text.splitlines():
        words = raw.split("#", 1)[0].split()
        if not words:
            continue
        if words[0] == "[Effect]":
            current = {}
        elif words[0] == "[/Effect]":
            result[current["Name"][0]] = current
            current = None
        elif current is not None and len(words) > 1:
            current[words[0]] = words[1:]
    return result


BEDS = ["*Bed", "Hammock"]
CRYPT_OBJECTS = ["StoneCoffin", "KnightCoffin", "KnightStatue", "KnightStatue2"]
CRYPT_ALL = set(CRYPT_OBJECTS)


def run_checks(texts):
    """Returns the list of problems found in the given texts (a dictionary name -> text)."""
    problems = []

    def need(condition, text):
        if not condition:
            problems.append(text)

    cfg = effects(texts["clips"])
    main_cfg = texts["main"]
    source = texts["source"]
    header = texts["header"]
    config_h = texts["config_h"]
    config_cpp = texts["config_cpp"]
    render_h = texts["render_h"]
    render_cpp = texts["render_cpp"]
    checker = texts["checker"]

    need("Include roomAmbienceWakeClips.cfg" in main_cfg, "roomAmbienceWakeClips.cfg is not included")

    # --- the effects and their clip names
    def effect(name):
        found = cfg.get(name)
        need(found is not None, "effect %s is missing" % name)
        return found or {}

    bed = effect("DormitoryBedSleepClip")
    need(bed.get("Kind") == ["Clip"] and bed.get("Clips") == ["Sleep"], "the bed sleeps with the clip Sleep")
    need(bed.get("Loop") == ["yes"] and bed.get("When") == ["Sleeping"], "the bed clip loops while a creature sleeps")
    need(set(bed.get("Match", [])) == {"*Bed", "Hammock"}, "the bed clip matches the beds (*Bed Hammock)")

    wake = effect("DormitoryWakeBedClip")
    need(wake.get("Target") == ["Event"] and wake.get("Event") == ["CreatureWoke"], "the bed wakes on CreatureWoke")
    need(wake.get("Kind") == ["Clip"] and wake.get("Clips") == ["Wake"], "the bed wakes with the clip Wake")
    need(set(wake.get("Object", [])) == {"*Bed", "Hammock"}, "the wake clip is played on the beds")
    need("Amount" in wake and "Loop" not in wake, "the wake clip needs a radius and plays once")

    stretch = effect("DormitoryWakeStretch")
    need(stretch.get("Event") == ["CreatureWoke"], "the stretch starts on CreatureWoke, with the bed")
    need(stretch.get("Kind") == ["CreatureClip"] and stretch.get("Clips") == ["Stretch"],
         "the creature stretches with the clip Stretch")

    crypt = effect("CryptSleepClip")
    need(crypt.get("Kind") == ["Clip"] and crypt.get("Clips") == ["Sleep"] and crypt.get("Loop") == ["yes"],
         "the crypt objects loop the clip Sleep")
    need(set(crypt.get("Match", [])) == CRYPT_ALL, "the crypt clip matches the coffins and the statues")

    raised = effect("CryptWakeClip")
    need(raised.get("Event") == ["CryptRaised"] and raised.get("Clips") == ["Wake"], "the crypt wakes with Wake on CryptRaised")
    need(set(raised.get("Object", [])) == CRYPT_ALL, "the crypt wake clip is played on the coffins and the statues")

    # The wake events are the ones of the effects that already exist
    need(effect("DormitoryWakeBedClip").get("Match") == ["dormitoryRoom"], "the wake clip belongs to the dormitory")
    gaps = texts["gaps"]
    need("Name        DormitoryWakeBlanket" in gaps and "Name        DormitoryWakeLight" in gaps,
         "the effects DormitoryWakeBlanket and DormitoryWakeLight must stay")
    for name in ("DormitoryBedSleepClip", "DormitoryWakeBedClip", "DormitoryWakeStretch", "CryptSleepClip", "CryptWakeClip"):
        need(cfg.get(name, {}).get("Reduced") == ["yes"], "%s is also shown in the mode reduced" % name)

    # --- the format is known to the code and to the config check
    for word in ("Object", "Loop", "Sleeping", "CreatureClip"):
        need('"%s"' % word in config_cpp, "RoomAmbienceConfig.cpp does not read %s" % word)
        need(word in checker, "tools/check_room_ambience.py does not know %s" % word)
        need(word in main_cfg, "config/roomAmbience.cfg does not describe %s" % word)
    need("creatureClip" in config_h and "sleeping" in config_h and "mObjects" in config_h and "mLoop" in config_h,
         "RoomAmbienceConfig.h lacks the new members")

    # --- code: events, guards, nothing for the server
    for name in ("playEventClip", "playCreatureClip", "stopLoopClip", "isSleeperNear", "hasClip"):
        need(re.search(r"\b%s\(" % name, header), "%s not declared" % name)
        need("RoomAmbience::%s(" % name in source, "%s not defined" % name)
    need("const std::string& creatureName" in header, "triggerEvent lacks the creature of the event")
    need("rrHasObjectClip" in render_h and "RenderManager::rrHasObjectClip" in render_cpp, "rrHasObjectClip missing")
    need("hasClip(" in source[source.index("bool RoomAmbience::playEventClip"):source.index("void RoomAmbience::playSound")],
         "an event clip must only be played when the skeleton has it")
    need(source.count("hasClip(") >= 4, "the loop clip, the event clip and the creature clip must ask hasClip")
    creature_part = source[source.index("bool RoomAmbience::playCreatureClip"):source.index("void RoomAmbience::playSound")]
    need("isMoving()" in creature_part, "a creature on its way must not be given a clip (it would be played after the walk)")
    need("OD_LOG" not in creature_part and "OD_LOG" not in source[source.index("bool RoomAmbience::playEventClip"):source.index("bool RoomAmbience::playCreatureClip")],
         "a missing clip must not be logged")
    need('"CreatureWoke"' in source and "change.mCreature" in source, "the creature of the event must be handed to triggerEvent")
    need(source.index("scanObjects(camera, cameraPosition);") < source.index("scanCreatureEvents();"),
         "the objects are scanned before the events, so the loop stops before the wake clip starts")
    need("mClipHoldUntil" in source, "the loop must wait while a wake clip plays")
    client_part = source[source.index("bool RoomAmbience::playEventClip"):source.index("void RoomAmbience::playSound")]
    need("ServerNotification" not in client_part and "ODServer" not in client_part and "ODClient" not in client_part,
         "the clips are client side only")
    return problems


def load():
    return {
        "clips": read("config", "roomAmbienceWakeClips.cfg"),
        "main": read("config", "roomAmbience.cfg"),
        "gaps": read("config", "roomAmbienceGaps.cfg"),
        "source": read("source", "render", "RoomAmbience.cpp"),
        "header": read("source", "render", "RoomAmbience.h"),
        "config_h": read("source", "render", "RoomAmbienceConfig.h"),
        "config_cpp": read("source", "render", "RoomAmbienceConfig.cpp"),
        "render_h": read("source", "render", "RenderManager.h"),
        "render_cpp": read("source", "render", "RenderManager.cpp"),
        "checker": read("tools", "check_room_ambience.py"),
    }


def mutations():
    """One thing is taken away from the texts each; the check must go red for every one."""
    def swap(key, old, new):
        return key, old, new

    return [
        ("effect missing", swap("clips", "Name        DormitoryWakeStretch", "Name        DormitoryWakeStretchGone")),
        ("bed clip name", swap("clips", "Clips       Wake\n            Amount      2.5", "Clips       Awake\n            Amount      2.5")),
        ("creature clip name", swap("clips", "Clips       Stretch", "Clips       Stretching")),
        ("crypt sleep name", swap("clips", "Name        CryptSleepClip", "Name        CryptRestClip")),
        ("crypt event", swap("clips", "Event       CryptRaised", "Event       CryptRisen")),
        ("not included", swap("main", "Include roomAmbienceWakeClips.cfg", "")),
        ("checker does not know the kind", swap("checker", "CreatureClip", "Creature_Clip")),
        ("clip guard", swap("source", "if(!hasClip(nearest, clip))", "if(false)")),
        ("walking creature", swap("source", "creature->isMoving())\n        return false;", "false)\n        return false;")),
        ("server code", swap("source", "bool RoomAmbience::playCreatureClip(", "bool RoomAmbience::playCreatureClip(ODServer* server, ")),
    ]


def main():
    texts = load()
    for key in texts:
        texts[key] = texts[key].replace("\r\n", "\n")
    problems = run_checks(texts)
    if problems:
        for problem in problems:
            print("PROBLEM:", problem)
        return 1

    # The counter test: a text that is wrong must be found
    failed = False
    for label, (key, old, new) in mutations():
        changed = dict(texts)
        if old not in changed[key]:
            print("PROBLEM: counter test %s: the text to change is not there" % label)
            failed = True
            continue
        changed[key] = changed[key].replace(old, new)
        try:
            found = run_checks(changed)
        except ValueError:
            # A part of the code that the check cuts out is gone: found as well
            found = ["part of the code missing"]
        if not found:
            print("PROBLEM: counter test %s: the check did not go red" % label)
            failed = True
    if failed:
        return 1
    print("fine")
    return 0


if __name__ == "__main__":
    sys.exit(main())
