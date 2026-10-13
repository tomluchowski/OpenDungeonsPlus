/*
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

#include "MenuModeMain.h"

#include "modes/ModeManager.h"

#include "ODApplication.h"
#include "game/Campaign.h"
#include "modes/MenuModeCampaign.h"
#include "gamemap/GameMap.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "render/TextRenderer.h"
#include "sound/MusicPlayer.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

#include <CEGUI/widgets/PushButton.h>

// Main buttons
const std::string BUTTON_CAMPAIGN = "StartCampaignButton";
const std::string BUTTON_SKIRMISH = "StartSkirmishButton";
const std::string BUTTON_START_REPLAY = "StartReplayButton";
const std::string BUTTON_MAPEDITOR = "MapEditorButton";
const std::string BUTTON_MULTIPLAYER = "MultiplayerModeButton";
const std::string BUTTON_SETTINGS = "SettingsButton";
const std::string BUTTON_QUIT = "QuitButton";

// Sub-menus windows & buttons
const std::string WINDOW_CAMPAIGN = "CampaignSubMenuWindow";
const std::string WINDOW_SKIRMISH = "SkirmishSubMenuWindow";
const std::string WINDOW_MULTIPLAYER = "MultiplayerSubMenuWindow";
const std::string WINDOW_EDITOR = "EditorSubMenuWindow";
const std::string WINDOW_SETTINGS = "SettingsSubMenuWindow";

const std::string BUTTON_NEW_CAMPAIGN = "NewCampaignButton";
const std::string BUTTON_CONTINUE_CAMPAIGN = "ContinueCampaignButton";
const std::string BUTTON_NEW_CAMPAIGN_CONFIRM = "NewCampaignConfirmButton";
const std::string TEXT_NEW_CAMPAIGN_CONFIRM = "NewCampaignConfirmText";
const std::string BUTTON_START_SKIRMISH = "StartSkirmishButton";
const std::string BUTTON_LOAD_SKIRMISH = "LoadSkirmishButton";
const std::string BUTTON_MASTERSERVER_JOIN = "MasterServerJoinButton";
const std::string BUTTON_MASTERSERVER_HOST = "MasterServerHostButton";
const std::string BUTTON_MULTIPLAYER_JOIN = "MultiplayerServerJoinButton";
const std::string BUTTON_MULTIPLAYER_HOST = "MultiplayerServerHostButton";
const std::string BUTTON_EDITOR_NEW = "EditorNewButton";
const std::string BUTTON_EDITOR_LOAD = "EditorLoadButton";

namespace
{
//! \brief Helper functor to change modes
class ModeChanger
{
public:
    bool operator()(const CEGUI::EventArgs& e)
    {
        mMode->changeModeEvent(mNewMode, e);
        return true;
    }

    MenuModeMain* mMode;
    AbstractModeManager::ModeType mNewMode;
};
} // namespace

MenuModeMain::MenuModeMain(ModeManager *modeManager):
    AbstractApplicationMode(modeManager, ModeManager::MENU_MAIN),
    mSettings(getModeManager().getGui().getGuiSheet(Gui::mainMenu), modeManager->getGui(), true)
{
    CEGUI::Window* rootWin = getModeManager().getGui().getGuiSheet(Gui::mainMenu);
    OD_ASSERT_TRUE(rootWin != nullptr);

    connectModeChangeEvent(BUTTON_START_REPLAY, AbstractModeManager::ModeType::MENU_REPLAY);

    connectModeChangeEvent(rootWin->getChild(BUTTON_QUIT),
                           AbstractModeManager::ModeType::ADVERTISMENT);
    addEventConnection(
        rootWin->getChild(BUTTON_SETTINGS)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeMain::toggleSettings, this)
        )
    );

    // Campaign & sub-menu events
    addEventConnection(
        rootWin->getChild(BUTTON_CAMPAIGN)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeMain::toggleCampaignSubMenu, this)
        )
    );
    CEGUI::Window* campaignWin = rootWin->getChild(WINDOW_CAMPAIGN);
    OD_ASSERT_TRUE(campaignWin != nullptr);
    addEventConnection(
        campaignWin->getChild(BUTTON_NEW_CAMPAIGN)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeMain::newCampaignPressed, this)
        )
    );
    addEventConnection(
        campaignWin->getChild(BUTTON_NEW_CAMPAIGN_CONFIRM)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeMain::newCampaignConfirmed, this)
        )
    );
    connectModeChangeEvent(campaignWin->getChild(BUTTON_CONTINUE_CAMPAIGN),
                           AbstractModeManager::ModeType::MENU_CAMPAIGN);

    // Skirmish & sub-menu events
    addEventConnection(
        rootWin->getChild(BUTTON_SKIRMISH)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeMain::toggleSkirmishSubMenu, this)
        )
    );
    CEGUI::Window* skirmishWin = rootWin->getChild(WINDOW_SKIRMISH);
    OD_ASSERT_TRUE(skirmishWin != nullptr);
    connectModeChangeEvent(skirmishWin->getChild(BUTTON_START_SKIRMISH),
                           AbstractModeManager::ModeType::MENU_SKIRMISH);
    connectModeChangeEvent(skirmishWin->getChild(BUTTON_LOAD_SKIRMISH),
                           AbstractModeManager::ModeType::MENU_LOAD_SAVEDGAME);

    // Multiplayer & sub-menu events
    addEventConnection(
        rootWin->getChild(BUTTON_MULTIPLAYER)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeMain::toggleMultiplayerSubMenu, this)
        )
    );
    CEGUI::Window* multiplayerWin = rootWin->getChild(WINDOW_MULTIPLAYER);
    OD_ASSERT_TRUE(multiplayerWin != nullptr);
    connectModeChangeEvent(multiplayerWin->getChild(BUTTON_MASTERSERVER_JOIN),
                           AbstractModeManager::ModeType::MENU_MASTERSERVER_JOIN);
    connectModeChangeEvent(multiplayerWin->getChild(BUTTON_MASTERSERVER_HOST),
                           AbstractModeManager::ModeType::MENU_MASTERSERVER_HOST);
    connectModeChangeEvent(multiplayerWin->getChild(BUTTON_MULTIPLAYER_JOIN),
                           AbstractModeManager::ModeType::MENU_MULTIPLAYER_CLIENT);
    connectModeChangeEvent(multiplayerWin->getChild(BUTTON_MULTIPLAYER_HOST),
                           AbstractModeManager::ModeType::MENU_MULTIPLAYER_SERVER);

    // Editor & sub-menu events
    addEventConnection(
        rootWin->getChild(BUTTON_MAPEDITOR)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeMain::toggleEditorSubMenu, this)
        )
    );
    CEGUI::Window* editorWin = rootWin->getChild(WINDOW_EDITOR);
    OD_ASSERT_TRUE(editorWin != nullptr);
    connectModeChangeEvent(editorWin->getChild(BUTTON_EDITOR_NEW),
                           AbstractModeManager::ModeType::MENU_EDITOR_NEW);
    connectModeChangeEvent(editorWin->getChild(BUTTON_EDITOR_LOAD),
                           AbstractModeManager::ModeType::MENU_EDITOR_LOAD);

    for(const std::string& page : {std::string("Video"), std::string("Audio"), std::string("Input"), std::string("Game")})
        addEventConnection(rootWin->getChild(WINDOW_SETTINGS + "/" + page + "Button")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber([this, page](const CEGUI::EventArgs&) { return openSettingsPage(page); })));
    addEventConnection(rootWin->getChild("SettingsWindow")->subscribeEvent(
        CEGUI::Window::EventHidden,
        CEGUI::Event::Subscriber(&MenuModeMain::settingsPageClosed, this)));

    for(const std::string& name : {WINDOW_CAMPAIGN, WINDOW_SKIRMISH, WINDOW_MULTIPLAYER, WINDOW_EDITOR, WINDOW_SETTINGS})
        addEventConnection(
            rootWin->getChild(name + "/BackButton")->subscribeEvent(
                CEGUI::PushButton::EventClicked,
                CEGUI::Event::Subscriber(&MenuModeMain::goBack, this)
            )
        );
}

void MenuModeMain::activate()
{
    // Loads the corresponding Gui sheet.
    getModeManager().getGui().loadGuiSheet(Gui::mainMenu);
    CEGUI::Window* window = getModeManager().getGui().getGuiSheet(Gui::mainMenu);
    OD_ASSERT_TRUE(window != nullptr);

    window->getChild(WINDOW_CAMPAIGN)->hide();
    showNewCampaignConfirm(false);
    window->getChild(WINDOW_SKIRMISH)->hide();
    window->getChild(WINDOW_MULTIPLAYER)->hide();
    window->getChild(WINDOW_EDITOR)->hide();
    window->getChild(WINDOW_SETTINGS)->hide();
    mSettingsPageOpen = false;
    mSkirmishSubMenuPending = false;
    showMainMenuButtons(true);
    // Coming from the defeat debriefing: fly into the menu, then go on in the skirmish sub-menu
    if(getModeManager().consumeSkirmishSubMenuRequest())
    {
        mSkirmishSubMenuPending = true;
        showMainMenuButtons(false);
    }

    // Reaching the main menu ends the campaign flow. The campaign buttons
    // depend on the campaign definition and the saved progress.
    Campaign::getSingleton().stopCampaign();
    bool campaignAvailable = MenuModeCampaign::loadCampaign();
    window->getChild(BUTTON_CAMPAIGN)->setDisabled(!campaignAvailable);
    window->getChild(WINDOW_CAMPAIGN)->getChild(BUTTON_CONTINUE_CAMPAIGN)->setDisabled(
        !Campaign::getSingleton().hasProgress());

    giveFocus();

    TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");

    // Play the main menu music
    MusicPlayer::getSingleton().play(ConfigManager::getSingleton().getMainMenuMusic());

    GameMap* gameMap = ODFrameListener::getSingletonPtr()->getClientGameMap();
    gameMap->clearAll();
    gameMap->setGamePaused(true);

    ODFrameListener::getSingleton().stopGameRenderer();
    ODFrameListener::getSingleton().createMainMenuScene();
    if(mSkirmishSubMenuPending)
        ODFrameListener::getSingleton().startMainMenuFlight();
}

void MenuModeMain::onFrameStarted(const Ogre::FrameEvent& /*evt*/)
{
    if(!mSkirmishSubMenuPending)
        return;
    if(ODFrameListener::getSingleton().isMainMenuFlightActive())
        return;

    mSkirmishSubMenuPending = false;
    toggleSubMenu(WINDOW_SKIRMISH);
}

