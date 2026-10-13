"""Static check that the region marker messages of the level editor are wired end to end."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    """Returns the text of a file of the repository."""
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
client_enum = client_h.split('};')[0]
assert client_enum.rstrip().endswith('editorRegionEdit'), 'client message not last'
assert 'case ClientNotificationType::editorRegionEdit:' in client_cpp
server_enum = server_h.split('};')[0]
assert server_enum.rstrip().endswith('editorRegionData'), 'server message not last'
assert 'case ServerNotificationType::editorRegionData:' in server_cpp

# The three operations of the request are named, the editor and the server use the names.
operations = client_h.split('enum class EditorRegionOperation')[1].split('};')[0]
for name in ('list', 'set', 'remove'):
    assert name + (',' if name != 'remove' else '') in operations, 'operation missing: ' + name
assert 'EditorRegionOperation::set' in od_server and 'EditorRegionOperation::remove' in od_server
assert 'EditorRegionOperation::list' in editor and 'EditorRegionOperation::set' in editor
assert 'EditorRegionOperation::remove' in editor

# The server only edits regions in editor mode, reads the request completely and always answers
# with the full list.
handler = od_server[od_server.index('case ClientNotificationType::editorRegionEdit:'):]
handler = handler[:handler.index('case ClientNotificationType::askSetSkillTree:')]
assert 'ServerMode::ModeEditor' in handler
assert 'setRegion(' in handler and 'removeRegion(' in handler
assert 'ServerNotificationType::editorRegionData' in handler
assert 'Truncated editorRegionEdit packet' in handler

# The client reads exactly what the server writes: name and four corners per region.
reply = od_client[od_client.index('case ServerNotificationType::editorRegionData:'):]
reply = reply[:reply.index('default:')]
assert 'region.mName >> region.mX1 >> region.mY1 >> region.mX2 >> region.mY2' in reply
assert 'setRegions(' in reply
assert 'Truncated editorRegionData packet' in reply
assert 'region.mName << region.mX1 << region.mY1 << region.mX2 << region.mY2' in handler

# The editor: key R marks, Shift + R removes, the markers are drawn and listed in the help.
assert 'case OIS::KC_R:' in editor
assert 'markRegion();' in editor and 'unmarkRegion();' in editor
assert 'drawCuboid(box.getAllCorners().data()' in editor
assert 'R - make the marked tiles a region' in editor
assert 'void setRegions(' in editor_h
print('editor region markers: ok')
