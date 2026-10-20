"""Static checks that unexplored (fogged) tiles reveal no type or lit relief.

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


# Unexplored walls use the same hover text and mark action regardless of their actual type.
tile_cpp = (root / "source/entities/Tile.cpp").read_text(encoding="utf-8")
check("mTileVisual == TileVisual::nullTileVisual && !mEverVisible && !getGameMap()->isServerGameMap()" in tile_cpp,
      "unexplored tiles are not markable on the client")
fog_hover = game_mode[game_mode.index("void GameMode::handlePlayerActionNone()"):game_mode.index("void GameMode::handlePlayerActionSelectTile()")]
check(fog_hover.index('"Unexplored. Click or drag to mark for digging."') < fog_hover.index('tile->fillWithEntities('),
      "neutral fog hover must precede entity and type lookup")

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
for forbidden in ["getLocalLighting", "normalmap", "TBN *", "lightPos", "shadowmap", "diffuseSurface", "seatColor"]:
    check(forbidden not in re.sub(r"//.*", "", frag.split("void main")[1]), "fog shader still uses " + forbidden)
# The fog references no normal map at all: not in the shader and not in its material.
check("normalmap" not in re.sub(r"//.*", "", frag), "fog shader still declares a normal map")
_fog_mat = (root / "materials/scripts/DirtInstanced.material").read_text(encoding="utf-8")
_fog_mat = re.sub(r"//.*", "", _fog_mat[_fog_mat.index("material DirtInstanced"):])
check("normalmap" not in _fog_mat.lower() and "normal.png" not in _fog_mat.lower(),
      "fog material still references a normal map")

# Unexplored earth looks like explored earth without torch light: the same texture and the same
# brightness (ambient term), never a darkening factor of its own.
def decal_texture(material_text, name):
    block = material_text[material_text.index("material " + name):]
    return re.search(r"texture_unit decalmap\s*\{\s*texture (\S+)", block).group(1)


fog_material = (root / "materials/scripts/DirtInstanced.material").read_text(encoding="utf-8")
dirt_material = (root / "materials/scripts/Dirt.material").read_text(encoding="utf-8")
dirt_frag = (root / "shaders/DirtTile.frag").read_text(encoding="utf-8")
fog_main = re.sub(r"//.*", "", frag.split("void main")[1])
check(decal_texture(fog_material, "DirtInstanced") == decal_texture(dirt_material, "Dirt"),
      "fog earth must use the same texture as explored earth")
check(not re.search(r"\*\s*0?\.\d+\s*;", fog_main), "fog earth must not be darkened by a constant factor")
check("texelColor = ambientLightColour.rgb * texelColor;" in fog_main,
      "fog earth must be the explored earth ambient term")
check("getLocalLighting(FragPos, Normal, cameraPosition.xyz, shadow.r) + ambientLightColour.rgb;" in dirt_frag
      and "color = vec4(result.xyz,  1.0);" in dirt_frag,
      "explored earth lighting changed: update the fog earth to match")

# Discovered tiles out of sight are never darkened: the colourized clone of a tile material must not
# lower its diffuse, specular, ambient or emissive colour for a missing vision.
_colourize = render[render.index("std::string RenderManager::colourizeMaterial("):]
_colourize = _colourize[:_colourize.index("\n}\n")]
_code = re.sub(r"//.*", "", _colourize)
check("current_color.r = 0.1" not in _code and "= 0.1;" not in _code,
      "tiles out of sight are darkened again")
check("if (!markedForDigging && !playerHasVision)" not in _code,
      "a vision-dependent colour change is back in colourizeMaterial")

if failures:
    print("\n".join("FAIL: " + f for f in failures))
    sys.exit(1)
print("OK")
