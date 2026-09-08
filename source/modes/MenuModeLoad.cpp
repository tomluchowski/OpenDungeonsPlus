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

#include "modes/MenuModeLoad.h"

#include "utils/Helper.h"
#include "render/Gui.h"
#include "modes/ModeManager.h"
#include "sound/MusicPlayer.h"
#include "gamemap/GameMap.h"
#include "render/ODFrameListener.h"
#include "network/ODServer.h"
#include "network/ODClient.h"
#include "network/ServerMode.h"
#include "network/ServerNotification.h"
#include "utils/LogManager.h"
#include "gamemap/MapHandler.h"
#include "utils/ConfigManager.h"
#include "utils/ResourceManager.h"

#include <CEGUI/CEGUI.h>
#include <CEGUI/widgets/Scrollbar.h>
#include "boost/filesystem.hpp"

const std::string SAVEGAME_EXTENSION = ".level";

MenuModeLoad::MenuModeLoad(ModeManager *modeManager, bool inGame, const std::string& savedGame):
    AbstractApplicationMode(modeManager, ModeManager::MENU_LOAD_SAVEDGAME),
    mInGame(inGame),
    mSavedGame(savedGame)
{
    CEGUI::Window* window = modeManager->getGui().getGuiSheet(Gui::guiSheet::loadSavedGameMenu);
    addEventConnection(
        window->getChild("LevelWindowFrame/BackButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeLoad::closeBrowser, this)
        )
    );
    addEventConnection(
        window->getChild("LevelWindowFrame/LaunchButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeLoad::launchSelectedButtonPressed, this)
        )
    );
    addEventConnection(
        window->getChild("LevelWindowFrame/DeleteButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeLoad::deleteSelectedButtonPressed, this)
        )
    );
    addEventConnection(
        window->getChild("LevelWindowFrame/SaveGameSelect")->subscribeEvent(
            CEGUI::Listbox::EventMouseClick,
            CEGUI::Event::Subscriber(&MenuModeLoad::updateDescription, this)
        )
    );
    addEventConnection(
        window->getChild("LevelWindowFrame/SaveGameSelect")->subscribeEvent(
            CEGUI::Listbox::EventMouseDoubleClick,
            CEGUI::Event::Subscriber(&MenuModeLoad::launchSelectedButtonPressed, this)
        )
    );
    addEventConnection(
        window->getChild("LevelWindowFrame")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&MenuModeLoad::closeBrowser, this)
        )
    );
}

MenuModeLoad::~MenuModeLoad()
{
    if(isOpenInGame())
        closeBrowser();
}

bool MenuModeLoad::closeBrowser(const CEGUI::EventArgs&)
{
    if(!mInGame)
        return goBack();
    mOpen = false;
    ODFrameListener::getSingleton().getClientGameMap()->setGamePaused(mWasPaused);
    getModeManager().getGui().loadGuiSheet(Gui::inGameMenu);
    return true;
}

void MenuModeLoad::activate()
{
    // Loads the corresponding Gui sheet.
    getModeManager().getGui().loadGuiSheet(Gui::guiSheet::loadSavedGameMenu);

    GameMap* gameMap = ODFrameListener::getSingleton().getClientGameMap();
    CEGUI::Window* sheet = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu);
    sheet->getChild("WelcomeBanner")->hide();
    sheet->getChild("VersionText")->setVisible(!mInGame);
    if(mInGame)
    {
        if(!mOpen)
            mWasPaused = gameMap->getGamePaused();
        mOpen = true;
        gameMap->setGamePaused(true);
    }
    else
    {
        giveFocus();
        MusicPlayer::getSingleton().play(ConfigManager::getSingleton().getMainMenuMusic());
        gameMap->clearAll();
        gameMap->setGamePaused(true);
        if(!mSavedGame.empty())
        {
            ODFrameListener::getSingleton().stopGameRenderer();
            ODFrameListener::getSingleton().createMainMenuScene();
        }
    }

    CEGUI::Window* tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LevelWindowFrame/SaveGameSelect");
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>(tmpWin);

    tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LoadingText");
    tmpWin->hide();
    mFilesList.clear();
    levelSelectList->resetList();
    sheet->getChild("LevelWindowFrame/MapDescriptionText")->setText("");

    std::string levelPath = ResourceManager::getSingleton().getSaveGamePath();
    if(Helper::fillFilesList(levelPath, mFilesList, SAVEGAME_EXTENSION))
    {
        for (uint32_t n = 0; n < mFilesList.size(); ++n)
        {
            std::string filename = boost::filesystem::path(mFilesList[n]).filename().string();
            CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(filename);
            item->setID(n);
            item->setSelectionBrushImage("OpenDungeonsSkin/SelectionBrush");
            levelSelectList->addItem(item);
        }
    }
    if(!mSavedGame.empty())
    {
        const std::string filename = mSavedGame;
        mSavedGame.clear();
        launchSavedGame(filename);
    }

}

