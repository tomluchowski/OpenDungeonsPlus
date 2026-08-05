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

#ifndef EDITORMODE_H
#define EDITORMODE_H

#include "GameEditorModeBase.h"

#include "gamemap/TileMarker.h"
#include "modes/InputCommand.h"
#include "modes/SettingsWindow.h"
#include "rooms/RoomPortalWave.h"

#include <boost/filesystem.hpp>
#include <boost/circular_buffer.hpp>

#include <iostream>



class DraggableTileContainer;
class GameMap;
class Gui; // Used to change the Current tile type

enum class TileVisual;

// static bool operator<=(const Ogre::Vector2 &a, const Ogre::Vector2 &b){
//     if (a.x != b.x)
//         return a.x <= b.x;
//     else
//         return a.y <= b.y;
// }





class EditorMode final: public GameEditorModeBase, public InputCommand
{
friend class ODClient;
    
public:
    EditorMode(ModeManager* modeManager);

    virtual ~EditorMode();
   

    virtual bool mouseMoved     (const OIS::MouseEvent &arg) override;
    virtual bool mousePressed   (const OIS::MouseEvent &arg, OIS::MouseButtonID id) override;
    virtual bool mouseReleased  (const OIS::MouseEvent &arg, OIS::MouseButtonID id) override;
    virtual bool keyPressed     (const OIS::KeyEvent &arg) override;
    virtual bool keyReleased    (const OIS::KeyEvent &arg) override;
    virtual void handleHotkeys  (OIS::KeyCode keycode) override;

    void onFrameStarted(const Ogre::FrameEvent& evt) override;
    void onFrameEnded(const Ogre::FrameEvent& evt) override;

    //! \brief Called when the game mode is activated
    //! Used to call the corresponding Gui Sheet.
    void activate() override;

    // ! Specific functions
    inline GameMap* getGameMap() const
    { return mGameMap; }

    virtual void notifyGuiAction(GuiAction guiAction) override;

    //! \brief Options window functions
    bool toggleOptionsWindow(const CEGUI::EventArgs& arg = {});
    bool showSettingsFromOptions(const CEGUI::EventArgs& e = {});
    bool showQuitMenuFromOptions(const CEGUI::EventArgs& arg = {});
    bool onSaveButtonClickFromOptions(const CEGUI::EventArgs& arg = {});

    //! \brief Quit menu functions
    bool showQuitMenu(const CEGUI::EventArgs& arg = {});
    bool hideQuitMenu(const CEGUI::EventArgs& arg = {});
    bool showEditorLoadMenu(const CEGUI::EventArgs& arg = {});
    bool quickSavePopUpMenu(const CEGUI::EventArgs& arg = {});    
    bool showEditorSaveMenu(const CEGUI::EventArgs& arg = {});    
    bool hideEditorLoadMenu(const CEGUI::EventArgs& arg = {});
    bool hideEditorSaveMenu(const CEGUI::EventArgs& arg = {});
    void selectCreature(unsigned ii, CEGUI::EventArgs args /*arg*/);
    bool showNewLevelDialog(const CEGUI::EventArgs& /*arg*/);
    bool hideNewLevelDialog(const CEGUI::EventArgs& /*arg*/);
    bool loadMenuFilePathTextChanged(const CEGUI::EventArgs& /*arg*/);
    bool loadMenuLevelSelectSelected(const CEGUI::EventArgs& /*arg*/);
    bool loadMenuLevelDoubleClicked(const CEGUI::EventArgs& /*arg*/);
    void loadMenuAskForConfirmation(const std::string&);
    bool saveMenuSaveButtonClicked(const CEGUI::EventArgs& /*arg*/);
    bool saveMenuFilePathTextChanged(const CEGUI::EventArgs& /*arg*/);
    bool saveMenuFileNameTextChanged(const CEGUI::EventArgs& /*arg*/);   
    bool saveMenuLevelSelectSelected(const CEGUI::EventArgs& /*arg*/);
    bool onClickYesQuitMenu(const CEGUI::EventArgs& arg = {});
    bool hideConfirmMenu(const CEGUI::EventArgs& /*arg*/);
    bool onYesConfirmMenu(const CEGUI::EventArgs& /*arg*/);
    bool launchNewLevelPressed(const CEGUI::EventArgs&);
    void setTileVisualIfArgNotNull(TileVisual tileVisual)
    { mCurrentTileVisual = tileVisual; }

    void selectSquaredTiles(int tileX1, int tileY1, int tileX2, int tileY2) override;
    void selectTiles(const std::vector<Tile*> tiles) override;
    void unselectAllTiles() override;

    void displayText(const Ogre::ColourValue& txtColour, const std::string& txt) override;
    bool updateDescription(const CEGUI::EventArgs& e = {});
    bool isCheckboxSelected(const CEGUI::String& checkbox);

    //! \brief Shows the waves of a wave portal, as the server has just described them.
    //! Called when the answer to the request sent by askPortalWaveData comes back.
    void showPortalWaveWindow(const std::string& roomName, const RoomPortalWaveConfig& config);
private:

    DraggableTileContainer* draggableTileContainer;
    
    void connectTileSelect(const std::string& buttonName, TileVisual tileVisual);


    //! \brief Tile area marker, the two points both to the first and third corner
    TileMarker mTileMarker;

    
    //! \brief Tile type (Dirt, Lava, Claimed, ...)
    TileVisual mCurrentTileVisual;

    //! \brief how of the wall type is there (0 - 100.0)
    //! < 1.0 means no walls.
    double mCurrentFullness;

