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

    //! \brief Opens the skirmish sub-menu once the flight into the menu (after the defeat debriefing) ended
    void onFrameStarted(const Ogre::FrameEvent& evt) override;

    bool goBack(const CEGUI::EventArgs& e = {}) override;

private:
    //! \brief The Settings window
    SettingsWindow mSettings;
    bool mSettingsPageOpen = false;
    //! True while the menu waits for the flight to end before it shows the skirmish sub-menu
    bool mSkirmishSubMenuPending = false;

    //! \brief Helper functions to connect a button to a mode change
    void connectModeChangeEvent(const std::string& buttonName, AbstractModeManager::ModeType mode);
    void connectModeChangeEvent(CEGUI::Window* button, AbstractModeManager::ModeType mode);

    //! \brief Function triggered when pushing a button
    bool quitButtonPressed(const CEGUI::EventArgs&);
    bool toggleSettings(const CEGUI::EventArgs&);
    bool openSettingsPage(const std::string& name);
    bool settingsPageClosed(const CEGUI::EventArgs&);
    void showMainMenuButtons(bool visible);
    bool toggleSubMenu(const std::string& name);

    //! \brief Sub menu button triggers
    //! \brief Shows or hides the campaign sub menu
    bool toggleCampaignSubMenu(const CEGUI::EventArgs&);
    //! \brief Starts a new campaign, after a confirmation when there is saved progress that it would replace
    bool newCampaignPressed(const CEGUI::EventArgs&);
    //! \brief Forgets the saved campaign progress and opens the campaign menu
    bool newCampaignConfirmed(const CEGUI::EventArgs&);

    //! \brief Shows or hides the question that asks whether the saved campaign progress
    //! may be replaced by a new campaign, in place of the two campaign buttons
    void showNewCampaignConfirm(bool visible);
    bool toggleSkirmishSubMenu(const CEGUI::EventArgs&);
    bool toggleMultiplayerSubMenu(const CEGUI::EventArgs&);
    bool toggleEditorSubMenu(const CEGUI::EventArgs&);
};

#endif // MENUMODEMAIN_H
