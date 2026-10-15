#!/usr/bin/env python3
# Checks the hatchery animal sounds: the files exist, have a CREDITS entry, and the server plays every family.
from pathlib import Path

root = Path(__file__).resolve().parents[2]
credits = (root / 'CREDITS').read_text()
room = (root / 'source/rooms/RoomHatchery.cpp').read_text() if (root / 'source/rooms/RoomHatchery.cpp').exists() else ''
chicken = (root / 'source/entities/ChickenEntity.cpp').read_text()

families = ('Crow', 'Protest', 'Cluck', 'Peep', 'EggCrack')
for family in families:
    folder = root / 'sounds/Spatial/Rooms/Hatchery' / family
    files = sorted(folder.glob('*.ogg'))
    assert len(files) >= 2, family
    for ogg in files:
        assert ogg.stat().st_size > 1000, ogg
        assert ogg.stat().st_size < 60000, ogg
        assert 'sounds/Spatial/Rooms/Hatchery/%s/%s' % (family, ogg.name) in credits, ogg.name

# The server plays them through the room sound message (family "Rooms/Hatchery/<Family>")
for family in families:
    assert '"Hatchery/%s"' % family in room, family
# the rooster in the keeper's hand protests with a family of his own, not with the hen cackle
assert 'fireRoomSound(tile, "Hatchery/Protest")' in room
assert 'RoomHatchery::fireProtest(*pickupTile)' in chicken, 'the pickup of the rooster starts the protest'
# the crow on the roof keeps its sound: it is fired when the rooster starts the crow mood, and only then
begin = room[room.index('void RoomHatchery::beginRoosterMood('):]
begin = begin[:begin.index('\n}\n')]
assert 'if(plan.mMood == RoosterMood::crow)' in begin and 'fireAnimalSound(*rooster, "Hatchery/Crow")' in begin
assert room.count('fireAnimalSound(*rooster, "Hatchery/Crow")') == 1, 'the roof crow is the only mood that crows'
assert 'fireAnimalSound(*winner, "Hatchery/Crow")' in room, 'the winner of a fight crows'
# the hen caught by the rooster cackles
assert 'fireAnimalSound(*target, "Hatchery/Cluck")' in room
# the chick peeps now and then, with the chance from the config
assert 'mChickPeepChance' in room and 'HatcheryChickPeepChance' in (root / 'config/rooms.cfg').read_text()
# the rooster does not call the hens: there is no food call sound (files, credits, code)
assert not (root / 'sounds/Spatial/Rooms/Hatchery/FoodCall').exists(), 'FoodCall files are removed'
assert 'FoodCall' not in credits, 'no credits entry for the food call'
assert 'FoodCall' not in room and 'Hatchery/FoodCall' not in room, 'the rooster does not call the hens'
for source_file in (root / 'source').rglob('*'):
    if source_file.suffix in ('.cpp', '.h') and 'FoodCall' in source_file.read_text(errors='ignore'):
        raise AssertionError('FoodCall still used in ' + str(source_file))
assert 'FoodCall' not in (root / 'config/rooms.cfg').read_text()
print('hatchery sounds ok')
