#!/usr/bin/env python3
"""Static check of the wall torches (no compiler needed).

    python scripts/check_wall_torches.py

Checks the config keys of config/rooms.cfg, the wiring server -> network -> client -> room ambience /
wall torch view and the build entries.
The placement rules themselves are covered by the unit test 00-WallTorches.
"""

import os
import re
import sys
import struct
import math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


def body(text, start):
    """The text of the function that starts with start, up to the first closing brace in column 0"""
    index = text.index(start)
    return text[index:text.index("\n}\n", index)]


problems = []


def check(condition, message):
    if not condition:
        problems.append(message)


# Config keys: every key read by the code is in rooms.cfg, with the default of the code
config_cpp = read("source", "rooms", "WallTorchConfig.cpp")
rooms_cfg = read("config", "rooms.cfg")
reads = re.findall(r'read(?:Count|Clamped)\("(WallTorch\w+)",\s*([0-9.]+)', config_cpp)
check(len(reads) >= 12, "WallTorchConfig reads fewer than 12 keys")
for key, default in reads:
    match = re.search(r"^\s*%s\s+([0-9.]+)\s*$" % key, rooms_cfg, re.M)
    check(match is not None, "config/rooms.cfg has no value for " + key)
    if match is not None:
        check(float(match.group(1)) == float(default), "%s: rooms.cfg %s differs from the default %s of the code"
              % (key, match.group(1), default))
for key in ("WallTorchRoomSideDivisor", "WallTorchCorridorSpacing", "WallTorchMinDistance", "WallTorchActiveLights",
            "WallTorchActiveLightsReduced", "WallTorchSoundLoops", "WallTorchLightRadius", "WallTorchLightIntensity",
            "WallTorchFlickerStrength", "WallTorchFlickerSpeed"):
    check(key in dict(reads), "WallTorchConfig does not read " + key)

# Server: the list is computed by the placement and sent after the visible tiles
game_map = read("source", "gamemap", "GameMap.cpp")
check("WallTorches::compute(" in body(game_map, "void GameMap::updateWallTorches"),
      "the server does not compute the torches with WallTorches::compute")
turn = body(game_map, "unsigned long int GameMap::doMiscUpkeep")
check("updateWallTorches();" in turn and turn.index("sendVisibleTiles") < turn.index("updateWallTorches();"),
      "updateWallTorches is not called after sendVisibleTiles in doMiscUpkeep")
for place in (("source", "entities", "Tile.cpp"), ("source", "entities", "Building.cpp")):
    check("markWallTorchesDirty" in read(*place), "%s does not mark the torches dirty" % place[-1])
check("markWallTorchesDirty" in body(game_map, "void GameMap::addRoom") or
      game_map.count("markWallTorchesDirty();") >= 3, "adding or removing a room does not mark the torches dirty")

# Network: the packet is written and read in the same order, the message is known on both sides
written = body(game_map, "void GameMap::updateWallTorches")
read_back = body(game_map, "void GameMap::updateWallTorchesFromPacket")
check("mPacket << full;" in written and "OD_ASSERT_TRUE(is >> full >> nbRemoved);" in read_back,
      "wall torch packet: header differs on the two sides")
check("<< (index % sizeX) << (index / sizeX) << dir;" in written and "is >> x >> y >> dir" in read_back,
      "wall torch packet: removed torches differ on the two sides")
check("<< torch->mX << torch->mY << torch->mDir << torch->mSeatId;" in written
      and "is >> torch.mX >> torch.mY >> torch.mDir >> torch.mSeatId" in read_back,
      "wall torch packet: added torches differ on the two sides")
check("wallTorches" in read("source", "network", "ServerNotification.h"), "no wallTorches server notification")
check('return "wallTorches";' in read("source", "network", "ServerNotification.cpp"), "wallTorches has no type string")
client = read("source", "network", "ODClient.cpp")
check("case ServerNotificationType::wallTorches:" in client and "updateWallTorchesFromPacket" in client,
      "the client does not handle wallTorches")
server = read("source", "network", "ODServer.cpp")
check("resetWallTorchesSent" in server and "setWallTorchesSynced" in server,
      "the server does not send the whole list to a joining player")

