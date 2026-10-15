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

#ifndef MENUMODELOAD_H
#define MENUMODELOAD_H

#include "AbstractApplicationMode.h"

class MenuModeLoad: public AbstractApplicationMode
{
public:
    //! \param inGame True if the browser is opened on top of a running game instead of from the main menu.
    //! \param savedGame If not empty, this saved game is launched right after activation.
    MenuModeLoad(ModeManager*, bool inGame = false, const std::string& savedGame = {});
    ~MenuModeLoad() override;

    //! \brief Called when the game mode is activated
    //! Used to call the corresponding Gui Sheet.
    void activate() final override;

    bool launchSelectedButtonPressed(const CEGUI::EventArgs&);
    bool deleteSelectedButtonPressed(const CEGUI::EventArgs&);
    bool updateDescription(const CEGUI::EventArgs&);
    //! \brief Leaves the browser: back to the previous menu, or back to the game when opened in game.
    bool closeBrowser(const CEGUI::EventArgs& = {});
    //! \brief True while the browser is shown on top of a running game.
    bool isOpenInGame() const { return mInGame && mOpen; }

private:
    //! \brief Starts a local server with the given saved game and connects to it.
    bool launchSavedGame(const std::string& level);
    //! \brief Whether the browser is used on top of a running game.
    bool mInGame;
    //! \brief Whether the in-game browser is currently shown.
    bool mOpen = false;
    //! \brief Whether the game was already paused when the in-game browser was opened.
    bool mWasPaused = false;
    //! \brief Saved game to launch right after activation, empty if none.
    std::string mSavedGame;
    std::vector<std::string> mFilesList;
};

#endif // MENUMODELOAD_H
