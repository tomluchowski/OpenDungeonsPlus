/*
 * \file   Gui.cpp
 * \date   05 April 2011
 * \author StefanP.MUC
 * \brief  Class Gui containing all the stuff for the GUI, including translation.
 *
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "render/Gui.h"

#include "ODApplication.h"
#include "sound/SoundEffectsManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

#include <CEGUI/CEGUI.h>
#include <CEGUI/BasicImage.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/RendererModules/Ogre/ResourceProvider.h>
#include <CEGUI/RendererModules/Ogre/ImageCodec.h>
#include <CEGUI/SchemeManager.h>
#include <CEGUI/System.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/TabControl.h>
#include <CEGUI/Event.h>

#include <algorithm>
#include <sstream>

namespace
{
const float LAYOUT_DESIGN_WIDTH = 1024.0f;
const float LAYOUT_DESIGN_HEIGHT = 768.0f;
const float FONT_DESIGN_WIDTH = 800.0f;
const float FONT_DESIGN_HEIGHT = 600.0f;

void scaleDimension(CEGUI::UDim& dimension, float scale)
{
    dimension.d_offset *= scale;
}

CEGUI::URect scaleRect(const CEGUI::URect& rect, float scale)
{
    CEGUI::URect scaled(rect);
    scaleDimension(scaled.d_min.d_x, scale);
    scaleDimension(scaled.d_min.d_y, scale);
    scaleDimension(scaled.d_max.d_x, scale);
    scaleDimension(scaled.d_max.d_y, scale);
    return scaled;
}

CEGUI::USize scaleSize(const CEGUI::USize& size, float scale)
{
    CEGUI::USize scaled(size);
    scaleDimension(scaled.d_width, scale);
    scaleDimension(scaled.d_height, scale);
    return scaled;
}

bool shouldScaleImage(const CEGUI::String& ceguiName)
{
    const std::string name(ceguiName.c_str());
    return name.compare(0, 17, "OpenDungeonsSkin/") == 0
        || name.compare(0, 18, "OpenDungeonsIcons/") == 0
        || name.compare(0, 17, "ODMainMenuButton/") == 0
        || name.compare(0, 7, "ODLogo/") == 0;
}

std::string scaleFormattedImageSizes(const CEGUI::String& ceguiText, float scale)
{
    std::string text(ceguiText.c_str());
    size_t tagStart = 0;
    while((tagStart = text.find("[image-size='", tagStart)) != std::string::npos)
    {
        size_t tagEnd = text.find("']", tagStart);
        if(tagEnd == std::string::npos)
            break;

        const char* dimensions[] = {"w:", "h:"};
        for(const char* dimension : dimensions)
        {
            const size_t marker = text.find(dimension, tagStart);
            if(marker == std::string::npos || marker >= tagEnd)
                continue;

            const size_t valueStart = marker + 2;
            size_t valueEnd = valueStart;
            while(valueEnd < tagEnd && ((text[valueEnd] >= '0' && text[valueEnd] <= '9') || text[valueEnd] == '.'))
                ++valueEnd;

            float value = 0.0f;
            std::istringstream valueParser(text.substr(valueStart, valueEnd - valueStart));
            if(!(valueParser >> value))
                continue;

            std::ostringstream scaledValue;
            scaledValue << std::max(1, static_cast<int>(value * scale + 0.5f));
            text.replace(valueStart, valueEnd - valueStart, scaledValue.str());
            tagEnd = text.find("']", tagStart);
        }

        tagStart = tagEnd + 2;
    }
    return text;
}
}

Gui::Gui(SoundEffectsManager* soundEffectsManager, const std::string& ceguiLogFileName, Ogre::RenderTarget &renderTarget)
  : mUserScale(1.0f),
    mSoundEffectsManager(soundEffectsManager)
{
    OD_LOG_INF("*** Initializing CEGUI ***");
    CEGUI::OgreRenderer& renderer = CEGUI::OgreRenderer::create(renderTarget);
    OD_LOG_INF("OgreRenderer created");
    CEGUI::OgreResourceProvider& rp = CEGUI::OgreRenderer::createOgreResourceProvider();
    OD_LOG_INF("OgreResourceProvider created");
    CEGUI::OgreImageCodec& ic = CEGUI::OgreRenderer::createOgreImageCodec();
    OD_LOG_INF("OgreImageCodec created");
    CEGUI::System::create(renderer, &rp, static_cast<CEGUI::XMLParser*>(nullptr), &ic, nullptr, "",
                          reinterpret_cast<const CEGUI::utf8*>(ceguiLogFileName.c_str()));
    OD_LOG_INF("CEGUI::System created");

    CEGUI::SchemeManager::getSingleton().createFromFile("ODSkin.scheme");
    OD_LOG_INF("CEGUI::SchemeManager created");

    float configuredScalePercent = 100.0f;
    std::istringstream scaleParser(ConfigManager::getSingleton().getGameValue(Config::UI_SCALE, "100", false));
    if(!(scaleParser >> configuredScalePercent))
        configuredScalePercent = 100.0f;
    configuredScalePercent = std::max(static_cast<float>(MIN_UI_SCALE_PERCENT),
        std::min(static_cast<float>(MAX_UI_SCALE_PERCENT), configuredScalePercent));
    mUserScale = configuredScalePercent / 100.0f;
    updateResourceScaling(renderer.getDisplaySize());

    // We want Ogre overlays to be displayed in front of CEGUI. According to
    // http://cegui.org.uk/forum/viewtopic.php?f=10&t=5694
    // the best way is to disable CEGUI auto rendering by calling setFrameControlExecutionEnabled
    // and render CEGUI in an Ogre::RenderQueueListener (done in ODFrameListener) by calling
    // CEGUI::System::getSingleton().renderAllGUIContexts();
    renderer.setFrameControlExecutionEnabled(false);

    // Needed to get the correct offset when using up to CEGUI 0.8.4
    // We're thus using an empty mouse cursor.
    CEGUI::GUIContext& context = CEGUI::System::getSingleton().getDefaultGUIContext();
    context.getMouseCursor().setDefaultImage("OpenDungeonsSkin/MouseArrow");
    context.getMouseCursor().setVisible(true);
    context.setDefaultTooltipType("OD/Tooltip");
    CEGUI::WindowManager* wmgr = CEGUI::WindowManager::getSingletonPtr();
    mSheets[hideGui] = wmgr->createWindow("DefaultWindow", "DummyWindow");
    mSheets[inGameMenu] = wmgr->loadLayoutFromFile("ModeGame.layout");
    mSheets[advertisment] = wmgr->loadLayoutFromFile("Advertisment.layout");
    mSheets[mainMenu] = wmgr->loadLayoutFromFile("MenuMain.layout");
    mSheets[skirmishMenu] = wmgr->loadLayoutFromFile("MenuSkirmish.layout");
    mSheets[multiplayerClientMenu] = wmgr->loadLayoutFromFile("MenuMultiplayerClient.layout");
    mSheets[multiplayerServerMenu] = wmgr->loadLayoutFromFile("MenuMultiplayerServer.layout");
    mSheets[multiMasterServerJoinMenu] = wmgr->loadLayoutFromFile("MenuMasterServerJoin.layout");
    mSheets[editorModeGui] =  wmgr->loadLayoutFromFile("ModeEditor.layout");
    mSheets[editorNewMenu] =  wmgr->loadLayoutFromFile("MenuEditorNew.layout");
    mSheets[editorLoadMenu] =  wmgr->loadLayoutFromFile("MenuEditorLoad.layout");
    mSheets[configureSeats] =  wmgr->loadLayoutFromFile("MenuConfigureSeats.layout");
    mSheets[replayMenu] =  wmgr->loadLayoutFromFile("MenuReplay.layout");
    mSheets[loadSavedGameMenu] =  wmgr->loadLayoutFromFile("MenuLoad.layout");
    mSheets[console] = wmgr->loadLayoutFromFile("WindowConsole.layout");

    mDisplaySizeChangedConnection = CEGUI::System::getSingleton().subscribeEvent(
        CEGUI::System::EventDisplaySizeChanged,
        CEGUI::Event::Subscriber(&Gui::onDisplaySizeChanged, this));
    mWindowDestroyedConnection = wmgr->subscribeEvent(
        CEGUI::WindowManager::EventWindowDestroyed,
        CEGUI::Event::Subscriber(&Gui::onWindowDestroyed, this));

    for(const auto& sheet : mSheets)
        registerWindow(sheet.second);
    applyScale(renderer.getDisplaySize());

    // Set the game version
    mSheets[mainMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[skirmishMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[multiplayerServerMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[multiplayerClientMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[editorNewMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[editorLoadMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[replayMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[loadSavedGameMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[multiMasterServerJoinMenu]->getChild("VersionText")->setText(ODApplication::VERSION);

    // Add sound to button clicks
    CEGUI::GlobalEventSet& ges = CEGUI::GlobalEventSet::getSingleton();
    ges.subscribeEvent(
        CEGUI::PushButton::EventNamespace + "/" + CEGUI::PushButton::EventClicked,
        CEGUI::Event::Subscriber(&Gui::playButtonClickSound, this));
}

Gui::~Gui()
{
    mDisplaySizeChangedConnection.disconnect();
    mWindowDestroyedConnection.disconnect();
    //This also calls CEGUI::System::destroy();
    CEGUI::OgreRenderer::destroySystem();
}

CEGUI::MouseButton Gui::convertButton(OIS::MouseButtonID buttonID)
{
    //OIS has 2 more button ids than CEGUI.
    if(static_cast<int>(buttonID) < static_cast<int>(CEGUI::MouseButton::MouseButtonCount))
        return static_cast<CEGUI::MouseButton>(buttonID);
    return CEGUI::MouseButton::NoButton;
}

void Gui::loadGuiSheet(guiSheet newSheet)
{
    registerWindowHierarchy(mSheets[newSheet]);
    CEGUI::System::getSingletonPtr()->getDefaultGUIContext().setRootWindow(mSheets[newSheet]);
    // This shouldn't be needed, but the gui seems to not allways change when using hideGui without it.
    CEGUI::System::getSingletonPtr()->getDefaultGUIContext().markAsDirty();
}

void Gui::registerWindowHierarchy(CEGUI::Window* window)
{
    if(window == nullptr)
        return;

    registerWindow(window);
    applyScale(CEGUI::System::getSingleton().getRenderer()->getDisplaySize());
}

void Gui::registerWindow(CEGUI::Window* window)
{
    if(!window->isAutoWindow() && mScaledWindows.find(window) == mScaledWindows.end())
    {
        WindowScaleData data;
        data.area = window->getArea();
        data.minSize = window->getMinSize();
        data.maxSize = window->getMaxSize();
        data.text = window->getText();
        data.hasFormattedImageSize = std::string(data.text.c_str()).find("[image-size='") != std::string::npos;
        data.hasTabHeight = false;

        CEGUI::TabControl* tabControl = dynamic_cast<CEGUI::TabControl*>(window);
        if(tabControl != nullptr)
        {
            data.tabHeight = tabControl->getTabHeight();
            data.hasTabHeight = true;
        }

        mScaledWindows.insert(std::make_pair(window, data));
    }

    for(size_t i = 0; i < window->getChildCount(); ++i)
        registerWindow(window->getChildAtIdx(i));
}

void Gui::setUserScalePercent(float scalePercent)
{
    if(scalePercent != scalePercent)
        scalePercent = 100.0f;

    scalePercent = std::max(static_cast<float>(MIN_UI_SCALE_PERCENT),
        std::min(static_cast<float>(MAX_UI_SCALE_PERCENT), scalePercent));
    mUserScale = scalePercent / 100.0f;
    applyScale(CEGUI::System::getSingleton().getRenderer()->getDisplaySize());
}

bool Gui::onDisplaySizeChanged(const CEGUI::EventArgs& e)
{
    const CEGUI::DisplayEventArgs& displayEvent = static_cast<const CEGUI::DisplayEventArgs&>(e);
    applyScale(displayEvent.size);
    return true;
}

bool Gui::onWindowDestroyed(const CEGUI::EventArgs& e)
{
    const CEGUI::WindowEventArgs& windowEvent = static_cast<const CEGUI::WindowEventArgs&>(e);
    mScaledWindows.erase(windowEvent.window);
    return true;
}

void Gui::applyScale(const CEGUI::Sizef& displaySize)
{
    const float resolutionScale = std::min(displaySize.d_width / LAYOUT_DESIGN_WIDTH,
        displaySize.d_height / LAYOUT_DESIGN_HEIGHT);
    const float scale = resolutionScale * mUserScale;

    updateResourceScaling(displaySize);

    for(const auto& scaledWindow : mScaledWindows)
        applyScale(scaledWindow.first, scaledWindow.second, scale);

    CEGUI::System::getSingleton().getDefaultGUIContext().markAsDirty();
}

void Gui::applyScale(CEGUI::Window* window, const WindowScaleData& data, float scale)
{
    window->setMinSize(scaleSize(data.minSize, scale));
    window->setMaxSize(scaleSize(data.maxSize, scale));
    window->setArea(scaleRect(data.area, scale));

    if(data.hasFormattedImageSize)
        window->setText(scaleFormattedImageSizes(data.text, scale));

    if(data.hasTabHeight)
    {
        CEGUI::UDim tabHeight(data.tabHeight);
        scaleDimension(tabHeight, scale);
        static_cast<CEGUI::TabControl*>(window)->setTabHeight(tabHeight);
    }
}

void Gui::updateResourceScaling(const CEGUI::Sizef& displaySize)
{
    const CEGUI::Sizef fontNativeResolution(FONT_DESIGN_WIDTH / mUserScale,
        FONT_DESIGN_HEIGHT / mUserScale);
    CEGUI::FontManager::FontIterator font = CEGUI::FontManager::getSingleton().getIterator();
    while(!font.isAtEnd())
    {
        font.getCurrentValue()->setNativeResolution(fontNativeResolution);
        font.getCurrentValue()->setAutoScaled(CEGUI::ASM_Min);
        ++font;
    }

    const CEGUI::Sizef imageNativeResolution(LAYOUT_DESIGN_WIDTH / mUserScale,
        LAYOUT_DESIGN_HEIGHT / mUserScale);
    CEGUI::ImageManager::ImageIterator image = CEGUI::ImageManager::getSingleton().getIterator();
    while(!image.isAtEnd())
    {
        if(shouldScaleImage(image.getCurrentKey()))
        {
            CEGUI::BasicImage* basicImage = dynamic_cast<CEGUI::BasicImage*>(image.getCurrentValue().first);
            if(basicImage != nullptr)
            {
                basicImage->setNativeResolution(imageNativeResolution);
                basicImage->setAutoScaled(CEGUI::ASM_Min);
            }
        }
        ++image;
    }

    CEGUI::FontManager::getSingleton().notifyDisplaySizeChanged(displaySize);
    CEGUI::ImageManager::getSingleton().notifyDisplaySizeChanged(displaySize);
}

CEGUI::Window* Gui::getGuiSheet(guiSheet sheet)
{
    auto it = mSheets.find(sheet);
    if(it != mSheets.end())
    {
        return it->second;
    }

    return nullptr;
}

void Gui::setRenderTarget(Ogre::RenderTarget& renderTarget)
{
    static_cast<CEGUI::OgreRenderer*>(CEGUI::System::getSingleton().getRenderer())
        ->setDefaultRootRenderTarget(renderTarget);
}

bool Gui::playButtonClickSound(const CEGUI::EventArgs&)
{
    mSoundEffectsManager->playRelativeSound(SoundRelativeInterface::Click);
    return true;
}

/* These constants are used to access the GUI element
 * NOTE: when add/remove/rename a GUI element, don't forget to change it here
 */
