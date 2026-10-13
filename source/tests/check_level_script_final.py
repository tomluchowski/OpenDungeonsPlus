"""Static wiring check for the make action and the time limit of level scripting: the make action,
the time limit and the two HUD countdowns.

The parser and the writer are run by check_level_script.py; this script checks that the pieces
that need the running game are connected, so a missing hook is found without playing.
"""
import re
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(relative):
    """Returns the text of a file of the repository."""
    return (repo / relative).read_text(encoding='utf-8')


def function(source, signature):
    """Returns the text of the function (or case block) that starts with the signature."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError('unterminated ' + signature)


runner = read('source/gamemap/LevelScriptRunner.cpp')
script_cpp = read('source/gamemap/LevelScript.cpp')
gamemap = read('source/gamemap/GameMap.cpp')
map_handler = read('source/gamemap/MapHandler.cpp')
server_h = read('source/network/ServerNotification.h')
server_cpp = read('source/network/ServerNotification.cpp')
client = read('source/network/ODClient.cpp')
client_h = read('source/network/ODClient.h')
game_mode = read('source/modes/GameMode.cpp')
game_layout = read('gui/ModeGame.layout')

# make: the action unlocks the skill through the seat, as research does
make = function(runner, 'void makeSkillAvailable(')
assert 'Skills::fromString(action.mText)' in make
assert 'seat->addSkill(skillType)' in make
assert 'seat->isSkillDone(skillType)' in make
assert 'case LevelScriptActionType::make:' in runner
assert 'type == "make"' in script_cpp and 'case LevelScriptActionType::make:' in script_cpp

# time limit: action, script state, server check and countdown
assert 'case LevelScriptActionType::timeLimit:' in runner and 'gameMap.setScriptTimeLimit(' in runner
assert 'type == "timelimit"' in script_cpp and 'key == "TimeLimit"' in script_cpp
assert '"TimeLimit\\t"' in script_cpp
check = function(gamemap, 'void GameMap::checkGameDuration()')
assert 'mLevelScript->getTimeLimitSeconds()' in check
assert 'sendTimeLimit(' in check
assert check.index('sendTimeLimit(') < check.index('notifyTimeUp')
assert 'Time is up!' in check
assert 'checkGameDuration();' in function(gamemap, 'void GameMap::doTurn(')
set_limit = function(gamemap, 'void GameMap::setScriptTimeLimit(')
assert 'mGameDurationAnnounced = false' in set_limit
assert 'TIME_LIMIT_REMOVED' in set_limit
send = function(gamemap, 'void GameMap::sendTimeLimit(')
assert 'ServerNotificationType::timeLimit' in send and 'getIsHuman()' in send
assert 'rebaseTimeLimit(' in map_handler
enum_body = server_h.split('enum class ServerNotificationType')[1].split('};')[0]
assert enum_body.rstrip().endswith('timeLimit'), 'timeLimit must be the last server notification'
assert 'case ServerNotificationType::timeLimit:' in server_cpp
handler = client[client.index('case ServerNotificationType::timeLimit:'):]
handler = handler[:handler.index('break;\n        }')]
assert 'mTimeLimitSeconds' in handler and 'mWaveCountdownSeconds' in handler
assert 'getTimeLimitSeconds' in client_h and 'getWaveCountdownSeconds' in client_h
assert 'mTimeLimitSeconds = -1;' in function(client, 'case ServerNotificationType::clientAccepted:')
assert 'getTimeLimitSeconds()' in game_mode and 'HorizontalPipe/TimeLimitDisplay' in game_mode
assert 'name="TimeLimitDisplay"' in game_layout

# the HUD countdown and the wave countdown
assert 'case LevelScriptActionType::countdown:' in runner and 'setScriptCountdown(' in runner
assert 'TIME_LIMIT_COUNTDOWN_FLAG' in game_mode and 'TIME_LIMIT_COUNTDOWN_FLAG' in gamemap
assert 'type == "countdown"' in script_cpp and 'os << "countdown' in script_cpp and 'key == "Countdown"' in script_cpp
assert 'case LevelScriptActionType::waveCountdown:' in runner and 'setScriptWaveCountdown(' in runner
assert 'type == "wavecountdown"' in script_cpp and 'os << "wavecountdown' in script_cpp and 'key == "WaveCountdown"' in script_cpp
assert 'mWaveCountdownSeconds' in client and 'mWaveCountdownToSend' in gamemap
assert 'WaveCountdownDisplay' in game_mode and 'WaveCountdownDisplay' in game_layout

# a time limit makes every keeper lose without a destroyed heart
player = read('source/game/Player.cpp')
time_up = function(player, 'void Player::notifyTimeUp(')
assert 'mHasLost' in time_up and 'notifyDefeat(true)' in time_up
assert 'notifyDefeat(hasTeamLost)' in function(player, 'void Player::notifyNoMoreDungeonTemple(')

# creature event conditions: the counters are filled where the event happens
creature = read('source/entities/Creature.cpp')
stats_h = read('source/game/SeatStatistics.h')
for counter in ('mCreaturesLost', 'mCreaturesPickedUp', 'mCreaturesDropped', 'mCreaturesSlapped'):
    assert counter in stats_h and counter + ' = 0;' in stats_h, counter
assert '++getSeat()->getStatistics().mCreaturesPickedUp' in function(creature, 'void Creature::pickup()')
assert '++getSeat()->getStatistics().mCreaturesDropped' in function(creature, 'void Creature::drop(')
assert '++getSeat()->getStatistics().mCreaturesSlapped' in function(creature, 'void Creature::slap()')
assert '++getSeat()->getStatistics().mCreaturesLost' in function(creature, 'double Creature::takeDamage(') or     '++getSeat()->getStatistics().mCreaturesLost' in creature
for name in ('happy', 'angry', 'atlevel', 'lost', 'pickedup', 'dropped', 'slapped'):
    assert 'type == "%s"' % name in script_cpp, name
for kind in ('happyCreatures', 'angryCreatures', 'creaturesAtLevel', 'creaturesLost', 'creaturesPickedUp',
             'creaturesDropped', 'creaturesSlapped'):
    assert 'case LevelScriptConditionType::%s:' % kind in runner, kind
    assert 'case LevelScriptConditionType::%s:' % kind in script_cpp, kind

# the region test level: a skirmish with 100 start mana, found by the level list
level = read('levels/skirmish/TestRegionScripting.level')
assert 'Region scripting' in level
assert re.search(r'^mana	100$', level, re.M), 'start mana must be 100'
section = level[level.index('[Triggers]'):level.index('[/Triggers]')]
for word in ('Cond	pickedup', 'Cond	dropped', 'Cond	slapped', 'Cond	happy', 'Cond	lost', 'Cond	kills',
             'Cond	claimed', 'Cond	region', 'Cond	gold', 'Action	make', 'Action	reveal',
             'Action	timelimit', 'Action	win'):
    assert word in section, word

print('level script make and time limit wiring: ok')
