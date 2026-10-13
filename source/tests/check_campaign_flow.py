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
print('campaign flow: ok')
