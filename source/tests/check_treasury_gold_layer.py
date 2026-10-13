"""Exercise the treasury gold layer: fill steps, the pile names that carry the level to the clients
(old names keep working), seamless edges between neighbouring piles, and the wiring of the server,
the renderer and the 'Treasury detail' option."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


treasury = read('source/rooms/RoomTreasury.cpp')
render = read('source/render/RenderManager.cpp')
mesh = read('source/render/TreasuryGoldMesh.cpp')

# The server names the pile after the level and the corners, and builds it at the tile centre without
# the old random offset or rotation, so neighbouring piles join up.
assert 'TreasuryGoldLayer::meshName(shape)' in treasury
assert 'TreasuryObject::getMeshNameForGold' not in treasury
assert 'Random::Double' not in treasury
assert 'static_cast<double>(x), static_cast<double>(y), 0.0, 0.0, false' in treasury
# Neighbours are refreshed when a tile leaves the room and when gold changes.
assert 'The piles next to the removed tile lose a neighbour' in treasury
# The renderer builds the pile from its name, and the option reaches it.
assert 'TreasuryGoldMesh::prepareMesh(mSceneManager, meshName)' in render
assert 'classicMeshForLevel' in mesh and 'Detail::off' in mesh
assert 'TREASURY_DETAIL' in read('source/utils/ConfigManager.h')
assert 'TreasuryDetail' in read('gui/WindowSettings.layout')
assert 'TreasuryGoldPile' in read('materials/scripts/TreasuryGoldPile.material')
assert (repo / 'materials/textures/TreasuryGoldPile.png').exists()
assert 'TreasuryGoldPile.png' in read('CREDITS')
assert 'TreasuryGoldMesh.cpp' in read('CMakeLists.txt')
# The classic stacks stay for free gold on the floor and for the 'off' setting.
assert 'GoldstackLv1' in read('source/entities/TreasuryObject.cpp')

probe = r"""
#include "rooms/TreasuryGoldLayer.h"
#include <iostream>
#include <set>

using namespace TreasuryGoldLayer;

static int checks = 0;
static int failures = 0;
static void check(bool ok, const char* what)
{
    ++checks;
    if(!ok)
    {
        ++failures;
        std::cout << "FAIL: " << what << '\n';
    }
}

static PileShape flatShape(int level, int variant)
{
    PileShape shape;
    shape.mLevel = level;
    shape.mVariant = variant;
    for(int i = 0; i < 4; ++i)
        shape.mCorner[i] = level;
    return shape;
}