const std::string Gui::DISPLAY_GOLD = "HorizontalPipe/GoldDisplay";
const std::string Gui::DISPLAY_MANA = "HorizontalPipe/ManaDisplay";
const std::string Gui::DISPLAY_TERRITORY = "HorizontalPipe/TerritoryDisplay";
const std::string Gui::DISPLAY_CREATURES = "HorizontalPipe/CreaturesDisplay";
const std::string Gui::MINIMAP = "MiniMap";
const std::string Gui::OBJECTIVE_TEXT = "ObjectivesWindow/ObjectivesText";
const std::string Gui::MAIN_TABCONTROL = "MainTabControl";
const std::string Gui::TAB_ROOMS = "MainTabControl/Rooms";
const std::string Gui::BUTTON_TEMPLE = "MainTabControl/Rooms/TempleButton";
const std::string Gui::BUTTON_PORTAL = "MainTabControl/Rooms/PortalButton";
const std::string Gui::BUTTON_PORTAL_WAVE = "MainTabControl/Rooms/WavePortalButton";
const std::string Gui::BUTTON_DESTROY_ROOM = "MainTabControl/Rooms/DestroyRoomButton";
const std::string Gui::TAB_TRAPS = "MainTabControl/Traps";
const std::string Gui::BUTTON_DESTROY_TRAP = "MainTabControl/Traps/DestroyTrapButton";
const std::string Gui::TAB_SPELLS = "MainTabControl/Spells";
const std::string Gui::TAB_CREATURES = "MainTabControl/Creatures";
const std::string Gui::BUTTON_CREATURE_WORKER = "MainTabControl/Creatures/WorkerButton";
const std::string Gui::BUTTON_CREATURE_FIGHTER = "MainTabControl/Creatures/FighterButton";
const std::string Gui::TAB_COMBAT = "MainTabControl/Combat";

