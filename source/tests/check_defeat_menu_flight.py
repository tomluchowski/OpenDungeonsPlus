"""Exercise the defeat follow-ups without a game: menu flight timing, its use by the main menu, camera symbol, stone imagery.

MenuFlight.h is compiled as it is. MenuModeMain::onFrameStarted and the two hand-over functions of ModeManager.h are cut out of
the production sources and compiled against small mocks. The image names used by the layout and by GameMode.cpp are checked
against the real imagesets and the icon picture.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ElementTree

from PIL import Image

repo = Path(__file__).resolve().parents[2]
game_mode = (repo / 'source/modes/GameMode.cpp').read_text()
mode_manager = (repo / 'source/modes/ModeManager.h').read_text()
menu_main = (repo / 'source/modes/MenuModeMain.cpp').read_text()
menu_scene = (repo / 'source/renderscene/RenderSceneMenu.cpp').read_text()
flight_header = (repo / 'source/render/MenuFlight.h').read_text()
gui = repo / 'gui'


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def imagesets():
    """Every image of the real imagesets as 'Set/Image': (picture file, x, y, width, height)."""
    found = {}
    for path in sorted(gui.glob('*.imageset')):
        root = ElementTree.parse(path).getroot()
        for image in root.findall('Image'):
            found[root.get('name') + '/' + image.get('name')] = (
                gui / root.get('imagefile'), int(image.get('xPos')), int(image.get('yPos')),
                int(image.get('width')), int(image.get('height')))
    return found


images = imagesets()

probe = r'''
#include <functional>
#include <iostream>
#include <string>
#include "render/MenuFlight.h"

int gChecks = 0, gFailures = 0;
void check(bool ok, const char* msg) {++gChecks;if(!ok){++gFailures;std::cout << "FAIL " << msg << '\n';}}

namespace Ogre {struct FrameEvent {};}
const std::string WINDOW_SKIRMISH = "SkirmishSubMenuWindow";
int gToggles = 0;
std::string gToggled;
MenuFlight gFlight;
struct ODFrameListener
{
    static ODFrameListener& getSingleton() {static ODFrameListener listener;return listener;}
    bool isMainMenuFlightActive() const {return gFlight.isActive();}
};

struct ModeManager
{
    enum ModeType {MENU_MAIN = 2};
    bool mOpenSkirmishSubMenu = false;
    void requestMode(ModeType, bool = true) {}
@@REQUESTMAINMENU@@
@@CONSUMEREQUEST@@
};

class MenuModeMain
{
public:
    void onFrameStarted(const Ogre::FrameEvent& evt);
    bool toggleSubMenu(const std::string& name) {++gToggles;gToggled = name;return true;}
    bool mSkirmishSubMenuPending = false;
};
@@FRAME@@

namespace S = MenuFlightSettings;

int main()
{
    // Pure timing and path functions
    check(MenuFlight::progressAt(-1.0f) == 0.0f && MenuFlight::progressAt(0.0f) == 0.0f, "progress: before and at the start is 0");
    check(MenuFlight::progressAt(S::DURATION * 0.5f) == 0.5f, "progress: halfway");
    check(MenuFlight::progressAt(S::DURATION) == 1.0f && MenuFlight::progressAt(100.0f) == 1.0f, "progress: at and after the end is 1");
    check(S::DURATION >= 2.5f && S::DURATION <= 3.5f, "the flight lasts about 3 seconds");
    check(MenuFlight::scaleAt(0.0f) == S::START_SCALE && MenuFlight::scaleAt(-5.0f) == S::START_SCALE, "scale: the start (and earlier) is the start scale");
    check(MenuFlight::scaleAt(S::DURATION) == 1.0f && MenuFlight::scaleAt(S::DURATION + 10.0f) == 1.0f, "scale: the end (and later) is exactly the normal menu view");
    bool monotonic = true;
    float last = MenuFlight::scaleAt(0.0f);
    for(int i = 1; i <= 300; ++i)
    {
        float scale = MenuFlight::scaleAt(S::DURATION * i / 300.0f);
        monotonic = monotonic && scale <= last && scale >= 1.0f && scale <= S::START_SCALE;
        last = scale;
    }
    check(monotonic, "scale: never grows, stays between 1 and the start scale");
    check(MenuFlight::scaleAt(S::DURATION * 0.9f) - 1.0f < (MenuFlight::scaleAt(S::DURATION * 0.1f) - 1.0f) * 0.1f, "scale: slows down towards the end");

    // The state
    MenuFlight flight;
    check(!flight.isActive() && flight.getScale() == 1.0f, "idle: not active, normal view (the normal menu entry is unchanged)");
    check(!flight.advance(1.0f) && flight.getScale() == 1.0f && !flight.isActive(), "idle: advancing does nothing");
    check(flight.start() && flight.isActive() && flight.getScale() == S::START_SCALE, "start: active at the start scale");
    check(!flight.start(), "start: a second start while running is refused");
    check(!flight.advance(S::MAX_STEP * 4.0f) && flight.isActive(), "one slow frame is clamped and does not end the flight");
    check(flight.getScale() < S::START_SCALE && flight.getScale() > 1.0f, "in the middle the picture is between the start and the normal size");
    check(!flight.advance(-1.0f) && flight.isActive(), "a negative step does nothing");
    int steps = 0;
    bool ended = false;
    while(!ended && steps < 1000)
    {
        ended = flight.advance(0.1f);
        ++steps;
    }
    check(ended && !flight.isActive() && flight.getScale() == 1.0f, "the flight ends at the normal view");
    check(steps >= 20 && steps <= 30, "after one slow frame of 0.25 s the remaining time takes about 27 steps of 0.1 s");
    check(!flight.advance(0.1f), "the end is reported once");
    check(flight.start(), "a new request may start a new flight after the end");
    flight.reset();
    check(!flight.isActive() && flight.getScale() == 1.0f, "reset stops the flight");

    // The hand-over flag works exactly once and the sub-menu waits for the end of the flight
    ModeManager manager;
    check(!manager.consumeSkirmishSubMenuRequest(), "no request: nothing to consume");
    manager.requestMainMenuWithSkirmishSubMenu();
    check(manager.consumeSkirmishSubMenuRequest() && !manager.consumeSkirmishSubMenuRequest(), "the hand-over is consumed exactly once");

    MenuModeMain menu;
    Ogre::FrameEvent event;
    menu.onFrameStarted(event);
    check(gToggles == 0, "without a pending request the frame hook never shows a sub-menu");
    menu.mSkirmishSubMenuPending = true;
    gFlight.start();
    for(int i = 0; i < 100 && gFlight.isActive(); ++i)
    {
        menu.onFrameStarted(event);
        gFlight.advance(0.1f);
    }
    check(gToggles == 0 && menu.mSkirmishSubMenuPending, "during the flight the sub-menu stays closed");
    menu.onFrameStarted(event);
    check(gToggles == 1 && gToggled == WINDOW_SKIRMISH && !menu.mSkirmishSubMenuPending, "after the flight the skirmish sub-menu opens");
    menu.onFrameStarted(event);
    menu.onFrameStarted(event);
    check(gToggles == 1, "and only once");
    menu.mSkirmishSubMenuPending = true;
    menu.onFrameStarted(event);
    check(gToggles == 2, "a request when the flight could not start opens the sub-menu at once instead of waiting forever");

    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
probe = probe.replace('@@REQUESTMAINMENU@@', function(mode_manager, 'void requestMainMenuWithSkirmishSubMenu('))
probe = probe.replace('@@CONSUMEREQUEST@@', function(mode_manager, 'bool consumeSkirmishSubMenuRequest('))
probe = probe.replace('@@FRAME@@', function(menu_main, 'void MenuModeMain::onFrameStarted('))
assert '@@' not in probe

# The camera symbol: an image window with an image that exists and is not empty
create = function(game_mode, 'void GameMode::createDefeatWindows(')
assert 'createWindow("OD/StaticImage", "DefeatCameraMarker")' in create
assert 'setText("CAM")' not in create
marker = re.search(r'^const std::string DEFEAT_CAMERA_MARKER_IMAGE = "([^"]+)";$', game_mode, re.MULTILINE)
assert marker and 'setProperty("Image", DEFEAT_CAMERA_MARKER_IMAGE)' in create
assert marker.group(1) in images, marker.group(1)
picture, x, y, width, height = images[marker.group(1)]
pixels = Image.open(picture).convert('RGBA')
assert x + width <= pixels.width and y + height <= pixels.height
assert pixels.crop((x, y, x + width, y + height)).getchannel('A').getbbox() is not None, 'the camera icon is empty'
for name, (other, ox, oy, ow, oh) in images.items():
    if name != marker.group(1) and other == picture:
        assert x + width <= ox or ox + ow <= x or y + height <= oy or oy + oh <= y, 'overlaps ' + name
print('CAMERA OK: the marker is an image window with', marker.group(1), 'and that image is a non-empty cell that overlaps no other image')

# The layout only names images the imagesets have, and uses the stone surface
layout = ElementTree.parse(gui / 'WindowDefeatDebriefing.layout').getroot()
used = []
for window in layout.iter('Window'):
    for prop in window.findall('Property'):
        if prop.get('name') in ('Image', 'NormalImage', 'HoverImage', 'PushedImage'):
            used.append(prop.get('value'))
assert used and all(name in images for name in used), [name for name in used if name not in images]
assert 'ODHudSurface/Stone' in used and 'OpenDungeonsIcons/CheckIcon' in used
types = {window.get('name'): window.get('type') for window in layout.iter('Window')}
assert types['Background'] == 'OD/StaticImage' and types['Panel'] == 'OD/StaticImage'
scheme = (gui / 'ODSkin.scheme').read_text()
assert 'windowType="OD/StaticImage"' in scheme and 'ODHudSurface.imageset' in scheme
for needed in ('Panel/Title', 'Panel/PlayerName', 'Panel/Outcome', 'Panel/Elapsed', 'Panel/StatisticsArea', 'Panel/ConfirmButton'):
    node = layout.find('Window')
    for part in needed.split('/'):
        found = [child for child in node.findall('Window') if child.get('name') == part]
        assert found, needed
        node = found[0]
print('LAYOUT OK: every image name exists in the imagesets, the stone surface backs the screen and the panel, the table area and button remain')

# Wiring of the flight
activate = function(menu_main, 'void MenuModeMain::activate(')
assert activate.index('showMainMenuButtons(true);') < activate.index('consumeSkirmishSubMenuRequest()') < activate.index('createMainMenuScene();') < activate.index('startMainMenuFlight();')
assert 'toggleSubMenu(WINDOW_SKIRMISH)' not in activate
assert 'if(mSkirmishSubMenuPending)\n        ODFrameListener::getSingleton().startMainMenuFlight();' in activate
assert 'mSkirmishSubMenuPending = false;' in activate
update = function(menu_scene, 'void RenderSceneMenu::updateMenu(')
assert 'mFlight.advance(timeSinceLastFrame)' in update and update.count('flightScale') == 3
assert 'mFlight.reset();' in function(menu_scene, 'void RenderSceneMenu::freeMenu(')
assert 'MenuFlight' in (repo / 'source/renderscene/RenderSceneMenu.h').read_text()
print('WIRING OK: the flight starts only through the hand-over flag, the sub-menu waits for it, the normal entry is unchanged')

assert re.search(r'\bauto\b', probe) is None and re.search(r'\bauto\b', flight_header) is None
assert re.search(r'\bauto\b', function(menu_main, 'void MenuModeMain::onFrameStarted(')) is None
print('STYLE OK: no auto in the new code or in the fixture')
for path in (Path(__file__), repo / 'source/render/MenuFlight.h', repo / 'source/renderscene/RenderSceneMenu.cpp', repo / 'source/renderscene/RenderSceneMenu.h',
             repo / 'source/render/ODFrameListener.cpp', repo / 'source/render/ODFrameListener.h', repo / 'source/modes/MenuModeMain.cpp',
             repo / 'source/modes/MenuModeMain.h', repo / 'source/modes/GameMode.cpp', repo / 'gui/WindowDefeatDebriefing.layout',
             repo / 'gui/ODIcons.imageset'):
    bad = [b for b in path.read_bytes() if b < 32 and b not in (9, 10, 13)]
    assert not bad, path
print('BYTES OK: no stray control characters')

with tempfile.TemporaryDirectory(prefix='odp-defeat-menu-flight-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/I', str(repo / 'source'), 'check.cpp', '/Fecheck.exe'],
                   cwd=work, check=True)
    # Windows may block a freshly compiled executable (WinError 4551) for a moment
    for attempt in range(4):
        try:
            subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
            break
        except OSError as error:
            if attempt == 3:
                raise
            print('retrying after', error)
            time.sleep(2)
