"""Static check that the region marker messages of the level editor are wired end to end."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


client_h = read('source/network/ClientNotification.h')
client_cpp = read('source/network/ClientNotification.cpp')
server_h = read('source/network/ServerNotification.h')
server_cpp = read('source/network/ServerNotification.cpp')
od_server = read('source/network/ODServer.cpp')
od_client = read('source/network/ODClient.cpp')
editor_h = read('source/modes/EditorMode.h')
editor = read('source/modes/EditorMode.cpp')

# New message ids are appended at the end of their enum so older ids keep their value.
client_ids = re.findall(r'^\s*([A-Za-z_]\w*)\s*,?\s*(?://.*)?$', client_h.split('};')[0], re.M)
# Later messages may be appended after it, but it must follow the previous last message directly
assert client_ids.index('editorRegionEdit') == client_ids.index('askPossessSkill') + 1, 'client message moved'
assert client_ids[-2:] == ['editorRegionEdit', 'askUseSpecial'], client_ids[-3:]
assert 'case ClientNotificationType::editorRegionEdit:' in client_cpp
assert [n for n in re.findall(r'^\s*([A-Za-z_]\w*)\s*,?\s*$', server_h.split('enum class ServerNotificationType')[1].split('};')[0], re.M) if n != 'creatureAppearance'][-2:] == ['timeLimit', 'chickenKindChanged'] and 'editorRegionData,' in server_h, 'server message appended out of order'
assert 'case ServerNotificationType::editorRegionData:' in server_cpp

# The server only edits regions in editor mode and always answers with the full list.
handler = od_server[od_server.index('case ClientNotificationType::editorRegionEdit:'):]
handler = handler[:handler.index('case ClientNotificationType::askSetSkillTree:')]
assert 'ServerMode::ModeEditor' in handler
assert 'setRegion(' in handler and 'removeRegion(' in handler
assert 'ServerNotificationType::editorRegionData' in handler

# The client reads exactly what the server writes: name and four corners per region.
reply = od_client[od_client.index('case ServerNotificationType::editorRegionData:'):]
reply = reply[:reply.index('case ServerNotificationType::possessionEnd:')]
assert 'region.mName >> region.mX1 >> region.mY1 >> region.mX2 >> region.mY2' in reply
assert 'setRegions(' in reply
assert 'region.mName << region.mX1 << region.mY1 << region.mX2 << region.mY2' in handler

# The editor: key R marks, Shift + R removes, the markers are drawn and listed in the help.
assert 'case OIS::KC_R:' in editor
assert 'markRegion();' in editor and 'unmarkRegion();' in editor
assert 'drawCuboid(box.getAllCorners().data()' in editor
assert 'R - make the marked tiles a region' in editor
assert 'void setRegions(' in editor_h
print('editor region markers: ok')
