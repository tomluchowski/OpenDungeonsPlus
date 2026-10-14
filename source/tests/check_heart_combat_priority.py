#!/usr/bin/env python3
"""Non-compiling execution of heart-defence early-return conditions from production source.
Does not execute C++ combat/pathfinding; the integrator must verify the running game.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
source = (root / 'source/entities/Creature.cpp').read_text()
start = source.index('void Creature::handleHeartDefence()')
body = source[start:source.index('bool Creature::handleIdleAction()', start)]
conditions = re.findall(r'if\((.*?)\)\s*return;', body, re.S)
assert len(conditions) == 4, 'all production early-return conditions must be modeled'
assert body.index('CreatureActionType::fight') < body.index('clearActionQueue();')
assert body.index('CreatureActionType::flee') < body.index('clearActionQueue();')
assert source.index('decidePrioritaryAction();') < source.index('handleHeartDefence();')
assert 'pushAction(Utils::make_unique<CreatureActionGoDefendHeart>(*this))' in body

def apply(queue, runner=True, seat=True, active=True, conditions=conditions):
    values = {'runner': runner, 'seat': seat, 'active': active, 'queue': queue}
    for expression in conditions:
        expression = expression.replace('getDefinition()->isHeartDefenceRunner()', 'runner')
        expression = expression.replace('seat == nullptr', '(not seat)')
        expression = expression.replace('seat->getHeartDefenceActive()', 'active')
        expression = re.sub(r'isActionInList\(CreatureActionType::(\w+)\)', r"('\1' in queue)", expression)
        expression = expression.replace('||', ' or ').replace('&&', ' and ')
        expression = re.sub(r'!(?!=)', ' not ', expression).strip()
        if eval(expression, {'__builtins__': {}}, values):
            return list(queue)
    return ['goDefendHeart']

# Prior to the correction the same source conditions always replaced chosen combat/retreat.
old_conditions = [e for e in conditions if 'CreatureActionType::fight' not in e]
assert apply(['fight'], conditions=old_conditions) == ['goDefendHeart']
assert apply(['flee'], conditions=old_conditions) == ['goDefendHeart']
for queue in (['fight'], ['fight', 'walkToTile'], ['flee'], ['flee', 'walkToTile']):
    assert apply(queue) == queue
# No combat chosen: established alarm gathering and normal jobs remain.
assert apply(['searchJob']) == ['goDefendHeart']
assert apply(['claimWallTile']) == ['goDefendHeart']
assert apply(['goDefendHeart', 'walkToTile']) == ['goDefendHeart', 'walkToTile']
assert apply(['searchFood'], active=False) == ['searchFood']
assert apply(['sleep'], runner=False) == ['sleep']
assert apply(['idle'], seat=False) == ['idle']
attack = (root / 'source/creaturebehaviour/CreatureBehaviourAttackEnemy.cpp').read_text()
assert 'creature.getVisibleEnemyObjects().empty()' in attack
assert 'creature.fight();' in attack and 'creature.flee();' in attack
fight = (root / 'source/creatureaction/CreatureActionFight.cpp').read_text()
assert 'searchBestTargetInList' in fight and 'getGameMap()->path' in fight
print('Heart combat priority: old override reproduced; combat/retreat/walking retained; normal alarm/jobs preserved. C++ not executed.')

# Preserve the existing alarm/range/target/wiring contracts without running its compiler fixture.
fixture = root / 'source/tests/check_heart_defence.py'
static_contracts = fixture.read_text().split('with tempfile.TemporaryDirectory(prefix="odp-heart-defence-")')[0]
exec(static_contracts, {'__file__': str(fixture)})
print('Existing heart-defence source contracts passed; compiled fixture deliberately not executed.')