void MenuModeMain::connectModeChangeEvent(const std::string& buttonName, AbstractModeManager::ModeType mode)
{
    CEGUI::Window* window = getModeManager().getGui().getGuiSheet(Gui::mainMenu);
    connectModeChangeEvent(window->getChild(buttonName), mode);
}

void MenuModeMain::connectModeChangeEvent(CEGUI::Window* button, AbstractModeManager::ModeType mode)
{
    OD_ASSERT_TRUE(button != nullptr);
    addEventConnection(
        button->subscribeEvent(
          CEGUI::PushButton::EventClicked,
          CEGUI::Event::Subscriber(ModeChanger{this, mode})
        )
    );
}

bool MenuModeMain::quitButtonPressed(const CEGUI::EventArgs&)
{
    ODFrameListener::getSingletonPtr()->requestExit();
    return true;
}

bool MenuModeMain::toggleSettings(const CEGUI::EventArgs&)
{
    return toggleSubMenu(WINDOW_SETTINGS);
}

bool MenuModeMain::openSettingsPage(const std::string& name)
{
    getModeManager().getGui().getGuiSheet(Gui::mainMenu)->getChild(WINDOW_SETTINGS)->hide();
    mSettingsPageOpen = true;
    mSettings.showPage(name);
    return true;
}