const std::string Gui::MM_BACKGROUND = "Background";
const std::string Gui::MM_WELCOME_MESSAGE = "WelcomeBanner";
const std::string Gui::EXIT_CONFIRMATION_POPUP = "ConfirmExit";
const std::string Gui::EXIT_CONFIRMATION_POPUP_YES_BUTTON = "ConfirmExit/YesOption";
const std::string Gui::EXIT_CONFIRMATION_POPUP_NO_BUTTON = "ConfirmExit/NoOption";

const std::string Gui::SKM_TEXT_LOADING = "LoadingText";
const std::string Gui::SKM_BUTTON_LAUNCH = "LevelWindowFrame/LaunchGameButton";
const std::string Gui::SKM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::SKM_LIST_LEVEL_TYPES = "LevelWindowFrame/LevelTypeSelect";
const std::string Gui::SKM_LIST_LEVELS = "LevelWindowFrame/LevelSelect";

const std::string Gui::MPM_TEXT_LOADING = "LoadingText";
const std::string Gui::MPM_BUTTON_SERVER = "LevelWindowFrame/ServerButton";
const std::string Gui::MPM_BUTTON_CLIENT = "LevelWindowFrame/ClientButton";
const std::string Gui::MPM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::MPM_LIST_LEVELS = "LevelWindowFrame/LevelSelect";
const std::string Gui::MPM_EDIT_IP = "LevelWindowFrame/IpEdit";
const std::string Gui::MPM_EDIT_NICK = "LevelWindowFrame/NickEdit";

