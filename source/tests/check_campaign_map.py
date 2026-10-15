"""Static checks of the campaign world map (layout, menu code, art description and the campaign definition).

Run from any directory. It does not start a game.
"""
from pathlib import Path
import json
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
layout = ET.fromstring((root / "gui/MenuCampaign.layout").read_text(encoding="utf-8"))
mode = (root / "source/modes/MenuModeCampaign.cpp").read_text(encoding="utf-8")
campaign_cpp = (root / "source/game/Campaign.cpp").read_text(encoding="utf-8")
definition = (root / "levels/campaign/Campaign.cfg").read_text(encoding="utf-8")
world = json.loads((root / "gui/campaign/campaign-world.json").read_text(encoding="utf-8"))
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


windows = {w.get("name"): w for w in layout.iter("Window")}

# Every window the campaign page code looks up exists in the layout; the level list is gone
for path in re.findall(r'const std::string CMP_\w+ = "CampaignWindowFrame/([\w/]+)";', mode):
    check(path.split("/")[-1] in windows, "missing in layout: " + path)
check("LevelSelect" not in windows, "the level list must be replaced by the map")
check("DifficultyButton" in windows, "the difficulty button must stay")
check("BriefingPanel" in windows, "the briefing panel is needed")

# The old territory polygons and map settings are gone
check("Map=" not in definition and not re.search(r"^# Map\b", definition, re.M), "Campaign.cfg must not have Map settings")
check('key == "Map"' not in campaign_cpp, "the Map setting must not be read any more")
check("CampaignSolid" not in mode and "mMapBlocks" not in campaign_cpp, "the old territory blocks must be gone")

# The art description has all provinces, unique mask colours and existing files
art = root / "gui/campaign"
provinces = world["provinces"]
check(len(provinces) == 26, "the world map needs 26 provinces")
check(len(world["sites"]) == 5, "the world map needs 5 bonus sites")
masks = [tuple(p["mask_rgb"]) for p in provinces]
check(len(set(masks)) == len(masks), "province mask colours must be unique")
for p in provinces:
    for key in ("locked", "available", "conquered", "lift"):
        check((art / p["layers"][key]).is_file(), "missing layer: " + p["layers"][key])
for s in world["sites"]:
    for key in ("hidden", "found", "done"):
        check((art / s["icons"][key]).is_file(), "missing site icon: " + s["icons"][key])
check((art / "world_base.png").is_file() and (art / "world_idmap.png").is_file(), "missing world images")

# Every province or site named by a level exists on the world map
ids = {p["id"] for p in provinces} | {s["id"] for s in world["sites"]}
levels = []
for part in definition.replace("\r", "").split("[Level]")[1:]:
    fields = dict(line.split("=", 1) for line in part.splitlines() if "=" in line and not line.startswith("#"))
    if "File" in fields:
        levels.append(fields)
check(len(levels) > 0, "no levels in the campaign definition")
used = [f["Province"] for f in levels if "Province" in f]
check(len(set(used)) == len(used), "two levels share a province")
for name in used:
    check(name in ids, name + " is not on the world map")
for f in levels:
    check("Province" in f, f["File"] + " has no Province setting")
    check(1 <= int(f.get("Difficulty", "1")) <= 5, f["File"] + " has a difficulty outside of 1 to 5")
check('key == "Province"' in campaign_cpp and 'key == "Warden"' in campaign_cpp, "the reader must know Province and Warden")

# The screen: map keeps its aspect ratio, hit test with the id map, lift fade, tooltip, briefing and progress
check("setAspectMode(CEGUI::AM_SHRINK)" in mode, "the map must keep its aspect ratio")
check("getProvinceAt(" in mode and "world_idmap.png" in mode, "hit test must use the id map")
check("LIFT_FADE_TIME = 0.12f" in mode and "LIFT_SCALE = 1.04f" in mode, "lift fades in over 120 ms to scale 1.04")
check("State::locked" in mode and "State::available" in mode and "State::conquered" in mode, "three province states")
hover = mode[mode.index("bool MenuModeCampaign::mapMoved"):mode.index("bool MenuModeCampaign::mapLeft")]
check("State::locked" in hover and "updateTooltip(" in hover, "locked provinces get a tooltip but no lift")
check("Warden: " in mode and "Difficulty: " in mode and "State: " in mode, "tooltip shows warden, state and difficulty")
click = mode[mode.index("bool MenuModeCampaign::mapClicked"):mode.index("void MenuModeCampaign::showBriefing")]
check("showBriefing(" in click and "startLevel(" not in click, "a click opens the briefing, it does not start the level")
check("startLevel(level)" in mode[mode.index("bool MenuModeCampaign::briefingStartPressed"):], "the briefing starts the level")
check("provinces conquered" in mode and "hidden sites" in mode and "percent" in mode, "progress panel texts")

if failures:
    print("\n".join(failures))
    sys.exit(1)
print("campaign map checks passed")
