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

#include "modes/EditorMode.h"

#include "camera/CameraManager.h"
#include "camera/CullingManager.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntity.h"
#include "entities/GameEntityType.h"
#include "entities/MapLight.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "game/SkillManager.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/DraggableTileContainer.h"
#include "gamemap/MapHandler.h"
#include "gamemap/MiniMap.h"
#include "gamemap/Pathfinding.h"
#include "gamemap/TileMarker.h"
#include "modes/GameEditorModeConsole.h"
#include "network/ChatEventMessage.h"
#include "network/ODClient.h"
#include "network/ODServer.h"
#include "network/ServerMode.h"
#include "render/DebugDrawer.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "render/TextRenderer.h"
#include "rooms/RoomManager.h"
#include "rooms/RoomType.h"
#include "sound/MusicPlayer.h"
#include "traps/TrapManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/ResourceManager.h"
#include "utils/Helper.h"
#include "ODApplication.h"


#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#include <fileapi.h>
#endif

#include <OgreEntity.h>
#include <OgreSceneNode.h>
#include <OgreMaterialManager.h>
#include <OgreRoot.h>
#include <OgreRenderWindow.h>
#include <OgreAxisAlignedBox.h>

#include <CEGUI/widgets/FrameWindow.h>
#include <CEGUI/widgets/ToggleButton.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/Editbox.h>
#include <CEGUI/widgets/ListboxTextItem.h>
#include <CEGUI/widgets/Listbox.h>
#include <CEGUI/widgets/Combobox.h>
#include <CEGUI/widgets/MultiLineEditbox.h>
#include <CEGUI/Window.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/ImageManager.h>


#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>


using namespace boost::filesystem;

namespace
{
    //! \brief Functor to select tile type from gui
    class TileSelector
    {
    public:
        bool operator()(const CEGUI::EventArgs& e)
        {
            playerSelection.setCurrentAction(SelectedAction::changeTile);
            editorMode.setTileVisualIfArgNotNull(tileVisual);
            return true;
        }
        TileVisual tileVisual;
        PlayerSelection& playerSelection;
        EditorMode& editorMode;
    };
}

EditorMode::EditorMode(ModeManager* modeManager):
    GameEditorModeBase(modeManager, ModeManager::EDITOR, modeManager->getGui().getGuiSheet(Gui::guiSheet::editorModeGui)),
    draggableTileContainer(nullptr),
    mCurrentTileVisual(TileVisual::nullTileVisual),
    mCurrentFullness(100.0),
    mCurrentCreatureIndex(0),
    mMouseX(0),
    mMouseY(0),
    mSettings(SettingsWindow(mRootWindow)),
    mModifiedMapBit(false)
{

    // Set per default the input on the map
    mModeManager->getInputManager().mMouseDownOnCEGUIWindow = false;

    ODFrameListener::getSingleton().getCameraManager()->setDefaultView();
   
    // The Quit menu handlers
    addEventConnection(
        mRootWindow->getChild("ConfirmExit")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&EditorMode::hideQuitMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("ConfirmExit/NoOption")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::hideQuitMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("ConfirmExit/YesOption")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onClickYesQuitMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("ConfirmLoad/NoOption")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::hideConfirmMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("ConfirmLoad/YesOption")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onYesConfirmMenu, this)
    ));

    addEventConnection(
        mRootWindow->getChild("LevelWindowFrame/LaunchNewLevel")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&EditorMode::launchNewLevelPressed, this)
        )
    );
    
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("File")->getChild("PopupMenu1")
        ->getChild("New")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::showNewLevelDialog, this)
    ));
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("File")->getChild("PopupMenu1")
        ->getChild("Quit")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onClickYesQuitMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("File")->getChild("PopupMenu1")
        ->getChild("Load")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::showEditorLoadMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("File")->getChild("PopupMenu1")
        ->getChild("Save")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::quickSavePopUpMenu, this)
    ));      
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("File")->getChild("PopupMenu1")
        ->getChild("SaveAs")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::showEditorSaveMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("Edit")->getChild("PopupMenu3")
        ->getChild("Copy")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onEditCopy, this)
    ));
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("Edit")->getChild("PopupMenu3")
        ->getChild("Paste")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onEditPaste, this)
    ));    
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("Edit")->getChild("PopupMenu3")
        ->getChild("Delete")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onEditDelete, this)
    ));
    addEventConnection(
        mRootWindow->getChild("Menubar")->getChild("Edit")->getChild("PopupMenu3")
        ->getChild("MirrorX")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onEditMirrorX, this)
    ));

    
    addEventConnection(
        mRootWindow->getChild("LevelWindowFrame")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&EditorMode::hideNewLevelDialog, this)
    ));
   
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
        ->getChild("BackButton")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::hideEditorLoadMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&EditorMode::hideEditorLoadMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
        ->getChild("FilePath")->subscribeEvent(
            CEGUI::Editbox::EventTextAccepted,
            CEGUI::Event::Subscriber(&EditorMode::loadMenuFilePathTextChanged, this)
    ));    
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
        ->getChild("LevelSelect")->subscribeEvent(
            CEGUI::Editbox::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::loadMenuLevelSelectSelected, this)
            ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
        ->getChild("LevelSelect")->subscribeEvent(
            CEGUI::Editbox::EventMouseDoubleClick,
            CEGUI::Event::Subscriber(&EditorMode::loadMenuLevelDoubleClicked, this)
            ));    
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("BackButton")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::hideEditorSaveMenu, this)
    ));

    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("SaveButton")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::saveMenuSaveButtonClicked, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
        ->getChild("LoadButton")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::loadMenuLevelDoubleClicked, this)
    ));
    
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&EditorMode::hideEditorSaveMenu, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("FilePath")->subscribeEvent(
            CEGUI::Editbox::EventTextAccepted,
            CEGUI::Event::Subscriber(&EditorMode::saveMenuFilePathTextChanged, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("FileName")->subscribeEvent(
            CEGUI::Editbox::EventTextAccepted,
            CEGUI::Event::Subscriber(&EditorMode::saveMenuFileNameTextChanged, this)
    ));     
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("LevelSelect")->subscribeEvent(
            CEGUI::Editbox::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::saveMenuLevelSelectSelected, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("LevelSelect")->subscribeEvent(
            CEGUI::Editbox::EventMouseDoubleClick,
            CEGUI::Event::Subscriber(&EditorMode::saveMenuSaveButtonClicked, this)
    ));
    
    for(unsigned int ii = 0 ;  ii < mGameMap->numClassDescriptions()   ; ++ii )
    {
        mGameMap->getClassDescription(ii);
        CEGUI::Window* ww = CEGUI::WindowManager::getSingletonPtr()->createWindow("OD/MenuItem");
        ww->setText(mGameMap->getClassDescription(ii)->getClassName());
        ww->setName(mGameMap->getClassDescription(ii)->getClassName());
        mRootWindow->getChild("Menubar")->getChild("Creatures")
        ->getChild("PopupMenu4")->addChild(ww);
        addEventConnection(
            ww->subscribeEvent(
                CEGUI::Window::EventMouseClick,
                CEGUI::Event::Subscriber([&, ii ] (const CEGUI::EventArgs& ea) {
                        this->selectCreature(ii,ea);
                        const CreatureDefinition* def = mGameMap->getClassDescription(mCurrentCreatureIndex);
                        if(def == nullptr)
                        {
                            OD_LOG_ERR("unexpected null CreatureDefinition mCurrentCreatureIndex=" + Helper::toString(mCurrentCreatureIndex));
                            return;
                        }
                        ClientNotification *clientNotification = new ClientNotification(
                            ClientNotificationType::editorCreateFighter);
                        clientNotification->mPacket << getModeManager().getInputManager().mSeatIdSelected;
                        clientNotification->mPacket << def->getClassName();
                        ODClient::getSingleton().queueClientNotification(clientNotification);
                    })
                ));
    }
    installRecentlyUsedFilesButtons();
    installSeatsMenuButtons();

    addEventConnection(
        mRootWindow->getChild("OptionsButton")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::toggleOptionsWindow, this)
    ));
    addEventConnection(
        mRootWindow->getChild("EditorOptionsWindow")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&EditorMode::toggleOptionsWindow, this)
    ));
    addEventConnection(
        mRootWindow->getChild("EditorOptionsWindow")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&EditorMode::toggleOptionsWindow, this)
    ));    
    addEventConnection(
        mRootWindow->getChild("EditorOptionsWindow/SaveLevelButton")
        ->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::onSaveButtonClickFromOptions, this)
    ));
    addEventConnection(
        mRootWindow->getChild("EditorOptionsWindow/SettingsButton")
        ->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&EditorMode::showSettingsFromOptions, this)
        )
    );
    addEventConnection(
        mRootWindow->getChild("EditorOptionsWindow/QuitEditorButton")
        ->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::showQuitMenuFromOptions, this)
    ));

    addEventConnection(
        mRootWindow->getChild("EditorOptionsWindow/QuitEditorButton")
        ->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::showQuitMenuFromOptions, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("OnlyLevel")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::saveMenuFilePathTextChanged, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
        ->getChild("OnlyLevel")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::loadMenuFilePathTextChanged, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("HiddenFiles")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::saveMenuFilePathTextChanged, this)
    ));
    addEventConnection(
        mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
        ->getChild("HiddenFiles")->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&EditorMode::loadMenuFilePathTextChanged, this)
    ));

    
    // Connect editor specific buttons (Rooms, traps, spells, tiles, lights, ...)

    //Map light
    connectGuiAction(Gui::EDITOR_MAPLIGHT_BUTTON,
                     AbstractApplicationMode::GuiAction::ButtonPressedMapLight);

    //Tile selection
    connectTileSelect(Gui::EDITOR_CLAIMED_BUTTON,TileVisual::claimedGround);
    connectTileSelect(Gui::EDITOR_GEM_BUTTON,TileVisual::gemGround);
    connectTileSelect(Gui::EDITOR_DIRT_BUTTON,TileVisual::dirtGround);
    connectTileSelect(Gui::EDITOR_GOLD_BUTTON,TileVisual::goldGround);
    connectTileSelect(Gui::EDITOR_LAVA_BUTTON,TileVisual::lavaGround);
    connectTileSelect(Gui::EDITOR_ROCK_BUTTON,TileVisual::rockGround);
    connectTileSelect(Gui::EDITOR_WATER_BUTTON,TileVisual::waterGround);

    addEventConnection(
        mRootWindow->getChild(Gui::BUTTON_TEMPLE)->subscribeEvent(
          CEGUI::PushButton::EventClicked,
          CEGUI::Event::Subscriber(RoomSelector(RoomType::dungeonTemple, mPlayerSelection))
        )
    );

    addEventConnection(
        mRootWindow->getChild(Gui::BUTTON_PORTAL)->subscribeEvent(
          CEGUI::PushButton::EventClicked,
          CEGUI::Event::Subscriber(RoomSelector(RoomType::portal, mPlayerSelection))
        )
    );

    // Fills the Level type combo box with the available level types.
    const CEGUI::Image* selImg = &CEGUI::ImageManager
    ::getSingleton().get("OpenDungeonsSkin/SelectionBrush");
    CEGUI::Combobox* levelTypeCb = static_cast<CEGUI::Combobox*>
    (mRootWindow->getChild("LevelWindowFrame/LevelTypeSelect"));
    levelTypeCb->resetList();

    CEGUI::ListboxTextItem* item =
    new CEGUI::ListboxTextItem("Skirmish Level", 0);
    item->setSelectionBrushImage(selImg);
    levelTypeCb->addItem(item);

    item = new CEGUI::ListboxTextItem("Multiplayer Level", 1);
    item->setSelectionBrushImage(selImg);
    levelTypeCb->addItem(item);

    
    // configureMenu(mRootWindow);

    updateFlagColor();

    syncTabButtonTooltips(Gui::EDITOR);
}

