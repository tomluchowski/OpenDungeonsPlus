"""Check that every client gets and shows the same Dungeonbook picture (static check, no game needed).

What is checked here: the server is the only place that rolls an appearance, every path that tells a
client about a creature (add entity, join, load, late assignment) carries the appearance for every seat
alike, and the code that turns appearance and name into pixels and remarks has no input that could differ
between clients (no random numbers, clock, hash of the standard library, seat or local player).

The runtime part (packet round trips of two clients and a late joiner fed from one server state, same
picture key, same pixels, same remarks) is the boost test 00-DungeonbookMultiplayer. A real run with two
game instances is not part of this check; that stays a manual test.
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(name):
    return (repo / name).read_text()


def body(source, signature):
    start = source.index(signature)
    brace = source.index('\n{\n', start)
    end = source.index('\n}\n', brace)
    return source[brace:end]


creature = read('source/entities/Creature.cpp')

# The server alone rolls: random picks only inside the server side assign function
assign = body(creature, 'void Creature::assignAppearance(bool firstSpawn)')
assert re.search(r'^\s+if\(!getIsOnServerMap\(\)', assign, re.M), 'assignAppearance must return early off the server map'
assert 'pickRandom(' in assign and 'getAppearanceRandom' in assign
assert 'Random::Uint' in creature[:creature.index('void Creature::assignAppearance(bool firstSpawn)')]
for other in re.finditer(r'pickRandom\(', creature):
    assert other.start() > creature.index('void Creature::assignAppearance(bool firstSpawn)'), 'pickRandom outside assign'
assert creature.count('pickRandom(') == 1, 'only one place may roll an appearance'

# Every way a creature reaches a client goes through exportToPacket, and the token is not seat dependent
fire = body(creature, 'void Creature::fireAddEntity(Seat* seat, bool async, NodeType nt )')
assert fire.count('exportToPacket(') == 2, 'async and sync add entity both use exportToPacket'
export_full = body(creature, 'void Creature::exportToPacket(ODPacket& os, const Seat* seat) const')
line = [l for l in export_full.splitlines() if 'toToken(mAppearance)' in l]
assert len(line) == 1 and line[0].startswith('    os <<'), 'the token is written at top level, for every seat'
assert 'seat' not in line[0]
assert 'getCreatureFromPacket' in read('source/entities/EntityLoading.cpp')
get_from_packet = body(creature, 'Creature* Creature::getCreatureFromPacket(GameMap* gameMap, ODPacket& is)')
assert 'importFromPacket(is)' in get_from_packet

# Late assignment: sent to the seats with vision of the creature, the others get it with the next full data
retry = body(creature, 'void Creature::retryAppearance()')
assert 'mSeatsWithVisionNotified' in retry and 'ServerNotificationType::creatureAppearance' in retry
client = read('source/network/ODClient.cpp')
handler = client[client.index('case ServerNotificationType::creatureAppearance:'):]
handler = handler[:handler.index('break;')]
assert 'setAppearanceFromServer(' in handler

# Join and load send visible creatures the same way: no separate creature path that skips the appearance
server = read('source/network/ODServer.cpp')
assert 'creatureAppearance' not in server, 'the appearance has no join path of its own, it rides on the creature data'
assert not re.search(r'[Cc]reature[^;\n]*exportToPacket\(', server), 'creature data is only sent via fireAddEntity'

# The pixels and remarks are a function of the appearance and the name only
forbidden = (r'\bRandom::', r'\brand\(', r'\bsrand\(', r'std::hash', r'<random>', r'\btime\(', r'std::chrono', r'getLocalPlayer',
             r'getSeat\(', r'ODFrameListener', r'getTurnNumber', r'std::random_device')
for name in ('source/render/AppearanceCompose.cpp', 'source/render/DungeonbookQuirks.cpp',
             'source/game/CreatureAppearance.cpp', 'source/render/CreatureAppearancePicture.cpp'):
    text = re.sub(r'//.*', '', read(name))
    # Only the appearance picture code may be checked for the random pick: CreatureAppearance takes the
    # random function as a parameter and never calls the global generator itself
    for pattern in forbidden:
        assert not re.search(pattern, text), '%s uses %s' % (name, pattern)

# The remark choice and the stable look use the fixed hash
quirks = read('source/render/DungeonbookQuirks.cpp')
assert 'stableHash(' in quirks

# The profile asks for the picture on every fill and sets it right away
fill = body(creature, 'float Creature::fillProfilePage(CEGUI::Window* page)')
assert 'getCreatureAppearanceImage(getName(), mAppearance)' in fill
assert 'appearanceImage' in fill

# The tests that give the runtime part are part of the build
tests = read('source/tests/CMakeLists.txt')
for test in ('00-DungeonbookMultiplayer', '00-DungeonbookQuirks', '00-CreatureAppearance', '00-AppearanceCompose',
             '00-PortraitManifest'):
    assert 'add_boost_test(%s' % test in tests, test

print('dungeonbook multiplayer wiring ok')
