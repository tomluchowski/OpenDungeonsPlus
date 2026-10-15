"""Click the defeat debriefing with the real CEGUI library and the real layout, without a game.

The user saw the debriefing, clicked, got a grey screen and could not leave it. Cause: the full screen
stone "Background" of gui/WindowDefeatDebriefing.layout is a sibling of the "Panel" that holds the
confirm button, drawn below it. CEGUI raises a clicked window to the front of its siblings
(Window::moveToFront_impl, RiseOnClickEnabled is on by default), so one click on the stone outside the
panel put the opaque grey stone in front of the panel. The button was covered and every further click
hit the stone; keys are blocked during the defeat, so the game could not be left any more.

This check links the installed CEGUIBase library, renders with the Null renderer of the CEGUI sources
(compiled in), loads the real scheme, imagesets, fonts, look and feel and layout from gui/, and drives the
real GameMode functions that build the defeat windows, open the debriefing, hide the interface each frame,
route the mouse and handle the confirm button. They are cut out of GameMode.cpp by signature; the game
objects they touch (client, map, mode manager) are small mocks.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ElementTree

repo = Path(__file__).resolve().parents[2]
deps = Path(os.environ.get('OD_DEPS_ROOT', str(Path.home() / 'od-deps')))
cegui_include = deps / 'install/include/cegui-0'
cegui_lib = deps / 'install/lib'
null_source = deps / 'src/cegui/cegui/src/RendererModules/Null'
null_include = deps / 'src/cegui/cegui/include/CEGUI/RendererModules/Null'
game_mode = (repo / 'source/modes/GameMode.cpp').read_text()
mode_manager = (repo / 'source/modes/ModeManager.h').read_text()
mode_manager_source = (repo / 'source/modes/ModeManager.cpp').read_text()
layout_path = repo / 'gui/WindowDefeatDebriefing.layout'


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def constant(name):
    match = re.search(r'^const std::string ' + name + r' = .*;$', game_mode, re.MULTILINE)
    assert match, name
    return match.group(0)


def handler_guard(signature):
    """The defeat guard of an input handler (right after the idle-hand reset), ending in a sentinel return."""
    start = game_mode.index(signature)
    body = game_mode.index('{\n', start) + 2
    first = '    resetIdleHand();\n'
    assert game_mode.startswith(first, body), signature
    guard = body + len(first)
    end = game_mode.index('        return true;\n', guard) + len('        return true;\n')
    if game_mode.startswith('    }\n', end):
        end += len('    }\n')
    return (game_mode[start:body] + game_mode[guard:end]) + '    return false;\n}\n'


probe = r'''
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <CEGUI/CEGUI.h>
#include "CEGUI/RendererModules/Null/Renderer.h"
#include "game/LevelStatistics.h"
#include "modes/DebriefingTable.h"
#include "modes/DefeatSequence.h"

std::vector<std::string> gLog;
#define OD_LOG_INF(x) gLog.push_back(x)
int gModeRequests = 0;
int gLastMode = 0;

namespace OIS
{
struct MouseEvent {float x;float y;};
enum MouseButtonID {MB_Left};
}
namespace Gui {inline CEGUI::MouseButton convertButton(OIS::MouseButtonID) {return CEGUI::LeftButton;}}
struct AbstractApplicationMode
{
    // As in the game: the absolute pointer position goes to the default GUI context
    bool mouseMoved(const OIS::MouseEvent& arg)
    {
        return CEGUI::System::getSingleton().getDefaultGUIContext().injectMousePosition(arg.x, arg.y);
    }
};
namespace Ogre
{
struct ColourValue
{
    uint32_t argb;
    explicit ColourValue(uint32_t value = 0xFFFFFFFF) : argb(value) {}
    uint32_t getAsARGB() const {return argb;}
    static const ColourValue White;
};
const ColourValue ColourValue::White(0xFFFFFFFF);
}
struct Seat
{
    Ogre::ColourValue colour;
    const Ogre::ColourValue& getColorValue() const {return colour;}
};
struct ODClient
{
    bool has = false;LevelStatistics statistics;
    static ODClient& getSingleton() {static ODClient client;return client;}
    bool hasLevelStatistics() const {return has;}
    const LevelStatistics& getLevelStatistics() const {return statistics;}
};
// The visible pointer of the game is the keeper hand (the CEGUI arrow image is transparent)
struct RenderManager {bool handVisible=false;int toggles=0;
 static RenderManager& getSingleton(){static RenderManager manager;return manager;}
 bool isKeeperHandVisible() const {return handVisible;}
 void rrToggleHandSelectorVisibility(){handVisible=!handVisible;++toggles;}};
struct ODApplication {static double turnsPerSecond;};
double ODApplication::turnsPerSecond = 1.4;
struct Player {std::string nick;const std::string& getNick() const {return nick;}};
struct GameMap
{
    Player player;int64_t turn = 84;
    Seat red;
    Player* getLocalPlayer() {return &player;}
    Seat* getSeatById(int id) {return id == 1 ? &red : nullptr;}
    int64_t getTurnNumber() const {return turn;}
};
struct ModeManager
{
    enum ModeType {NONE = 0, MENU_MAIN = 1};
    bool mOpenSkirmishSubMenu = false;
    void requestMode(ModeType mode, bool = true) {++gModeRequests;gLastMode = mode;}
@@REQUESTMAINMENU@@
@@CONSUMEREQUEST@@
};
@@CONSTANTS@@

class GameMode : public AbstractApplicationMode
{
public:
    explicit GameMode(CEGUI::Window* root) : mRootWindow(root), mGameMap(&mMap), mModeManager(&mManager) {}
    ~GameMode()
    {
        // AbstractApplicationMode::~AbstractApplicationMode
        for(size_t i = 0; i < mEventConnections.size(); ++i)
            mEventConnections[i]->disconnect();
    }
    void addEventConnection(CEGUI::Event::Connection connection) {mEventConnections.push_back(connection);}
    bool mouseMoved(const OIS::MouseEvent &arg);
    bool mousePressed(const OIS::MouseEvent& arg, OIS::MouseButtonID id);
    bool mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id);
    void createDefeatWindows();
    void destroyDefeatWindows();
    void hideInterfaceForDefeat();
    void onDefeatSequenceFinished();
    // The finished hook removes the scene objects of the sequence (checked in check_defeat_heart_burst.py)
    void stopDefeatEffects() {}
    void showDefeatDebriefing();
    void fillDefeatStatistics(const std::vector<DebriefingTableRow>& rows);
    bool onClickDefeatDebriefingConfirm(const CEGUI::EventArgs& arg);
    std::vector<CEGUI::Event::Connection> mEventConnections;
    DefeatSequence mDefeatSequence;
    CEGUI::Window* mDefeatTint = nullptr;
    CEGUI::Window* mDefeatFade = nullptr;
    CEGUI::Window* mDefeatSubtitle = nullptr;
    CEGUI::Window* mDefeatCameraMarker = nullptr;
    CEGUI::Window* mDefeatDebriefing = nullptr;
    CEGUI::Window* mRootWindow;
    GameMap mMap;
    GameMap* mGameMap;
    ModeManager mManager;
    ModeManager* mModeManager;
};
@@MOVED@@
@@PRESSED@@
@@RELEASED@@
@@CREATE@@
@@DESTROY@@
@@HIDE@@
@@FINISHED@@
@@SHOW@@
@@FILL@@
@@CONFIRM@@

class StubImageCodec : public CEGUI::ImageCodec
{
public:
    StubImageCodec() : CEGUI::ImageCodec("StubImageCodec") {}
    // The Null renderer draws nothing, so the pixels of the imagesets are not needed
    CEGUI::Texture* load(const CEGUI::RawDataContainer&, CEGUI::Texture* result) {return result;}
};

int gChecks = 0, gFailures = 0;
void check(bool ok, const char* msg) {++gChecks;if(!ok){++gFailures;std::cout << "FAIL " << msg << '\n';}}

// One frame of the running game: GameMode::updateDefeatSequence hides the interface, CEGUI gets its time
void frame(GameMode& mode)
{
    mode.hideInterfaceForDefeat();
    CEGUI::System::getSingleton().injectTimePulse(0.02f);
    CEGUI::System::getSingleton().getDefaultGUIContext().injectTimePulse(0.02f);
}

void moveTo(GameMode& mode, const CEGUI::Vector2f& point)
{
    OIS::MouseEvent event = {point.d_x, point.d_y};
    mode.mouseMoved(event);
    frame(mode);
}

void click(GameMode& mode, const CEGUI::Vector2f& point)
{
    OIS::MouseEvent event = {point.d_x, point.d_y};
    moveTo(mode, point);
    mode.mousePressed(event, OIS::MB_Left);
    frame(mode);
    mode.mouseReleased(event, OIS::MB_Left);
    frame(mode);
}

CEGUI::Vector2f centre(CEGUI::Window* window)
{
    const CEGUI::Rectf area = window->getUnclippedOuterRect().get();
    return CEGUI::Vector2f((area.left() + area.right()) * 0.5f, (area.top() + area.bottom()) * 0.5f);
}

// The window that receives a click at this point (without input capture)
CEGUI::Window* hitAt(const CEGUI::Vector2f& point)
{
    return CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow()->getTargetChildAtPosition(point);
}

int gSheets = 0;
// The game sheet with a few interface windows, then the sequence up to the open debriefing
CEGUI::Window* openDebriefing(GameMode*& mode)
{
    CEGUI::WindowManager& windowManager = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root = windowManager.createWindow("DefaultWindow", "GameSheet" + std::to_string(++gSheets));
    root->setArea(CEGUI::UDim(0, 0), CEGUI::UDim(0, 0), CEGUI::UDim(1, 0), CEGUI::UDim(1, 0));
    CEGUI::Window* hud = windowManager.createWindow("OD/StaticText", "Hud");
    hud->setArea(CEGUI::UDim(0, 0), CEGUI::UDim(0.8f, 0), CEGUI::UDim(1, 0), CEGUI::UDim(0.2f, 0));
    root->addChild(hud);
    CEGUI::Window* popup = windowManager.createWindow("OD/FrameWindow", "Popup");
    popup->setArea(CEGUI::UDim(0.3f, 0), CEGUI::UDim(0.3f, 0), CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.4f, 0));
    root->addChild(popup);
    CEGUI::System::getSingleton().getDefaultGUIContext().setRootWindow(root);

    mode = new GameMode(root);
    mode->mMap.player.nick = "Keeper";
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::time_point();
    mode->mDefeatSequence.start(1, 3, 4, start);
    mode->createDefeatWindows();
    frame(*mode);
    mode->mDefeatSequence.advanceTo(start + std::chrono::seconds(30));
    mode->onDefeatSequenceFinished();
    mode->mDefeatFade->setAlpha(1.0f);
    frame(*mode);
    return root;
}

void closeDebriefing(GameMode* mode, CEGUI::Window* root)
{
    mode->destroyDefeatWindows();
    delete mode;
    CEGUI::System::getSingleton().getDefaultGUIContext().setRootWindow(nullptr);
    CEGUI::WindowManager::getSingleton().destroyWindow(root);
    CEGUI::WindowManager::getSingleton().cleanDeadPool();
    gModeRequests = 0;
    gLastMode = 0;
}

int run()
{
    CEGUI::NullRenderer& renderer = CEGUI::NullRenderer::create();
    renderer.setDisplaySize(CEGUI::Sizef(1920.0f, 1200.0f));
    CEGUI::DefaultResourceProvider* provider = new CEGUI::DefaultResourceProvider();
    provider->setResourceGroupDirectory("gui", "@@GUI@@");
    provider->setResourceGroupDirectory("fonts", "@@FONTS@@");
    StubImageCodec codec;
    CEGUI::System::create(renderer, provider, static_cast<CEGUI::XMLParser*>(nullptr), &codec, nullptr, "", "cegui-check.log");
    CEGUI::Scheme::setDefaultResourceGroup("gui");
    CEGUI::ImageManager::setImagesetDefaultResourceGroup("gui");
    CEGUI::Font::setDefaultResourceGroup("fonts");
    CEGUI::WidgetLookManager::setDefaultResourceGroup("gui");
    CEGUI::WindowManager::setDefaultResourceGroup("gui");
    CEGUI::SchemeManager::getSingleton().createFromFile("ODSkin.scheme");
    CEGUI::System::getSingleton().notifyDisplaySizeChanged(CEGUI::Sizef(1920.0f, 1200.0f));
    CEGUI::GUIContext& context = CEGUI::System::getSingleton().getDefaultGUIContext();
    context.setDefaultTooltipType("OD/Tooltip");

    // 1. A click on the stone outside the panel, then on the confirm button
    {
        GameMode* mode = nullptr;
        CEGUI::Window* root = openDebriefing(mode);
        CEGUI::Window* background = mode->mDefeatDebriefing->getChild("Background");
        CEGUI::Window* panel = mode->mDefeatDebriefing->getChild("Panel");
        CEGUI::Window* button = mode->mDefeatDebriefing->getChild("Panel/ConfirmButton");
        check(mode->mDefeatDebriefing->getParent() == root && mode->mDefeatDebriefing->isEffectiveVisible(), "the debriefing is on the game sheet and visible");
        check(!root->getChild("Hud")->isVisible() && !root->getChild("Popup")->isVisible(), "the interface of the game is hidden");
        check(hitAt(centre(button)) == button, "the confirm button is what a click at its centre reaches");
        click(*mode, CEGUI::Vector2f(40.0f, 40.0f));
        check(hitAt(CEGUI::Vector2f(40.0f, 40.0f)) == background, "the click hit the stone outside the panel");
        check(panel->isInFront(*background), "after a click on the stone the panel stays in front of it");
        check(hitAt(centre(button)) == button, "after a click on the stone the confirm button can still be reached");
        check(gModeRequests == 0, "a click on the stone requests nothing");
        click(*mode, centre(panel->getChild("Title")));
        check(panel->isInFront(*background) && hitAt(centre(button)) == button, "a click on the panel text leaves the button reachable");
        click(*mode, centre(button));
        check(gModeRequests == 1 && gLastMode == ModeManager::MENU_MAIN, "the confirm button requests the main menu");
        check(mode->mManager.consumeSkirmishSubMenuRequest(), "with the skirmish sub-menu hand-over");
        check(CEGUI::WindowManager::getSingleton().isAlive(mode->mDefeatDebriefing) && mode->mDefeatDebriefing->getParent() == root,
            "the click only requests: the debriefing still exists until the mode is changed on the next frame");
        click(*mode, centre(button));
        check(gModeRequests == 1, "a second click requests nothing more");
        closeDebriefing(mode, root);
    }
    // 2. The pointer rests on the button until its tooltip shows, while the interface is hidden every frame
    {
        GameMode* mode = nullptr;
        CEGUI::Window* root = openDebriefing(mode);
        CEGUI::Window* button = mode->mDefeatDebriefing->getChild("Panel/ConfirmButton");
        moveTo(*mode, centre(button));
        for(int i = 0; i < 150; ++i)
            frame(*mode);
        click(*mode, centre(button));
        check(gModeRequests == 1 && gLastMode == ModeManager::MENU_MAIN, "the button works after its tooltip was due");
        closeDebriefing(mode, root);
    }
    // 3. Several clicks around the screen first
    {
        GameMode* mode = nullptr;
        CEGUI::Window* root = openDebriefing(mode);
        CEGUI::Window* button = mode->mDefeatDebriefing->getChild("Panel/ConfirmButton");
        const CEGUI::Vector2f points[4] = {CEGUI::Vector2f(5.0f, 5.0f), CEGUI::Vector2f(1900.0f, 1180.0f),
            CEGUI::Vector2f(960.0f, 20.0f), CEGUI::Vector2f(20.0f, 1100.0f)};
        for(int i = 0; i < 4; ++i)
            click(*mode, points[i]);
        check(gModeRequests == 0 && hitAt(centre(button)) == button, "clicks all around the stone keep the button reachable");
        click(*mode, centre(button));
        check(gModeRequests == 1, "and the button still leaves the game");
        closeDebriefing(mode, root);
    }
    // 4. The same scene with the stone rising on click, as the layout was before: the grey screen of the report
    {
        GameMode* mode = nullptr;
        CEGUI::Window* root = openDebriefing(mode);
        CEGUI::Window* background = mode->mDefeatDebriefing->getChild("Background");
        CEGUI::Window* panel = mode->mDefeatDebriefing->getChild("Panel");
        CEGUI::Window* button = mode->mDefeatDebriefing->getChild("Panel/ConfirmButton");
        background->setRiseOnClickEnabled(true);
        click(*mode, CEGUI::Vector2f(40.0f, 40.0f));
        check(background->isInFront(*panel) && hitAt(centre(button)) == background,
            "reproduced: with rise on click the stone covers the panel after one click beside it");
        click(*mode, centre(button));
        check(gModeRequests == 0, "reproduced: the covered confirm button cannot be clicked any more");
        closeDebriefing(mode, root);
    }
    // 5. Leaving the mode: windows are destroyed, pointers are cleared, connections are released
    {
        GameMode* mode = nullptr;
        CEGUI::Window* root = openDebriefing(mode);
        CEGUI::Window* debriefing = mode->mDefeatDebriefing;
        mode->destroyDefeatWindows();
        check(mode->mDefeatTint == nullptr && mode->mDefeatFade == nullptr && mode->mDefeatSubtitle == nullptr
            && mode->mDefeatCameraMarker == nullptr && mode->mDefeatDebriefing == nullptr, "destroying clears every defeat window pointer");
        check(debriefing->getParent() == nullptr, "the debriefing is taken off the game sheet");
        mode->hideInterfaceForDefeat();
        CEGUI::WindowManager::getSingleton().cleanDeadPool();
        check(!CEGUI::WindowManager::getSingleton().isAlive(debriefing), "the debriefing window is gone");
        delete mode;
        CEGUI::System::getSingleton().getDefaultGUIContext().setRootWindow(nullptr);
        CEGUI::WindowManager::getSingleton().destroyWindow(root);
    }

    CEGUI::System::destroy();
    delete provider;
    CEGUI::NullRenderer::destroy(renderer);
    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}

int main()
{
    try
    {
        return run();
    }
    catch(const CEGUI::Exception& error)
    {
        std::cout << "CEGUI exception: " << error.getMessage() << '\n';
    }
    return 2;
}
'''

constants = '\n'.join(constant(name) for name in ('DEFEAT_CAMERA_MARKER_IMAGE', 'DEFEAT_DEBRIEFING_ELAPSED'))
for marker, text in (
        ('@@REQUESTMAINMENU@@', function(mode_manager, 'void requestMainMenuWithSkirmishSubMenu(')),
        ('@@CONSUMEREQUEST@@', function(mode_manager, 'bool consumeSkirmishSubMenuRequest(')),
        ('@@CONSTANTS@@', constants),
        ('@@MOVED@@', handler_guard('bool GameMode::mouseMoved(')),
        ('@@PRESSED@@', handler_guard('bool GameMode::mousePressed(')),
        ('@@RELEASED@@', handler_guard('bool GameMode::mouseReleased(')),
        ('@@CREATE@@', function(game_mode, 'void GameMode::createDefeatWindows(')),
        ('@@DESTROY@@', function(game_mode, 'void GameMode::destroyDefeatWindows(')),
        ('@@HIDE@@', function(game_mode, 'void GameMode::hideInterfaceForDefeat(')),
        ('@@FINISHED@@', function(game_mode, 'void GameMode::onDefeatSequenceFinished(')),
        ('@@SHOW@@', function(game_mode, 'void GameMode::showDefeatDebriefing(')),
        ('@@FILL@@', function(game_mode, 'void GameMode::fillDefeatStatistics(')),
        ('@@CONFIRM@@', function(game_mode, 'bool GameMode::onClickDefeatDebriefingConfirm(')),
        ('@@GUI@@', (repo / 'gui').as_posix() + '/'),
        ('@@FONTS@@', (repo / 'gui/fonts').as_posix() + '/')):
    assert marker in probe, marker
    probe = probe.replace(marker, text)

# Static checks on the production sources.
root = ElementTree.parse(layout_path).getroot().find('Window')
children = root.findall('Window')
names = [child.get('name') for child in children]
assert names.index('Background') < names.index('Panel')
background = children[names.index('Background')]
properties = {prop.get('name'): prop.get('value') for prop in background.findall('Property')}
assert properties['Area'] == '{{0,0},{0,0},{1,0},{1,0}}' and properties.get('RiseOnClickEnabled') == 'False'
print('LAYOUT OK: the full screen stone below the panel does not rise when clicked')
# The confirm handler only requests; the mode is changed at the start of the next ModeManager::update
confirm = function(game_mode, 'bool GameMode::onClickDefeatDebriefingConfirm(')
assert 'requestMainMenuWithSkirmishSubMenu();' in confirm and 'destroy' not in confirm and 'delete' not in confirm
assert function(mode_manager, 'void requestMode(').count(';') == 2
assert mode_manager_source.count('checkModeChange()') == 2
assert function(mode_manager_source, 'void ModeManager::update(').startswith('void ModeManager::update(const Ogre::FrameEvent& evt)\n{\n    checkModeChange();')
print('WIRING OK: the button only sets requests, the mode change runs on the next frame before any input')
assert re.search(r'\bauto\b', probe) is None
print('STYLE OK: no auto in the fixture')
for path in (Path(__file__), layout_path):
    bad = [b for b in path.read_bytes() if b < 32 and b not in (9, 10, 13)]
    assert not bad, path
print('BYTES OK: no stray control characters')

with tempfile.TemporaryDirectory(prefix='odp-defeat-click-') as directory:
    work = Path(directory)
    header_dir = work / 'CEGUI/RendererModules/Null'
    header_dir.mkdir(parents=True)
    for header in null_include.glob('*.h'):
        text = header.read_text().replace('"../../', '"CEGUI/')
        (header_dir / header.name).write_text(text)
    sources = []
    for source in null_source.glob('*.cpp'):
        shutil.copy(source, work / source.name)
        sources.append(source.name)
    shutil.copy(null_source / 'RenderTarget.inl', work / 'RenderTarget.inl')
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/W1', '/DCEGUINULLRENDERER_EXPORTS',
                    '/I', str(work), '/I', str(cegui_include), '/I', str(repo / 'source'), 'check.cpp'] + sources +
                   ['/Fecheck.exe', '/link', '/LIBPATH:' + str(cegui_lib), 'CEGUIBase-0.lib'],
                   cwd=work, check=True)
    environment = dict(os.environ)
    environment['PATH'] = str(deps / 'install/bin') + os.pathsep + environment.get('PATH', '')
    # Windows may block a freshly compiled executable (WinError 4551) for a moment
    for attempt in range(4):
        try:
            subprocess.run([str(work / 'check.exe')], cwd=work, check=True, env=environment)
            break
        except OSError as error:
            if attempt == 3:
                raise
            print('retrying after', error)
            time.sleep(2)
