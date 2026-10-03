"""Sanity checks for the creature social profile data and generator sources (Dungeonbook, S2).

Checks:
  * every creature class of config/creatures.cfg is served by a name group and every class
    listed in a group exists (so the class to group table cannot silently rot);
  * both data files have the right number of columns per line, known keys, known slots,
    known scopes, enough entries per pool, no duplicates, length limits, plain ASCII;
  * every class has a job text, every post category and mood state has enough templates,
    and the longest possible bio fits into 160 characters;
  * source/social/ includes no network, server, packet, random or save code;
  * the server side files never include the social headers;
  * the golden file has 50 lines.
Run with: python source/tests/check_social_data.py
"""
from pathlib import Path
import itertools
import re
import sys

repo = Path(__file__).resolve().parents[2]
failures = []
checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        failures.append(message)


def read_lines(path):
    text = path.read_bytes().decode('utf-8')
    check(not re.search('[\x00-\x08\x0b\x0c\x0e-\x1f]', text), '%s contains control characters' % path.name)
    check(all(ord(c) < 128 for c in text), '%s is not plain ASCII' % path.name)
    return [(number, line.rstrip('\r')) for number, line in enumerate(text.split('\n'), 1)]


# ---- creature classes ----
creatures = (repo / 'config/creatures.cfg').read_text(encoding='utf-8')
classes = re.findall(r'\[Creature\]\s*Name\s+(\S+)', creatures)
jobs = re.findall(r'\[Creature\]\s*Name\s+\S+\s*\[Stats\]\s*CreatureJob\s+(\S+)', creatures)
check(len(classes) >= 30, 'too few creature classes found in creatures.cfg')
workers = set(name for name, job in zip(classes, jobs) if job == 'Worker')
check(set(['Kobold', 'DwarfWorker']) <= workers, 'the worker classes Kobold and DwarfWorker were not found')

# ---- names file ----
MIN_GIVEN = 12
groups = {}
current = None
for number, line in read_lines(repo / 'config/social-names.cfg'):
    where = 'social-names.cfg:%d' % number
    stripped = line.strip()
    if not stripped or stripped.startswith('#'):
        continue
    if stripped == '[NameGroup]':
        check(current is None, where + ': nested [NameGroup]')
        current = {'given': {'F': [], 'M': [], 'X': []}, 'Classes': [], 'Surname': [], 'Title': [],
                   'Hometown': [], 'AgeJoke': [], 'line': number}
        continue
    if stripped == '[/NameGroup]':
        check(current is not None and 'Group' in current, where + ': group without a name')
        if current is not None and 'Group' in current:
            check(current['Group'] not in groups, where + ': duplicate group')
            groups[current['Group']] = current
        current = None
        continue
    check(current is not None, where + ': line outside of a group')
    if current is None:
        continue
    fields = [field.strip() for field in line.strip().split('\t')]
    key = fields[0]
    check(all(fields), where + ': empty column')
    if key == 'Group':
        check(len(fields) == 2, where + ': Group needs one name')
        current['Group'] = fields[1]
    elif key == 'Classes':
        current['Classes'] += fields[1:]
    elif key == 'AgeRange':
        check(len(fields) == 3 and fields[1].isdigit() and fields[2].isdigit() and int(fields[1]) <= int(fields[2]),
              where + ': bad AgeRange')
        current['AgeRange'] = fields[1:]
    elif key == 'GenderWeights':
        check(len(fields) == 4 and all(f.isdigit() for f in fields[1:]) and sum(int(f) for f in fields[1:]) > 0,
              where + ': bad GenderWeights')
    elif key == 'Given':
        check(len(fields) >= 3 and fields[1] in ('F', 'M', 'X'), where + ': bad Given')
        if len(fields) >= 3 and fields[1] in ('F', 'M', 'X'):
            current['given'][fields[1]] += fields[2:]
    elif key in ('Surname', 'Title', 'Hometown', 'AgeJoke'):
        current[key] += fields[1:]
    else:
        check(False, where + ': unknown key ' + key)
check(current is None, 'social-names.cfg: missing [/NameGroup]')
check(len(groups) == 11, 'expected 11 name groups, found %d' % len(groups))
check('monster' in groups, 'the fallback group monster is missing')

