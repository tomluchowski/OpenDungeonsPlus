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
#include <CEGUI/CEGUI.h>


AdvertMode::AdvertMode(ModeManager* modeManager):
    AbstractApplicationMode(modeManager, ModeManager::ADVERTISMENT)
{

    CEGUI::Window* rootWin = getModeManager().getGui().getGuiSheet(Gui::advertisment);
    OD_ASSERT_TRUE(rootWin != nullptr);
    addEventConnection(
        rootWin->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&AdvertMode::quitPressed, this)
            )
        );


    addEventConnection(
        rootWin->getChild("AdvertismentLink")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&AdvertMode::showWWW, this)
        )
    );


    addEventConnection(
        rootWin->getChild("AdvertismentLink")->subscribeEvent(
            CEGUI::Window::EventMouseEntersArea,
            CEGUI::Event::Subscriber(&AdvertMode::hilightLink, this)
        )
   );

    addEventConnection(
        rootWin->getChild("AdvertismentLink")->subscribeEvent(
            CEGUI::Window::EventMouseLeavesArea,
            CEGUI::Event::Subscriber(&AdvertMode::unHilightLink, this)
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
    system("xdg-open 'https://discord.gg/K2JPXuchZV'");
    return true;

}


bool AdvertMode::hilightLink()
{
    getModeManager().getGui().getGuiSheet(Gui::advertisment)->getChild("AdvertismentLink")->setProperty("FrameColours","tl:00000000 tr:00000000 bl:00000000 br:00000000" );
    return true;
}

bool AdvertMode::unHilightLink()
{
    getModeManager().getGui().getGuiSheet(Gui::advertisment)->getChild("AdvertismentLink")->setProperty("FrameColours","tl:FFFFFFFF tr:FFFFFFFF bl:FFFFFFFF br:FFFFFFFF" );
    return true;
}


        
bool AdvertMode::quitPressed(const CEGUI::EventArgs&)
{
    ODFrameListener::getSingletonPtr()->requestExit();
    return true;
}
