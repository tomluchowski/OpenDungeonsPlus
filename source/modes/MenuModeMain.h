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

#ifndef MENUMODEMAIN_H
#define MENUMODEMAIN_H

#include "AbstractApplicationMode.h"

#include "modes/SettingsWindow.h"

class MenuModeMain: public AbstractApplicationMode
{
public:
    MenuModeMain(ModeManager*);

    virtual ~MenuModeMain()
    {}

    //! \brief Called when the game mode is activated
    //! Used to call the corresponding Gui Sheet.
    void activate() final override;

    bool goBack(const CEGUI::EventArgs& e = {}) override;

private:
    //! \brief The Settings window
    SettingsWindow mSettings;
    //! \brief True while a settings page opened from the settings sub menu is shown,
    //! so that closing it shows the sub menu again.
    bool mSettingsPageOpen = false;

    //! \brief Helper functions to connect a button to a mode change
    void connectModeChangeEvent(const std::string& buttonName, AbstractModeManager::ModeType mode);
    void connectModeChangeEvent(CEGUI::Window* button, AbstractModeManager::ModeType mode);

    //! \brief Function triggered when pushing a button
    bool quitButtonPressed(const CEGUI::EventArgs&);
    bool toggleSettings(const CEGUI::EventArgs&);
    //! \brief Hides the settings sub menu and shows the named settings page.
    bool openSettingsPage(const std::string& name);
    //! \brief Opens the settings page belonging to the clicked sub menu button.
    bool settingsPageButtonClicked(const CEGUI::EventArgs& e);
    //! \brief Shows the settings sub menu again after a settings page was closed.
    bool settingsPageClosed(const CEGUI::EventArgs&);
    //! \brief Shows or hides the main menu buttons behind the sub menus.
    void showMainMenuButtons(bool visible);
    //! \brief Toggles the named sub menu window and hides the others.
    bool toggleSubMenu(const std::string& name);

    //! \brief Sub menu button triggers
    bool toggleSkirmishSubMenu(const CEGUI::EventArgs&);
    bool toggleMultiplayerSubMenu(const CEGUI::EventArgs&);
    bool toggleEditorSubMenu(const CEGUI::EventArgs&);
};

#endif // MENUMODEMAIN_H