void EditorMode::activate()
{
    // Loads the corresponding Gui sheet.
    getModeManager().getGui().loadGuiSheet(Gui::editorModeGui);

    // We free the menu scene as it is not required anymore
    ODFrameListener::getSingleton().freeMainMenuScene();

    CEGUI::EventArgs args;
    
    CEGUI::Window* guiSheet = mRootWindow;
    guiSheet->getChild("LevelWindowFrame")->hide();
    guiSheet->getChild("EditorOptionsWindow")->hide();
    guiSheet->getChild("ConfirmExit")->hide();
    guiSheet->getChild("ConfirmLoad")->hide();
    // Hide also the Replay check-box as it doesn't make sense for the editor
    guiSheet->getChild("ConfirmExit/SaveReplayCheckbox")->hide();
    guiSheet->getChild("GameChatWindow/GameChatEditBox")->hide();
    guiSheet->getChild("MenuEditorLoad")->hide();
    guiSheet->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
    ->getChild("FilePath")->setText(getEnv("HOME"));
    guiSheet->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
    ->getChild("FilePath")->fireEvent(CEGUI::Editbox::EventTextAccepted,args); 
    guiSheet->getChild("MenuEditorSave")->hide();
    guiSheet->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
    ->getChild("FilePath")->setText(getEnv("HOME"));
    guiSheet->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
    ->getChild("FilePath")->fireEvent(CEGUI::Editbox::EventTextAccepted,args);     
    CEGUI::Combobox* levelTypeCb = static_cast<CEGUI::Combobox*>
    (mRootWindow->getChild("LevelWindowFrame/LevelTypeSelect"));
    levelTypeCb->setItemSelectState(static_cast<size_t>(0), true);
    
    giveFocus();

    // Stop the game music.
    MusicPlayer::getSingleton().stop();

    // By default, we set the current seat id to the connected player
    Player* player = mGameMap->getLocalPlayer();
    getModeManager().getInputManager().mSeatIdSelected = player->getSeat()->getId();

    refreshGuiSkill();

}

bool EditorMode::mouseMoved(const OIS::MouseEvent &arg)
{
    AbstractApplicationMode::mouseMoved(arg);

    if (!isConnected())
        return true;

    if (mCurrentInputMode == InputModeChat || mCurrentInputMode == InputModeSave
        || mCurrentInputMode == InputModeLoad || mCurrentInputMode == InputModeNew)
        return true;
    
    InputManager& inputManager = mModeManager->getInputManager();
    inputManager.mCommandState = (inputManager.mLMouseDown ? InputCommandState::building
                                  : InputCommandState::infoOnly);


    if(inputManager.mMouseDownOnDraggableTileContainer)
    {
        Ogre::Vector3 mKeeperHandPosOverBlock2;
        if(ODFrameListener::getSingleton().findWorldPositionFromMouse(
               arg,mKeeperHandPosOverBlock2,inputManager.mKeeperHandPosOverBlock.z))
        {
            ClientNotification* clientNotification = new ClientNotification(ClientNotificationType::editorAskSetRoundedPositionDraggableTileContainer);
            clientNotification->mPacket << mKeeperHandPosOverBlock2.x;
            clientNotification->mPacket << mKeeperHandPosOverBlock2.y;
            clientNotification->mPacket << inputManager.offsetDraggableTileContainer.x;
            clientNotification->mPacket << inputManager.offsetDraggableTileContainer.y;
            ODClient::getSingleton().queueClientNotification(clientNotification);
        }
    }
    // If we have a room/trap/spell selected, show it
    // TODO: This should be changed, or combined with an icon or something later.
    TextRenderer& textRenderer = TextRenderer::getSingleton();
    textRenderer.moveText(ODApplication::POINTER_INFO_STRING,
        static_cast<Ogre::Real>(arg.state.X.abs + 30), static_cast<Ogre::Real>(arg.state.Y.abs));

    // We notify current selection input
    checkInputCommand();

    handleMouseWheel(arg);

    // Since this is a tile selection query we loop over the result set
    // and look for the first object which is actually a tile.
    Ogre::Vector3 keeperHandPos;
    if(!ODFrameListener::getSingleton().findWorldPositionFromMouse(
           arg, keeperHandPos,RenderManager::KEEPER_HAND_WORLD_Z))
        return true;

    RenderManager::getSingleton().moveWorldCoords(keeperHandPos.x, keeperHandPos.y);

    int tileX = Helper::round(keeperHandPos.x);
    int tileY = Helper::round(keeperHandPos.y);
    Tile* tileClicked = mGameMap->getTile(tileX, tileY);
    if(tileClicked == nullptr)
        return true;

    std::vector<GameEntity*> entities;
    tileClicked->fillWithEntities(entities, SelectionEntityWanted::creatureAlive,
                                  mGameMap->getLocalPlayer());
    // We search the closest creature alive
    Creature* closestCreature = nullptr;
    double closestDist = 0;
    for(GameEntity* entity : entities)
    {
        if(entity->getObjectType() != GameEntityType::creature)
        {
            OD_LOG_ERR("entityName=" + entity->getName() + ", entityType=" + Helper::toString(static_cast<uint32_t>(entity->getObjectType())));
            continue;
        }

        const Ogre::Vector3& entityPos = entity->getPosition();
        double dist = Pathfinding::squaredDistance(entityPos.x, keeperHandPos.x, entityPos.y, keeperHandPos.y);
        if(closestCreature == nullptr)
        {
            closestDist = dist;
            closestCreature = static_cast<Creature*>(entity);
            continue;
        }

        if(dist >= closestDist)
            continue;

        closestDist = dist;
        closestCreature = static_cast<Creature*>(entity);
    }

    if(closestCreature != nullptr)
    {
        RenderManager::getSingleton().rrTemporaryDisplayCreaturesTextOverlay(closestCreature, 0.5f);
    }

    inputManager.mXPos = tileClicked->getX();
    inputManager.mYPos = tileClicked->getY();
    if (mMouseX != inputManager.mXPos || mMouseY != inputManager.mYPos)
    {
        mMouseX = inputManager.mXPos;
        mMouseY = inputManager.mYPos;
        updateCursorText();
    }
    if(mTileMarker.isExtensible)
    {
        mTileMarker.setPoint( Ogre::Vector2( inputManager.mXPos, inputManager.mYPos  ));
    }

    
    return true;
}

void EditorMode::handleMouseWheel(const OIS::MouseEvent& arg)
{
    ODFrameListener& frameListener = ODFrameListener::getSingleton();

    if (arg.state.Z.rel > 0)
    {
        if (getKeyboard()->isModifierDown(OIS::Keyboard::Ctrl))
        {
            mGameMap->getLocalPlayer()->rotateHand(Player::Direction::left);
        }
        else
        {
            frameListener.moveCamera(CameraManager::moveDown);
        }
    }
    else if (arg.state.Z.rel < 0)
    {
        if (getKeyboard()->isModifierDown(OIS::Keyboard::Ctrl))
        {
            mGameMap->getLocalPlayer()->rotateHand(Player::Direction::right);
        }
        else
        {
            frameListener.moveCamera(CameraManager::moveUp);
        }
    }
}

