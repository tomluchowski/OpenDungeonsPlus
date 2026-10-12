"""Check the Dungeonbook (social browser): window layout, HUD button, hotkey, event hooks and limits.

The window is a fixed size OD/FrameWindow imported into the game layout (so it is registered for UI
scaling like the other windows), it closes with its cross and with Escape (closeTopWindow), the
hotkey B is used only once, and the posts come only from client side events that are guarded so they
cost nothing while the feed is not running.
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
checks = 0


def check(condition, message):
    global checks
    checks += 1
    assert condition, message


def read(path):
    return (repo / path).read_text(encoding='utf-8', errors='replace')


layout = read('gui/WindowSocial.layout')
check('name="SocialWindow"' in layout and 'type="OD/FrameWindow"' in layout, 'frame window')
check('<Property name="SizingEnabled" value="False" />' in layout, 'the window must not be resizable')
m = re.search(r'name="Area" value="\{\{0\.5,(-?\d+)\},\{0\.5,(-?\d+)\},\{0\.5,(\d+)\},\{0\.5,(\d+)\}\}"', layout)
check(m is not None, 'a fixed size centred area is expected')
width, height = int(m[3]) - int(m[1]), int(m[4]) - int(m[2])
check(width <= 1024 - 64 and height <= 768 - 120, 'the window must fit into 1024x768 with the HUD: %dx%d' % (width, height))
check('name="Visible" value="False"' in layout, 'hidden by default')
for name in ('CreaturesLabel', 'FilterButton', 'CreatureList', 'FeedTab', 'ProfileTab', 'FeedPane', 'FeedModeButton',
             'FeedText', 'ProfilePane', 'ProfileHint', 'ProfileHolder',
             'OwnPostsText'):
    check('name="%s"' % name in layout, name)
check('OpenProfileButton' not in layout, 'the profile opens in the right pane, not in a window')
# the right part (feed or profile) starts right of the list and both panes use the same area
list_block = layout[layout.index('name="CreatureList"'):]
list_right = int(re.search(r'Area" value="\{\{0,\d+\},\{0,\d+\},\{0,(\d+)\}', list_block)[1])
pane_lefts = [int(re.search(r'Area" value="\{\{0,(\d+)\}', layout[layout.index('name="%s"' % n):])[1])
              for n in ('FeedTab', 'FeedPane', 'ProfilePane')]
check(all(left > list_right for left in pane_lefts), 'the right part must not overlap the list')
check(width - 16 - min(pane_lefts) >= 400, 'the profile needs about 400 design pixels')
check('name="Visible" value="False"' in layout[layout.index('name="ProfilePane"'):layout.index('name="ProfileHint"')],
      'the feed is shown first')
check('Area" value="{{0.5' in layout and 'UDim' not in layout, 'design pixels only')

hud = read('gui/ModeGame.layout')
check('filename="WindowSocial.layout"' in hud, 'window not imported')
button = hud[hud.index('name="SocialButton"'):]
button = button[:button.index('</Window>')]
check('{{1,-79},{0,5},{1,-54},{0,30}}' in button, 'button is not in the free slot of the top bar')
check('TooltipText' in button, 'tooltip')
icon = re.search(r'ButtonImage" value="OpenDungeonsIcons/(\w+)"', button)
check(icon is not None and 'name="%s"' % icon[1] in read('gui/ODIcons.imageset'), 'button icon must exist')

gamemode = read('source/modes/GameMode.cpp')
check(gamemode.count('case OIS::KC_B:') == 1, 'one B case in GameMode')
for path in Path(repo / 'source').rglob('*.cpp'):
    if path.name != 'GameMode.cpp':
        check('KC_B' not in path.read_text(encoding='utf-8', errors='replace'), 'KC_B used in ' + path.name)
check('toggleSocialWindow' in gamemode and 'EventCloseClicked, CEGUI::Event::Subscriber(&SocialWindow::onCloseClicked' in gamemode,
      'button and close handler')
body = gamemode[gamemode.index('bool GameMode::toggleSocialWindow'):]
body = body[:body.index('\n}\n')]
check('getLocalPlayer() == nullptr' in body, 'no local player')
check('B opens the Dungeonbook' in gamemode, 'help text')
check('PostLog::getSingleton().start(' in gamemode and 'PostLog::getSingleton().stop()' in gamemode
      and 'SocialProfileCache::getSingleton().clear()' in gamemode, 'the feed is reset when a game starts and ends')
frame = gamemode[gamemode.index('void GameMode::onFrameStarted'):]
frame = frame[:frame.index('\n}\n')]
check('mSocialWindow->update(' in frame and 'PostLog' not in frame, 'the feed is not polled per frame')

window = read('source/render/SocialWindow.cpp')
header = read('source/render/SocialWindow.h')
check('onOpenProfileClicked' not in window + gamemode + header and 'createStatsWindow' not in window,
      'no window is opened from the Dungeonbook')
for handler in ('onFeedTabClicked', 'onProfileTabClicked'):
    check('SocialWindow::' + handler in window and '&SocialWindow::' + handler in gamemode, handler)
check('SocialWindow::onLinkClicked' in window and '&SocialWindow::onLinkClicked' in window,
      'the names in the friends and foe rows select a creature')
page = read('gui/WindowCreatureProfilePage.layout')
for name in ('FriendLink0', 'FriendLink1', 'FoeLink'):
    check('type="OD/InvisibleButton" name="%s"' % name in page, name + ' is a borderless button')
check('LinkButton' not in layout + window + gamemode, 'the friend and foe buttons below the profile are gone')
check('getHorzScrollbar' not in window and 'elideToWidth' in window,
      'long list entries are shortened, no horizontal scrollbar')
check('EventMouseDoubleClick' not in gamemode[gamemode.index('SocialWindow/CreatureList'):gamemode.index('GameMode::~GameMode')],
      'no double click window')
selection = window[window.index('bool SocialWindow::onSelectionChanged'):]
selection = selection[:selection.index('\n}\n')]
check('showTab(true)' in selection, 'selecting a creature shows its profile')
check('fillProfilePage(mProfilePage)' in window and 'createCreatureProfilePage(holder)' in window,
      'the profile uses the shared fill function')
check('setTabState' in window and 'setDisabled(' not in window and 'setEnabled(' not in window,
      'the active tab stays enabled')
check('log.getVersion() != mShownProfilePostVersion' in window, 'the profile redraws only when needed')
check('if(!mWindow->isVisible())' in window, 'update does nothing while hidden')
check('REFRESH_CHECK_INTERVAL = 0.25f' in window and 'MAX_FEED_ROWS = 25' in window, 'redraw throttle and row limit')
check('getVersion() != mShownPostVersion' in window, 'redraw only when the feed changed')

postlog = read('source/social/PostLog.h')
check('MAX_POSTS = 200' in postlog, 'post limit')

creature = read('source/entities/Creature.cpp')
update = creature[creature.index('void Creature::updateFromPacket'):]
update = update[:update.index('\n}\n')]
check('isSocialFeedSource()' in update and 'reportUpdate(' in update, 'the update hook must be guarded')
source = creature[creature.index('bool Creature::isSocialFeedSource'):]
source = source[:source.index('\n}\n')]
check('isActive()' in source and 'getIsOnServerMap()' in source, 'posts only from client side creatures')

client = read('source/network/ODClient.cpp')
for call in ('socialCreatureAdded()', 'socialCreatureRemoved()', 'PostCategory::PickedUp'):
    check(call in client, call)

for pattern in ('network/ODServer*', 'network/ServerNotification*', 'network/ServerMode*', 'game/*'):
    for path in (repo / 'source').glob(pattern):
        if path.is_file():
            check('social/' not in read(str(path.relative_to(repo))), '%s includes the social code' % path.name)

cache = read('source/social/SocialProfileCache.cpp')
check('findFriendsAndFoe' in cache and 'getRosterVersion' in creature, 'friends cached per roster version')
for path in ('source/render/SocialWindow.cpp', 'source/render/SocialWindow.h', 'source/social/PostLog.cpp',
             'source/social/PostLog.h', 'source/social/CreaturePosts.cpp', 'source/social/CreaturePosts.h'):
    text = read(path)
    check(not re.search(r'\bauto\b', text), path + ' uses auto')
    check(not re.search('[\x00-\x08\x0b\x0c\x0e-\x1f]', text), path + ' contains control characters')
print('ok (%d checks)' % checks)
