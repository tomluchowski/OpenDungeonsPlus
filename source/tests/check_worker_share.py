"""Worker share rules: how many workers re-arm traps, and the old limits for digging, claiming and carrying.

Calls (from the repository root):
  python source/tests/check_worker_share.py            source wiring checks only (no compiler, no game)
  python source/tests/check_worker_share.py --probe    additionally compiles and runs a small C++ probe of the
                                                       pure rules in source/game/WorkerShare.h

The probe needs the MSVC compiler on the path (run it in a Visual Studio developer shell, like the other
probes, e.g. check_room_takeover.py --probe). The full build and check run does that at the end of a plan.
The probe checks: the counting of the workers (total), the 20 percent carry limit, the 80 percent limit for
claiming walls first, and the reload share (first reloading worker always allowed, then blocked or open by the
configured share, edge cases 0 percent, 100 percent and no workers)."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


header = read('source/game/WorkerShare.h')
player = read('source/game/Player.cpp')
traps_cfg = read('config/traps.cfg')

# The header is pure: no game includes, only the integer types
includes = re.findall(r'^#include\s+(.+)$', header, re.M)
assert includes == ['<cstdint>'], includes
for name in ('totalWorkers', 'isCarryFirst', 'isClaimWallFirst', 'isReloadShareOpen'):
    assert re.search(r'inline \w+ ' + name + r'\(', header), name + ' missing in the header'

# Player uses the header for both functions and no longer repeats the arithmetic
assert '#include "game/WorkerShare.h"' in player
prefs = function_body(player, 'std::vector<CreatureActionType> Player::getWorkerPreferredActions(')
assert 'WorkerShare::totalWorkers(' in prefs
assert 'WorkerShare::isCarryFirst(' in prefs and 'WorkerShare::isClaimWallFirst(' in prefs
assert 'getNbWorkersDoing(CreatureActionType::reloadTrap)' in prefs
assert '0.2' not in prefs and '0.8' not in prefs
share = function_body(player, 'bool Player::isWorkerReloadShareOpen()')
assert 'WorkerShare::totalWorkers(' in share and 'WorkerShare::isReloadShareOpen(' in share
assert 'TrapReloadWorkerSharePercent' in share and '20.0' in share
assert re.search(r'^    TrapReloadWorkerSharePercent\t\d+', traps_cfg, re.M)

# The limits of the header are the old ones
assert re.search(r'percent <= 0\.2;', function_body(header, 'inline bool isCarryFirst('))
assert re.search(r'percent > 0\.8;', function_body(header, 'inline bool isClaimWallFirst('))

if '--probe' in sys.argv:
    probe = r'''
#include "game/WorkerShare.h"
#include <iostream>
static int gFailures = 0;
static void check(bool ok, const char* msg)
{
    if(!ok)
    {
        ++gFailures;
        std::cout << "FAIL: " << msg << '\n';
    }
}
int main()
{
    // Counting: all listed jobs and the reloading workers, plus the worker that chooses
    check(WorkerShare::totalWorkers(0, 0, 0, 0, 0) == 1, "no workers: only the one choosing");
    check(WorkerShare::totalWorkers(2, 3, 1, 4, 5) == 16, "total counts every job once, plus one");
    check(WorkerShare::totalWorkers(0, 0, 0, 0, 3) == 4, "reloading workers are part of the total");

    // The old limits stay: carry first up to 20 percent, claim walls first above 80 percent
    check(WorkerShare::isCarryFirst(0, 1), "carry: nobody carries");
    check(WorkerShare::isCarryFirst(1, 5), "carry: exactly 20 percent still carries first");
    check(!WorkerShare::isCarryFirst(2, 5), "carry: 40 percent does not");
    check(!WorkerShare::isClaimWallFirst(4, 0, 5), "wall: exactly 80 percent does not");
    check(WorkerShare::isClaimWallFirst(3, 2, 6), "wall: 5 of 6 digging or claiming ground does");
    check(!WorkerShare::isClaimWallFirst(0, 0, 1), "wall: nobody digging or claiming");
    // Reloading workers make the others a smaller part, without touching the limits themselves
    check(WorkerShare::isClaimWallFirst(5, 0, WorkerShare::totalWorkers(5, 0, 0, 0, 0)), "wall: 5 of 6 without reloaders");
    check(!WorkerShare::isClaimWallFirst(5, 0, WorkerShare::totalWorkers(5, 0, 0, 0, 2)), "wall: 5 of 8 with two reloaders");

    // Reload share: the first worker is always allowed
    check(WorkerShare::isReloadShareOpen(0, 1, 20.0), "first worker, no other workers");
    check(WorkerShare::isReloadShareOpen(0, 100, 20.0), "first worker, many others");
    check(WorkerShare::isReloadShareOpen(0, 5, 0.0), "first worker at 0 percent");
    // Then by share: 20 percent
    check(WorkerShare::isReloadShareOpen(1, 6, 20.0), "one of 6 is below 20 percent");
    check(WorkerShare::isReloadShareOpen(1, 5, 20.0), "one of 5 is exactly 20 percent and open");
    check(!WorkerShare::isReloadShareOpen(1, 4, 20.0), "one of 4 is above 20 percent and blocked");
    check(WorkerShare::isReloadShareOpen(2, WorkerShare::totalWorkers(8, 0, 0, 0, 2), 20.0), "2 reloading, 8 others is open");
    check(!WorkerShare::isReloadShareOpen(3, WorkerShare::totalWorkers(8, 0, 0, 0, 3), 20.0), "3 reloading, 8 others is blocked");
    // Edge cases: 0 percent blocks every second worker, 100 percent never blocks
    check(!WorkerShare::isReloadShareOpen(1, 10, 0.0), "0 percent: the second worker is blocked");
    check(!WorkerShare::isReloadShareOpen(1, 1000, 0.0), "0 percent: also with many workers");
    check(WorkerShare::isReloadShareOpen(1, 2, 100.0), "100 percent: second worker allowed");
    check(WorkerShare::isReloadShareOpen(9, 10, 100.0), "100 percent: never blocked");
    // No workers doing anything else: total is 1 (the chooser) and the first worker is open
    check(WorkerShare::isReloadShareOpen(0, WorkerShare::totalWorkers(0, 0, 0, 0, 0), 20.0), "no workers at all");
    std::cout << "FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='odp-worker-share-') as directory:
        work = Path(directory)
        (work / 'check.cpp').write_text(probe)
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/I', str(root / 'source'), 'check.cpp', '/Fecheck.exe'],
                       cwd=work, check=True, stdout=subprocess.DEVNULL)
        result = subprocess.run([str(work / 'check.exe')], cwd=work, capture_output=True, text=True)
        sys.stdout.write(result.stdout)
        assert result.returncode == 0, 'probe failed'

print('worker share: ok')