bool MenuModeMain::settingsPageClosed(const CEGUI::EventArgs&)
{
    if(mSettingsPageOpen)
    {
        mSettingsPageOpen = false;
        getModeManager().getGui().getGuiSheet(Gui::mainMenu)->getChild(WINDOW_SETTINGS)->show();
    }
    return true;
}

bool MenuModeMain::goBack(const CEGUI::EventArgs&)
{
    CEGUI::Window* mainWin = getModeManager().getGui().getGuiSheet(Gui::mainMenu);
    for(const std::string& name : {WINDOW_CAMPAIGN, WINDOW_SKIRMISH, WINDOW_MULTIPLAYER, WINDOW_EDITOR, WINDOW_SETTINGS})
    {
        CEGUI::Window* window = mainWin->getChild(name);
        if(window->isVisible())
        {
            window->hide();
            showNewCampaignConfirm(false);
            showMainMenuButtons(true);
            break;
        }
    }
    return true;
}

void MenuModeMain::showMainMenuButtons(bool visible)
{
    CEGUI::Window* mainWin = getModeManager().getGui().getGuiSheet(Gui::mainMenu);
    for(const std::string& name : {std::string("StartCampaignButton"), BUTTON_SKIRMISH,
        BUTTON_MULTIPLAYER, BUTTON_START_REPLAY, BUTTON_MAPEDITOR, BUTTON_SETTINGS, BUTTON_QUIT})
        mainWin->getChild(name)->setVisible(visible);
}

