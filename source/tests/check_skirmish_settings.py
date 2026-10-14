"""Static checks of the skirmish Game settings (layout, seat page code and the packet layout).

Run from any directory. It does not start a game.
"""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
layout = ET.fromstring((root / "gui/MenuConfigureSeats.layout").read_text(encoding="utf-8"))
mode = (root / "source/modes/MenuModeConfigureSeats.cpp").read_text(encoding="utf-8")
server = (root / "source/network/ODServer.cpp").read_text(encoding="utf-8")
failures = []


def check(condition, message):
    """Records a failure when the condition does not hold."""
    if not condition:
        failures.append(message)


windows = {w.get("name"): w for w in layout.iter("Window")}


def area(name):
    """Returns the four (scale, offset) pairs of the Area property of a layout window."""
    value = windows[name].find("Property[@name='Area']").get("value")
    numbers = [float(n) for n in re.findall(r"-?\d+(?:\.\d+)?", value)]
    # {{sx,ox},{sy,oy},{sx,ox},{sy,oy}}
    return [(numbers[i], numbers[i + 1]) for i in range(0, 8, 2)]


# Every window the seat page code looks up exists in the layout
for constant in re.findall(r'const std::string (?:COMBOBOX|SPINNER)_\w+ = "(\w+)";', mode):
    if constant.startswith("ComboPlayer") or constant.startswith("ComboTeam"):
        continue
    check(constant in windows, "missing in layout: " + constant)
for name in ["GameSettingsWindow", "SettingsTabs", "General", "Creatures", "Rooms", "Spells", "Traps", "Doors",
             "CreaturesSP", "RoomsSP", "SpellsSP", "TrapsSP", "DoorsSP", "CloseSettingsButton",
             "GameSettingsButton", "BackButton", "LaunchGameButton"]:
    check(name in windows, "missing in layout: " + name)

# The three buttons of the seat page do not overlap (the drop-down lists of the settings are in their own window)
buttons = sorted((area(n)[0][1], area(n)[2][1], n) for n in ["BackButton", "GameSettingsButton", "LaunchGameButton"])
for first, second in zip(buttons, buttons[1:]):
    check(first[1] <= second[0], first[2] + " overlaps " + second[2])
check("ComboMaxCreatures" not in windows and "TextGoldDensity" in windows, "settings must be in the settings window")
check("Default (" not in mode, "the max creatures choices must be sorted, not a list that starts with Default")

# The order of the values in the packet is the same on the server and on the client
write_order = ["getGoldDensityPercent", "getManaRegenerationPercent", "getMaxCreaturesSetting", "getGameSpeedPercent",
               "getGameDurationMinutes", "getIsFOWActivated", "getCreatureClassLimit", "SkirmishSkillStates"]
positions = [server.index(token, server.index("void ODServer::fireSeatConfigurationRefresh")) for token in write_order]
check(positions == sorted(positions), "ODServer::fireSeatConfigurationRefresh writes the settings in another order")
read_order = ["goldDensityPercent", "manaRegenerationPercent", "maxCreaturesSetting", "gameSpeedPercent",
              "gameDurationMinutes", "fogOfWar", "nbCreatureLimits", "nbSkillStates"]
for source, start, label in [(server, "case ClientNotificationType::seatConfigurationRefresh", "server read"),
                             (mode, "void MenuModeConfigureSeats::refreshSeatConfiguration", "client read")]:
    base = source.index(start)
    reads = [source.index(">> " + token, base) for token in read_order]
    check(reads == sorted(reads), label + " reads the settings in another order")
send = mode[mode.index("void MenuModeConfigureSeats::fireSeatConfigurationToServer"):]
send_tokens = ["COMBOBOX_GOLD_DENSITY", "COMBOBOX_MANA_REGENERATION", "SPINNER_MAX_CREATURES", "SPINNER_GAME_SPEED",
               "SPINNER_GAME_DURATION", "COMBOBOX_FOG_OF_WAR", "nbCreatureLimits", "nbSkillStates"]
sends = [send.index(token) for token in send_tokens]
check(sends == sorted(sends), "the client sends the settings in another order")

# The new availability state is appended after the existing ones, the limit bound is the reference one
header = (root / "source/gamemap/GameMap.h").read_text(encoding="utf-8")
states = re.search(r"enum class SkirmishItemState[^{]*\{([^}]*)\}", header).group(1)
names = [re.sub(r"//.*", "", n).strip() for n in states.split(",")]
names = [n for n in names if n]
check(names[:3] == ["notAvailable", "availableAtStart", "needsResearch"], "item state order changed")
check("SKIRMISH_CREATURE_LIMIT_NONE = 32" in header, "creature limit range is 0 to 32")
check("SKIRMISH_SPEED_MIN = 25" in header and "SKIRMISH_SPEED_MAX = 400" in header, "game speed range is 25 to 400 percent")

if failures:
    print("\n".join(failures))
    sys.exit(1)
print("skirmish settings checks passed")
