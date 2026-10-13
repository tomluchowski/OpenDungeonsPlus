"""Chicken flight: decision (trigger, limits, cooldown), wiring and compatibility. Pure Python, compiles nothing."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


header = read('source/entities/ChickenFlight.h')


def constant(name):
    match = re.search(r'const\s+(?:double|int32_t)\s+' + name + r'\s*=\s*([0-9.]+);', header)
    assert match, name
    return float(match.group(1))


DIST_MAX = constant('TRIGGER_DISTANCE_MAX')
DIST_MIN = constant('TRIGGER_DISTANCE_MIN')
COOLDOWN = constant('COOLDOWN_SECONDS')
MAX_HOPS = int(constant('MAX_HOPS_IN_ROW'))
HOLD = constant('HOLD_STILL_SECONDS')
TPS = 20.0


class State:
    def __init__(self):
        self.cooldown = 0
        self.hops = 0


def tick(state):
    if state.cooldown > 0:
        state.cooldown -= 1


def should_flee(state, distance, can_move):
    if not can_move:
        return False
    if distance > DIST_MAX or distance <= DIST_MIN:
        return False
    return state.cooldown <= 0


def register(state):
    state.hops += 1
    if state.hops >= MAX_HOPS:
        state.cooldown = int(HOLD * TPS)
        state.hops = 0
    else:
        state.cooldown = int(COOLDOWN * TPS)


# The port above must match the header line by line
for needle in ('if(state.mCooldownTurns > 0)', 'if(!canMove)', '(distance > TRIGGER_DISTANCE_MAX) || (distance <= TRIGGER_DISTANCE_MIN)',
               'return state.mCooldownTurns <= 0;', 'state.mHopsInRow >= MAX_HOPS_IN_ROW',
               'state.mCooldownTurns = static_cast<int32_t>(HOLD_STILL_SECONDS * turnsPerSecond);',
               'state.mCooldownTurns = static_cast<int32_t>(COOLDOWN_SECONDS * turnsPerSecond);'):
    assert needle in header, needle

# Sanity of the constants: catchable at all
assert 0.0 < DIST_MIN < DIST_MAX and MAX_HOPS >= 1 and COOLDOWN > 0 and HOLD > COOLDOWN
# A creature next to the chicken (squared tile distance <= 1) always catches it: no hop at that range
assert DIST_MIN >= 1.0

# Trigger
state = State()
assert should_flee(state, 2.5, True)
assert not should_flee(state, 3.5, True), 'too far'
assert not should_flee(state, 1.0, True), 'about to be caught'
assert not should_flee(state, 1.6, True), 'edge of the catch range'
assert not should_flee(state, 2.5, False), 'no place to hop to'

# Limits and cooldown: a creature standing at 2 tiles for a long time
state = State()
hops = []
for turn in range(int(120 * TPS)):
    tick(state)
    if should_flee(state, 2.0, True):
        register(state)
        hops.append(turn)
assert hops, 'it hops'
assert hops[1] - hops[0] >= COOLDOWN * TPS - 1, 'cooldown between two hops'
assert hops[2] - hops[1] >= HOLD * TPS - 1, 'hold still after the hops in a row'
per_minute = sum(1 for h in hops if h < 60 * TPS)
assert per_minute <= 2 * (60 / (COOLDOWN + HOLD)) + 2, per_minute
# Frequency over the long run is bounded
assert len(hops) <= 120 / ((COOLDOWN + HOLD) / MAX_HOPS) + 1

# Always catchable: whatever the creature does, at range <= DIST_MIN the chicken does not move away
for turn in range(1000):
    tick(state)
    assert not should_flee(state, DIST_MIN, True)

# Wiring on the server
cpp = read('source/entities/ChickenEntity.cpp')
assert '#include "entities/ChickenFlight.h"' in cpp
upkeep = cpp[cpp.index('void ChickenEntity::doUpkeep()'):]
upkeep = upkeep[:upkeep.index('\n}\n')]
assert upkeep.index('ChickenFlight::tick(mFlight);') < upkeep.index('if(isMoving())') < upkeep.index('tryFlee(tile, currentHatchery)')
flee = cpp[cpp.index('bool ChickenEntity::tryFlee('):]
flee = flee[:flee.index('\n}\n')]
for needle in ('!mLockedEat', 'currentHatchery == nullptr', 'mChickenState != ChickenState::free', 'collectMovePositions(tile, currentHatchery, positions)',
               'ChickenFlight::shouldFlee(', 'ChickenFlight::registerFlight(', 'setWalkPath('):
    assert needle in flee, needle
# Report only: nothing in the flight touches the lock, the state, the hunger or the creature
for forbidden in ('setLockEat', 'mChickenState =', 'foodEaten', 'setHunger', 'popAction', 'clearActionQueue', 'eatChicken'):
    assert forbidden not in flee, forbidden
# The positions are only inside the room the chicken stands in (the normal wander rule)
assert 'currentHatchery != tile->getCoveringBuilding()' in cpp
# Not saved: the stream format is unchanged
assert 'mFlight' not in cpp[cpp.index('void ChickenEntity::exportToStream'):]

print('chicken flight checks passed')
