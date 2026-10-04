"""Check the wiring of the Dungeonbook appearance in the network code without a game.

The server is authoritative: the appearance travels once with the full creature data
(exportToPacket / importFromPacket), never with the per-turn update, and a later assignment is
announced with one notification. The client only takes the appearance over.
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
creature_header = read('source/entities/Creature.h')
client = read('source/network/ODClient.cpp')
notification_header = read('source/network/ServerNotification.h')
notification_source = read('source/network/ServerNotification.cpp')

export_full = body(creature, 'void Creature::exportToPacket(ODPacket& os, const Seat* seat) const')
import_full = body(creature, 'void Creature::importFromPacket(ODPacket& is)')
export_update = body(creature, 'void Creature::exportToPacketForUpdate(ODPacket& os, Seat* seat)')

# Full creature data carries the appearance at the end, symmetric on both sides
assert 'CreatureAppearanceLogic::toToken(mAppearance)' in export_full
assert export_full.index('exportProgressToPacket(os, seat);') < export_full.index('toToken(mAppearance)')
assert 'CreatureAppearanceLogic::fromToken(' in import_full
assert import_full.index('importProgressFromPacket(is);') < import_full.index('fromToken(')
assert import_full.index('fromToken(') < import_full.index('setupDefinition(')

# Never per turn
assert 'ppearance' not in export_update

# The client never rolls: assignAppearance returns early off the server map, the setter is ignored on the server
assert re.search(r'void Creature::assignAppearance\(bool firstSpawn\)\n\{\n    if\(!getIsOnServerMap\(\)', creature)
setter = body(creature, 'void Creature::setAppearanceFromServer(')
assert 'if(getIsOnServerMap())' in setter and 'return;' in setter
assert 'assignAppearance' not in import_full

# A missing appearance is retried on the server in doUpkeep and announced once
upkeep = body(creature, 'void Creature::doUpkeep()')
assert 'mAppearance.isEmpty() && getIsOnServerMap()' in upkeep and 'retryAppearance();' in upkeep
retry = body(creature, 'void Creature::retryAppearance()')
assert 'assignAppearance(false);' in retry
assert 'ServerNotificationType::creatureAppearance' in retry

# Notification: appended last, named, handled by the client
last = re.findall(r'^\s+([A-Za-z]+),?\s*$', notification_header.split('enum class ServerNotificationType')[1].split('};')[0], re.M)
assert last[-1] == 'creatureAppearance', last[-3:]
assert 'case ServerNotificationType::creatureAppearance:' in notification_source
assert 'case ServerNotificationType::creatureAppearance:' in client
assert 'setAppearanceFromServer(' in client
assert 'setAppearanceFromServer' in creature_header

print('dungeonbook appearance network wiring ok')
