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

#ifndef MENUMODECONFIGURESEATS_H
#define MENUMODECONFIGURESEATS_H

#include "AbstractApplicationMode.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class ChatMessage;
class ODPacket;
class Player;
class Seat;

namespace CEGUI
{
class Combobox;
class EventArgs;
class Spinner;
class Window;
}

class MenuModeConfigureSeats: public AbstractApplicationMode
{
public:
    MenuModeConfigureSeats(ModeManager*);

    virtual ~MenuModeConfigureSeats();

    //! \brief Called when the game mode is activated
    //! Used to call the corresponding Gui Sheet.
    void activate() override;

    void receiveChat(const ChatMessage& chat) override;

    bool launchSelectedButtonPressed(const CEGUI::EventArgs&);
    bool goBack(const CEGUI::EventArgs& e = {}) override;
    bool chatText(const CEGUI::EventArgs& e);

    bool comboChanged(const CEGUI::EventArgs& ea);
    //! \brief Shows the Game settings window
    bool openGameSettings(const CEGUI::EventArgs& ea);
    //! \brief Hides the Game settings window
    bool closeGameSettings(const CEGUI::EventArgs& ea);
    //! \brief Sends the settings to the server when the host changed a spinner of the Game settings window
    bool settingChanged(const CEGUI::EventArgs& ea);
    //! \brief Switches the availability of a room, spell, trap or door to the next state and sends it to the server
    bool itemStateClicked(const CEGUI::EventArgs& ea);
    void addPlayer(const std::string& nick, int32_t id);
    void removePlayer(int32_t id);

    void activatePlayerConfig();
    void refreshSeatConfiguration(ODPacket& packet);

private:
    bool mIsActivePlayerConfig;
    std::vector<int> mSeatIds;
    std::vector<std::pair<std::string, int32_t> > mPlayers;

    //! \brief The values of the Game settings window are only sent once the server has sent them to us
    bool mSettingsReceived;
    //! \brief True while the window is refreshed from the server, to not send the values back
    bool mIsRefreshing;
    //! \brief The availability chosen for each skill, indexed by SkillType
    std::vector<uint32_t> mItemStates;
    //! \brief The buttons of the rooms, spells, traps and doors pages, by SkillType
    std::map<uint32_t, CEGUI::Window*> mItemButtons;
    //! \brief The creature limit spinners, by creature class name
    std::map<std::string, CEGUI::Spinner*> mLimitSpinners;
    //! \brief The windows that only the host may change
    std::vector<CEGUI::Window*> mHostSettingWindows;

    void fireSeatConfigurationToServer();

    //! \brief Fills the skirmish setting combos and the maximum creatures spinner with their choices
    void initSettingCombos();

    //! \brief Fills the pages of the Game settings window with one line per creature, room, spell, trap and door
    void initSettingPages();
};

#endif // MENUMODECONFIGURESEATS_H
