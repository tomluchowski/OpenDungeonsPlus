"""Recorded nest-wait simulation and source-contract regression, not C++ execution.
The record covers finite collision-grid/roaming/nominal-clock approximations;
it is not a proof of all possible layouts. Source changes require revalidation.
"""
from pathlib import Path
import hashlib
import itertools
import json
import re

ROOT = Path(__file__).resolve().parents[2]
RECORD = ROOT/'source/tests/fixtures/hatchery_nest_parity.json'
ACCEPTANCE = 'akzeptiert von Mario 2026-10-08'
EXCEPTIONS = {
    ('6x6-two', 10, 3): (6194, 6380),
    ('6x6-two', 10, 777): (6194, 6381),
}

def case_key(row):
    return row['layout'], row['demand'], row['seed']

def allowed(row):
    if row['old'] <= 0 or row['diff'] != 100*(row['new']-row['old'])/row['old']:
        return False
    key = case_key(row)
    if key in EXCEPTIONS:
        return (row['old'], row['new']) == EXCEPTIONS[key]
    return abs(row['diff']) <= 3.0

# Only the exact two accepted outcomes can pass above the original limit.
accepted = dict(layout='6x6-two', demand=10, seed=3, old=6194, new=6380, diff=100*186/6194)
assert allowed(accepted)
for field, value in [('layout', '6x6-four'), ('demand', 20), ('seed', 99), ('old', 6195), ('new', 6381)]:
    changed = dict(accepted)
    changed[field] = value
    changed['diff'] = 100*(changed['new']-changed['old'])/changed['old']
    assert not allowed(changed), (field, value)
assert not allowed(dict(layout='12x12-eight', demand=10, seed=99, old=10000, new=10301, diff=3.01))
assert not allowed(dict(accepted, diff=3.0)), 'rounding cannot disguise a different result'

record = json.loads(RECORD.read_text())
assert record['turns'] == 84000
assert record['settings'] == dict(min=3, max=9, factor='0.6516666667', hatch=2, grow=4)
config = (ROOT/'config/rooms.cfg').read_text()
for name, value in [('HatcheryLayMin','3'), ('HatcheryLayMax','9'), ('HatcheryLayFactor','0.6516666667'),
                    ('HatcheryHatchTurns','2'), ('HatcheryGrowTurns','4'), ('HatcheryLayShowTurns','2'),
                    ('HatcheryNestArrive','0.3')]:
    match = re.search(r'^\s*'+name+r'\s+(\S+)', config, re.M)
    assert match and match.group(1) == value, name

# Function slices preserve nesting/interval provenance without binding unrelated rooster functions.
def section(text, name):
    start = text.index(name+'(')
    opening = text.index('{', start)
    depth = 0
    for pos in range(opening, len(text)):
        depth += (text[pos] == '{') - (text[pos] == '}')
        if depth == 0:
            return text[start:pos+1]
    raise AssertionError(name)

expected_sections = {
    'source/rooms/RoomHatchery.cpp#RoomHatchery::'+name
    for name in ['getCycleSettings','releasePendingEggs','nestWalkTurns','updateNestTrips','doUpkeep']
} | {'source/rooms/HatcheryCycle.cpp#HatcheryCycle::layInterval', 'source/rooms/HatcheryNestField.h', 'source/entities/ChickenEntity.h#virtual double getMoveSpeed'}
assert set(record['source_sha256']) == expected_sections
for key, expected in record['source_sha256'].items():
    path, separator, name = key.partition('#')
    data = section((ROOT/path).read_text(), name).encode() if separator else (ROOT/path).read_bytes()
    assert hashlib.sha256(data).hexdigest() == expected, 'Simulation source changed: '+key

rows = record['rows']
expected = set(itertools.product(['3x3-one','6x6-two','6x6-four','12x12-eight'], [2,5,10,20], [99,3,777]))
assert len(rows) == 48 and {case_key(row) for row in rows} == expected
for row in rows:
    assert set(row) == {'layout','demand','seed','old','new','diff'}
    assert allowed(row), row
violations = {case_key(row) for row in rows if abs(row['diff']) > 3.0}
assert violations == set(EXCEPTIONS)
assert len(record['accepted_exceptions']) == 2
for exception in record['accepted_exceptions']:
    key = case_key(exception)
    assert key in EXCEPTIONS and (exception['old'],exception['new']) == EXCEPTIONS[key]
    assert exception['diff'] == 100*(exception['new']-exception['old'])/exception['old']
    assert exception['acceptance'] == ACCEPTANCE
assert {case_key(row) for row in record['accepted_exceptions']} == set(EXCEPTIONS)
print('Recorded nest-wait parity: 46 cases <=3%, exactly two unchanged accepted outcomes; config/source provenance verified. No model or C++ executed.')
