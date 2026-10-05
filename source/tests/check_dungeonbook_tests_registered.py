"""Check that every Dungeonbook related unit test is registered in source/tests/CMakeLists.txt (static check).

A test file that is not registered never runs in any build. For every source/tests/test_*.cpp that deals with
the Dungeonbook (name contains Dungeonbook, Appearance or Portrait) this requires an add_boost_test entry that
lists the file, and that the entry also lists the .cpp of every game header the test includes from render/ or
game/ (the pure sources a test links directly).
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
cmake = (repo / 'source/tests/CMakeLists.txt').read_text()
blocks = {}
for match in re.finditer(r'add_boost_test\(([^\s)]+)([^)]*)\)', cmake):
    blocks[match[1]] = match[2]

tests = sorted(p for p in (repo / 'source/tests').glob('test_*.cpp') if re.search('Dungeonbook|Appearance|Portrait', p.name))
assert tests, 'no Dungeonbook tests found'
for test in tests:
    entries = [name for name, body in blocks.items() if test.name in body]
    assert entries, '%s is not registered with add_boost_test in tests/CMakeLists.txt' % test.name
    body = blocks[entries[0]]
    text = test.read_text(errors='replace')
    for header in re.findall(r'#include "((?:render|game)/[A-Za-z]+)\.h"', text):
        source = repo / 'source' / (header + '.cpp')
        if source.exists():
            assert header + '.cpp' in body, '%s includes %s.h but its test entry %s does not list %s.cpp' % (test.name, header, entries[0], header)
print('dungeonbook tests registered: %s' % ', '.join(sorted(b for b in blocks if any(t.name in blocks[b] for t in tests))))
