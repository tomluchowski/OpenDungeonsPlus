"""Exercise the treasury gold layer: fill steps, the pile names that carry the level to the clients
(old names keep working), rounded pile footprints, and the wiring of the server,
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
# the old random offset or rotation.
assert 'TreasuryGoldLayer::meshName(shape)' in treasury
assert 'TreasuryObject::getMeshNameForGold' not in treasury
assert 'Random::Double' not in treasury
assert 'static_cast<double>(x), static_cast<double>(y), 0.0, 0.0, false' in treasury
# Neighbours are refreshed when a tile leaves the room and when gold changes.
assert 'The piles next to the removed tile lose a neighbour' in treasury
# The renderer builds the pile from its name, and the option reaches it.
assert 'TreasuryGoldMesh::prepareMesh(mSceneManager, meshName, pileFar)' in render
assert 'classicMeshForLevel' not in mesh and 'const bool reduced = (currentDetail != Detail::full)' in mesh
assert 'TREASURY_DETAIL' in read('source/utils/ConfigManager.h')
assert 'TreasuryDetail' in read('gui/WindowSettings.layout')
assert 'TreasuryGoldPile' in read('materials/scripts/TreasuryGoldPile.material')
assert (repo / 'materials/textures/TreasuryGoldPile.png').exists()
assert 'TreasuryGoldPile.png' in read('CREDITS')
assert 'TreasuryGoldMesh.cpp' in read('CMakeLists.txt')
assert 'addBand(surface, 1 + (rings - 1) * PileSectors, 1 + rings * PileSectors)' not in mesh
assert 'buildRoundSurface(shape, rings, roundSurface)' in mesh
# The classic stack names stay for free gold on the floor; room storage always uses round piles.
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

    // Heights and footprints: taller and wider with more gold, with the tile border left bare
    for(int level = 1; level <= maxLevel; ++level)
    {
        float center = heightAt(flatShape(level, 0), 0.5f, 0.5f);
        check(center > heightAt(PileShape(), 0.5f, 0.5f), "a pile is higher than the bare floor");
        if(level > 1)
            check(center > heightAt(flatShape(level - 1, 0), 0.5f, 0.5f), "each step is higher than the one below");
    }

    check(pileRadius(1) < pileRadius(maxLevel), "a full pile has a wider footprint");
    check(pileRadius(maxLevel) < 0.5f, "a full pile stays inside its tile");
    PileShape full = flatShape(maxLevel, 0);
    check(heightAt(full, 0.0f, 0.5f) < 0.01f && heightAt(full, 1.0f, 0.5f) < 0.01f,
        "a full pile leaves both tile edges bare");
    check(heightAt(flatShape(1, 0), 0.75f, 0.5f) < 0.01f && heightAt(full, 0.75f, 0.5f) > 0.01f,
        "a small pile exposes floor that a full pile covers");
    check(pileRadiusAt(full, 1.0f, 0.5f) != pileRadiusAt(full, 0.5f, 1.0f),
        "a full pile has an uneven outline");

    PileShape wall;
    wall.mLevel = 7;
    wall.mVariant = 3;
    check(heightAt(wall, 0.0f, 0.5f) < 0.01f, "an edge towards a wall slopes down to the floor");
    check(heightAt(wall, 0.5f, 0.5f) > 0.3f, "a full pile has a high middle");

    // Coins on top: a sea of coins on a full pile, growing with the level, none on a thin layer
    int previousCoins = 0;
    bool coinsGrow = true;
    for(int level = 0; level <= maxLevel; ++level)
    {
        int coins = topCoinCount(flatShape(level, 0));
        if(coins < previousCoins || coins > TreasurySettings::current().maxTopCoins)
            coinsGrow = false;
        previousCoins = coins;
    }
    check(coinsGrow, "top coins never drop with the level and never exceed the maximum");
    check(topCoinCount(flatShape(1, 0)) == 0, "a thin layer carries no coins on top");
    check(topCoinCount(flatShape(maxLevel, 0)) == 8, "a full pile carries eight coins (the triangle budget of round coins)");
    check(topCoinCount(flatShape(maxLevel, 0)) == TreasurySettings::current().maxTopCoins, "a full pile carries every coin the setting allows, none fewer");
    check(topCoinCount(flatShape(maxLevel, 0)) > 3 * topCoinCount(flatShape(2, 0)) / 2, "coins grow in several steps");
    check(gemCount(flatShape(4, 3)) == 0 && gemCount(flatShape(maxLevel, 3)) == 2, "rich piles carry up to two gems");
    int gemsMax = 0;
    for(int variant = 0; variant < variantCount; ++variant)
        for(int level = 0; level <= maxLevel; ++level)
            if(gemCount(flatShape(level, variant)) > gemsMax)
                gemsMax = gemCount(flatShape(level, variant));
    check(gemsMax <= TreasurySettings::current().maxGems, "gems never exceed the maximum");
    check(maxSpillCoins() == 4, "four spilled coins by default");

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
