#!/usr/bin/env python3
"""Static check of the watch banner that fires when it calls the guards (no compiler needed).

    python source/tests/check_watch_banner_fire.py

Checks that the call of the guards is reported to the clients as fired, reloading and ready (once each, not every
turn), that the banner skeleton has the clips Loop and Droop and is in CREDITS, and that the config waves the flag
while the post is ready, hangs it while the post reloads and flares it at the call.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts, binary=False):
    """Returns the text (or the bytes) of a file below the repository root."""
    if binary:
        with open(os.path.join(ROOT, *parts), "rb") as handle:
            return handle.read()
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


problems = []
banner = read("source", "traps", "TrapWatchBanner.cpp")
upkeep = banner[banner.index("void TrapWatchBanner::doUpkeep"):]
upkeep = upkeep[:upkeep.index("\n}\n")]
for needle in ("TrapEffectKind::fired", "TrapEffectKind::reloading", "TrapEffectKind::ready", "mReloading = true",
               "mReloading = false", "callingTile"):
    if needle not in upkeep:
        problems.append("TrapWatchBanner::doUpkeep lacks %s" % needle)
if upkeep.index("TrapEffectKind::ready") > upkeep.index("TrapEffectKind::fired"):
    problems.append("the ready message of the last call must come before the next call")
if "mReloading" not in read("source", "traps", "TrapWatchBanner.h"):
    problems.append("TrapWatchBanner.h does not keep the reload state")

skeleton = read("models", "WarBanner.skeleton", binary=True)
for clip in (b"Loop", b"Droop"):
    if clip not in skeleton:
        problems.append("WarBanner.skeleton lacks the clip %s" % clip.decode())
credits = read("CREDITS")
for needle in ("WarBanner.skeleton (clip Droop)", "banner_droop_clip.py"):
    if needle not in credits:
        problems.append("CREDITS has no entry for %s" % needle)

config = read("config", "roomAmbienceFixEffects.cfg")
effects = {}
for block in re.findall(r"\[Effect\](.*?)\[/Effect\]", config, re.S):
    entry = {}
    for line in block.splitlines():
        words = line.split("#", 1)[0].split()
        if len(words) >= 2:
            entry[words[0]] = words[1:]
    if "Name" in entry:
        effects[entry["Name"][0]] = entry
wave = effects.get("WatchBannerWave")
if wave is None or wave.get("When") != ["Ready"] or wave.get("Clips") != ["Loop"]:
    problems.append("WatchBannerWave must play the clip Loop while the post is ready")
droop = effects.get("WatchBannerDroop")
if droop is None or droop.get("When") != ["Reloading"] or droop.get("Clips") != ["Droop"]:
    problems.append("WatchBannerDroop must play the clip Droop while the post reloads")
for name in ("WatchBannerFlare", "WatchBannerSparks", "WatchBannerAlertSound"):
    entry = effects.get(name)
    if entry is None or entry.get("Event") != ["TrapFired"] or entry.get("Match") != ["WatchBanner"]:
        problems.append("%s must be an effect of the event TrapFired of the watch banner" % name)

if problems:
    for problem in problems:
        print("PROBLEM:", problem)
    sys.exit(1)
print("fine")
