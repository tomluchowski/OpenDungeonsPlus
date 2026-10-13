"""Exercise the creature rules on the treasury gold (lift, deep gold, step spacing, splash budget per room
and detail option) and the wiring of the renderer, the deposit effect, the particles and the sound."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


render = read('source/render/RenderManager.cpp')
mesh = read('source/render/TreasuryGoldMesh.cpp')

# Creatures are drawn on the gold through the render node only; the creature position is not touched.
assert 'TreasuryGoldMesh::surfaceHeight(position.x, position.y, goldLevel)' in render
assert 'node->setPosition(position + Ogre::Vector3(0, 0, lift))' in render
assert 'creature->setPosition' not in render.split('void RenderManager::updateCreatureStep')[1].split(
    'void RenderManager::cancelCreatureStep')[0]
# The piles are registered when drawn and forgotten when removed; creatures on the tile follow.
assert 'TreasuryGoldMesh::registerPile(' in render and 'TreasuryGoldMesh::unregisterPile(' in render
assert 'refreshCreaturesOnTile' in render
assert 'currentDetail == Detail::off' in mesh
# Splashes are limited by view, by room budget and by the detail option, and expire.
assert 'camera->isVisible(position)' in render
assert 'splashBudget(TreasuryGoldMesh::getDetail())' in render
assert 'updateTreasuryEffects(timeSinceLastFrame)' in render
assert 'Rooms/Treasury/CoinStep' in render
# Workers pouring gold: the client reacts to the existing deposit sound message, no new packet.
assert 'rrTreasuryDeposit(gameMap, xPos, yPos)' in read('source/network/ODClient.cpp')
assert 'TreasuryCoinSplash' in render and 'TreasuryCoinPour' in render
particles = read('particles/TreasuryCoins.particle')
assert 'particle_system TreasuryCoinSplash' in particles and 'particle_system TreasuryCoinPour' in particles
assert 'material        ReactionParticleCoin' in particles
# Assets are credited.
credits = read('CREDITS')
assert 'TreasuryCoins.particle' in credits and 'CoinStep*.ogg' in credits
for number in (1, 2, 3):
    assert (repo / ('sounds/Spatial/Rooms/Treasury/CoinStep/CoinStep%d.ogg' % number)).exists()

probe = r"""
#include "render/TreasuryCreatureRules.h"
#include <iostream>

using namespace TreasuryCreatureRules;

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

int main()
{
    check(liftOnGold(0.0f, 0.0f) == 0.0f, "no pile, no lift");
    check(liftOnGold(0.3f, 0.0f) == 0.3f, "a walking creature stands on the surface");
    check(liftOnGold(0.3f, 1.0f) == 0.0f, "a flying creature is not lifted");
    check(!isDeep(deepLevel - 1) && isDeep(deepLevel) && isDeep(7), "only deep gold splashes");
    check(!stepDue(0.1f, 0.1f), "a short way is no new step");
    check(stepDue(stepDistance, 0.0f) && stepDue(0.0f, -1.0f), "a long way is a new step");

    check(splashBudget(TreasuryGoldMesh::Detail::full) > splashBudget(TreasuryGoldMesh::Detail::reduced),
        "reduced detail shows fewer splashes than full");
    check(splashBudget(TreasuryGoldMesh::Detail::reduced) > 0, "reduced detail still splashes");
    check(splashBudget(TreasuryGoldMesh::Detail::off) == 0, "no splashes when the detail is off");

    // The local dent forms, is deepest after dentShare of the settle time and is gone when the pile has settled
    check(localDentFactor(0.0f) == 0.0f, "no dent before the gold is taken");
    check(localDentFactor(pileSettleTime * dentShare) > 0.99f, "the dent is deepest early in the settle time");
    check(localDentFactor(pileSettleTime * dentShare) > localDentFactor(pileSettleTime * 0.8f), "the dent fills up again");
    check(localDentFactor(pileSettleTime) == 0.0f && localDentFactor(pileSettleTime * 2.0f) == 0.0f,
        "the dent is gone when the pile has settled");
    check(dentRadius > 0.0f && dentRadius <= 0.5f && dentLocalDepth > 0.0f, "the dent stays inside its tile");

    // Level of detail: far piles are reduced, with a hysteresis so a pile on the border does not flip
    check(!lodReducedAt(false, lodFarDistance - 1.0f), "a near pile keeps the full mesh");
    check(lodReducedAt(false, lodFarDistance + 1.0f), "a far pile uses the reduced mesh");
    check(lodReducedAt(true, lodFarDistance - 1.0f) == (lodHysteresis < 1.0f ? false : true),
        "a reduced pile just inside the border stays reduced");
    check(!lodReducedAt(true, lodFarDistance - lodHysteresis - 1.0f), "a reduced pile returns to full when clearly near");
    check(lodHysteresis >= 0.0f && lodSwitchesPerUpdate >= 1 && lodInterval > 0.0f, "the level of detail is bounded");

    check(rebuildPerFrame >= 1, "the option change rebuilds at least one pile per frame");

    SplashBudget budget;
    int roomA = 0;
    int roomB = 0;
    const int limit = splashBudget(TreasuryGoldMesh::Detail::full);
    int granted = 0;
    for(int i = 0; i < limit + 5; ++i)
        if(budget.tryAcquire(&roomA, limit))
            ++granted;
    check(granted == limit, "a room never shows more splashes than its budget");
    check(budget.tryAcquire(&roomB, limit), "another room has a budget of its own");
    budget.release(&roomA);
    check(budget.active(&roomA) == limit - 1, "an expired splash frees a place");
    check(budget.tryAcquire(&roomA, limit), "the freed place can be used again");
    check(!budget.tryAcquire(&roomA, 0), "a budget of zero grants nothing");
    for(int i = 0; i < limit; ++i)
        budget.release(&roomA);
    check(budget.active(&roomA) == 0, "all places are free again");
    budget.release(&roomA);
    check(budget.active(&roomA) == 0, "releasing too often does not go negative");
    budget.clear();
    check(budget.active(&roomB) == 0, "clear forgets every room");

    std::cout << "CHECKS=" << checks << " FAILURES=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
"""

with tempfile.TemporaryDirectory(prefix='odp-treasury-creatures-') as tmp:
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
