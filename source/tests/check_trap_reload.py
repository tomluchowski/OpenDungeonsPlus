"""Workers re-arm traps that used up their shots; doors and traps are carried by workers.

Pure source wiring checks (no compiler, no game): server authority, configuration, one worker per
tile, save compatibility (optional trailing field), the action in the worker's idle choice."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


traps_cfg = read('config/traps.cfg')
trap = read('source/traps/Trap.cpp')
trap_h = read('source/traps/Trap.h')
action = read('source/creatureaction/CreatureActionReloadTrap.cpp')
action_h = read('source/creatureaction/CreatureActionReloadTrap.h')
action_enum = read('source/creatureaction/CreatureAction.h')
action_names = read('source/creatureaction/CreatureAction.cpp')
creature = read('source/entities/Creature.cpp')
cmake = read('CMakeLists.txt')
config_h = read('source/utils/ConfigManager.h')

# Every value is in the configuration: documented, set, read with a default
keys = ['TrapReloadByWorkers', 'TrapReloadWorkTurns', 'TrapReloadCostDefault', 'TrapReloadSearchRadius',
        'TrapReloadRetryTurns', 'TrapReloadExperience', 'TrapReloadWorkerSharePercent']
player = read('source/game/Player.cpp')
for key in keys:
    assert re.search(r'^# ' + key + r'\s', traps_cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\t\d+', traps_cfg, re.M), key + ' not set'
    assert re.search(r'getTrapConfigDoubleOrDefault\(\s*"' + key + '"', trap + action + player), key + ' not read'
assert 'getTrapConfigDoubleOrDefault' in config_h

# The old percentage of the build price is gone; every trap type with shots has its own price in the configuration
assert 'TrapReloadCostPercent' not in traps_cfg + trap + action + player
price = function_body(trap, 'int32_t Trap::getReloadPrice()')
doc_start = traps_cfg.index('# <Trap>ReloadCost')
doc_block = traps_cfg[doc_start:traps_cfg.index('# TrapReloadWorkerSharePercent')]
assert 'costPerTile' not in price
type_keys = {'cannon': 'Cannon', 'spike': 'Spike', 'boulder': 'Boulder', 'fear': 'Fear', 'gas': 'Gas',
             'lightning': 'Lightning', 'fireburst': 'Fireburst', 'freeze': 'Freeze', 'watchBanner': 'WatchBanner',
             'alarm': 'Alarm', 'trigger': 'Trigger'}
for trap_type, prefix in type_keys.items():
    key = prefix + 'ReloadCost'
    assert prefix in doc_block, prefix + ' not documented'
    assert re.search(r'^    ' + key + r'\t\d+', traps_cfg, re.M), key + ' not set'
    assert 'case TrapType::' + trap_type + ':' in price, trap_type + ' not priced'
    assert re.search(r'getTrapConfigDoubleOrDefault\("' + key + '", defaultPrice\)', price), key + ' not read'
# Every trap type that has a shot count in the configuration has a reload price
for prefix in re.findall(r'^    (\w+)NbShootsBeforeDeactivation\t', traps_cfg, re.M):
    assert re.search(r'^    ' + prefix + r'ReloadCost\t\d+', traps_cfg, re.M), prefix + ' has no reload price'

# Reloading counts in the worker share rules: counted by the action, part of the total, own share, gate in the idle choice
assert 'notifyWorkerAction(mCreature, getType())' in action.split('CreatureActionReloadTrap::~')[0]
assert 'notifyWorkerStopsAction(mCreature, getType())' in action.split('CreatureActionReloadTrap::~')[1].split('}')[0]
prefs = function_body(player, 'std::vector<CreatureActionType> Player::getWorkerPreferredActions(')
assert 'getNbWorkersDoing(CreatureActionType::reloadTrap)' in prefs and 'nbWorkersReloading)' in prefs
share = function_body(player, 'bool Player::isWorkerReloadShareOpen()')
assert 'CreatureActionType::reloadTrap' in share and 'TrapReloadWorkerSharePercent' in share
assert 'isWorkerReloadShareOpen' in read('source/game/Player.h')

# The action exists, is named, built and chosen by idle workers before the other jobs
assert 'reloadTrap,' in action_enum and 'case CreatureActionType::reloadTrap:' in action_names
assert 'CreatureActionReloadTrap.cpp' in cmake
idle = function_body(creature, 'bool Creature::handleIdleAction()')
assert 'CreatureActionReloadTrap::tryStart(*this)' in idle
assert idle.index('CreatureActionReloadTrap::tryStart') < idle.index('getWorkerPreferredActions')
assert idle.index('isWorkerReloadShareOpen()') < idle.index('CreatureActionReloadTrap::tryStart')

# Server only, own seat, price from the config, paid when the work is done, one worker per tile
start = function_body(action, 'bool CreatureActionReloadTrap::tryStart(')
assert 'isServerGameMap()' in start and 'trap->getSeat() != creature.getSeat()' in start
assert 'canBeReloadedByWorker(' in start and 'pathExists(' in start and 'getReloadPrice()' in start
work = function_body(action, 'bool CreatureActionReloadTrap::handleReloadTrap(')
assert 'withdrawFromTreasuries(' in work and 'trap->activate(&tileReload)' in work
assert work.index('workTurns < workNeeded') < work.index('withdrawFromTreasuries(')
assert 'postponeReload(' in work
assert 'setReloadWorker(&mTileReload, &mCreature)' in action
assert 'setReloadWorker(&mTileReload, nullptr)' in action

# Only tiles that used up their shots: never doors, never a newly placed trap, never one with a crafted trap on its way
can = function_body(trap, 'bool Trap::canBeReloadedByWorker(')
for needle in ('isDoor()', 'isServerGameMap()', 'isExhausted()', 'getCarriedCraftedTrap()', 'getReloadWorker()',
               'TrapReloadByWorkers'):
    assert needle in can, needle
fire = function_body(trap, 'bool Trap::fireTile(')
assert 'setExhausted(true)' in fire
assert 'setExhausted(false)' in function_body(trap, 'void Trap::activate(')

# Save: the exhausted flag is appended after the old fields and read optionally; the reservation is runtime only
export = function_body(trap, 'void Trap::exportTileDataToStream(')
assert export.rindex('isExhausted()') > export.index('seat->getId()')
importBody = function_body(trap, 'bool Trap::importTileDataFromStream(')
assert importBody.index('(is >> exhausted)') > importBody.index('seatSawTriggering(seat)')
assert 'mReloadWorker(nullptr)' in trap_h

# The existing carry of crafted traps and doors stays: the trap accepts a crafted trap on an unarmed tile
assert 'EntityCarryType::craftedTrap' in read('source/entities/CraftedTrap.h')
assert 'getNbNeededCraftedTrap' in function_body(trap, 'bool Trap::hasCarryEntitySpot(')

print('trap reload and door carry: ok')