bool EditorMode::mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonDown(
        Gui::convertButton(id));

    if (!isConnected())
        return true;

    CEGUI::Window *tempWindow = CEGUI::System::getSingleton().getDefaultGUIContext().getWindowContainingMouse();

    InputManager& inputManager = mModeManager->getInputManager();

    // If the mouse press is on a CEGUI window ignore it
    if (tempWindow != nullptr && tempWindow->getName().compare("EDITORGUI") != 0)
    {
        inputManager.mMouseDownOnCEGUIWindow = true;
        return true;
    }

    inputManager.mMouseDownOnCEGUIWindow = false;

    if(mGameMap->getLocalPlayer() == nullptr)
    {
        static bool log = true;
        if(log)
        {
            log = false;
            OD_LOG_ERR("LOCAL PLAYER DOES NOT EXIST!!");
        }
        return true;
    }

    // There is a bug in OIS. When playing in windowed mode, if we clic outside the window
    // and then we restore the window, we will receive a clic event on the last place where
    // the mouse was.
    Ogre::RenderWindow* mainWindows = static_cast<Ogre::RenderWindow*>(
        Ogre::Root::getSingleton().getRenderTarget("OpenDungeons " + ODApplication::VERSION));
    if((!mainWindows->isFullScreen()) &&
       ((arg.state.X.abs == 0) || (arg.state.Y.abs == 0) ||
        (static_cast<Ogre::uint32>(arg.state.X.abs) == mainWindows->getWidth()) ||
        (static_cast<Ogre::uint32>(arg.state.Y.abs) == mainWindows->getHeight())))
    {
        return true;
    }

    Ogre::Vector3 keeperHandPos;
    if(!ODFrameListener::getSingleton().findWorldPositionFromMouse(
           arg, keeperHandPos,RenderManager::KEEPER_HAND_WORLD_Z))
        return true;
    
    if(!ODFrameListener::getSingleton().findWorldPositionFromMouse(
           arg, keeperHandPos,RenderManager::getSingletonPtr()
           ->getSceneManager()->getRootSceneNode()->getChild("Draggable_scene_node")->getPosition().z))
        return true;

            
    RenderManager::getSingleton().moveWorldCoords(keeperHandPos.x, keeperHandPos.y);

    int tileX = Helper::round(keeperHandPos.x);
    int tileY = Helper::round(keeperHandPos.y);
    Tile* tileClicked = mGameMap->getTile(tileX, tileY);
    if(tileClicked == nullptr)
        return true;

    if (id == OIS::MB_Middle)
    {
        mTileMarker.isVisible = false;
        // See if the mouse is over any entity that might display a stats window
        std::vector<GameEntity*> entities;
        tileClicked->fillWithEntities(entities, SelectionEntityWanted::any, mGameMap->getLocalPlayer());
        // We search the closest creature alive
        GameEntity* closestEntity = nullptr;
        double closestDist = 0;
        for(GameEntity* entity : entities)
        {
            if(!entity->canDisplayStatsWindow(mGameMap->getLocalPlayer()->getSeat()))
                continue;

            const Ogre::Vector3& entityPos = entity->getPosition();
            double dist = Pathfinding::squaredDistance(entityPos.x, keeperHandPos.x, entityPos.y, keeperHandPos.y);
            if(closestEntity == nullptr)
            {
                closestDist = dist;
                closestEntity = entity;
                continue;
            }

            if(dist >= closestDist)
                continue;

            closestDist = dist;
            closestEntity = entity;
        }

        if(closestEntity == nullptr)
            return true;

        closestEntity->createStatsWindow();

        return true;
    }

    // Right mouse button down
    if (id == OIS::MB_Right)
    {
        mTileMarker.isVisible = false;
        inputManager.mRMouseDown = true;
        inputManager.mRStartDragX = inputManager.mXPos;
        inputManager.mRStartDragY = inputManager.mYPos;

        // Stop creating rooms, traps, etc.
        unselectAllTiles();
        mCurrentTileVisual = TileVisual::nullTileVisual;
        TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");
        // If we have a currently selected action, we cancel it and don't try to slap or
        // drop what we have in hand
        if(mPlayerSelection.getCurrentAction() != SelectedAction::none)
        {
            mPlayerSelection.setCurrentAction(SelectedAction::none);
            return true;
        }

        // If we right clicked with the mouse over a valid map tile, try to drop a creature onto the map.
        Tile* curTile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);

        if (curTile == nullptr)
            return true;


        if(mGameMap->getLocalPlayer()->numObjectsInHand() > 0)
        {
            // If we right clicked with the mouse over a valid map tile, try to drop what we have in hand on the map.
            Tile* curTile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);

            if (curTile == nullptr)
                return true;

            if (mGameMap->getLocalPlayer()->isDropHandPossible(curTile, 0))
            {
                if(ODClient::getSingleton().isConnected())
                {
                    // Send a message to the server telling it we want to drop the creature
                    ClientNotification *clientNotification = new ClientNotification(
                        ClientNotificationType::askHandDrop);
                    mGameMap->tileToPacket(clientNotification->mPacket, curTile);
                    ODClient::getSingleton().queueClientNotification(clientNotification);
                    mModifiedMapBit = true;
                }

                return true;
            }
        }
        else
        {
            // No creature in hand. We check if we want to slap something
            std::vector<GameEntity*> entities;
            tileClicked->fillWithEntities(entities, SelectionEntityWanted::any, mGameMap->getLocalPlayer());
            // We search the closest creature alive
            GameEntity* closestEntity = nullptr;
            double closestDist = 0;
            for(GameEntity* entity : entities)
            {
                if(!entity->canSlap(mGameMap->getLocalPlayer()->getSeat()))
                    continue;

                const Ogre::Vector3& entityPos = entity->getPosition();
                double dist = Pathfinding::squaredDistance(entityPos.x, keeperHandPos.x, entityPos.y, keeperHandPos.y);
                if(closestEntity == nullptr)
                {
                    closestDist = dist;
                    closestEntity = entity;
                    continue;
                }

                if(dist >= closestDist)
                    continue;

                closestDist = dist;
                closestEntity = entity;
            }

            if(closestEntity != nullptr)
            {
                ODClient::getSingleton().queueClientNotification(ClientNotificationType::askSlapEntity,
                     closestEntity->getObjectType(),
                     closestEntity->getName());
                return true;
            }
        }
    }

    if (id != OIS::MB_Left)
        return true;


    if(draggableTileContainer)
        if(ODFrameListener::getSingleton().rayIntersectionGameMap(
               arg, inputManager.mKeeperHandPosOverBlock, draggableTileContainer))
        {
            inputManager.mMouseDownOnDraggableTileContainer = true;
            inputManager.offsetDraggableTileContainer =
            draggableTileContainer->getPosition() - Ogre::Vector2(
                inputManager.mKeeperHandPosOverBlock.x,inputManager.mKeeperHandPosOverBlock.y) ;
            return true;
        }


    
    // Left mouse button down
    inputManager.mLMouseDown = true;
    inputManager.mLStartDragX = inputManager.mXPos;
    inputManager.mLStartDragY = inputManager.mYPos;

    mTileMarker.isVisible = true;
    mTileMarker.isExtensible = true;
    mTileMarker.setMark(Ogre::Vector2(inputManager.mLStartDragX,inputManager.mLStartDragY));
    mTileMarker.setPoint(Ogre::Vector2(inputManager.mLStartDragX,inputManager.mLStartDragY));

    
    // Check whether the player is already placing rooms or traps.
    if (mPlayerSelection.getCurrentAction() != SelectedAction::none)
    {
        // Skip picking up creatures when placing rooms or traps
        // as creatures often get in the way.
        return true;
    }

    // See if the mouse is over any pickup-able entity
    std::vector<GameEntity*> entities;
    tileClicked->fillWithEntities(entities, SelectionEntityWanted::any, mGameMap->getLocalPlayer());
    // We search the closest creature alive
    GameEntity* closestEntity = nullptr;
    double closestDist = 0;
    for(GameEntity* entity : entities)
    {
        if(!entity->canSlap(mGameMap->getLocalPlayer()->getSeat()))
            continue;

        const Ogre::Vector3& entityPos = entity->getPosition();
        double dist = Pathfinding::squaredDistance(entityPos.x, keeperHandPos.x, entityPos.y, keeperHandPos.y);
        if(closestEntity == nullptr)
        {
            closestDist = dist;
            closestEntity = entity;
            continue;
        }

        if(dist >= closestDist)
            continue;

        closestDist = dist;
        closestEntity = entity;
    }

    if(closestEntity != nullptr)
    {
        ODClient::getSingleton().queueClientNotification(ClientNotificationType::askEntityPickUp,
            closestEntity->getObjectType(),
            closestEntity->getName());
        return true;
    }

    // If we are doing nothing and we click on a tile, it is a tile selection
    if(mPlayerSelection.getCurrentAction() == SelectedAction::none)
        mPlayerSelection.setCurrentAction(SelectedAction::selectTile);


    return true;
}

bool EditorMode::mouseReleased(const OIS::MouseEvent& arg, OIS::MouseButtonID id)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonUp(Gui::convertButton(id));

    InputManager& inputManager = mModeManager->getInputManager();
    inputManager.mCommandState = InputCommandState::validated;
    // If the mouse press was on a CEGUI window ignore it
    if (inputManager.mMouseDownOnCEGUIWindow)
        return true;

    // Right mouse button up
    if (id == OIS::MB_Right)
    {
        inputManager.mRMouseDown = false;
        return true;
    }

    if (id != OIS::MB_Left)
        return true;

    // Left mouse button up
    inputManager.mLMouseDown = false;

    inputManager.mMouseDownOnDraggableTileContainer = false;
    
    // We notify current selection input
    checkInputCommand();

    // mTileMarker.isVisible = false;
    mTileMarker.isExtensible = false;
    return true;
}