bool MenuModeMain::toggleCampaignSubMenu(const CEGUI::EventArgs&)
{
    return toggleSubMenu(WINDOW_CAMPAIGN);
}

bool MenuModeMain::newCampaignPressed(const CEGUI::EventArgs& e)
{
    // A new campaign has to be confirmed: the cross (back) returns
    // to the previous screen and the tick starts it. Without saved progress there is
    // nothing to lose, so it starts at once.
    if(Campaign::getSingleton().hasProgress())
    {
        showNewCampaignConfirm(true);
        return true;
    }
    return newCampaignConfirmed(e);
}

bool MenuModeMain::newCampaignConfirmed(const CEGUI::EventArgs& e)
{
    // A new campaign forgets the saved progress
    Campaign::getSingleton().resetProgress();
    changeModeEvent(AbstractModeManager::ModeType::MENU_CAMPAIGN, e);
    return true;
}

void MenuModeMain::showNewCampaignConfirm(bool visible)
{
    CEGUI::Window* campaignWin = getModeManager().getGui().getGuiSheet(Gui::mainMenu)->getChild(WINDOW_CAMPAIGN);
    campaignWin->getChild(BUTTON_NEW_CAMPAIGN)->setVisible(!visible);
    campaignWin->getChild(BUTTON_CONTINUE_CAMPAIGN)->setVisible(!visible);
    campaignWin->getChild(TEXT_NEW_CAMPAIGN_CONFIRM)->setVisible(visible);
    campaignWin->getChild(BUTTON_NEW_CAMPAIGN_CONFIRM)->setVisible(visible);
}

bool MenuModeMain::toggleSkirmishSubMenu(const CEGUI::EventArgs&)
{
    return toggleSubMenu(WINDOW_SKIRMISH);
}

bool MenuModeMain::toggleMultiplayerSubMenu(const CEGUI::EventArgs&)
{
    return toggleSubMenu(WINDOW_MULTIPLAYER);
}

bool MenuModeMain::toggleEditorSubMenu(const CEGUI::EventArgs&)
{
    return toggleSubMenu(WINDOW_EDITOR);
}

bool MenuModeMain::toggleSubMenu(const std::string& name)
{
    CEGUI::Window* mainWin = getModeManager().getGui().getGuiSheet(Gui::mainMenu);
    OD_ASSERT_TRUE(mainWin);
    const bool visible = !mainWin->getChild(name)->isVisible();
    for(const std::string& other : {WINDOW_CAMPAIGN, WINDOW_SKIRMISH, WINDOW_MULTIPLAYER, WINDOW_EDITOR, WINDOW_SETTINGS})
        mainWin->getChild(other)->setVisible(visible && other == name);
    showMainMenuButtons(!visible);
    return true;
}
