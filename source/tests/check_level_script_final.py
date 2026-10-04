"""Static wiring check for the later stages of level scripting: the make action, the time limit with
its countdown, the creature event conditions, the victory statistics and the region test level.

The parser and the writer are run by check_level_script.py; this script checks that the pieces
that need the running game are connected, so a missing hook is found without playing.
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(relative):
    return (repo / relative).read_text(encoding='utf-8')


def function(source, signature):
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
script_h = read('source/gamemap/LevelScript.h')
script_cpp = read('source/gamemap/LevelScript.cpp')
gamemap = read('source/gamemap/GameMap.cpp')
gamemap_h = read('source/gamemap/GameMap.h')
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
set_limit = function(gamemap, 'void GameMap::setScriptTimeLimit(')
assert 'mGameDurationAnnounced = false' in set_limit
assert 'TIME_LIMIT_REMOVED' in set_limit
send = function(gamemap, 'void GameMap::sendTimeLimit(')
assert 'ServerNotificationType::timeLimit' in send and 'getIsHuman()' in send
assert 'rebaseTimeLimit(' in map_handler
enum_body = server_h.split('enum class ServerNotificationType')[1].split('};')[0]
assert [n for n in re.findall(r'^\s*([A-Za-z_]\w*)\s*,?\s*$', enum_body, re.M) if n != 'creatureAppearance'][-2:] == ['timeLimit', 'chickenKindChanged'], 'timeLimit and then chickenKindChanged must be the last server notifications'
assert 'case ServerNotificationType::timeLimit:' in server_cpp
handler = client[client.index('case ServerNotificationType::timeLimit:'):]
handler = handler[:handler.index('break;')]
assert 'mTimeLimitSeconds' in handler
assert 'getTimeLimitSeconds' in client_h
assert 'mTimeLimitSeconds = -1;' in function(client, 'case ServerNotificationType::clientAccepted:') or 'mTimeLimitSeconds = -1;' in client
assert 'getTimeLimitSeconds()' in game_mode and 'HorizontalPipe/TimeLimitDisplay' in game_mode
assert 'name="TimeLimitDisplay"' in game_layout

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

# victory statistics: sent at the win, carried to the campaign menu
player = read('source/game/Player.cpp')
send = function(player, 'void Player::sendLevelStatistics(')
assert 'ServerNotificationType::levelStatistics' in send and 'levelWon' in send
assert 'sendLevelStatistics(false)' in function(player, 'void Player::notifyDefeat(')
won = function(gamemap, 'void GameMap::addWinningSeat(')
assert 'player->sendLevelStatistics(true)' in won
assert won.index('sendLevelStatistics(true)') < won.index('Campaign::getSingleton().onLevelWon()')
levels_handler = client[client.index('case ServerNotificationType::levelStatistics:'):]
levels_handler = levels_handler[:levels_handler.index('break;')]
assert 'Campaign::getSingleton().setLevelSummary(' in levels_handler and 'debriefingSeatSummary(' in levels_handler
campaign_h = read('source/game/Campaign.h')
campaign_cpp = read('source/game/Campaign.cpp')
assert 'setLevelSummary' in campaign_h and 'getLevelSummary' in campaign_h
assert 'mLevelSummary.clear()' in function(campaign_cpp, 'void Campaign::startLevel(')
assert 'mLevelSummary.clear()' in function(campaign_cpp, 'void Campaign::clearPlayedLevel(')
menu = read('source/modes/MenuModeCampaign.cpp')
assert 'campaign.getLevelSummary()' in menu
assert menu.index('campaign.getLevelSummary()') < menu.index('campaign.clearPlayedLevel()')

# the region test level: a skirmish with 100 start mana, found by the level list
level = read('levels/skirmish/TestRegionScripting.level')
assert 'Region scripting' in level
assert re.search(r'^mana	100$', level, re.M), 'start mana must be 100'
section = level[level.index('[Triggers]'):level.index('[/Triggers]')]
for word in ('Cond	pickedup', 'Cond	dropped', 'Cond	slapped', 'Cond	happy', 'Cond	lost', 'Cond	kills',
             'Cond	claimed', 'Cond	region', 'Cond	gold', 'Action	make', 'Action	reveal',
             'Action	timelimit', 'Action	win'):
    assert word in section, word

print('level script final wiring: ok')