bool MenuModeLoad::launchSelectedButtonPressed(const CEGUI::EventArgs&)
{
    CEGUI::Window* tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LevelWindowFrame/SaveGameSelect");
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>(tmpWin);

    if(levelSelectList->getSelectedCount() == 0)
    {
        tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LoadingText");
        tmpWin->setText("Please select a saved game first.");
        tmpWin->show();
        return true;
    }

    const uint32_t id = levelSelectList->getFirstSelectedItem()->getID();
    if(id >= mFilesList.size())
        return true;
    const std::string level = mFilesList[id];
    if(mInGame)
    {
        getModeManager().requestSavedGame(level);
        return true;
    }
    return launchSavedGame(level);
}

bool MenuModeLoad::launchSavedGame(const std::string& level)
{
    std::string nickname = ConfigManager::getSingleton().getGameValue(Config::NICKNAME, std::string(), false);
    if(!nickname.empty())
        ODFrameListener::getSingleton().getClientGameMap()->setLocalPlayerNick(nickname);
    CEGUI::Window* tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LoadingText");
    tmpWin->setText("Loading...");
    tmpWin->show();

    // In single player mode, we act as a server
    if(!ODServer::getSingleton().startServer(nickname, level, ServerMode::ModeGameLoaded, false))
    {
        OD_LOG_ERR("Could not start server for single player game !!!");
        tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LoadingText");
        tmpWin->setText("ERROR: Could not start server for single player game !!!");
        tmpWin->show();
        return true;
    }

    int port = ODServer::getSingleton().getNetworkPort();
    uint32_t timeout = ConfigManager::getSingleton().getClientConnectionTimeout();
    std::string replayFilename = ResourceManager::getSingleton().getReplayDataPath()
        + ResourceManager::getSingleton().buildReplayFilename();
    if(!ODClient::getSingleton().connect("localhost", port, timeout, replayFilename))
    {
        ODServer::getSingleton().stopServer();
        OD_LOG_ERR("Could not connect to server for single player game !!!");
        tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LoadingText");
        tmpWin->setText("Error: Couldn't connect to local server!");
        tmpWin->show();
        return true;
    }
    return true;
}

bool MenuModeLoad::deleteSelectedButtonPressed(const CEGUI::EventArgs&)
{
    CEGUI::Window* tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LevelWindowFrame/SaveGameSelect");
    CEGUI::Listbox* selectList = static_cast<CEGUI::Listbox*>(tmpWin);

    if(selectList->getSelectedCount() == 0)
    {
        tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LoadingText");
        tmpWin->setText("Please select a saved game first.");
        tmpWin->show();
        return true;
    }

    CEGUI::ListboxItem* selItem = selectList->getFirstSelectedItem();
    int id = selItem->getID();

    const std::string& levelFile = mFilesList[id];
    if(!boost::filesystem::remove(levelFile))
    {
        OD_LOG_ERR("Could not delete saved game !!!");
        tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LoadingText");
        tmpWin->setText("Error: Couldn't delete saved game!");
        tmpWin->show();
        return true;
    }

    activate();
    return true;
}

bool MenuModeLoad::updateDescription(const CEGUI::EventArgs&)
{
    // Get the level corresponding id
    CEGUI::Window* tmpWin = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LevelWindowFrame/SaveGameSelect");
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>(tmpWin);

    CEGUI::Window* descTxt = getModeManager().getGui().getGuiSheet(Gui::loadSavedGameMenu)->getChild("LevelWindowFrame/MapDescriptionText");

    if(levelSelectList->getSelectedCount() == 0)
    {
        descTxt->setText("");
        return true;
    }

    getModeManager().getGui().playButtonClickSound();

    CEGUI::ListboxItem* selItem = levelSelectList->getFirstSelectedItem();
    int id = selItem->getID();

    std::string filename = mFilesList[id];

    LevelInfo levelInfo;
    std::string mapDescription;
    if(MapHandler::getMapInfo(filename, levelInfo))
        mapDescription = levelInfo.mLevelDescription;
    else
        mapDescription = "invalid map";

    descTxt->setText(mapDescription);
    static_cast<CEGUI::Scrollbar*>(descTxt->getChild("__auto_vscrollbar__"))->setScrollPosition(0);

    return true;
}
