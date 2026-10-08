"""Check the creature profile card (gui/WindowCreatureProfile.layout) opened by a middle click.

The window is fixed size, created from the layout (registered for UI scaling), keeps the old stats
text on its own page in the small body font and still tells the server to start and stop sending
the statistics.
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
layout = (repo / 'gui/WindowCreatureProfile.layout').read_text()
assert 'name="CreatureProfileWindow"' in layout and 'type="OD/FrameWindow"' in layout
assert '<Property name="SizingEnabled" value="False" />' in layout, 'the window must not be resizable'
m = re.search(r'name="Area" value="\{\{0\.5,(-?\d+)\},\{0\.5,(-?\d+)\},\{0\.5,(\d+)\},\{0\.5,(\d+)\}\}"', layout)
assert m, 'a fixed size centred area is expected'
width, height = int(m[3]) - int(m[1]), int(m[4]) - int(m[2])
assert width >= 480 and height >= 440, (width, height)
for name in ('ProfilePage', 'Portrait', 'StatsText', 'ProfileTab', 'StatsTab', 'BookTab', 'HealthBar',
             'ExperienceBar', 'BioText', 'FriendsText', 'StatusText'):
    assert 'name="%s"' % name in layout, name
stats = layout[layout.index('name="StatsText"'):]
stats = stats[:stats.index('</Window>')]
for prop in ('Font" value="MedievalSharp-8', 'VertFormatting" value="TopAligned',
             'HorzFormatting" value="WordWrapLeftAligned'):
    assert prop in stats, prop
assert 'VertScrollbar' not in layout, 'no scrolling wanted'

gui = (repo / 'source/render/Gui.cpp').read_text()
body = gui[gui.index('Gui::createCreatureProfileWindow'):]
body = body[:body.index('\n}\n')]
assert 'WindowCreatureProfile.layout' in body and 'registerWindowHierarchy(window)' in body

creature = (repo / 'source/entities/Creature.cpp').read_text()
assert 'createCreatureProfileWindow(' in creature
assert 'CEGUI::UDim(0, 380)' not in creature, 'the card sets an own size'
assert creature.count('ClientNotificationType::askCreatureInfos') >= 2, 'server info start/stop is missing'
for handler in ('ProfileTabClicked', 'StatsTabClicked'):
    assert 'Creature::' + handler in creature, handler
assert 'EventCloseClicked' in creature
assert 'social/SocialProfileCache.h' in creature

# ---- rows: visible labels for the bars, no overlap, nothing below the tab buttons ----
for name in ('HealthLabel', 'ExperienceLabel', 'RelationText', 'FoeText'):
    assert 'name="%s"' % name in layout, name
rows = []
for name in ('NameText', 'HandleText', 'AgeText', 'RelationText', 'FromText', 'JobText', 'BioText', 'LikesText',
             'DislikesText', 'FriendsText', 'FoeText', 'StatusText'):
    block = layout[layout.index('name="%s"' % name):]
    area = re.search(r'name="Area" value="\{\{[^,]*,(-?\d+)\},\{0\.0,(\d+)\},\{[^,]*,(-?\d+)\},\{0\.0,(\d+)\}\}"', block)
    rows.append((name, int(area[2]), int(area[4])))
for (name, top, bottom), (nextName, nextTop, nextBottom) in zip(rows, rows[1:]):
    if name in ('NameText', 'HandleText', 'AgeText', 'RelationText', 'FromText', 'JobText') and             nextName in ('HandleText', 'AgeText', 'RelationText', 'FromText', 'JobText', 'BioText'):
        assert bottom <= nextTop, (name, nextName)
    elif nextName not in ('NameText',):
        assert bottom <= nextTop, (name, nextName)
lastBottom = max(bottom for _, _, bottom in rows)
assert lastBottom <= height - 56, 'a row reaches into the tab buttons'
assert 'setDisabled(' not in creature[creature.index('void Creature::showStatsPage'):creature.index('void Creature::refreshProfilePage')],     'the active tab must not be shown disabled'
assert 'Unspecified' not in (repo / 'source/social/SocialGenerator.cpp').read_text()
assert '"@" + getName()' not in creature, 'the handle must not be the internal name'
assert 'setText(getName() + " (" + getDefinition()->getClassName() + ")")' not in creature
game = (repo / 'source/modes/GameMode.cpp').read_text()
pick = game[game.index('void GameMode::openStatsWindowUnderPointer'):game.index('bool GameMode::mousePressed')]
assert 'findWorldPositionFromMouse' not in pick, 'the middle click must pick along the pointer ray'
assert 'getCameraToViewportRay' in pick
print('ok')
