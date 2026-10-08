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

#include "modes/AdvertMode.h"
#include "render/ODFrameListener.h"
#include "render/Gui.h"
#include "utils/LogManager.h"

#include <cstdlib>
#include <string>
#include <CEGUI/CEGUI.h>
#include <CEGUI/widgets/PushButton.h>

//! Community page opened by the button of the exit page.
static const std::string COMMUNITY_LINK = "https://discord.gg/K2JPXuchZV";

AdvertMode::AdvertMode(ModeManager* modeManager):
    AbstractApplicationMode(modeManager, ModeManager::ADVERTISMENT)
{

    CEGUI::Window* rootWin = getModeManager().getGui().getGuiSheet(Gui::advertisment);
    OD_ASSERT_TRUE(rootWin != nullptr);
    addEventConnection(
        rootWin->getChild("CommunityPanel/DiscordButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&AdvertMode::showWWW, this)
        )
    );

    addEventConnection(
        rootWin->getChild("CommunityPanel/CloseButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&AdvertMode::quitPressed, this)
        )
    );
}

void AdvertMode::activate()
{
    // Loads the corresponding Gui sheet.
    getModeManager().getGui().loadGuiSheet(Gui::advertisment);

    giveFocus();

    // Play the main menu music
    // MusicPlayer::getSingleton().play(ConfigManager::getSingleton().getMainMenuMusic());

    // GameMap* gameMap = ODFrameListener::getSingleton().getClientGameMap();
    // gameMap->clearAll();
    // gameMap->setGamePaused(true);

    // CEGUI::Window* window = getModeManager().getGui().getGuiSheet(Gui::guiSheet::editorNewMenu);
    // CEGUI::Combobox* levelTypeCb = static_cast<CEGUI::Combobox*>(window->getChild(LIST_LEVEL_TYPES));
    // levelTypeCb->setItemSelectState(static_cast<size_t>(0), true);

    // window->getChild(TEXT_LOADING)->setText("");
}

bool AdvertMode::showWWW()
{
#if defined(_WIN32)
    const std::string command = "start \"\" \"" + COMMUNITY_LINK + "\"";
#elif defined(__APPLE__)
    const std::string command = "open '" + COMMUNITY_LINK + "'";
#else
    const std::string command = "xdg-open '" + COMMUNITY_LINK + "'";
#endif
    system(command.c_str());
    ODFrameListener::getSingletonPtr()->requestExit();
    return true;
}


bool AdvertMode::quitPressed(const CEGUI::EventArgs&)
{
    ODFrameListener::getSingletonPtr()->requestExit();
    return true;
}
