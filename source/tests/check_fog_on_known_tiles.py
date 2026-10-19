"""Static check that fog and mark surfaces never stay on tiles whose state does not call for them.

Known tiles (seen at least once) show their real mesh. Fog surfaces, dig-mark surfaces and clouds may only
exist for a tile when its state asks for them:
  - the neutral earth surface or its marked variant only on a tile that was never seen,
  - the marked variant only while the local player has the tile marked,
  - the cloud only on a seen tile of enemy territory that is currently fogged,
  - all of them are removed again as soon as the tile state no longer asks for them.
Every fog instance is also placed at zero offset from the tile's own scene node, which already sits at the
tile coordinates: a tile position there is added a second time and moves the fog onto other tiles, which
shows as large black or brown patches on known ground. Run from any directory; no build, no game start.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
render = (root / "source/render/RenderManager.cpp").read_text(encoding="utf-8")
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def between(text, start, end):
    """Return the part of text from the first start marker up to the following end marker."""
    first = text.index(start)
    return text[first:text.index(end, first)]


# 1. Placement: one helper sets the zero offset, no other code positions a fog instance.
helper = between(render, "static void rrPlaceFogInstance(", "\n}\n")
check("setPosition(Ogre::Vector3::ZERO)" in helper, "fog placement helper must use the zero offset")
refresh = between(render, "void RenderManager::rrRefreshTile(", "\nvoid RenderManager::rrCreateTile(")
for match in re.finditer(r"getFogOfWar(Mesh|Cloud)\(\)\s*->\s*setPosition\(", render):
    failures.append("fog instance positioned directly at offset " + str(match.start()) + ", use rrPlaceFogInstance")
check(len(re.findall(r"rrPlaceFogInstance\(", refresh)) == 4, "every fog attach must call rrPlaceFogInstance")
check(len(re.findall(r"attachObject\(tile\.getFogOfWar(Mesh|Cloud)\(\)\)", refresh)) == 4,
      "the fog attach count changed, update this check together with the placement calls")
check("getPosition()" not in "".join(re.findall(r"[^\n]*FogOfWar[^\n]*", refresh)),
      "a fog line in the tile refresh uses a tile position")
create = between(render, "void RenderManager::rrCreateTile(", "\nvoid RenderManager::rrDestroyTile(")
check("node->setPosition(static_cast<Ogre::Real>(tile.getX()), static_cast<Ogre::Real>(tile.getY())" in create,
      "the tile scene node must sit at the tile coordinates, otherwise fog needs an offset")

# 2. The neutral earth surface (and its marked variant) is created only for never-seen tiles.
for match in re.finditer(r'createInstancedEntity\("DirtInstanced"\)', refresh):
    before = refresh[:match.start()]
    branch = before[max(before.rfind("else if"), before.rfind("\n        if ")):]
    check("!tile.getEverVisible()" in branch or "wasMarked != isMarked" in branch,
          "a fog surface is created outside the never-seen branch at offset " + str(match.start()))
switch = between(refresh, "const bool wasMarked", "// Only previously discovered enemy territory")
check("tile.getHasFogOfWar()" in refresh[refresh.index("if(tile.getHasFogOfWar() && tile.getFogOfWarMesh() != nullptr)"):],
      "the mark switch must only touch tiles that still carry the fog surface")
check("isMarked" in switch and "mInstanceManagerMarkedFog" in switch,
      "the marked surface must follow the mark state of the tile")

# 3. Revealing a tile removes the surface and the cloud and clears both handles.
reveal = between(refresh, "else if( tile.getEverVisible() && tile.getHasFogOfWar())", "else if ( !tile.getEverVisible() && tile.getHasFogOfWar())")
for needle in ["destroyInstancedEntity(tile.getFogOfWarMesh())", "destroyInstancedEntity(tile.getFogOfWarCloud())",
               "tile.setFogOfWarMesh(nullptr", "tile.setFogOfWarCloud(nullptr)", "tile.setHasFogOfWar(false)"]:
    check(needle in reveal, "revealing a tile does not clean up: missing " + needle)

# 4. The cloud exists only on seen tiles with the enemy fog state and goes away with that state.
cloud = refresh[refresh.index("// Only previously discovered enemy territory"):]
cloud = cloud[:cloud.index("// We rescale and set the orientation")]
check("if(tile.getEverVisible() && tileMeshNode != nullptr)" in cloud, "cloud creation must require a seen tile")
check("if(tile.getEnemyTerritoryFog() && tile.getFogOfWarCloud() == nullptr)" in cloud,
      "cloud creation must require the enemy fog state")
check("else if(!tile.getEnemyTerritoryFog() && tile.getFogOfWarCloud() != nullptr)" in cloud,
      "the cloud must be removed when the enemy fog state ends")
check(refresh.count('createInstancedEntity("Fog")') == 1, "the cloud must have exactly one creation site")
check(render.count('createInstancedEntity("Fog")') == 1, "the cloud is created somewhere outside the tile refresh")
destroy = between(render, "void RenderManager::rrDestroyTile(", "\n}\n")
check("destroyInstancedEntity(tile.getFogOfWarMesh())" in destroy and "destroyInstancedEntity(tile.getFogOfWarCloud())" in destroy,
      "destroying a tile must remove its fog instances")

if failures:
    print("\n".join("FAIL: " + f for f in failures))
    sys.exit(1)
print("OK")