void EditorMode::updateCursorText()
{
    // Gets the current action from the drag type
    std::stringstream textSS;

    // Update the fullness info
    CEGUI::Window *posWin = mRootWindow->getChild(Gui::EDITOR_FULLNESS);
    textSS.str("");
    textSS << "Tile Fullness (T): " << mCurrentFullness << "%";
    posWin->setText(textSS.str());

    // Update the cursor position
    posWin = mRootWindow->getChild(Gui::EDITOR_CURSOR_POS);
    textSS.str("");
    textSS << "Cursor: x: " << mMouseX << ", y: " << mMouseY;
    posWin->setText(textSS.str());

    // Update the seat id
    posWin = mRootWindow->getChild(Gui::EDITOR_SEAT_ID);
    textSS.str("");
    textSS << "Seat id (Y): " << getModeManager().getInputManager().mSeatIdSelected;
    posWin->setText(textSS.str());

    // Update the seat id
    posWin = mRootWindow->getChild(Gui::EDITOR_CREATURE_SPAWN);
    textSS.str("");
    const CreatureDefinition* def = mGameMap->getClassDescription(mCurrentCreatureIndex);
    if(def == nullptr)
        textSS << "Creature (C): ?";
    else
        textSS << "Creature (C): " << def->getClassName();

    posWin->setText(textSS.str());
}

bool EditorMode::keyPressed(const OIS::KeyEvent &arg)
{
    // Inject key to the gui currently displayed
    CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyDown(static_cast<CEGUI::Key::Scan>(arg.key));
    CEGUI::System::getSingleton().getDefaultGUIContext().injectChar(arg.text);

    if (mCurrentInputMode == InputModeChat || mCurrentInputMode == InputModeSave || mCurrentInputMode == InputModeLoad || mCurrentInputMode == InputModeNew)
        return true;

    if (mCurrentInputMode == InputModeConsole)
        return getConsole()->keyPressed(arg);

    ODFrameListener& frameListener = ODFrameListener::getSingleton();

    switch (arg.key)
    {
    case OIS::KC_F5:
        onSaveButtonClickFromOptions();
        break;

    case OIS::KC_F10:
        toggleOptionsWindow();
        break;

    case OIS::KC_F11:
        frameListener.toggleDebugInfo();
        break;

    case OIS::KC_GRAVE:
    case OIS::KC_F12:
        enterConsole();
        break;

    case OIS::KC_LEFT:
    case OIS::KC_A:
        frameListener.moveCamera(CameraManager::Direction::moveLeft);
        break;

    case OIS::KC_RIGHT:
    case OIS::KC_D:
        frameListener.moveCamera(CameraManager::Direction::moveRight);
        break;

    case OIS::KC_UP:
    case OIS::KC_W:
        frameListener.moveCamera(CameraManager::Direction::moveForward);
        break;

    case OIS::KC_DOWN:
    case OIS::KC_S:
        frameListener.moveCamera(CameraManager::Direction::moveBackward);
        break;

    case OIS::KC_Q:
        frameListener.moveCamera(CameraManager::Direction::rotateLeft);
        break;

    case OIS::KC_E:
        frameListener.moveCamera(CameraManager::Direction::rotateRight);
        break;

    case OIS::KC_PGUP:
        frameListener.moveCamera(CameraManager::Direction::rotateUp);
        break;

    case OIS::KC_PGDOWN:
        frameListener.moveCamera(CameraManager::Direction::rotateDown);
        break;

    case OIS::KC_HOME:
        frameListener.moveCamera(CameraManager::Direction::moveUp);
        break;

    case OIS::KC_END:
        frameListener.moveCamera(CameraManager::Direction::moveDown);
        break;

    //Toggle mCurrentFullness
    case OIS::KC_T:
        mCurrentFullness = Tile::nextTileFullness(static_cast<int>(mCurrentFullness));
        updateCursorText();
        break;

    //Toggle selected seat ID
    case OIS::KC_Y:
        getModeManager().getInputManager().mSeatIdSelected = mGameMap->nextSeatId(getModeManager().getInputManager().mSeatIdSelected);
        updateCursorText();
        updateFlagColor();
        break;

    //Toggle mCurrentCreatureIndex
    case OIS::KC_C:
        if( getKeyboard()->isModifierDown(OIS::Keyboard::Ctrl))
        {
            onEditCopy();
        }
        else if(++mCurrentCreatureIndex >= mGameMap->numClassDescriptions())
        {
            mCurrentCreatureIndex = 0;
        }
        updateCursorText();
        break;

    case OIS::KC_V:
        if( getKeyboard()->isModifierDown(OIS::Keyboard::Ctrl))
        {
            onEditPaste();
        }
        break;

    case OIS::KC_DELETE:
        onEditDelete();
        break;
    // Quit the Editor Mode
    case OIS::KC_ESCAPE:
        showQuitMenu();
        break;

    // Print a screenshot
    case OIS::KC_SYSRQ:
        ResourceManager::getSingleton().takeScreenshot(frameListener.getRenderWindow());
        break;

    case OIS::KC_1:
    case OIS::KC_2:
    case OIS::KC_3:
    case OIS::KC_4:
    case OIS::KC_5:
    case OIS::KC_6:
    case OIS::KC_7:
    case OIS::KC_8:
    case OIS::KC_9:
    case OIS::KC_0:
        handleHotkeys(arg.key);
        break;

    default:
        break;
    }

    return true;
}

bool EditorMode::keyReleased(const OIS::KeyEvent& arg)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyUp(static_cast<CEGUI::Key::Scan>(arg.key));

    if (mCurrentInputMode == InputModeChat || mCurrentInputMode == InputModeConsole)
        return true;

    ODFrameListener& frameListener = ODFrameListener::getSingleton();

    switch (arg.key)
    {
    case OIS::KC_LEFT:
    case OIS::KC_A:
        frameListener.moveCamera(CameraManager::Direction::stopLeft);
        break;

    case OIS::KC_RIGHT:
    case OIS::KC_D:
        frameListener.moveCamera(CameraManager::Direction::stopRight);
        break;

    case OIS::KC_UP:
    case OIS::KC_W:
        frameListener.moveCamera(CameraManager::Direction::stopForward);
        break;

    case OIS::KC_DOWN:
    case OIS::KC_S:
        frameListener.moveCamera(CameraManager::Direction::stopBackward);
        break;

    case OIS::KC_Q:
        frameListener.moveCamera(CameraManager::Direction::stopRotLeft);
        break;

    case OIS::KC_E:
        frameListener.moveCamera(CameraManager::Direction::stopRotRight);
        break;

    case OIS::KC_PGUP:
        frameListener.moveCamera(CameraManager::Direction::stopRotUp);
        break;

    case OIS::KC_PGDOWN:
        frameListener.moveCamera(CameraManager::Direction::stopRotDown);
        break;

    case OIS::KC_HOME:
        frameListener.moveCamera(CameraManager::Direction::stopDown);
        break;

    case OIS::KC_END:
        frameListener.moveCamera(CameraManager::Direction::stopUp);
        break;

    default:
        break;
    }

    return true;
}

void EditorMode::handleHotkeys(OIS::KeyCode keycode)
{
    ODFrameListener& frameListener = ODFrameListener::getSingleton();
    InputManager& inputManager = mModeManager->getInputManager();

    // keycode minus two because the codes are shifted by two against the actual number
    unsigned int keynumber = keycode - 2;

    if (getKeyboard()->isModifierDown(OIS::Keyboard::Shift))
    {
        inputManager.mHotkeyLocationIsValid[keynumber] = true;
        inputManager.mHotkeyLocation[keynumber].vv  = frameListener.getCameraManager()
        ->getActiveCameraNode()->getPosition();
        inputManager.mHotkeyLocation[keynumber].qq  = frameListener.getCameraManager()
        ->getActiveCameraNode()->getOrientation();
        inputManager.mHotkeyLocation[keynumber].qq2  = frameListener.getCameraManager()
        ->getActiveCameraNode()->getChild(0)->getOrientation();        
    }
    else if (inputManager.mHotkeyLocationIsValid[keynumber])
    {
        frameListener.getCameraManager()->getActiveCameraNode()
        ->setPosition(inputManager.mHotkeyLocation[keynumber].vv);
        frameListener.getCameraManager()->getActiveCameraNode()
        ->setOrientation(  inputManager.mHotkeyLocation[keynumber].qq);
        frameListener.getCameraManager()->getActiveCameraNode()
        ->getChild(0)->setOrientation(  inputManager.mHotkeyLocation[keynumber].qq2);                
    }
}

void EditorMode::onEditCopy()
{

    ClientNotification* notif1 = new ClientNotification(ClientNotificationType::editorAskCreateDraggableTileContainer);
         
    notif1->mPacket << static_cast<int>(mTileMarker.getMaxX() - mTileMarker.getMinX() + 1);
    notif1->mPacket << static_cast<int>(mTileMarker.getMaxY() - mTileMarker.getMinY() + 1);
    notif1->mPacket << static_cast<int>(std::min(mTileMarker.mark.x,mTileMarker.point.x));
    notif1->mPacket << static_cast<int>(std::min(mTileMarker.mark.y,mTileMarker.point.y));
    notif1->mPacket << static_cast<int>(mTileMarker.getMinX());
    notif1->mPacket << static_cast<int>(mTileMarker.getMinY());
    notif1->mPacket << static_cast<int>(mTileMarker.getMaxX());
    notif1->mPacket << static_cast<int>(mTileMarker.getMaxY());
         
        
    ODClient::getSingleton().queueClientNotification(notif1);
        

        

    // demand from server recreating of all gameEntities ( since we created the traps and tiles ...)             

}