class_to_group = {}
for name, group in groups.items():
    where = 'group ' + name
    check('AgeRange' in group, where + ': AgeRange missing')
    for gender, names in group['given'].items():
        check(len(names) >= MIN_GIVEN, '%s: only %d given names for %s' % (where, len(names), gender))
        check(len(set(names)) == len(names), '%s: duplicate given names for %s' % (where, gender))
    for key, minimum in (('Surname', 10), ('Title', 10), ('Hometown', 8)):
        check(len(group[key]) >= minimum, '%s: only %d %s entries' % (where, len(group[key]), key))
        check(len(set(group[key])) == len(group[key]), '%s: duplicate %s entries' % (where, key))
        check(all(len(entry) <= 28 for entry in group[key]), '%s: %s entry longer than 28' % (where, key))
    check(all(len(entry) <= 18 for names in group['given'].values() for entry in names),
          where + ': given name longer than 18')
    check(all(entry.startswith(('the ', 'of ')) for entry in group['Title']),
          where + ': a title must start with "the " or "of "')
    for class_name in group['Classes']:
        check(class_name not in class_to_group, 'class %s is in two groups' % class_name)
        class_to_group[class_name] = name
for class_name in classes:
    check(class_name in class_to_group, 'creature class %s has no name group' % class_name)
for class_name in class_to_group:
    check(class_name in classes, 'group lists the unknown class %s' % class_name)

# ---- texts file ----
SLOTS = set(['name', 'hometown', 'job', 'like', 'dislike', 'quirk', 'level', 'room', 'friend'])
SIMPLE_KEYS = ('Job', 'Like', 'Dislike', 'Quirk', 'Bio', 'Relation', 'ClassName')
MOOD_STATES = ['Hungry', 'Tired', 'GetFee', 'LeaveDungeon', 'KoTemp', 'InJail', 'Happy', 'Neutral', 'Upset',
               'Angry', 'Furious', 'Unknown']
POST_CATEGORIES = ['eat', 'sleep', 'train', 'work', 'fight', 'hurt', 'levelup', 'payday', 'unhappy', 'ko', 'jail',
                   'pickedup', 'slapped', 'arrived', 'left', 'died', 'idle', 'friendship', 'hatred', 'nemesis', 'breakup', 'converted', 'couple', 'splitup']
MAX_LENGTH = {'Job': 40, 'Like': 40, 'Dislike': 40, 'Quirk': 40, 'Bio': 100, 'Relation': 40, 'ClassName': 24, 'MoodLine': 80,
              'Post': 110}
valid_scopes = set(['*', 'worker', 'fighter']) | set(groups) | set(classes)
texts = {}
for number, line in read_lines(repo / 'config/social-texts.cfg'):
    where = 'social-texts.cfg:%d' % number
    stripped = line.strip()
    if not stripped or stripped.startswith('#'):
        continue
    fields = [field.strip() for field in stripped.split('\t')]
    key = fields[0]
    if key in SIMPLE_KEYS:
        check(len(fields) == 3, where + ': %s needs 3 columns' % key)
        if len(fields) != 3:
            continue
        scope, text = fields[1], fields[2]
        store = key
    elif key in ('MoodLine', 'Post'):
        check(len(fields) == 4, where + ': %s needs 4 columns' % key)
        if len(fields) != 4:
            continue
        check(fields[1] in (MOOD_STATES if key == 'MoodLine' else POST_CATEGORIES),
              where + ': unknown state or category ' + fields[1])
        scope, text = fields[2], fields[3]
        store = key + ':' + fields[1]
    else:
        check(False, where + ': unknown key ' + key)
        continue
    check(all(fields), where + ': empty column')
    check(scope in valid_scopes, where + ': unknown scope ' + scope)
    for slot in re.findall(r'\{([^}]*)\}', text):
        check(slot in SLOTS, where + ': unknown slot {%s}' % slot)
    check(text.count('{') == text.count('}'), where + ': unbalanced braces')
    check(len(text) <= MAX_LENGTH[key], where + ': text longer than %d' % MAX_LENGTH[key])
    check('|' not in text, where + ': contains |')
    texts.setdefault(store, []).append((scope, text))

for store, entries in texts.items():
    check(len(set(entries)) == len(entries), 'duplicate entries for ' + store)
for class_name in classes:
    check(any(scope == class_name for scope, _ in texts.get('Job', [])), 'no Job text for class ' + class_name)
    check(sum(1 for scope, _ in texts.get('ClassName', []) if scope == class_name) == 1,
          'class %s needs exactly one ClassName' % class_name)
for scope, text in texts.get('Bio', []):
    check(not re.search(r'\{(hometown|job|like|dislike|name)\}', text),
          'bio repeats a field of the card: ' + text)