# Client: the room ambience hands the list to the wall torch view, which reads the same config
ambience = read("source", "render", "RoomAmbience.cpp")
check("getWallTorchesVersion()" in body(ambience, "void RoomAmbience::syncWallTorches")
      and "setWallTorchSpots(spots)" in body(ambience, "void RoomAmbience::syncWallTorches"),
      "RoomAmbience does not pass the torch list on")
check("syncWallTorches();" in body(ambience, "void RoomAmbience::update"), "RoomAmbience::update does not sync the torches")
check("mWallTorches.update(" in ambience, "RoomAmbience does not update the wall torch view")
# The client shows exactly the list of the server: it stores what it receives and derives nothing
game_map_cpp = read("source", "gamemap", "GameMap.cpp")
received = body(game_map_cpp, "void GameMap::updateWallTorchesFromPacket")
check("WallTorches::compute(" not in received and "getTile(" not in received and "isServerGameMap" not in received,
      "the client derives torches from the map instead of taking the server list")
check("mWallTorches[WallTorches::getKey(torch.mX, torch.mY, torch.mDir, sizeX)] = torch;" in received
      and "mWallTorches.erase(" in received and "mWallTorches.clear();" in received,
      "updateWallTorchesFromPacket does not store the received list as it is")
check(game_map_cpp.count("WallTorches::compute(") == 1, "the torches are computed in more than one place")
check(game_map_cpp.count("mWallTorches[") == 2, "mWallTorches is written in an unexpected place")
sync = body(ambience, "void RoomAmbience::syncWallTorches")
check("getWallTorches()" in sync and "WallTorches::compute(" not in sync and "getTile(" not in sync
      and sync.count("spots.push_back(") == 1, "RoomAmbience::syncWallTorches does not pass on the list one to one")
# Recomputation at most once per turn: a flag, cleared when the list is computed
update = body(game_map_cpp, "void GameMap::updateWallTorches")
check("if(mWallTorchesDirty)" in update and "mWallTorchesDirty = false;" in update,
      "updateWallTorches does not compute only when the dirty flag is set")
check(game_map_cpp.count("updateWallTorches();") == 1, "updateWallTorches is called more than once per turn")
view = read("source", "render", "WallTorchView.cpp")
check("WallTorchConfig::load()" in view, "WallTorchView does not read WallTorchConfig")
for system in ("RoomAmbTorchBracket", "RoomAmbTorchFlame", "RoomAmbTorchSmoke"):
    check(system in view, "WallTorchView does not use " + system)
    check(system in read("particles", "RoomAmbienceDeferred.particle"), "particle system %s is missing" % system)
check("RoomAmbTorchGlow" not in view and "RoomAmbTorchGlow" not in read("particles", "RoomAmbienceDeferred.particle"),
      "the torch glow sprite is still present")

# The visible flame emitter must start at the actual mesh head, on all sides.
# Read the shared position buffer directly; no converter, compiler or game needed.
with open(os.path.join(ROOT, "models", "WallTorch.mesh"), "rb") as handle:
    mesh = handle.read()
vertices = []
def mesh_chunks(start, end, count=0, binding=-1, stride=0):
    while start + 6 <= end:
        chunk, length = struct.unpack_from("<HI", mesh, start)
        data, stop = start + 6, start + length
        if length < 6 or stop > len(mesh):
            raise ValueError("Invalid wall torch mesh chunk")
        if chunk == 0x3000:
            mesh_chunks(data + 1, stop)
        elif chunk == 0x5000:
            mesh_chunks(data + 4, stop, struct.unpack_from("<I", mesh, data)[0])
        elif chunk == 0x5200:
            binding, stride = struct.unpack_from("<HH", mesh, data)
            mesh_chunks(data + 4, stop, count, binding, stride)
        elif chunk == 0x5210 and binding == 0:
            vertices.extend(struct.unpack_from("<fff", mesh, data + i * stride) for i in range(count))
        start = stop
