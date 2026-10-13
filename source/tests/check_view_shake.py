#!/usr/bin/env python3
"""Static check of the view shake of the room ambience (no compiler needed).

    python source/tests/check_view_shake.py

Checks that the kind Shake is read from the config, that the shake is only applied while a frame is
rendered and taken away again after it, that it is not applied while the game is paused, and that
every shake effect of the config has the values the code needs and stays within the limits that keep
it gentle.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    """Returns the text of a file below the repository root."""
    with open(os.path.join(ROOT, *parts), encoding="utf-8", errors="replace") as handle:
        return handle.read()


problems = []

# The kind is part of the config format and keeps the order of the older kinds
config_h = read("source", "render", "RoomAmbienceConfig.h")
config_cpp = read("source", "render", "RoomAmbienceConfig.cpp")
kinds = re.search(r"enum class AmbienceKind\s*\{(.*?)\};", config_h, re.S).group(1)
order = re.findall(r"^\s*(\w+),?\s*$", re.sub(r"//.*", "", kinds), re.M)
if order[:3] != ["particle", "motion", "clip"] or "shake" not in order:
    problems.append("AmbienceKind values changed: %s" % order)
if 'words[1] == "Shake"' not in config_cpp or "AmbienceKind::shake" not in config_cpp:
    problems.append("kind Shake is not read from the config")

# The code
ambience = read("source", "render", "RoomAmbience.cpp")
for needle in ("AmbienceKind::shake", "void RoomAmbience::applyShake", "void RoomAmbience::clearShake",
               "void RoomAmbience::startShake", "void RoomAmbience::updateShake"):
    if needle not in ambience:
        problems.append("RoomAmbience.cpp lacks %s" % needle)
apply_part = ambience[ambience.index("void RoomAmbience::applyShake"):ambience.index("void RoomAmbience::clearShake")]
if "getGamePaused" not in apply_part:
    problems.append("the shake must not be applied while the game is paused")
if "mShakeApplied = offset" not in apply_part:
    problems.append("applyShake does not remember the offset")
stop_part = ambience[ambience.index("void RoomAmbience::stopAll"):ambience.index("void RoomAmbience::update")]
if "clearShake();" not in stop_part:
    problems.append("stopAll does not clear the shake")

# The frame: applied before rendering, removed after it
listener = read("source", "render", "ODFrameListener.cpp")
started = listener[listener.index("bool ODFrameListener::frameStarted"):]
if "mRoomAmbience->applyShake()" not in started:
    problems.append("frameStarted does not apply the shake")
ended = listener[listener.index("bool ODFrameListener::frameEnded"):listener.index("bool ODFrameListener::frameStarted")]
if "mRoomAmbience->clearShake()" not in ended:
    problems.append("frameEnded does not clear the shake")

# The config: every shake has what the code reads and stays gentle
for path in sorted(os.listdir(os.path.join(ROOT, "config"))):
    if not path.startswith("roomAmbience") or not path.endswith(".cfg"):
        continue
    text = read("config", path)
    for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", text, re.S):
        if not re.search(r"^\s*Kind\s+Shake\s*$", block, re.M):
            continue
        name = re.search(r"^\s*Name\s+(\S+)", block, re.M).group(1)
        values = dict(re.findall(r"^\s*(Amount|Duration|Speed|MaxDistance)\s+(\S+)", block, re.M))
        for key in ("Amount", "Duration", "Speed", "MaxDistance"):
            if key not in values:
                problems.append("%s: shake %s without %s" % (path, name, key))
        if float(values.get("Amount", 0)) > 0.5:
            problems.append("%s: shake %s is too strong" % (path, name))
        if float(values.get("Duration", 0)) > 2.0:
            problems.append("%s: shake %s lasts too long" % (path, name))

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