void EditorMode::onEditPaste()
{
    if(draggableTileContainer!=nullptr)
    {
        mGameMap->askServerCopyTilesWithOffsetFrom(*draggableTileContainer,0,0,
                                                   draggableTileContainer->getMapSizeX(),draggableTileContainer->getMapSizeY(),
                                                   draggableTileContainer->getPosition().x, draggableTileContainer->getPosition().y );

        mGameMap->askServerCopyRoomsWithOffsetFrom(*draggableTileContainer,0,0,
                                                   draggableTileContainer->getMapSizeX(),draggableTileContainer->getMapSizeY(),
                                                   draggableTileContainer->getPosition().x, draggableTileContainer->getPosition().y );        

        mGameMap->askServerCopyTrapsWithOffsetFrom(*draggableTileContainer,0,0,
                                                   draggableTileContainer->getMapSizeX(),draggableTileContainer->getMapSizeY(),
                                                   draggableTileContainer->getPosition().x, draggableTileContainer->getPosition().y );


        
        mModifiedMapBit = true;
    }
}

void EditorMode::onEditDelete()
{

    ClientNotification* notif1 = new ClientNotification(ClientNotificationType::editorAskDeleteDraggableTileContainer);
    ODClient::getSingleton().queueClientNotification(notif1);
}

void EditorMode::onEditMirrorX()
{

    ClientNotification* notif1 = new ClientNotification(ClientNotificationType::editorAskDeleteDraggableTileContainer);
    ODClient::getSingleton().queueClientNotification(notif1);
}


//! Rendering methods
void EditorMode::onFrameStarted(const Ogre::FrameEvent& evt)
{
    
    if( mTileMarker.isVisible )
    {
        Ogre::ColourValue cv;
        cv.setAsRGBA(0x77333300);
        DebugDrawer::getSingleton().drawCuboid(mTileMarker.getAABB()
                                               .getAllCorners().data(), cv, true);

    }
    DebugDrawer::getSingleton().build();
    GameEditorModeBase::onFrameStarted(evt);
}

void EditorMode::onFrameEnded(const Ogre::FrameEvent& evt)
{
    DebugDrawer::getSingleton().clear();
}

void EditorMode::notifyGuiAction(GuiAction guiAction)
{
    switch(guiAction)
    {
            case GuiAction::ButtonPressedCreatureWorker:
            {
                if(ODClient::getSingleton().isConnected())
                {
                    ClientNotification *clientNotification = new ClientNotification(
                        ClientNotificationType::editorCreateWorker);
                    clientNotification->mPacket << getModeManager().getInputManager().mSeatIdSelected;
                    ODClient::getSingleton().queueClientNotification(clientNotification);
                }
                break;
            }
            case GuiAction::ButtonPressedCreatureFighter:
            {
                if(ODClient::getSingleton().isConnected())
                {
                    const CreatureDefinition* def = mGameMap->getClassDescription(mCurrentCreatureIndex);
                    if(def == nullptr)
                    {
                        OD_LOG_ERR("unexpected null CreatureDefinition mCurrentCreatureIndex=" + Helper::toString(mCurrentCreatureIndex));
                        break;
                    }
                    ClientNotification *clientNotification = new ClientNotification(
                        ClientNotificationType::editorCreateFighter);
                    clientNotification->mPacket << getModeManager().getInputManager().mSeatIdSelected;
                    clientNotification->mPacket << def->getClassName();
                    ODClient::getSingleton().queueClientNotification(clientNotification);
                }
                break;
            }
            case GuiAction::ButtonPressedMapLight:
            {
                if(ODClient::getSingleton().isConnected())
                {
                    ClientNotification *clientNotification = new ClientNotification(
                        ClientNotificationType::editorAskCreateMapLight);
                    ODClient::getSingleton().queueClientNotification(clientNotification);
                }
                break;
            }
            default:
                break;
    }
}

bool EditorMode::showNewLevelDialog(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("LevelWindowFrame")->show();
    mCurrentInputMode = InputModeNew;
    return true;
}

bool EditorMode::hideNewLevelDialog(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("LevelWindowFrame")->hide();
    mCurrentInputMode = InputModeNormal;
    return true;
}


bool EditorMode::toggleOptionsWindow(const CEGUI::EventArgs& /*arg*/)
{
    CEGUI::Window* options = mRootWindow->getChild("EditorOptionsWindow");
    if (options == nullptr)
        return true;

    if (options->isVisible())
        options->hide();
    else
        options->show();
    return true;
}

bool EditorMode::showSettingsFromOptions(const CEGUI::EventArgs& /*e*/)
{
    mRootWindow->getChild("EditorOptionsWindow")->hide();
    mSettings.show();
    return true;
}

bool EditorMode::showQuitMenuFromOptions(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("ConfirmExit")->show();
    mRootWindow->getChild("EditorOptionsWindow")->hide();
    return true;
}

bool EditorMode::onSaveButtonClickFromOptions(const CEGUI::EventArgs& /*arg*/)
{
    if(ODClient::getSingleton().isConnected())
    {
        // Send a message to the server telling it we want to drop the creature
        ClientNotification *clientNotification = new ClientNotification(
            ClientNotificationType::askSaveMap);
        ODClient::getSingleton().queueClientNotification(clientNotification);
    }
    mModifiedMapBit = false;
    return true;
}

bool EditorMode::showQuitMenu(const CEGUI::EventArgs& /*arg*/)
{
    //TODO: Test whether the level was modified and ask accordingly.
    mRootWindow->getChild("ConfirmExit")->show();
    return true;
}

bool EditorMode::hideQuitMenu(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("ConfirmExit")->hide();
    return true;
}

bool EditorMode::hideConfirmMenu(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("ConfirmLoad")->hide();
    return true;
}
bool EditorMode::onYesConfirmMenu(const CEGUI::EventArgs& /*arg*/)
{
    uninstallRecentlyUsedFilesButtons();
    installRecentlyUsedFilesButtons();
    return loadLevelFromFile(dialogFullPath);
}

bool EditorMode::loadLevelFromFile(const std::string& fileName)
{
    // Ogre::MaterialManager::getSingletonPtr()->unload("LiftedGold");    
    Ogre::MaterialManager::getSingletonPtr()->remove("LiftedGold","Graphics");
    Ogre::TextureManager::getSingletonPtr()->remove("smokeTexture", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);


    ODFrameListener::getSingleton().getCameraManager()->destroyCamera("RenderToTexture");
    ODFrameListener::getSingleton().getCameraManager()->destroyCameraNode("RenderToTexture");    
    
    mMainCullingManager->stopTileCulling(mCameraTilesIntersections);
    // eliminating the race condition
    MiniMap* mm;
    mm = mMiniMap;
    mMiniMap = nullptr;
    delete mm;

    CullingManager* cc;
    cc = mMainCullingManager;
    mMainCullingManager = nullptr;
    delete cc;
    
    // Delete the potential pending event messages
    for (EventMessage* message : mEventMessages)
        delete message;
    
    mEventMessages.clear();
    
    if(ODClient::getSingleton().isConnected())
        ODClient::getSingleton().disconnect(mKeepReplayAtDisconnect);
    if(ODServer::getSingleton().isConnected())
        ODServer::getSingleton().stopServer();

    // Now that the server is stopped, we can clear the client game map
    mGameMap->clearAll();
    ConfigManager& config = ConfigManager::getSingleton();    
    std::string nickname = config.getGameValue(Config::NICKNAME, std::string(), false);
    if(!ODServer::getSingleton().startServer(nickname, fileName, ServerMode::ModeEditor, false))
    {
        OD_LOG_ERR("Could not start server for editor !!!");
        return true;
    }

    int port = ODServer::getSingleton().getNetworkPort();
    uint32_t timeout = ConfigManager::getSingleton().getClientConnectionTimeout();
    std::string replayFilename = ResourceManager::getSingleton().getReplayDataPath()
        + ResourceManager::getSingleton().buildReplayFilename();
    // We connect ourself
    if(!ODClient::getSingleton().connect("localhost", port, timeout, replayFilename))
    {
        OD_LOG_ERR("Could not connect to server for editor !!!");
        return true;
    }
    ODClient::getSingleton().processClientSocketMessages(75);
    ODClient::getSingleton().processClientNotifications();
    uninstallSeatsMenuButtons();
    installSeatsMenuButtons();
    mGameMap = ODFrameListener::getSingletonPtr()->getClientGameMap();
    mMiniMap = MiniMap::createMiniMap(mRootWindow->getChild(Gui::MINIMAP));
    mMainCullingManager = new CullingManager(mGameMap, CullingType::SHOW_MAIN_WINDOW);
    mMainCullingManager->startTileCulling(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera(), mCameraTilesIntersections);
    mRootWindow->getChild("ConfirmLoad")->hide();
    mModifiedMapBit = false;
    addToRecentlyUsed(fileName);
    uninstallRecentlyUsedFilesButtons();
    installRecentlyUsedFilesButtons();
    return true;
}


bool EditorMode::quickSavePopUpMenu(const CEGUI::EventArgs& /*arg*/)
{
    if(ODClient::getSingleton().isConnected())
    {
        // Send a message to the server telling it we want to drop the creature
        ODClient::getSingleton().queueClientNotification(ClientNotificationType::askSaveMap);
    }
    mModifiedMapBit = false;
    return true;
}

