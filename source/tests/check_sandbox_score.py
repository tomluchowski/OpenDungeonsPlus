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


def active_seat_mana(text):
    """The start mana of every seat whose player is not Inactive."""
    result = []
    for seat in re.findall(r"\[Seat\]\n(.*?)\[/Seat\]", text, re.S):
        if re.search(r"^player\tInactive$", seat, re.M):
            continue
        result.extend(re.findall(r"^mana\t(\d+)$", seat, re.M))
    return result


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
names = [n for n in names if n != "creatureAppearance"]  # appended after them by the Dungeonbook appearance
check(names[-2:] == ["timeLimit", "chickenKindChanged"] and "sandboxStatus" in names and names.index("sandboxStatus") + 1 == names.index("sandboxRealmComplete"),
      "the sandbox notifications follow each other and timeLimit and chickenKindChanged stay last: %s" % names[-4:])
for entry in ("sandboxStatus", "sandboxRealmComplete"):
    check('return "%s";' % entry in notification_cpp, "ServerNotification.cpp has no name for " + entry)

# Bonus kinds of the code and of the level files
kinds = re.findall(r'"(\w+)"', code[code.index("BONUS_KIND_NAMES[]"):code.index("const uint32_t NB_BONUS_KINDS")])
check("NB_BONUS_KINDS = %d;" % len(kinds) in code, "NB_BONUS_KINDS does not match the kind names")
enum_kinds = re.findall(r"^\s{4}(\w+),?\s*$", header[header.index("enum class SandboxBonusKind"):header.index("};")], re.M)
check(enum_kinds == kinds, "the bonus kinds of the enum and of the name list differ: %s / %s" % (enum_kinds, kinds))

# The realms: file order is the chain, target = 8000 + 6000 x index rounded to 500, two bonuses of 20 % of it
REALMS = ["Quietcrag", "Lanternreach", "Splitstone", "Barrackdeep", "Embervault", "GrandDelve", "Gloomkeep", "Wyrmhollow"]


def rounded(value):
    return int(round(value / 500.0)) * 500


for index, stem in enumerate(REALMS, start=1):
    f = root / "levels/skirmish" / (stem + ".level")
    text = f.read_text(encoding="utf-8")
    info = text[text.index("[Info]"):text.index("[/Info]")]
    lines = [l.rstrip("\r") for l in info.splitlines()]
    check("Sandbox\t1" in lines, f.name + ": missing the Sandbox flag")
    check(("SandboxRealm\t" + stem) in lines, f.name + ": SandboxRealm must be the file name")
    expected = rounded(8000 + 6000 * index)
    target = [int(l.split("\t")[1]) for l in lines if l.startswith("SandboxTarget\t")]
    check(target == [expected], f.name + ": the target must be %d, found %s" % (expected, target))
    bonus_lines = [l for l in lines if l.startswith("SandboxBonus\t")]
    check(len(bonus_lines) == 2, f.name + ": needs two bonus objectives")
    for l in bonus_lines:
        fields = l.split("\t")
        check(len(fields) == 6 and fields[1] in kinds, f.name + ": bad bonus line " + l)
        if len(fields) == 6:
            check(int(fields[3]) == rounded(expected * 0.2), f.name + ": a bonus must be 20 %% of the target: " + l)
    next_lines = [l.split("\t")[1] for l in lines if l.startswith("SandboxNext\t")]
    if index < len(REALMS):
        check(next_lines == ["skirmish/%s.level" % REALMS[index]], f.name + ": the next realm must be " + REALMS[index])
    else:
        check(next_lines == [], f.name + ": the last realm has no next realm")
    goals = text[text.index("[Goals]"):text.index("[/Goals]")]
    check(len([l for l in goals.splitlines()[1:] if l.strip() and not l.startswith("#")]) == 0, f.name + ": a realm has no goals")
    check("Action\twin" not in text and "Action\tlose" not in text, f.name + ": no win or lose action, the score decides")
    check(active_seat_mana(text) and all(m == "100" for m in active_seat_mana(text)), f.name + ": start mana must be 100")
    check(re.search(r"^10\tPortalWave\w*\t2\t\d+$", text, re.M) is not None, f.name + ": needs a wave portal of the hero seat")
    seat = text[text.index("[Seat]"):text.index("[/Seat]")]
    done = seat[seat.index("[SkillDone]"):seat.index("[/SkillDone]")].split()[1:]
    if stem != "GrandDelve":
        check(done == ["roomDormitory", "roomHatchery", "spellSummonWorker"], f.name + ": start skills %s" % done)
    else:
        check("roomTemple" in done and "roomLibrary" in done and "spellPossess" in done, f.name + ": GrandDelve has everything from the start")

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