for key in ('Like', 'Dislike', 'Quirk', 'Bio', 'Relation'):
    check(sum(1 for scope, _ in texts.get(key, []) if scope == '*') >= 10 or key == 'Relation',
          'too few generic %s texts' % key)
for state in MOOD_STATES:
    check(sum(1 for scope, _ in texts.get('MoodLine:' + state, []) if scope == '*') >= 3,
          'fewer than 3 generic mood lines for ' + state)
for category in POST_CATEGORIES:
    check(sum(1 for scope, _ in texts.get('Post:' + category, []) if scope == '*') >= 4,
          'fewer than 4 generic posts for ' + category)
for category in ('eat', 'sleep', 'work'):
    for name in groups:
        check(sum(1 for scope, _ in texts.get('Post:' + category, []) if scope == name) >= 2,
              'fewer than 2 %s posts for group %s' % (category, name))

# Longest possible bio per class (slots replaced by the longest value that can occur)
for class_name in classes:
    group = groups[class_to_group[class_name]]
    kind = 'worker' if class_name in workers else 'fighter'
    scopes = set(['*', group['Group'], class_name, kind])

    def longest(key, extra=()):
        values = [text for scope, text in texts.get(key, []) if scope in scopes] + list(extra)
        return max(len(value) for value in values)

    surnames = max(len(first + ' ' + second) for first, second in
                   itertools.product(list(itertools.chain(*group['given'].values())), group['Surname'] + group['Title']))
    lengths = {'name': surnames, 'hometown': max(len(h) for h in group['Hometown']), 'job': longest('Job'),
               'like': longest('Like'), 'dislike': longest('Dislike'), 'quirk': longest('Quirk')}
    longest_bio = 0
    for scope, text in texts.get('Bio', []):
        if scope in scopes:
            expanded = len(re.sub(r'\{(\w+)\}', '', text)) + sum(lengths.get(s, 0) for s in re.findall(r'\{(\w+)\}', text))
            longest_bio = max(longest_bio, expanded)
    check(longest_bio <= 160, 'class %s: the longest bio can reach %d characters' % (class_name, longest_bio))
    check(sum(1 for scope, _ in texts.get('Bio', []) if scope in scopes) >= 10,
          'class %s has fewer than 10 bio templates' % class_name)

# ---- generator sources ----
FORBIDDEN_INCLUDES = ('network/', 'ODServer', 'ODClient', 'ODPacket', 'Random.h', '<random>', 'ServerNotification',
                      'ClientNotification', 'GameMap', 'Creature.h', 'Ogre', 'CEGUI')
social_dir = repo / 'source/social'
sources = sorted(social_dir.glob('*.h')) + sorted(social_dir.glob('*.cpp'))
check(len(sources) >= 6, 'expected the social sources in source/social')
for source in sources:
    text = source.read_text(encoding='utf-8')
    check(not re.search('[\x00-\x08\x0b\x0c\x0e-\x1f]', text), source.name + ' contains control characters')
    for number, line in enumerate(text.split('\n'), 1):
        if line.lstrip().startswith('#include'):
            for forbidden in FORBIDDEN_INCLUDES:
                check(forbidden not in line, '%s:%d includes %s' % (source.name, number, forbidden))
    check(not re.search(r'\bauto\b', text), source.name + ' uses auto')
    check('Random::' not in text and 'std::rand' not in text and 'srand' not in text and '<chrono>' not in text
          and 'time(' not in text, source.name + ' uses time or random sources')

# ---- server path ----
for pattern in ('network/ODServer*', 'network/ServerNotification*', 'network/ServerMode*', 'network/ODSocketServer*',
                'game/*'):
    for path in (repo / 'source').glob(pattern):
        if path.is_file():
            check('social/' not in path.read_text(encoding='utf-8', errors='replace'),
                  '%s includes the social generator' % path.name)

# ---- golden file ----
golden = [line for line in (repo / 'source/tests/social-golden.txt').read_text(encoding='utf-8').split('\n')
          if line.strip() and not line.startswith('#')]
check(len(golden) == 50, 'the golden file must have 50 lines, found %d' % len(golden))
for line in golden:
    check(len(line.rstrip('\r').split('\t')) == 3, 'golden line without 3 columns: ' + line)

print('%d checks, %d failures' % (checks, len(failures)))
for failure in failures:
    print('FAIL: ' + failure)
sys.exit(1 if failures else 0)