const std::string Gui::EDM_TEXT_LOADING = "LoadingText";
const std::string Gui::EDM_BUTTON_LAUNCH = "LevelWindowFrame/LaunchEditorButton";
const std::string Gui::EDM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::EDM_LIST_LEVELS = "LevelWindowFrame/LevelSelect";
const std::string Gui::EDM_LIST_LEVEL_TYPES = "LevelWindowFrame/LevelTypeSelect";

const std::string Gui::EDITOR = "MainTabControl";
const std::string Gui::EDITOR_LAVA_BUTTON = "MainTabControl/Tiles/LavaButton";
const std::string Gui::EDITOR_GOLD_BUTTON = "MainTabControl/Tiles/GoldButton";
const std::string Gui::EDITOR_DIRT_BUTTON = "MainTabControl/Tiles/DirtButton";
const std::string Gui::EDITOR_WATER_BUTTON = "MainTabControl/Tiles/WaterButton";
const std::string Gui::EDITOR_ROCK_BUTTON = "MainTabControl/Tiles/RockButton";
const std::string Gui::EDITOR_CLAIMED_BUTTON = "MainTabControl/Tiles/ClaimedButton";
const std::string Gui::EDITOR_GEM_BUTTON = "MainTabControl/Tiles/GemButton";
const std::string Gui::EDITOR_FULLNESS = "HorizontalPipe/FullnessDisplay";
const std::string Gui::EDITOR_CURSOR_POS = "HorizontalPipe/PositionDisplay";
const std::string Gui::EDITOR_SEAT_ID = "HorizontalPipe/SeatIdDisplay";
const std::string Gui::EDITOR_CREATURE_SPAWN = "HorizontalPipe/CreatureSpawnDisplay";
const std::string Gui::EDITOR_LEVEL_NAME = "LevelNameDisplay";
const std::string Gui::EDITOR_MAPLIGHT_BUTTON = "MainTabControl/Lights/MapLightButton";

const std::string Gui::REM_TEXT_LOADING = "LoadingText";
const std::string Gui::REM_BUTTON_LAUNCH = "LevelWindowFrame/LaunchReplayButton";
const std::string Gui::REM_BUTTON_DELETE = "LevelWindowFrame/DeleteReplayButton";
const std::string Gui::REM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::REM_LIST_REPLAYS = "LevelWindowFrame/ReplaySelect";