int main()
{
    // Fill steps: empty, then at least six steps up to full, never decreasing
    check(levelForGold(0, 1000) == 0, "no gold is level 0");
    check(levelForGold(-5, 1000) == 0, "negative gold is level 0");
    check(levelForGold(1, 1000) == 1, "one coin already shows a pile");
    check(levelForGold(1000, 1000) == maxLevel, "full tile is the top level");
    check(levelForGold(5000, 1000) == maxLevel, "gold above the capacity stays at the top level");
    check(levelForGold(10, 0) == maxLevel, "no capacity does not divide by zero");
    std::set<int> seen;
    int previous = 0;
    bool monotonic = true;
    for(int gold = 0; gold <= 1000; ++gold)
    {
        int level = levelForGold(gold, 1000);
        seen.insert(level);
        if(level < previous)
            monotonic = false;
        previous = level;
    }
    check(monotonic, "fill step never drops while gold grows");
    check(seen.size() >= 7, "empty plus at least six pile steps");
    check(levelForGold(500, 1000) > levelForGold(200, 1000), "more gold, higher step");
    check(levelForGold(300, 300) == maxLevel, "capacity scales with research");

    // Names: every shape round trips, anything else (old stack names, damaged names) is not a pile
    int roundTrips = 0;
    for(int level = 0; level <= maxLevel; ++level)
        for(int corner = 0; corner <= maxLevel; ++corner)
            for(int variant = 0; variant < variantCount; ++variant)
            {
                PileShape shape;
                shape.mLevel = level;
                for(int i = 0; i < 4; ++i)
                    shape.mCorner[i] = (corner + i) % (maxLevel + 1);
                shape.mVariant = variant;
                PileShape back;
                if(parseMeshName(meshName(shape), back) && back.mLevel == level && back.mVariant == variant &&
                   back.mCorner[0] == shape.mCorner[0] && back.mCorner[3] == shape.mCorner[3])
                    ++roundTrips;
            }
    check(roundTrips == (maxLevel + 1) * (maxLevel + 1) * variantCount, "all pile names round trip");
    PileShape other;
    check(!parseMeshName("GoldstackLv2", other), "old stack names are not piles");
    check(!parseMeshName("TreasuryGold_5_5577", other), "short names are rejected");
    check(!parseMeshName("TreasuryGold_9_5577_1", other), "level above the maximum is rejected");
    check(!parseMeshName("TreasuryGold_5_5577_7", other), "unknown variant is rejected");
    check(!parseMeshName("", other), "empty name is rejected");
    PileShape sample;
    sample.mLevel = 5;
    check(meshName(sample) == "TreasuryGold_5_0000_0", "name format is stable");

    // Heights: taller with more gold, and two tiles share the height along their common edge
    for(int level = 1; level <= maxLevel; ++level)
    {
        float center = heightAt(flatShape(level, 0), 0.5f, 0.5f);
        check(center > heightAt(PileShape(), 0.5f, 0.5f), "a pile is higher than the bare floor");
        if(level > 1)
            check(center > heightAt(flatShape(level - 1, 0), 0.5f, 0.5f), "each step is higher than the one below");
    }

    PileShape left;
    PileShape right;
    left.mLevel = 7;
    right.mLevel = 3;
    left.mVariant = 1;
    right.mVariant = 2;
    // left tile is west of right tile: left NE and SE corners are right NW and SW corners
    left.mCorner[0] = 7; left.mCorner[1] = 3; left.mCorner[2] = 3; left.mCorner[3] = 7;
    right.mCorner[0] = 3; right.mCorner[1] = 3; right.mCorner[2] = 3; right.mCorner[3] = 3;
    bool seamless = true;
    for(int i = 0; i <= 20; ++i)
    {
        float v = static_cast<float>(i) / 20.0f;
        float a = heightAt(left, 1.0f, v);
        float b = heightAt(right, 0.0f, v);
        if(a < b - 0.0001f || a > b + 0.0001f)
            seamless = false;
    }
    check(seamless, "piles of different level meet without a step at their shared edge");

    PileShape wall;
    wall.mLevel = 7;
    wall.mVariant = 3;
    check(heightAt(wall, 0.0f, 0.5f) < 0.01f, "an edge towards a wall slopes down to the floor");
    check(heightAt(wall, 0.5f, 0.5f) > 0.3f, "a full pile has a high middle");

    bool nonNegative = true;
    for(int variant = 0; variant < variantCount; ++variant)
        for(int level = 1; level <= maxLevel; ++level)
            for(int i = 0; i <= 10; ++i)
                for(int j = 0; j <= 10; ++j)
                    if(heightAt(flatShape(level, variant), i / 10.0f, j / 10.0f) < 0.0f)
                        nonNegative = false;
    check(nonNegative, "the surface never dips below the floor");

    check(std::string(classicMeshForLevel(1)) == "GoldstackLv1", "level 1 falls back to the small stack");
    check(std::string(classicMeshForLevel(7)) == "GoldstackLv4", "level 7 falls back to the big stack");

    std::cout << "CHECKS=" << checks << " FAILURES=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
"""

with tempfile.TemporaryDirectory(prefix='odp-treasury-gold-') as tmp:
    probe_path = Path(tmp) / 'check.cpp'
    probe_path.write_text(probe)
    exe = Path(tmp) / 'check.exe'
    cmd = 'cl /nologo /EHsc /MD /std:c++14 /I source /Fo"{}" {} /Fe"{}"'.format(
        str(Path(tmp) / 'check.obj'), str(probe_path), str(exe))
    build = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=str(repo))
    if build.returncode != 0:
        print(build.stdout)
        print(build.stderr)
        raise SystemExit(1)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    print(run.stdout)
    if run.returncode != 0:
        print(run.stderr)
        raise SystemExit(1)

print('WIRING OK')
