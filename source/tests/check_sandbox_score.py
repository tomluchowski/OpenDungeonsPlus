"""Static checks of the sandbox score, the bonus objectives and the realms.

Run from any directory. It does not start a game.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
code = (root / "source/gamemap/SandboxMode.cpp").read_text(encoding="utf-8")
header = (root / "source/gamemap/SandboxMode.h").read_text(encoding="utf-8")
statistics = (root / "source/game/SeatStatistics.h").read_text(encoding="utf-8")
notification_h = (root / "source/network/ServerNotification.h").read_text(encoding="utf-8")
notification_cpp = (root / "source/network/ServerNotification.cpp").read_text(encoding="utf-8")
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


# The score values of the sandbox
for name, value in (("SCORE_HERO_KILLED", 60), ("SCORE_LAND_TILE", 4), ("SCORE_GOLD_TILE", 2), ("SCORE_ITEM_MADE", 8),
                    ("SCORE_CREATURE_ENTERED", 15), ("SCORE_CREATURE_CONVERTED", 30)):
    match = re.search(r"const int32_t SandboxMode::%s = (\d+);" % name, code)
    check(match is not None and int(match.group(1)) == value, "%s must be %d" % (name, value))

# Every score source is counted from a statistic or the claimed tiles
body = code[code.index("void SandboxMode::updateScore("):code.index("bool SandboxMode::isBonusReached(")]
for token in ("mHeroesDestroyed", "mItemsMade", "mCreaturesConverted", "mGoldTilesMined", "mCreaturesEntered",
              "getNumClaimedTiles", "SCORE_HERO_KILLED", "SCORE_LAND_TILE", "SCORE_GOLD_TILE", "SCORE_ITEM_MADE",
              "SCORE_CREATURE_ENTERED", "SCORE_CREATURE_CONVERTED"):
    check(token in body, "updateScore does not use " + token)
check("mScore = 0" in code[code.index("void SandboxMode::reset()"):] and "if(mScore < 0)" in body,
      "the score must start at 0 and never go below it")

# The new statistics are reset and counted where the events happen
check("mGoldTilesMined = 0;" in statistics and "mCreaturesEntered = 0;" in statistics, "SeatStatistics must reset the new counters")
dig = (root / "source/creatureaction/CreatureActionDigTile.cpp").read_text(encoding="utf-8")
check("++creature.getSeat()->getStatistics().mGoldTilesMined;" in dig, "digging out a gold tile must count mGoldTilesMined")
portal = (root / "source/rooms/RoomPortal.cpp").read_text(encoding="utf-8")
check("++getSeat()->getStatistics().mCreaturesEntered;" in portal, "a portal spawn must count mCreaturesEntered")

# The wave reward is announced
check("Reward: " in code and "mWaveHeroPoints" in code, "a beaten wave must announce its reward")

# The notifications are appended after the existing ones and named
names = re.findall(r"^\s*(\w+),?\s*$", notification_h[notification_h.index("enum class ServerNotificationType"):notification_h.index("};")], re.M)
check(names[-1] == "timeLimit" and "sandboxStatus" in names and names.index("sandboxStatus") + 1 == names.index("sandboxRealmComplete"),
      "the sandbox notifications follow each other and timeLimit stays last: %s" % names[-4:])
for entry in ("sandboxStatus", "sandboxRealmComplete"):
    check('return "%s";' % entry in notification_cpp, "ServerNotification.cpp has no name for " + entry)

# Bonus kinds of the code and of the level files
kinds = re.findall(r'"(\w+)"', code[code.index("BONUS_KIND_NAMES[]"):code.index("const uint32_t NB_BONUS_KINDS")])
check("NB_BONUS_KINDS = %d;" % len(kinds) in code, "NB_BONUS_KINDS does not match the kind names")
enum_kinds = re.findall(r"^\s{4}(\w+),?\s*$", header[header.index("enum class SandboxBonusKind"):header.index("};")], re.M)
check(enum_kinds == kinds, "the bonus kinds of the enum and of the name list differ: %s / %s" % (enum_kinds, kinds))

# The progress file is read and written by the menu and the game
progress = (root / "source/game/SandboxProgress.h").read_text(encoding="utf-8")
check("sandbox-progress.txt" in progress, "SandboxProgress.h must name the progress file")
menu = (root / "source/modes/MenuModeSkirmish.cpp").read_text(encoding="utf-8")
check("SandboxProgress::loadCompleted()" in menu and "setDisabled(isLocked)" in menu, "the level list must lock realms")
game_mode = (root / "source/modes/GameMode.cpp").read_text(encoding="utf-8")
check("SandboxProgress::markCompleted(" in game_mode, "the game must store a completed realm")

if failures:
    print("\n".join(failures))
    sys.exit(1)
print("sandbox score checks passed")
