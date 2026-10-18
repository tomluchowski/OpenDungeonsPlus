/*!
 * \file   Gui.cpp
 * \date   05 April 2011
 * \author StefanP.MUC
 * \brief  Header for class Gui containing all the stuff for the GUI,
 *         including translation.
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

#ifndef GUI_H_
#define GUI_H_

#include <CEGUI/Event.h>
#include <CEGUI/InputEvent.h>
#include <CEGUI/Window.h>

#include <OISMouse.h>

#include <string>
#include <map>
#include <initializer_list>

class SoundEffectsManager;

namespace CEGUI
{
    class Window;
    class EventArgs;
}

namespace Ogre
{
    class RenderTarget;
}

//! \brief This class handles the CEGUI system
class Gui
{
public:
    enum guiSheet
    {
        hideGui,
        advertisment,
        mainMenu,
        skirmishMenu,
        multiplayerClientMenu,
        multiplayerServerMenu,
        multiMasterServerJoinMenu,
        editorNewMenu,
        editorLoadMenu,
        editorModeGui,
        optionsMenu,
        inGameMenu,
        replayMenu,
        loadSavedGameMenu,
        configureSeats,
        console
    };

    /*! \brief Constructor that initializes the whole CEGUI system
     *  including renderer, system, resource provider, setting defaults,
     *  loading all sheets, assigning all event handler
     */
    Gui(SoundEffectsManager* soundEffectsManager, const std::string& ceguiLogFileName, Ogre::RenderTarget& renderTarget);
    Gui(const Gui&) = delete;

    ~Gui();

    //! \brief loads the specified gui sheet
    void loadGuiSheet(guiSheet newSheet);

    //! \brief A required function to pass input to the OIS system.
    static CEGUI::MouseButton convertButton (OIS::MouseButtonID buttonID);

    CEGUI::Window* getGuiSheet(guiSheet sheet);

    //! \brief Move CEGUI rendering to another Ogre render target.
    void setRenderTarget(Ogre::RenderTarget& renderTarget);

    enum
    {
        MIN_UI_SCALE_PERCENT = 80,
        MAX_UI_SCALE_PERCENT = 120
    };

    //! \brief Registers a window tree for resolution-independent scaling.
    void registerWindowHierarchy(CEGUI::Window* window);

    //! \brief Sets the user-selected UI scale and applies it immediately.
    void setUserScalePercent(float scalePercent);

    //! \brief Arranges visible gameplay actions and retains their scaled layout.
    void arrangeRoomButtons(CEGUI::Window* rooms);
    void arrangeTrapButtons(CEGUI::Window* traps);
    void arrangeSpellButtons(CEGUI::Window* spells);

    // Access names of the GUI elements
    static const std::string ROOT;
    static const std::string DISPLAY_GOLD;
    static const std::string DISPLAY_MANA;
    static const std::string DISPLAY_TERRITORY;
    static const std::string DISPLAY_CREATURES;
    static const std::string MINIMAP;
    static const std::string OBJECTIVE_TEXT;
    static const std::string MAIN_TABCONTROL;
    static const std::string TAB_ROOMS;
    static const std::string BUTTON_TEMPLE;
    static const std::string BUTTON_PORTAL;
    static const std::string BUTTON_PORTAL_WAVE;
    static const std::string BUTTON_DESTROY_ROOM;
    static const std::string TAB_TRAPS;
    static const std::string BUTTON_DESTROY_TRAP;
    static const std::string TAB_SPELLS;
    static const std::string TAB_CREATURES;
    static const std::string BUTTON_CREATURE_WORKER;
    static const std::string BUTTON_CREATURE_FIGHTER;
    static const std::string TAB_COMBAT;
    static const std::string MM_BACKGROUND;
    static const std::string MM_WELCOME_MESSAGE;
    static const std::string EDITOR;
    static const std::string EDITOR_LAVA_BUTTON;
    static const std::string EDITOR_GOLD_BUTTON;
    static const std::string EDITOR_ROCK_BUTTON;
    static const std::string EDITOR_WATER_BUTTON;
    static const std::string EDITOR_DIRT_BUTTON;
    static const std::string EDITOR_CLAIMED_BUTTON;
    static const std::string EDITOR_GEM_BUTTON;
    static const std::string EDITOR_FULLNESS;
    static const std::string EDITOR_CURSOR_POS;
    static const std::string EDITOR_SEAT_ID;
    static const std::string EDITOR_CREATURE_SPAWN;
    static const std::string EDITOR_LEVEL_NAME;
    static const std::string EDITOR_MAPLIGHT_BUTTON;
    static const std::string EXIT_CONFIRMATION_POPUP;
    static const std::string EXIT_CONFIRMATION_POPUP_YES_BUTTON;
    static const std::string EXIT_CONFIRMATION_POPUP_NO_BUTTON;
    static const std::string SKM_TEXT_LOADING;
    static const std::string SKM_BUTTON_LAUNCH;
    static const std::string SKM_BUTTON_BACK;
    static const std::string SKM_LIST_LEVEL_TYPES;
    static const std::string SKM_LIST_LEVELS;
    static const std::string MPM_TEXT_LOADING;
    static const std::string MPM_BUTTON_SERVER;
    static const std::string MPM_BUTTON_CLIENT;
    static const std::string MPM_BUTTON_BACK;
    static const std::string MPM_LIST_LEVELS;
    static const std::string MPM_EDIT_IP;
    static const std::string MPM_EDIT_NICK;
    static const std::string EDM_TEXT_LOADING;
    static const std::string EDM_BUTTON_LAUNCH;
    static const std::string EDM_BUTTON_BACK;
    static const std::string EDM_LIST_LEVELS;
    static const std::string EDM_LIST_LEVEL_TYPES;
    static const std::string REM_TEXT_LOADING;
    static const std::string REM_BUTTON_LAUNCH;
    static const std::string REM_BUTTON_DELETE;
    static const std::string REM_BUTTON_BACK;
    static const std::string REM_LIST_REPLAYS;

    //! \brief Callback function that plays a button click sound.
    bool playButtonClickSound(const CEGUI::EventArgs& e = {});

private:
    void arrangeActionButtons(CEGUI::Window* panel, std::initializer_list<const char*> names);
    struct WindowScaleData
    {
        CEGUI::URect area;
        CEGUI::USize minSize;
        CEGUI::USize maxSize;
        CEGUI::String text;
        CEGUI::UDim tabHeight;
        CEGUI::UDim tabTextPadding;
        bool hasFormattedImageSize;
        bool hasTabHeight;
    };

    std::map<guiSheet, CEGUI::Window*> mSheets;
    std::map<CEGUI::Window*, WindowScaleData> mScaledWindows;

    float mUserScale;

    CEGUI::Event::ScopedConnection mDisplaySizeChangedConnection;
    CEGUI::Event::ScopedConnection mWindowDestroyedConnection;

    SoundEffectsManager* mSoundEffectsManager;

    bool onDisplaySizeChanged(const CEGUI::EventArgs& e);
    bool onWindowDestroyed(const CEGUI::EventArgs& e);
    void registerWindow(CEGUI::Window* window);
    void applyScale(const CEGUI::Sizef& displaySize);
    void applyScale(CEGUI::Window* window, const WindowScaleData& data, float scale);
    void updateResourceScaling(const CEGUI::Sizef& displaySize);
};

#endif // GUI_H_
