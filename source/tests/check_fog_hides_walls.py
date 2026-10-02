"""Static checks that unexplored (fogged) tiles reveal nothing: no hover text and no lit relief.

Run from any directory. It does not start a game.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
game_mode = (root / "source/modes/GameMode.cpp").read_text(encoding="utf-8")
render = (root / "source/render/RenderManager.cpp").read_text(encoding="utf-8")
frag = (root / "shaders/DirtTileInstanced.frag").read_text(encoding="utf-8")
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


# The wall hover text and the hover selection need a tile the player has already seen
check(re.search(r"else if\(tile->getEverVisible\(\) && tile->isDiggable\(.*?\)\)\s*\{\s*displayText\([^;]*Click or drag to mark for digging",
                game_mode, re.S) is not None,
      "hover wall text is not guarded by getEverVisible()")

# The selection error texts must not describe a fogged tile
select = game_mode[game_mode.index("void GameMode::handlePlayerActionSelectTile()"):]
select = select[:select.index("\n}\n")]
fogged = select.index("!tile->getEverVisible()")
check(fogged < select.index("is already dug out") and fogged < select.index("enemy's claimed wall"),
      "selection error texts describe a fogged tile")

# Unexplored tiles get the fog mesh and the cloud instead of the real tile mesh
check("!tile.getEverVisible() && !tile.getHasFogOfWar()" in render and "createInstancedEntity(\"DirtInstanced\")" in render,
      "unexplored tiles are not replaced by the fog mesh")

# The fog shader is unlit: no normal map, no lighting
for forbidden in ["getLocalLighting", "normalmap", "enhanceDungeonColour", "TBN *"]:
    check(forbidden not in re.sub(r"//.*", "", frag.split("void main")[1]), "fog shader still uses " + forbidden)

if failures:
    print("\n".join("FAIL: " + f for f in failures))
    sys.exit(1)
print("OK")