bool EditorMode::showEditorLoadMenu(const CEGUI::EventArgs& /*arg*/)
{
    //TODO: Test whether the level was modified and ask accordingly.
    mRootWindow->getChild("MenuEditorLoad")->show();
    mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")
    ->getChild("FilePath")->activate();
    mCurrentInputMode = InputModeLoad;
    return true;
}

bool EditorMode::showEditorSaveMenu(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("MenuEditorSave")->show();
    mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
    ->getChild("FilePath")->activate();
    mCurrentInputMode = InputModeSave;
    return true;
}


bool EditorMode::hideEditorLoadMenu(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("MenuEditorLoad")->hide();
    mCurrentInputMode = InputModeNormal;
    return true;
}


bool EditorMode::hideEditorSaveMenu(const CEGUI::EventArgs& /*arg*/)
{
    mRootWindow->getChild("MenuEditorSave")->hide();
    mCurrentInputMode = InputModeNormal;
    return true;
}


bool EditorMode::onClickYesQuitMenu(const CEGUI::EventArgs& /*arg*/)
{
    mModeManager->requestMode(AbstractModeManager::MENU_MAIN);
    return true;
}

bool EditorMode::loadMenuFilePathTextChanged( const CEGUI::EventArgs& /*arg*/)
{

    CEGUI::String ss = mRootWindow->getChild("MenuEditorLoad")
    ->getChild("LevelWindowFrame")->getChild("FilePath")->getText();
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>
    (mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")->getChild("LevelSelect"));


    path pp (ss.c_str());

    try
    {
        if (exists(pp))
        {
            if (is_directory(pp))
            {
                levelSelectList->resetList();
                
                CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem("../");
                item->setID(00);
                item->setSelectionBrushImage("OpenDungeonsSkin/SelectionBrush");
                levelSelectList->addItem(item);
                int nn = 1;
                for (directory_entry& xx : directory_iterator(pp))
                {
                    if(!(isFileHidden(xx.path().filename().generic_string())
                         && !isCheckboxSelected("MenuEditorLoad/LevelWindowFrame/HiddenFiles")))
                    {
                        if(xx.path().has_extension() && xx.path().extension().compare(L".level") == 0)
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(1,0.64,0), nn);
                        }
                    
                        else if(is_regular_file(xx.path())
                                && !isCheckboxSelected("MenuEditorLoad/LevelWindowFrame/OnlyLevel") )
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(0,1,0), nn);
                        }
                        else if(is_directory(xx.path()))
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(0,0,1), nn);
                        }                    
                        else if(!isCheckboxSelected("MenuEditorLoad/LevelWindowFrame/OnlyLevel"))
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(1,0,0), nn);
                        }
                    }
                }
            }
        }
    }

    catch (const filesystem_error& ex)
    {
        std::cerr << ex.what() << '\n';
    }    
    return true;
}

bool EditorMode::loadMenuLevelSelectSelected(const CEGUI::EventArgs& /*arg*/)
{
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>(mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")->getChild("LevelSelect"));
    CEGUI::Editbox* levelEditBox = static_cast<CEGUI::Editbox*>(mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")->getChild("FilePath"));
    CEGUI::EventArgs args;
    CEGUI::String ss = mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")->getChild("FilePath")->getText();

    path ll (levelEditBox->getText().c_str());

    levelSelectList->getFirstSelectedItem();
    if(levelSelectList->getFirstSelectedItem())
    {
        path pp (levelSelectList->getFirstSelectedItem()->getText().c_str());
        if(levelSelectList->getFirstSelectedItem()->getID() ==0)
        {
            levelEditBox->setText((ll.parent_path().generic_string()));
            levelEditBox->fireEvent(CEGUI::Editbox::EventTextAccepted,args);               
        }

        else if(pp.has_extension() && pp.extension().compare(std::string(".level"))==0)
        {
            CEGUI::Window* descTxt =  mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame/MapDescriptionText");
            LevelInfo levelInfo;
            std::string fullPath = (levelEditBox->getText() + "/" + levelSelectList->getFirstSelectedItem()->getText()).c_str();
            if(MapHandler::getMapInfo( fullPath , levelInfo))
                descTxt->setText(levelInfo.mLevelDescription);
            else
                descTxt->setText("Invalid map");        
        }
    
        else if(is_directory(path((levelEditBox->getText() + "/" + levelSelectList->getFirstSelectedItem()->getText()).c_str())))
        {
            levelEditBox->setText(levelEditBox->getText() + "/" + levelSelectList->getFirstSelectedItem()->getText());
            levelEditBox->fireEvent(CEGUI::Editbox::EventTextAccepted,args);
        }
        return true;        
    }
    else
        return false;
}


bool EditorMode::loadMenuLevelDoubleClicked(const CEGUI::EventArgs& /*arg*/)
{
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>(mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")->getChild("LevelSelect"));
    CEGUI::Editbox* levelEditBox = static_cast<CEGUI::Editbox*>(mRootWindow->getChild("MenuEditorLoad")->getChild("LevelWindowFrame")->getChild("FilePath"));
    CEGUI::EventArgs args;
    if(levelSelectList->getFirstSelectedItem())
    {
        path pp (levelSelectList->getFirstSelectedItem()->getText().c_str());
        if(pp.has_extension() && pp.extension().compare(std::string(".level"))==0)
        {
            loadMenuAskForConfirmation((levelEditBox->getText() + "/" + levelSelectList->getFirstSelectedItem()->getText()).c_str());
        }
        return true;        
    }
    else
        return false;
}

void EditorMode::addToRecentlyUsed(const std::string& fileName)
{
    boost::filesystem::path pp (fileName.c_str());
    if(std::find(ConfigManager::getSingleton().getRecentlyUsedFiles().begin(), ConfigManager::getSingleton().getRecentlyUsedFiles().end(),pp)==ConfigManager::getSingleton().getRecentlyUsedFiles().end())    
    {
        ConfigManager::getSingleton().getRecentlyUsedFiles().push_back(pp);
    }

}

void EditorMode::loadMenuAskForConfirmation(const std::string& fileName)
{
    dialogFullPath = fileName;
    if(mModifiedMapBit)
        mRootWindow->getChild("ConfirmLoad")->show();
    else
    {
        loadLevelFromFile(dialogFullPath);
    }
}

bool EditorMode::saveMenuSaveButtonClicked(const CEGUI::EventArgs& /*arg*/)
{

    std::string filePath  = mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")->getChild("FilePath")->getText().c_str();
    std::string fileName = mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")->getChild("FileName")->getText().c_str();
    path pp (filePath.c_str());
    if(ODClient::getSingleton().isConnected() && exists(pp))
    {
        // Send a message to the server telling it we want to drop the creature
        ODClient::getSingleton().queueClientNotification(ClientNotificationType::askSaveMap,filePath, fileName);
    }
    mModifiedMapBit = false;
    return true;
}

bool EditorMode::saveMenuFilePathTextChanged(const CEGUI::EventArgs& /*arg*/)
{

    CEGUI::String ss = mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")->getChild("FilePath")->getText();
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>(mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")->getChild("LevelSelect"));


    path pp (ss.c_str());

    try
    {
        if (exists(pp))
        {
            if (is_directory(pp))
            {
                levelSelectList->resetList();
                
                CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem("../");
                item->setID(00);
                item->setSelectionBrushImage("OpenDungeonsSkin/SelectionBrush");
                levelSelectList->addItem(item);
                int nn = 1;
                for (directory_entry& xx : directory_iterator(pp))
                {
                    if(!(isFileHidden(xx.path().filename().generic_string()) && !isCheckboxSelected("MenuEditorSave/LevelWindowFrame/HiddenFiles")))
                    {
                        if(xx.path().has_extension() && xx.path().extension().compare(std::string(".level")) == 0)
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(1,0.64,0), nn);
                        }
                    
                        else if(is_regular_file(xx.path()) &&  !isCheckboxSelected("MenuEditorSave/LevelWindowFrame/OnlyLevel") )
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(0,1,0), nn);
                        }
                        else if(is_directory(xx.path()))
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(0,0,1), nn);
                        }                    
                        else if(!isCheckboxSelected("MenuEditorSave/LevelWindowFrame/OnlyLevel"))
                        {
                            addPathNameToList(xx, levelSelectList, CEGUI::Colour(1,0,0), nn);
                        }
                    }
                }
            }
        }
    }

    catch (const filesystem_error& ex)
    {
        std::cerr << ex.what() << '\n';
    }    
    return true;
}

bool EditorMode::saveMenuFileNameTextChanged(const CEGUI::EventArgs& /*arg*/)
{

    return true;
}

