"""Static check of two campaign flow rules: new campaign confirmation and the way out of a lost level."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


layout = read('gui/WindowCampaignSubMenu.layout')
menu = read('source/modes/MenuModeMain.cpp')
game = read('source/modes/GameMode.cpp')

# The confirmation widgets exist, start hidden and are the ones the code looks up.
for name in ('NewCampaignConfirmText', 'NewCampaignConfirmButton'):
    start = layout.index('name="%s"' % name)
    assert 'name="Visible" value="False"' in layout[start:layout.index('</Window>', start)], name
    assert '"%s"' % name in menu, name

# Progress is only reset by the confirmation (or at once when there is nothing to lose).
pressed = menu[menu.index('bool MenuModeMain::newCampaignPressed('):menu.index('bool MenuModeMain::newCampaignConfirmed(')]
assert 'hasProgress()' in pressed and 'resetProgress' not in pressed
confirmed = menu[menu.index('bool MenuModeMain::newCampaignConfirmed('):menu.index('void MenuModeMain::showNewCampaignConfirm(')]
assert 'resetProgress()' in confirmed

# A lost campaign level leads back to the campaign menu, anything else to the skirmish sub-menu.
defeat = game[game.index('bool GameMode::onClickDefeatDebriefingConfirm('):game.index('bool GameMode::onClickYesQuitMenu(')]
assert 'Campaign::getSingleton().isActive()' in defeat
assert 'MENU_CAMPAIGN' in defeat and 'requestMainMenuWithSkirmishSubMenu()' in defeat
game_map = read('source/gamemap/GameMap.cpp')
# The Heartstone has four pieces (B01 to B04); the fifth bonus site has its own reward.
campaign = read('levels/campaign/Campaign.cfg')
entries = [e for e in campaign.replace(chr(13), '').split('[Level]')[1:] if 'Bonus=1' in e]
assert len(entries) == 5
pieces = [e for e in entries if 'Heartstone=0' not in e]
assert len(pieces) == 4 and all('Province=B0' in e for e in pieces)
assert 'Province=B05' in [e for e in entries if 'Heartstone=0' in e][0]
assert 'key == "Heartstone"' in read('source/game/Campaign.cpp')
# The complete Heartstone gives the keepers the Summon champion spell when a campaign level starts
server = read('source/network/ODServer.cpp')
assert 'isHeartstoneComplete()' in server and 'SkillType::spellSummonChampion, true, true' in server
# Victory keeps its authoritative goal list and saves progress before notifying the client.
victory = game_map[game_map.index('void GameMap::addWinningSeat('):game_map.index('bool GameMap::seatIsAWinner(')]
assert victory.index('mWinningSeats.push_back(s)') < victory.index('getGoalsStringForPlayer(player)')
assert victory.index('sendLevelStatistics(true)') < victory.index('onLevelWon()') < victory.index('ServerNotificationType::campaignVictory')
assert 'Open the menu and quit' not in victory
goals = game_map[game_map.index('std::string GameMap::getGoalsStringForPlayer('):game_map.index('int GameMap::addGoldToSeat(')]
assert 'else if (seat->numFailedGoals()' not in goals
for state in ('Open', 'Reached', 'Failed'):
    assert "[image='GoalStatus/" + state + "'] " in goals, state
for old_label in ('Open: ', 'Done: ', 'Failed: '):
    assert old_label not in goals, old_label
assert 'Victory!\\n' in goals
assert 'script.getTriggers()' in goals and 'LevelScriptActionType::objective' in goals
assert 'action.mSeatId >= 0 && action.mSeatId != seat->getId()' in goals
assert 'Script objectives:' not in goals and 'scriptObjectives.push_back(action.mText)' in goals
assert 'scriptObjectives.push_back(stateMark + action.mText)' in goals
assert 'stateMark = reachedGoalMark' in goals and 'stateMark = failedGoalMark' in goals
assert 'stateText' not in goals
assert 'getObjectiveState(seat->getId(), action.mObjectiveId)' in goals
script_header = read('source/gamemap/LevelScript.h')
script_source = read('source/gamemap/LevelScript.cpp')
script_runner = read('source/gamemap/LevelScriptRunner.cpp')
assert 'std::string mObjectiveId;' in script_header and 'objectiveState' in script_header
assert 'if(type == "objectiveid")' in script_source and 'if(type == "objectivestate")' in script_source
assert 'if((type == "message") || (type == "objective"))' in script_source
assert 'if(key == "ObjectiveState")' in script_source and 'os << "ObjectiveState\\t"' in script_source
assert 'mObjectiveStates.clear()' in script_source
assert 'script.setObjectiveState(player->getSeat()->getId(), action.mObjectiveId' in script_runner

# Each normal turn sends the objective text regardless of the seat's old goal-dirty flag.
server = read('source/network/ODServer.cpp')
snapshot_start = server.index('// We notify the clients about what they got')
snapshot = server[snapshot_start:server.index('notifyHeartHealth(gameMap, sock, player)', snapshot_start)]
assert 'for (ODSocketClient* sock : mSockClients)' in snapshot
assert 'ServerNotificationType::refreshPlayerSeat' in snapshot
assert 'std::string goals = gameMap->getGoalsStringForPlayer(player);' in snapshot
assert 'serverNotification->mPacket << goals;' in snapshot
assert 'getHasGoalsChanged' not in snapshot
client_source = read('source/network/ODClient.cpp')
refresh = client_source[client_source.index('case ServerNotificationType::refreshPlayerSeat:'):client_source.index('case ServerNotificationType::creaturePanel:')]
assert 'packetReceived >> goalsString' in refresh and 'refreshMainUI(goalsString)' in refresh
refresh_ui = client_source[client_source.index('void ODClient::refreshMainUI('):client_source.index('bool ODClient::connect(')]
assert 'gm->refreshPlayerGoals(goalsString);' in refresh_ui and 'getHasGoalsChanged' not in refresh_ui

# The approved status marks are graphical, registered, scalable, and separate from text labels.
import xml.etree.ElementTree as ET
from PIL import Image
scheme = ET.fromstring(read('gui/ODSkin.scheme'))
assert scheme.find("Imageset[@filename='GoalStatus.imageset']") is not None
atlas = ET.fromstring(read('gui/GoalStatus.imageset'))
assert atlas.get('autoScaled') == 'true'
assert atlas.get('nativeHorzRes') == '1024' and atlas.get('nativeVertRes') == '768'
marks = {item.get('name'): item for item in atlas.findall('Image')}
assert set(marks) == {'Open', 'Reached', 'Failed'}
with Image.open(repo / 'gui' / atlas.get('imagefile')) as artwork:
    assert artwork.mode == 'RGBA' and artwork.size == (60, 20)
    assert artwork.getpixel((0, 0))[3] == 0
    for index, state in enumerate(('Open', 'Reached', 'Failed')):
        mark = marks[state]
        assert [int(mark.get(key)) for key in ('xPos', 'yPos', 'width', 'height')] == [index * 20, 0, 20, 20]
        pixels = [artwork.getpixel((x, y)) for y in range(20) for x in range(index * 20, (index + 1) * 20)]
        if state == 'Reached':
            assert any(g > 140 and g > r * 1.5 and g > b * 1.5 and a > 200 for r, g, b, a in pixels)
        if state == 'Failed':
            assert any(r > 160 and r > g * 1.5 and r > b * 1.5 and a > 200 for r, g, b, a in pixels)
assert 'name.compare(0, 11, "GoalStatus/") == 0' in read('source/render/Gui.cpp')
assert 'gui/GoalStatus.svg, gui/GoalStatus.png' in read('CREDITS')
briefing = read('source/modes/MenuModeCampaign.cpp')
assert "[image='GoalStatus/Open'] " in briefing
assert 'Open: ' not in briefing

# A new minimap entry uses existing artwork and the same objectives toggle.
import xml.etree.ElementTree as ET
hud = ET.fromstring(read('gui/ModeGame.layout'))
button = hud.find(".//Window[@name='GoalsButton']")
assert button is not None
properties = {item.get('name'): item.get('value') for item in button.findall('Property')}
assert properties['TooltipText'] == 'Goals'
assert properties['NormalImage'] == 'OpenDungeonsIcons/ObjectivesIcon'
assert properties['Area'] == '{{0,4},{1,-224},{0,44},{1,-184}}'
connection = game[game.index('guiSheet->getChild("GoalsButton")'):]
connection = connection[:connection.index('//Casino payout window')]
assert '&GameMode::toggleObjectivesWindow' in connection

# Only the saved campaign victory opens the existing result and its explicit map confirmation.
notification = read('source/network/ServerNotification.h')
enum = notification[notification.index('enum class ServerNotificationType'):notification.index('ODPacket& operator<<')]
assert enum.index('timeLimit,') < enum.index('campaignVictory')
client = read('source/network/ODClient.cpp')
packet = client[client.index('case ServerNotificationType::campaignVictory:'):client.index('case ServerNotificationType::timeLimit:')]
assert 'std::string goalsDisplay;' in packet and 'packetReceived >> goalsDisplay' in packet
assert 'showCampaignVictory(goalsDisplay)' in packet
result = game[game.index('void GameMode::showCampaignVictory('):game.index('bool GameMode::onClickDefeatDebriefingConfirm(')]
assert '!Campaign::getSingleton().isActive()' in result and 'mDefeatDebriefing != nullptr' in result
assert result.index('refreshPlayerGoals(goalsDisplay)') < result.index('showDefeatDebriefing()')
assert 'requestMode(AbstractModeManager::MENU_CAMPAIGN)' in result
print('campaign flow: ok')
