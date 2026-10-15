#!/usr/bin/env python3
"""A skill that the map does not allow must not show a cast or build button.

The cast/build button of a room, spell, trap or door is visible only when the seat has
learned it (level above 0). The server refuses everything else
(SkillManager::isRoomAvailable, isSpellAvailable, isTrapAvailable), so a visible button
for a locked skill is a button that does nothing.
"""

import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
text = (root / "source/modes/GameMode.cpp").read_text(encoding="utf-8")

failures = 0


def check(condition, message):
    global failures
    if not condition:
        failures += 1
        print("FAIL: " + message)


calls = re.findall(r"getChild\(castButtonName\)->setVisible\(([^;]*)\);", text)
check(len(calls) == 1, "expected exactly one setVisible call for the cast button, found %d" % len(calls))
for argument in calls:
    check("level > 0" in argument, "the cast button must depend on the skill level: " + argument)
    check("isAllowed" not in argument, "a locked skill must not show its cast button: " + argument)

server = (root / "source/network/ODServer.cpp").read_text(encoding="utf-8")
for name in ("isRoomAvailable", "isSpellAvailable", "isTrapAvailable"):
    check(name in server or name in (root / "source/game/SkillManager.cpp").read_text(encoding="utf-8"),
          name + " must exist so the server refuses locked skills")

if failures:
    sys.exit(1)
print("locked skill buttons: ok")
