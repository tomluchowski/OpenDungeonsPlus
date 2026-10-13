"""Static wiring checks for the game mechanics that the converted campaign levels need.

One section per mechanic, in the order they were added.
"""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


# Traps of the neutral and the hero seats fire without mana
seat_h = read('source/game/Seat.h')
assert 'isTrapManaFree' in seat_h and 'getFaction() == "Hero"' in seat_h
trap = read('source/traps/Trap.cpp')
assert 'getSeat()->isTrapManaFree() ? 0.0 : getManaToFire()' in trap
door = read('source/traps/TrapDoor.cpp')
assert 'getSeat()->isTrapManaFree() ? 0.0' in door

# Flag comparisons, timers
script_h = read('source/gamemap/LevelScript.h')
script = read('source/gamemap/LevelScript.cpp')
runner = read('source/gamemap/LevelScriptRunner.cpp')
assert 'enum class LevelScriptCompare' in script_h
assert 'startTimer' in script_h and 'advanceTimers' in script_h
assert 'case LevelScriptConditionType::timer:' in runner
assert 'script.advanceTimers();' in runner
assert 'case LevelScriptActionType::startTimer:' in runner
assert 'if(cond.mAtLeast)' not in runner, 'the runner must use levelScriptCompare for every number'
assert 'key == "Timer"' in script and 'os << "Timer' in script

# Seat, creature, spell, trap and room conditions
for name in ('seatDefeated', 'ownsCreature', 'creatureHealth', 'spellKnown', 'trapsBuilt', 'roomTiles', 'largestRoom'):
    assert 'case LevelScriptConditionType::%s:' % name in runner, name
assert 'player->getHasLost()' in runner
assert 'isClassMatching(creature, cond.mName2)' in runner

# Creature events and parties
creature = read('source/entities/Creature.cpp')
assert 'case LevelScriptConditionType::creatureEvent:' in runner
for event in ('attacked', 'incapacitated', 'killed', 'pickedup', 'slapped'):
    assert 'recordScriptEvent(*this, "%s")' % event in creature, event
assert 'addPartyMember(action.mParty' in runner
game_map_h = read('source/gamemap/GameMap.h')
assert 'LevelScript.h' not in game_map_h, 'GameMap.h must not include the script header'

# Tiles and tags, possession and boulders, creature names, alliances, terrain, portals, availability
for name in ('tileKinds', 'tilesTagged', 'possessedInRegion', 'boulderInRegion'):
    assert 'case LevelScriptConditionType::%s:' % name in runner, name
for name in ('alterTerrain', 'portalStatus', 'creatureAvailable', 'removeCreature', 'alliance', 'generateCreature',
             'possessCreature'):
    assert 'case LevelScriptActionType::%s:' % name in runner, name
assert 'TileType::manaWell' in runner and 'kind == "manawell"' in runner
assert 'case LevelScriptActionType::countdown:' in runner and 'setScriptCountdown(' in runner
assert 'TIME_LIMIT_COUNTDOWN_FLAG' in read('source/modes/GameMode.cpp') and 'TIME_LIMIT_COUNTDOWN_FLAG' in read('source/gamemap/GameMap.cpp')
assert 'type == "countdown"' in script and 'os << "countdown' in script and 'key == "Countdown"' in script
assert 'case LevelScriptActionType::waveCountdown:' in runner and 'setScriptWaveCountdown(' in runner
assert 'type == "wavecountdown"' in script and 'os << "wavecountdown' in script and 'key == "WaveCountdown"' in script
assert 'mWaveCountdownSeconds' in read('source/network/ODClient.cpp') and 'mWaveCountdownToSend' in read('source/gamemap/GameMap.cpp')
assert 'WaveCountdownDisplay' in read('source/modes/GameMode.cpp') and 'WaveCountdownDisplay' in read('gui/ModeGame.layout')
assert 'startPossession(' in runner and 'newCreature->setName(' in runner
assert 'isPortalOff(getSeat()->getId())' in read('source/rooms/RoomPortal.cpp')
assert 'isCreatureBlocked(getId()' in read('source/game/Seat.cpp')
server_h = read('source/network/ServerNotification.h')
assert server_h.index('seatTeam,') < server_h.index('timeLimit')
assert 'case ServerNotificationType::seatTeam:' in read('source/network/ODClient.cpp')
assert 'case ServerNotificationType::seatTeam:' in read('source/network/ServerNotification.cpp')

# A scripted possession is free
assert 'isFreePossession()' in read('source/creatureaction/CreatureActionPossessed.cpp')
assert 'script.setFreePossession(true)' in runner

# Standing orders, run speed, room owner, slap limit and the extra conditions
for name in ('creatureOrder', 'creatureSpeed', 'roomOwner', 'slapLimit'):
    assert 'case LevelScriptActionType::%s:' % name in runner, name
for name in ('playerSlaps', 'roomFurniture', 'dungeonBreached'):
    assert 'case LevelScriptConditionType::%s:' % name in runner, name
assert 'doCreatureOrder(' in read('source/entities/Creature.cpp')
assert 'registerSlap(' in read('source/network/ODServer.cpp')
assert 'RUN_SPEED_FACTOR = 1.5;' in runner

# Portal stone: a stone that lies on a tile and is carried by a creature of the level
stone_h = read('source/entities/MissileStone.h')
assert 'class MissileStone' in stone_h and 'MissileObjectType::stone' in stone_h
assert 'MissileStone.cpp' in read('CMakeLists.txt')
for name in ('stoneCreate', 'stoneAttach'):
    assert 'case LevelScriptActionType::%s:' % name in runner, name
assert 'type == "stonecreate"' in script and 'type == "stoneattach"' in script
assert 'key == "Stone"' in script and 'os << "Stone' in script

# Rolling ball and bridges
assert 'case LevelScriptActionType::golfBall:' in runner
assert 'slapFrom(' in read('source/entities/MissileBoulder.h')
assert 'type == "golfball"' in script and 'isBoulderHole(' in script

# Kept minion: the strongest fighter comes along to the next level of the campaign (script action keepminion)
campaign = read('source/game/Campaign.cpp')
assert 'case LevelScriptActionType::keepMinion:' in runner and 'setKeptMinion(' in runner
assert 'type == "keepminion"' in script and 'os << "keepminion' in script
assert 'takeKeptMinion(' in runner and 'KeptMinion' in campaign

# The campaign pieces are called Heartstone pieces
assert 'isHeartstoneComplete()' in read('source/network/ODServer.cpp')
assert 'getHeartstonePieces()' in read('source/modes/MenuModeCampaign.cpp')

print('check_campaign_mechanics: all checks passed')