bool EditorMode::saveMenuLevelSelectSelected(const CEGUI::EventArgs& /*arg*/)
{
    CEGUI::Listbox* levelSelectList = static_cast<CEGUI::Listbox*>
        (mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")->getChild("LevelSelect"));
    CEGUI::Editbox* levelPathEditBox = static_cast<CEGUI::Editbox*>
        (mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")->getChild("FilePath"));
    CEGUI::Editbox* levelNameEditBox = static_cast<CEGUI::Editbox*>
        (mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")->getChild("FileName"));    
    CEGUI::EventArgs args;
    CEGUI::String ss = mRootWindow->getChild("MenuEditorSave")->getChild("LevelWindowFrame")
        ->getChild("FilePath")->getText();

    path ll (levelPathEditBox->getText().c_str());

    if(levelSelectList->getFirstSelectedItem())
    {
        path pp (levelSelectList->getFirstSelectedItem()->getText().c_str());
        if(levelSelectList->getFirstSelectedItem()->getID() ==0)
        {
            levelPathEditBox->setText(ll.parent_path().generic_string());
            levelPathEditBox->fireEvent(CEGUI::Editbox::EventTextAccepted,args);               
        }

        else if(pp.has_extension() && pp.extension().compare(std::string(".level"))==0)
        {
            levelNameEditBox->setText(pp.generic_string());
            levelNameEditBox->fireEvent(CEGUI::Editbox::EventTextAccepted,args);                  
        }
    
        else if(is_directory(path((levelPathEditBox->getText() + "/" + levelSelectList->getFirstSelectedItem()->getText()).c_str())))
        {
            levelPathEditBox->setText(levelPathEditBox->getText() + "/" + levelSelectList->getFirstSelectedItem()->getText());
            levelPathEditBox->fireEvent(CEGUI::Editbox::EventTextAccepted,args);
        }
        return true;        
    }
    else
        return false;
}



void EditorMode::selectCreature( unsigned ii, CEGUI::EventArgs args)
{
    mCurrentCreatureIndex = ii;
    updateCursorText();
}

void EditorMode::refreshGuiSkill()
{
    // We show/hide the icons depending on available skills
    CEGUI::Window* guiSheet = mRootWindow;

    SkillManager::listAllSkills([&](const std::string& skillButtonName, const std::string& castButtonName,
        const std::string& skillProgressBarName, SkillType resType)
    {
        guiSheet->getChild(castButtonName)->show();
    });


    // We also display the editor only buttons
    guiSheet->getChild(Gui::BUTTON_TEMPLE)->show();
    guiSheet->getChild(Gui::BUTTON_PORTAL)->show();
}

void EditorMode::connectTileSelect(const std::string& buttonName, TileVisual tileVisual)
{
    addEventConnection(
        mRootWindow->getChild(buttonName)->subscribeEvent(
          CEGUI::PushButton::EventClicked,
          CEGUI::Event::Subscriber(TileSelector{tileVisual, mPlayerSelection, *this})
        )
    );
}

void EditorMode::updateFlagColor()
{
    std::string colorStr = Helper::getImageColoursStringFromColourValue(
        mGameMap->getSeatById(getModeManager().getInputManager().mSeatIdSelected)->getColorValue());
    mRootWindow->getChild("HorizontalPipe/SeatIdDisplay/Icon")->setProperty("ImageColours", colorStr);
}

void EditorMode::selectSquaredTiles(int tileX1, int tileY1, int tileX2, int tileY2)
{
    // Loop over the tiles in the rectangular selection region and set their setSelected flag accordingly.
    std::vector<Tile*> affectedTiles = mGameMap->rectangularRegion(tileX1,
        tileY1, tileX2, tileY2);

    selectTiles(affectedTiles);
}

void EditorMode::selectTiles(const std::vector<Tile*> tiles)
{
    unselectAllTiles();

    Player* player = mGameMap->getLocalPlayer();
    for(Tile* tile : tiles)
    {
        tile->setSelected(true, player);
    }
}

void EditorMode::unselectAllTiles()
{
    Player* player = mGameMap->getLocalPlayer();
    // Compute selected tiles
    for (int jj = 0; jj < mGameMap->getMapSizeY(); ++jj)
    {
        for (int ii = 0; ii < mGameMap->getMapSizeX(); ++ii)
        {
            mGameMap->getTile(ii, jj)->setSelected(false, player);
        }
    }
}

void EditorMode::displayText(const Ogre::ColourValue& txtColour, const std::string& txt)
{
    TextRenderer& textRenderer = TextRenderer::getSingleton();
    textRenderer.setColor(ODApplication::POINTER_INFO_STRING, txtColour);
    textRenderer.setText(ODApplication::POINTER_INFO_STRING, txt);
}

void EditorMode::checkInputCommand()
{
    // In the editor mode, by default, we do nothing if mouse dragged while no action selected
    const InputManager& inputManager = mModeManager->getInputManager();

    switch(mPlayerSelection.getCurrentAction())
    {
        case SelectedAction::none:
            handlePlayerActionNone();
            return;
        case SelectedAction::selectTile:
            handlePlayerActionSelectTile();
            return;            
        case SelectedAction::changeTile:
            handlePlayerActionChangeTile();
            mModifiedMapBit = true;
            return;
        case SelectedAction::buildRoom:
            RoomManager::checkBuildRoomEditor(mGameMap, mPlayerSelection.getNewRoomType(), inputManager, *this);
            mModifiedMapBit = true;
            return;
        case SelectedAction::destroyRoom:
            RoomManager::checkSellRoomTilesEditor(mGameMap, inputManager, *this);
            mModifiedMapBit = true;
            return;
        case SelectedAction::buildTrap:
            TrapManager::checkBuildTrapEditor(mGameMap, mPlayerSelection.getNewTrapType(), inputManager, *this);
            mModifiedMapBit = true;
            return;
        case SelectedAction::destroyTrap:
            TrapManager::checkSellTrapTilesEditor(mGameMap, inputManager, *this);
            return;
            mModifiedMapBit = true;
        case SelectedAction::castSpell:
            // TODO: create skill entity with corresponding spell
            return;
        default:
            return;
    }
}

void EditorMode::handlePlayerActionChangeTile()
{
    const InputManager& inputManager = mModeManager->getInputManager();
    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
            inputManager.mYPos);
        displayText(Ogre::ColourValue::White, Tile::tileVisualToString(mCurrentTileVisual));
        return;
    }

    if(inputManager.mCommandState == InputCommandState::building)
    {
        selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mLStartDragX,
            inputManager.mLStartDragY);
        displayText(Ogre::ColourValue::White, Tile::tileVisualToString(mCurrentTileVisual));
        return;
    }

    unselectAllTiles();
    displayText(Ogre::ColourValue::White, "");

    TileType tileType = TileType::nullTileType;
    double fullness = mCurrentFullness;
    int seatId = -1;
    switch(mCurrentTileVisual)
    {
        case TileVisual::dirtGround:
            tileType = TileType::dirt;
            break;
        case TileVisual::goldGround:
            tileType = TileType::gold;
            break;
        case TileVisual::rockGround:
            tileType = TileType::rock;
            break;
        case TileVisual::claimedGround:
        case TileVisual::claimedFull:
            tileType = TileType::dirt;
            seatId = inputManager.mSeatIdSelected;
            break;
        case TileVisual::waterGround:
            tileType = TileType::water;
            fullness = 0.0;
            break;
        case TileVisual::lavaGround:
            tileType = TileType::lava;
            fullness = 0.0;
            break;
        case TileVisual::gemGround:
            tileType = TileType::gem;
            break;
        default:
            return;
    }

    ClientNotification *clientNotification = new ClientNotification(
        ClientNotificationType::editorAskChangeTiles);
    clientNotification->mPacket << inputManager.mXPos << inputManager.mYPos;
    clientNotification->mPacket << inputManager.mLStartDragX << inputManager.mLStartDragY;
    clientNotification->mPacket << tileType;
    clientNotification->mPacket << fullness;
    clientNotification->mPacket << seatId;
    ODClient::getSingleton().queueClientNotification(clientNotification);
}

void EditorMode::handlePlayerActionSelectTile()
{
    const InputManager& inputManager = mModeManager->getInputManager();
    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
                           inputManager.mYPos);
        return;
    }

    if(inputManager.mCommandState == InputCommandState::building)
    {
        selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mLStartDragX,
                           inputManager.mLStartDragY);
        return;
    }

    unselectAllTiles();
}

void EditorMode::handlePlayerActionNone()
{
    const InputManager& inputManager = mModeManager->getInputManager();
    // We only display the selection cursor on the hovered tile
    if(inputManager.mCommandState == InputCommandState::validated)
    {
        unselectAllTiles();
        return;
    }

    selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
        inputManager.mYPos);
}

bool EditorMode::updateDescription(const CEGUI::EventArgs&)
{

    return true;
}

std::string EditorMode::getEnv( const std::string & var )
{
    // WINDOWS:
    // "you could look at HOMEDRIVE, HOMEPATH or USERPROFILE
    // env variables on windows. or i guess SHGetFolderPathA()"
    const char * val = std::getenv( var.c_str() );
    if ( val == nullptr )
    { // invalid to assign nullptr to std::string
        return "";
    }
    else
    {
        return val;
    }
}


void EditorMode::uninstallRecentlyUsedFilesButtons()
{
    CEGUI::Window *pm = mRootWindow->getChild("Menubar")->getChild("File")->getChild("PopupMenu1")->getChild("RecentlyUsed")->getChild("PopupMenu2");
    while(pm->getChildCount() > 0)
        pm->removeChild(pm->getChildAtIdx(0));

}

void EditorMode::installRecentlyUsedFilesButtons()
{
    for(auto &ii :  ConfigManager::getSingleton().getRecentlyUsedFiles())
    {
        CEGUI::Window* ww = CEGUI::WindowManager::getSingletonPtr()->createWindow("OD/MenuItem");
        ww->setText(ii.generic_string());
        ww->setName(ii.generic_string());
        mRootWindow->getChild("Menubar")->getChild("File")->getChild("PopupMenu1")->getChild("RecentlyUsed")->getChild("PopupMenu2")->addChild(ww);
        ww->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber([&, ii ] (const CEGUI::EventArgs& ea) {
                    ConfigManager::getSingleton().getRecentlyUsedFiles().erase(
                        find(ConfigManager::getSingleton().getRecentlyUsedFiles().begin(),
                             ConfigManager::getSingleton().getRecentlyUsedFiles().end(), ii));
                    ConfigManager::getSingleton().getRecentlyUsedFiles().linearize();
                    ConfigManager::getSingleton().getRecentlyUsedFiles().push_back(ii);
                    this->loadMenuAskForConfirmation(ii.generic_string());
                    this->uninstallRecentlyUsedFilesButtons();
                    this->installRecentlyUsedFilesButtons();
                })
            );
    }
}