mesh_chunks(mesh.index(b"\n") + 1, len(mesh))
# The head's tilted top ring has the mesh's highest vertices; its center is the
# midpoint of that ring's front/back and highest/lowest extrema.
top = max(vertex[2] for vertex in vertices)
head = [vertex for vertex in vertices if vertex[2] > top - 0.004]
head_y = (min(vertex[1] for vertex in head) + max(vertex[1] for vertex in head)) / 2
head_z = (min(vertex[2] for vertex in head) + max(vertex[2] for vertex in head)) / 2
model_height = float(re.search(r"MODEL_HEIGHT = ([0-9.]+)", view).group(1))
flame_offset = float(re.search(r"FLAME_WALL_OFFSET = ([0-9.]+)", view).group(1))
heights = [float(number) for number in re.search(r"PART_HEIGHTS\[NB_PARTS\] = \{([^}]+)", view).group(1).split(",")]
particle = read("particles", "RoomAmbienceDeferred.particle")
flame = particle[particle.index("particle_system RoomAmbTorchFlame"):particle.index("particle_system RoomAmbTorchSmoke")]
emitter = tuple(float(number) for number in re.search(r"position\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)", flame).groups())
check("billboard_origin bottom_center" in flame, "torch flame is not anchored at its bottom")
check(emitter == (0, 0, 0), "torch flame emitter adds an offset above the torch head")
check(abs(flame_offset - head_y) < 0.00001, "flame forward offset does not match the torch head")
check(abs(heights[1] - (model_height + head_z)) < 0.00001, "flame height does not match the torch head")
check("position += torch.mDirection" in body(view, "void WallTorchView::createPart"), "flame offset does not follow wall side")
check("std::atan2(-torch.mDirection.x, torch.mDirection.y)" in body(view, "bool WallTorchView::createModel"), "holder orientation does not follow wall side")
for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
    angle = math.atan2(-dx, dy)
    rotated = (-math.sin(angle) * head_y, math.cos(angle) * head_y, model_height + head_z)
    emitted = (dx * flame_offset, dy * flame_offset, heights[1])
    check(all(abs(a - b) < 0.00001 for a, b in zip(rotated, emitted)), "flame misses head for wall side %s" % ((dx, dy),))

refresh = body(view, "void WallTorchView::refresh")
check("((part < NB_REDUCED_PARTS) || (torch.mLight != nullptr))" in refresh,
      "unlit torches must show only the holder and flame")
check(refresh.index("if(lit[i])") < refresh.index("bool wanted ="),
      "torch particles are selected before nearest-N lights")
check("for(uint32_t i = 0; i < NB_PARTS; ++i)\n                destroyPart(torch.mParts[i]);" in refresh,
      "hidden torches do not remove all particle parts")

# Build entries
cmake = read("CMakeLists.txt")
for source in ("rooms/WallTorches.cpp", "rooms/WallTorchConfig.cpp", "render/WallTorchView.cpp"):
    check("${SRC}/" + source in cmake, source + " is not in CMakeLists.txt")
tests_cmake = read("source", "tests", "CMakeLists.txt")
check("add_boost_test(00-WallTorches" in tests_cmake and "test_WallTorches.cpp" in tests_cmake,
      "the unit test 00-WallTorches is not registered")

# Crackling loop: nearest N torches, loop family from the real recording, stopped on removal / stopAll / hide
sounds = read("source", "sound", "SoundEffectsManager.cpp")
check("startSpatialLoop" in sounds and "stopSpatialLoop" in sounds and "setLoop(true)" in sounds,
      "SoundEffectsManager has no start/stop for positioned loops")
check(os.path.exists(os.path.join(ROOT, "sounds", "Spatial", "Rooms", "Torch", "Loop", "FxWallTorchLoop01.ogg")),
      "the torch loop recording is missing")
check('"Rooms/Torch/Loop"' in view and "startSpatialLoop(LOOP_FAMILY" in view and "mSettings.mSoundLoops" in view,
      "WallTorchView does not start the loop for the nearest torches")
check("stopSound(torch);" in body(view, "void WallTorchView::destroyTorch")
      and "stopSound(torch);" in body(view, "void WallTorchView::refresh"),
      "the loop is not stopped when a torch is removed or hidden")
check("destroyTorch(it->second);" in body(view, "void WallTorchView::stopAll"), "stopAll does not free the loops")
if problems:
    print("\n".join("PROBLEM: " + p for p in problems))
    sys.exit(1)

print("wall torch checks passed")