    //! \brief Current selected creature to spawn
    uint32_t mCurrentCreatureIndex;

    //! \brief The level name currently written in the status bar, kept so that the text is
    //! only rewritten when it actually changes.
    std::string mDisplayedLevelName;

    //! \brief Level given to the creatures spawned from now on. It is also applied to the
    //! creatures held in the hand when it changes, which is how the level of a creature
    //! already on the map is changed: pick it up, set the level, drop it.
    uint32_t mCurrentCreatureLevel;

    //! \brief The wave portal being edited, and the copy of its waves the window works on.
    //! The copy is only sent back to the server when the player applies it. The client has
    //! no rooms of its own, so the portal is named to the server by one of its tiles.
    std::string mPortalWaveRoomName;
    int32_t mPortalWaveTileX;
    int32_t mPortalWaveTileY;
    RoomPortalWaveConfig mPortalWaveConfig;

    //! \brief Index in mPortalWaveConfig.mWaves of the wave shown, -1 when there is none
    int32_t mPortalWaveSelectedWave;

    //! \brief Set while the wave window is being rewritten, so that the changes we make to
    //! the widgets ourselves are not read back as if the player had made them.
    bool mPortalWaveRefreshing;

    //! \brief The creature node name being dragged by the mouse
    std::string mDraggedCreature;

    //! \brief The map light node name being dragged by the mouse
    std::string mDraggedMapLight;

    //! \brief Stores the lastest mouse cursor and light positions.
    int mMouseX;
    int mMouseY;

    //! \brief The settings window.
    SettingsWindow mSettings;

    //! \brief A sub-function called by mouseMoved()
    //! It will handle the potential mouse wheel logic
    void handleMouseWheel(const OIS::MouseEvent& arg);

    //! \brief Updates the text seen next to the cursor position.
    //! This text gives the tile position, and the current left-click action
    void updateCursorText();

    //! \brief Asks the server to give the creatures currently held in the hand the level
    //! selected in the editor.
    void setLevelOfCreaturesInHand();

    //! \brief Writes the list of the editor's keyboard shortcuts into the controls window.
    void fillControlsWindow();

    //! \brief Shows the name of the level being edited. Called on every frame because the
    //! name is only known once the client has been told about the level, which happens
    //! after the editor is up.
    void updateLevelNameText();

    bool showControlsWindow(const CEGUI::EventArgs& e = {});
    bool hideControlsWindow(const CEGUI::EventArgs& e = {});

    //! \brief Asks the server what the waves of the wave portal covering the given tile are.
    //! They only exist on the server, so the window can only be filled once it answers.
    void askPortalWaveData(Tile* tile);

    //! \brief Writes the room wide settings and the fields of the wave being shown back into
    //! the working copy. Called before anything that changes which wave is shown, so that
    //! what was typed is not lost.
    void readPortalWaveWindow();

    //! \brief Rewrites the whole window from the working copy.
    void refreshPortalWaveWindow();

    //! \brief Rewrites the part of the window that describes the selected wave.
    void refreshPortalWaveSelectedWave();

    bool onPortalWaveSelectionChanged(const CEGUI::EventArgs& e = {});
    bool onPortalWaveAddWave(const CEGUI::EventArgs& e = {});
    bool onPortalWaveRemoveWave(const CEGUI::EventArgs& e = {});
    bool onPortalWaveAddCreature(const CEGUI::EventArgs& e = {});
    bool onPortalWaveRemoveCreature(const CEGUI::EventArgs& e = {});
    bool onPortalWaveApply(const CEGUI::EventArgs& e = {});
    bool hidePortalWaveWindow(const CEGUI::EventArgs& e = {});

    //! \brief Refreshes the gui buttons. It will be called at level loading only since
    //! it shouldn't change in the editor
    void refreshGuiSkill();

    //! \brief Updates the flag icon color when switching seats.
    void updateFlagColor();
    void onEditCopy();
    void onEditPaste();
    void onEditDelete();
    void onEditMirrorX();
    
    //! \brief Called when there is a mouse input change
    void checkInputCommand();
    void handlePlayerActionNone();
    void handlePlayerActionChangeTile();
    void handlePlayerActionSelectTile();
    void installCreaturesMenuButtons();
    void uninstallCreaturesMenuButtons();
    void installRecentlyUsedFilesButtons();
    void uninstallRecentlyUsedFilesButtons();
    void installSeatsMenuButtons();
    void uninstallSeatsMenuButtons();
    void addToRecentlyUsed(const std::string&);
    // void configureMenu(CEGUI::Window*);
    // bool onMouseEntersMenuItem(const CEGUI::EventArgs& e);
    // bool onMouseLeavesMenuItem(const CEGUI::EventArgs& e);
    
    bool loadLevelFromFile(const std::string&);
    //! \brief file path to currently choosen file via load / save menu
    //! \brief Whether the file should be kept out of the level lists unless the player
    //! asked for hidden ones. Takes the whole path: on Windows being hidden is an attribute
    //! of the file, not a dot in front of its name.
    bool isFileHidden(const boost::filesystem::path& path);
    void addPathNameToList(boost::filesystem::directory_entry& xx, CEGUI::Listbox* levelSelectList, CEGUI::Colour cc, int& nn );
    std::string dialogFullPath;

    boost::circular_buffer<boost::filesystem::path> recentlyUsedLevels;

    bool mModifiedMapBit; 
    
};

#endif // EDITORMODE_H