void EditorMode::uninstallSeatsMenuButtons()
{
    CEGUI::Window *pm = mRootWindow->getChild("Menubar")->getChild("Seats")->getChild("PopupMenu5");
    while(pm->getChildCount() > 0)
        pm->removeChild(pm->getChildAtIdx(0));
}

void EditorMode::installSeatsMenuButtons()
{
    for(Seat* seat : mGameMap->getSeats()){
        CEGUI::Window* ww = CEGUI::WindowManager::getSingletonPtr()->createWindow("OD/MenuItem");
        std::stringstream ss;
        ss <<  "[colour='" ;
        ss <<  std::hex << seat->getColorValue().getAsARGB();
        ss << "']" ;
        ss << "Seat";
        ss << seat->getId();
        ww->setText(ss.str());
        ww->setName(ss.str());
        mRootWindow->getChild("Menubar")->getChild("Seats")->getChild("PopupMenu5")->addChild(ww);
        addEventConnection(
            ww->subscribeEvent(
                CEGUI::Window::EventMouseClick,
                CEGUI::Event::Subscriber([&, seat] (const CEGUI::EventArgs& ea) {
                        getModeManager().getInputManager().mSeatIdSelected = seat->getId();
                        updateCursorText();
                        updateFlagColor();   
                    })
                ));

    }

}

bool EditorMode::isCheckboxSelected(const CEGUI::String& checkbox)
{
    // Check
    if (mRootWindow->isChild(checkbox))
    {
        CEGUI::ToggleButton* button = static_cast<CEGUI::ToggleButton*>(mRootWindow->getChild(checkbox));
        return button->isSelected();
    }
    return false;
}


bool EditorMode::isFileHidden(std::string path)
{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
    
    //DWORD attributes = GetFileAttributes(path);
    //return (attributes & FILE_ATTRIBUTE_HIDDEN);
    return false;
#else
    return (path[0] == '.');
#endif    
}


void EditorMode::addPathNameToList(directory_entry& xx, CEGUI::Listbox* levelSelectList, CEGUI::Colour cc, int& nn )
{
    CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(xx.path().generic_string());
    item->setTextColours(cc);
    item->setText(xx.path().filename().generic_string());
    item->setID(nn);
    item->setSelectionBrushImage("OpenDungeonsSkin/SelectionBrush");
    levelSelectList->addItem(item);
    nn++;
}


EditorMode::~EditorMode()
{
    ConfigManager::getSingleton().saveEditorSettings();
    if(draggableTileContainer!=nullptr)
    {
        delete draggableTileContainer;
    }
    DebugDrawer::getSingleton().clear();    
    delete DebugDrawer::getSingletonPtr();
}

bool EditorMode::launchNewLevelPressed(const CEGUI::EventArgs&)
{
    // Creating the filename
    
    CEGUI::Combobox* levelTypeCb = static_cast<CEGUI::Combobox*>
        (mRootWindow->getChild("LevelWindowFrame/LevelTypeSelect"));
    std::string levelPath;
    size_t selection = levelTypeCb->getItemIndex(levelTypeCb->getSelectedItem());
    switch (selection)
    {
        default:
        case 0:
            levelPath = ResourceManager::getSingleton().getUserLevelPathSkirmish();
            break;
        case 1:
            levelPath = ResourceManager::getSingleton().getUserLevelPathMultiplayer();
            break;
    }
    CEGUI::Editbox* editWin = static_cast<CEGUI::Editbox*>
        (mRootWindow->getChild("LevelWindowFrame/LevelFilenameEdit"));
    std::string level = editWin->getText().c_str();
    Helper::trim(level);
    if (level.empty()) {
        mRootWindow->getChild("LevelWindowFrame/LevelTypeText")
            ->setText("Please set a level filename.");
        return true;
    }

    level = levelPath + "/" + level;
    if (boost::filesystem::exists(level)) {
        mRootWindow->getChild("LevelWindowFrame/LevelTypeText")
            ->setText("The level filename already exists.\nPlease set a different filename.");
        return true;
    }
    boost::filesystem::path pp(level);
    if (pp.extension() != ".level") {
        level.append(".level");
    }

    // Get the map size.
    editWin = static_cast<CEGUI::Editbox*>(mRootWindow->getChild("LevelWindowFrame/LevelWidthEdit"));
    std::string widthStr = editWin->getText().c_str();
    uint32_t width = Helper::toUInt32(widthStr);
    editWin = static_cast<CEGUI::Editbox*>(mRootWindow->getChild("LevelWindowFrame/LevelHeightEdit"));
    std::string heightStr = editWin->getText().c_str();
    uint32_t height = Helper::toUInt32(heightStr);

    if (width == 0 || height == 0) {
        mRootWindow->getChild("LevelWindowFrame/LevelTypeText")->setText("Invalid map size.");
        return true;
    }
    mMainCullingManager->stopTileCulling(mCameraTilesIntersections);
    // eliminating the race condition
    MiniMap* mm;
    mm = mMiniMap;
    mMiniMap = nullptr;
    delete mm;

    CullingManager* cc;
    cc = mMainCullingManager;
    mMainCullingManager = nullptr;
    delete cc;
    
    // Delete the potential pending event messages
    for (EventMessage* message : mEventMessages)
        delete message;

    mEventMessages.clear();
    
    if(ODClient::getSingleton().isConnected())
        ODClient::getSingleton().disconnect(mKeepReplayAtDisconnect);
    if(ODServer::getSingleton().isConnected())
        ODServer::getSingleton().stopServer();

    
    // Create the level before opening it.
    GameMap* gameMap = ODFrameListener::getSingleton().getClientGameMap();
    gameMap->clearAll();
    gameMap->setGamePaused(true);
    gameMap->createNewMap(width, height);
    gameMap->setLevelFileName(level);
    editWin = static_cast<CEGUI::Editbox*>(mRootWindow->getChild("LevelWindowFrame/LevelTitleEdit"));
    std::string levelTitle = editWin->getText().c_str();
    if (levelTitle.empty()) {
        //     mRootWindow->getChild("LevelWindowFrame/LevelTypeText")->setText("Please set a level title.");
        return true;
    }
    gameMap->setLevelName(levelTitle);
    CEGUI::MultiLineEditbox* meditWin = static_cast<CEGUI::MultiLineEditbox*>(
        mRootWindow->getChild("LevelWindowFrame/LevelDescriptionEdit"));
    gameMap->setLevelDescription(meditWin->getText().c_str());

    // We create some basic map at first. The map maker will be able to add/edit map properties
    // once the map editor has run.
    gameMap->setTileSetName(std::string()); // default one.
    gameMap->setLevelMusicFile("Searching_yd.ogg");
    gameMap->setLevelFightMusicFile("TheDarkAmulet_MP.ogg");
    Seat* seat = new Seat(gameMap);
    seat->setId(1);
    seat->setTeamId(1);
    seat->setPlayerType("Human");
    seat->setFaction("Keeper");
    seat->setColorId("1");
    if(!gameMap->addSeat(seat))
    {
        OD_LOG_WRN("Couldn't add seat id=" + Helper::toString(seat->getId()));
        delete seat;
        mRootWindow->getChild("LevelWindowFrame/LevelTypeText")->setText("Invalid seat data...");
        return true;
    }

    if (!MapHandler::writeGameMapToFile(level, *gameMap)) {
        OD_LOG_WRN("Couldn't write new map before loading: " + level);
        mRootWindow->getChild("LevelWindowFrame/LevelTypeText")
            ->setText("Couldn't write new map before loading.\nPlease check logs.");
        return true;
    }

    // In editor mode, we act as a server
    mRootWindow->getChild("LevelWindowFrame/LevelTypeText")->setText("Loading...");
    ConfigManager& config = ConfigManager::getSingleton();
    std::string nickname = config.getGameValue(Config::NICKNAME, std::string(), false);
    if(!ODServer::getSingleton().startServer(nickname, level, ServerMode::ModeEditor, false))
    {
        OD_LOG_ERR("Could not start server for editor!");
        mRootWindow->getChild("LevelWindowFrame/LevelTypeText")
            ->setText("ERROR: Could not start server for editor!");
        return true;
    }

    int port = ODServer::getSingleton().getNetworkPort();
    uint32_t timeout = ConfigManager::getSingleton().getClientConnectionTimeout();
    std::string replayFilename = ResourceManager::getSingleton().getReplayDataPath()
        + ResourceManager::getSingleton().buildReplayFilename();
    if(!ODClient::getSingleton().connect("localhost", port, timeout, replayFilename))
    {
        OD_LOG_ERR("Could not connect to server for editor!");
        mRootWindow->getChild("LevelWindowFrame/LevelTypeText")
            ->setText("Error: Couldn't connect to local server!");
        return true;
    }
    mRootWindow->getChild("LevelWindowFrame/LevelTypeText")
        ->setText("The level was created successfully.");

    ODClient::getSingleton().processClientSocketMessages(75);
    ODClient::getSingleton().processClientNotifications();
    uninstallSeatsMenuButtons();
    installSeatsMenuButtons();
    mGameMap = ODFrameListener::getSingletonPtr()->getClientGameMap();
    mMiniMap = MiniMap::createMiniMap(mRootWindow->getChild(Gui::MINIMAP));
    mMainCullingManager = new CullingManager(mGameMap, CullingType::SHOW_MAIN_WINDOW);
    mMainCullingManager->startTileCulling(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera(), mCameraTilesIntersections);
    
    return true;
}
