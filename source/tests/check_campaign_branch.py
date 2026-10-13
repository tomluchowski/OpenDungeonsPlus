"""Static checks of the two campaign branches (T09A/T09B and T18A/T18B).

One sister is enough to open the levels after them, the other stays playable and
both count in the progress of the world map. Run from any directory. It does not
start a game.
"""
from pathlib import Path
import json
import re
import sys

root = Path(__file__).resolve().parents[2]
campaign_cpp = (root / "source/game/Campaign.cpp").read_text(encoding="utf-8")
mode = (root / "source/modes/MenuModeCampaign.cpp").read_text(encoding="utf-8")
definition = (root / "levels/campaign/Campaign.cfg").read_text(encoding="utf-8").replace("\r", "")
world = json.loads((root / "gui/campaign/campaign-world.json").read_text(encoding="utf-8"))
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


entries = []
for block in definition.split("[Level]")[1:]:
    fields = dict(re.findall(r"^(\w+)=(.*)$", block, re.M))
    entries.append(fields)
provinces = [f.get("Province", "") for f in entries]
ids = [p["id"] for p in world["provinces"]]

# Both sisters name each other, follow each other and are not bonus sites
for first, second in (("T09A", "T09B"), ("T18A", "T18B")):
    check(first in provinces and second in provinces, first + "/" + second + " missing in the definition")
    if first not in provinces or second not in provinces:
        continue
    a, b = provinces.index(first), provinces.index(second)
    check(b == a + 1, first + " and " + second + " must follow each other")
    check(entries[a].get("Branch") == second and entries[b].get("Branch") == first,
          first + " and " + second + " must name each other")
    check("Bonus" not in entries[a] and "Bonus" not in entries[b], first + "/" + second + " must not be bonus levels")
    check(first in ids and second in ids, first + "/" + second + " must be on the world map")

# Only the four branch levels have a Branch setting
check(sorted(f.get("Province") for f in entries if "Branch" in f) == ["T09A", "T09B", "T18A", "T18B"],
      "unexpected Branch settings")

# The loader reads Branch and lets one sister open the later levels without blocking the other
check('key == "Branch"' in campaign_cpp, "the reader must know Branch")
unlocked = campaign_cpp[campaign_cpp.index("bool Campaign::isUnlockedNoLock"):campaign_cpp.index("bool Campaign::isUnlocked(")]
check("findBranchSisterNoLock(i)" in unlocked and "(i == ownSister)" in unlocked,
      "one sister must be enough and a sister must not block the other")
current = campaign_cpp[campaign_cpp.index("size_t Campaign::getCurrentLevelNoLock"):campaign_cpp.index("size_t Campaign::getCurrentLevel()")]
check("findBranchSisterNoLock(i)" in current, "the open sister must not be the current level once the other is done")

# The map progress counts every conquered main level against all provinces (26: both sisters count)
check(len(ids) == 26, "the world map must list 26 provinces")
check("!campaign.getLevel(i).mBonus && campaign.isCompleted(i)" in mode, "progress must count each completed main level")
check("provinces.size()" in mode, "progress total must be the number of provinces")

if failures:
    print("\n".join(failures))
    sys.exit(1)
print("campaign branch checks passed")
