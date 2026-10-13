"""Text check of config/treasury.cfg against the settings struct: every key of the file is documented in its head
comment, every setting of the struct is in the file with the same default (a missing file changes nothing), every
key is read by fromConfig and every setting is used somewhere in the sources. No compiler needed."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8').replace('\r\n', '\n')


cfg = read('config/treasury.cfg')
header = read('source/rooms/TreasurySettings.h')

# Keys and values of the [Treasury] block (a '#' starts a comment)
body = []
for line in cfg.split('\n'):
    line = line.split('#')[0].strip()
    if line:
        body.append(line)
assert body[0] == '[Treasury]' and body[-1] == '[/Treasury]'
values = {}
for line in body[1:-1]:
    parts = line.split()
    assert len(parts) == 2, line
    assert parts[0] not in values, 'duplicate key ' + parts[0]
    values[parts[0]] = parts[1]

# Every key is documented in the head comment ("#   Key (default): text")
documented = set(re.findall(r'^#   (\w+) \(', cfg, re.M))
assert set(values) == documented, (set(values) ^ documented)

# Settings of the struct with their defaults
struct = header.split('struct TreasurySettings')[1].split('typedef std::map')[0]
members = {}
for kind, name, default in re.findall(r'^    (float|int) (\w+) = ([-0-9.]+)f?;', struct, re.M):
    members[name[0].upper() + name[1:]] = (kind, float(default))
assert len(members) >= 7
assert set(members) == set(values), (set(members) ^ set(values))
for key, (kind, default) in members.items():
    assert abs(float(values[key]) - default) < 1e-6, 'default of %s differs from the file' % key

# Every key is read by fromConfig, with a limit
reads = dict((key, kind) for kind, key in re.findall(r'read(Float|Int)\(config, "(\w+)"', header))
assert set(reads) == set(values), (set(reads) ^ set(values))
for key, (kind, default) in members.items():
    assert reads[key] == kind.capitalize(), key

# Loaded at start-up, and every setting is used outside of the struct
manager = read('source/utils/ConfigManager.cpp')
assert 'loadTreasury(configPath + "treasury.cfg")' in manager and 'TreasurySettings::fromConfig(values)' in manager
sources = ''
for path in (repo / 'source').rglob('*'):
    if path.suffix in ('.cpp', '.h') and path.name != 'TreasurySettings.h':
        sources += path.read_text(encoding='utf-8', errors='replace')
for key in members:
    name = key[0].lower() + key[1:]
    assert re.search(r'current\(\)\.' + name + r'\b|\bs(ettings)?\.' + name + r'\b', sources), 'unused setting ' + name

# Out of range values are limited, a missing key keeps the default
assert 'if(parsed < low)' in header and 'else if(parsed > high)' in header
assert 'if(it == config.end())' in header
print('ok')
