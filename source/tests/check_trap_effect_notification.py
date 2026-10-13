#!/usr/bin/env python3
"""Static check of the trap effect notification (no compiler needed).

    python source/tests/check_trap_effect_notification.py

Checks that the server notification trapEffect is the last value of the enum (so no existing value
changes), that the packet fields are written and read in the same order, that the trap effect kinds
only grew at the end, and that every event the client raises has a handler in the config and every
trap event of the config is raised.
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
header = read("source", "network", "ServerNotification.h")
body = header[header.index("enum class ServerNotificationType"):]
body = body[:body.index("};")]
names = re.findall(r"^\s*([A-Za-z_]\w*)\s*,?\s*(?://.*)?$", body, re.M)
if names[-1] != "trapEffect":
    problems.append("trapEffect is not the last server notification")

if '"trapEffect"' not in read("source", "network", "ServerNotification.cpp"):
    problems.append("trapEffect has no name in ServerNotification.cpp")

trap_h = read("source", "traps", "Trap.h")
kinds = re.search(r"enum class TrapEffectKind[^{]*\{(.*?)\}", trap_h, re.S)
values = re.findall(r"(\w+)\s*=\s*(\d+)", kinds.group(1)) if kinds else []
if [name for name, _ in values] != ["fired", "linked", "reloading", "ready"] or \
        [int(number) for _, number in values] != [0, 1, 4, 5]:
    problems.append("TrapEffectKind values changed: %s" % values)

trap_cpp = read("source", "traps", "Trap.cpp")
client = read("source", "network", "ODClient.cpp")
send = trap_cpp[trap_cpp.index("void Trap::fireTrapEffect"):]
send = send[:send.index("bool Trap::forceTrigger")]
if "static_cast<int32_t>(kind) << tile->getX() << tile->getY()" not in send or \
        "<< typeName << static_cast<float>(fraction)" not in send:
    problems.append("server packet order is not kind, x, y, type name, fraction")
handler = client[client.index("case ServerNotificationType::trapEffect:"):]
handler = handler[:handler.index("break;")]
if "packetReceived >> effectKind >> tileX >> tileY >> typeName >> fraction" not in handler:
    problems.append("client reads the packet in another order")

ambience = read("source", "render", "RoomAmbience.cpp")
raised = set(re.findall(r'triggerEvent\("(Trap\w+)"', ambience))
config = read("config", "roomAmbienceTraps.cfg")
configured = set(re.findall(r"^\s*Event\s+(Trap\w+)", config, re.M))
if raised != configured:
    problems.append("events raised %s, events configured %s" % (sorted(raised), sorted(configured)))

if "TrapEffectKind::linked" not in read("source", "traps", "TrapTrigger.cpp"):
    problems.append("the trigger trap does not send the linked effect")

# Reload state: sent once per change (not per turn), only for traps, and used by the client
doup = trap_cpp[trap_cpp.index("void Trap::doUpkeep"):]
doup = doup[:doup.index("bool Trap::fireTile")]
if "wasReloading" not in doup or "TrapEffectKind::ready" not in doup:
    problems.append("the end of a reload is not sent once")
fire = trap_cpp[trap_cpp.index("bool Trap::fireTile"):trap_cpp.index("void Trap::fireTrapEffect")]
if "TrapEffectKind::reloading" not in fire or "!isDoor()" not in fire:
    problems.append("the start of a reload is not sent for traps only")
if "TrapEffectKind::reloading" not in ambience or "TrapEffectKind::ready" not in ambience or \
        "mReloadingUntil" not in ambience:
    problems.append("the client does not keep the reload state")
for word in ("AmbienceWhen::reloading", "AmbienceWhen::ready"):
    if word not in ambience:
        problems.append("the client does not use " + word)
if "When        Reloading" not in config or "When        Ready" not in config:
    problems.append("the config has no effects for reloading and ready")

if problems:
    for problem in problems:
        print("PROBLEM: " + problem)
    sys.exit(1)
print("fine")
