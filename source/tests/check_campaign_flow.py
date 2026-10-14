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
# A bonus level can opt out of the Heartstone pieces with its own reward.
assert 'key == "Heartstone"' in read('source/game/Campaign.cpp')
print('campaign flow: ok')
