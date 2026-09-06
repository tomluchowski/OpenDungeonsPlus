/*!
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

#include "modes/GameMode.h"

#include "camera/CameraManager.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Skill.h"
#include "game/SkillManager.h"
#include "game/Seat.h"
#include "game/SkillType.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "modes/GameEditorModeConsole.h"
#include "modes/InputBridge.h"
#include "network/ChatEventMessage.h"
#include "network/ODClient.h"
#include "network/ODServer.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "render/TextRenderer.h"
#include "rooms/Room.h"
#include "rooms/RoomManager.h"
#include "rooms/RoomType.h"
#include "sound/MusicPlayer.h"
#include "sound/SoundEffectsManager.h"
#include "spells/SpellManager.h"
#include "spells/SpellType.h"
#include "traps/Trap.h"
#include "traps/TrapManager.h"
#include "traps/TrapType.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/ResourceManager.h"
#include "ODApplication.h"

#include <CEGUI/CEGUI.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/ToggleButton.h>
#include <CEGUI/widgets/ProgressBar.h>

#include <OgreRoot.h>
#include <OgreRenderWindow.h>
#include <OgreSceneNode.h>


#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

const std::string TEXT_SEAT_ID_PREFIX = "TextSeat";
const std::string TEXT_SEAT_PLAYER_NICKNAME_PREFIX = "TextSeatPlayerNick";
const std::string TEXT_SEAT_TEAM_ID_PREFIX = "TextSeatTeam";

const double AUTOSCROLL_EDGE_RATIO = 0.02;

static double getAutoscrollIntensity(int mousePosition, int screenSize, bool minimumEdge)
{
    if(screenSize <= 1)
        return 0.0;

    const double edgeSize = AUTOSCROLL_EDGE_RATIO * screenSize;
    const int distanceFromEdge = minimumEdge ? mousePosition : screenSize - 1 - mousePosition;
    return std::max(0.0, std::min(1.0, (edgeSize - distanceFromEdge) / edgeSize));
}

static bool blocksEdgeScrolling(CEGUI::Window* window)
{
    if(window == nullptr || window->getName() == "Root")
        return false;

    // Fixed HUD surfaces must not cover the physical screen edges. Dialogs
    // still own their input, as do clicks and wheel events over the HUD.
    for(CEGUI::Window* parent = window; parent != nullptr; parent = parent->getParent())
    {
        if(parent->isUserStringDefined("AllowEdgeScrolling") &&
           parent->getUserString("AllowEdgeScrolling") == "true")
            return false;
    }
    return true;
}

GameMode::GameMode(ModeManager *modeManager):
    GameEditorModeBase(modeManager, ModeManager::GAME, modeManager->getGui().getGuiSheet(Gui::guiSheet::inGameMenu)),
    mDigSetBool(false),
    mIndexEvent(0),
    mSettings(mRootWindow, modeManager->getGui()),
    mIsSkillWindowOpen(false),
    mCurrentSkillType(SkillType::nullSkillType),
    mCurrentSkillProgress(0.0),
    mPreviousMousePosition(MouseMoveEvent{0, 0}),
    directionKeyPressed(false),
    showTileDebugWindow(false),
    config(ConfigManager::getSingleton())
{
    // Set per default the input on the map
    mModeManager->getInputManager().mMouseDownOnCEGUIWindow = false;

    ODFrameListener::getSingleton().getCameraManager()->setDefaultView();

    CEGUI::Window* guiSheet = mRootWindow;

    addEventConnection(guiSheet->getChild("PanelToggleButton")->subscribeEvent(
        CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameMode::toggleControlPanel, this)));
    addEventConnection(guiSheet->getChild("EventsButton")->subscribeEvent(
        CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber([this](const CEGUI::EventArgs&)
        {
            CEGUI::Window* events = mRootWindow->getChild("GameEventText");
            if(events->isVisible())
                events->hide();
            else
                showEventMessages();
            return true;
        })));
    addEventConnection(guiSheet->getChild("EventsButton")->subscribeEvent(
        CEGUI::Window::EventMouseClick, CEGUI::Event::Subscriber(&GameMode::onEventMessagesClicked, this)));
    guiSheet->getChild("GameEventText")->hide();
    guiSheet->getChild("EventsButton")->setAlpha(1.0f);

    //Help window
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow/HelpButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::toggleHelpWindow, this)
        )
    );

    //Objectives window
    addEventConnection(
        guiSheet->getChild("ObjectivesButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::toggleObjectivesWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("ObjectivesWindow")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&GameMode::hideObjectivesWindow, this)
        )
    );

    //Player settings window
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow/PlayerSettingsButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::togglePlayerSettingsWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("PlayerSettingsWindow")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&GameMode::cancelPlayerSettings, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("PlayerSettingsWindow/CancelButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::cancelPlayerSettings, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("PlayerSettingsWindow/ApplyButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::applyPlayerSettings, this)
        )
    );

    // The skill tree window
    addEventConnection(
        guiSheet->getChild("SkillTreeWindow")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&GameMode::hideSkillWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("SkillTreeWindow/AutoFill")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::autoFillSkillWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("SkillTreeWindow/UnselectAll")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::unselectAllSkillWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("SkillTreeWindow/CancelButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::hideSkillWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("SkillTreeWindow/ApplyButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::applySkillWindow, this)
        )
    );

    // The Game Option menu events
    addEventConnection(
        guiSheet->getChild("OptionsButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::toggleOptionsWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&GameMode::hideOptionsWindow, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow/ObjectivesButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::showObjectivesFromOptions, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow/SkillButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::showSkillFromOptions, this)
        )
    );
    CEGUI::Window* saveGameButtonWindow = guiSheet->getChild("GameOptionsWindow/SaveGameButton");
    addEventConnection(
        saveGameButtonWindow->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::saveGame, this)
        )
    );
    saveGameButtonWindow->setEnabled(ODServer::getSingleton().isConnected());
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow/SettingsButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::showSettingsFromOptions, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow/QuitGameButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::showQuitMenuFromOptions, this)
        )
    );
    addEventConnection(
        guiSheet->getChild("GameOptionsWindow/ExitGameButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::showExitApplicationFromOptions, this)
        )
    );

    //Exit confirmation box
    addEventConnection(
        guiSheet->getChild(Gui::EXIT_CONFIRMATION_POPUP_YES_BUTTON)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&GameMode::onClickYesQuitMenu, this)
        )
    );

    //Exit confirmation box
    auto cancelExitWindow =
          [this](const CEGUI::EventArgs&)
          {
                  popupExit(false);
                  return true;
          };
    addEventConnection(
        guiSheet->getChild(Gui::EXIT_CONFIRMATION_POPUP_NO_BUTTON)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(cancelExitWindow)
        )
    );
    addEventConnection(
        guiSheet->getChild("ConfirmExit")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(cancelExitWindow)
        )
    );

    // Help window
    addEventConnection(
        guiSheet->getChild("GameHelpWindow")->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&GameMode::hideHelpWindow, this)
        )
    );

    // Set the help window text
    setHelpWindowText();

    SkillManager::connectSkills(this, mRootWindow);

    syncTabButtonTooltips(Gui::MAIN_TABCONTROL);
}

GameMode::~GameMode()
{
    CEGUI::ToggleButton* checkBox =
        dynamic_cast<CEGUI::ToggleButton*>(
            mRootWindow->getChild(
                Gui::EXIT_CONFIRMATION_POPUP)->getChild("SaveReplayCheckbox"));

    mKeepReplayAtDisconnect = checkBox->isSelected();

    CEGUI::Window* settingsWin = mRootWindow->getChild("PlayerSettingsWindow/Seats/SeatsSP");
    CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
    for(int seatId : mSeatIds)
    {
        CEGUI::Window* tmpWin;
        std::string name;
        name = TEXT_SEAT_ID_PREFIX + Helper::toString(seatId);
        tmpWin = settingsWin->getChild(name);
        settingsWin->removeChild(tmpWin);
        winMgr.destroyWindow(tmpWin);

        name = TEXT_SEAT_PLAYER_NICKNAME_PREFIX + Helper::toString(seatId);
        tmpWin = settingsWin->getChild(name);
        settingsWin->removeChild(tmpWin);
        winMgr.destroyWindow(tmpWin);

        name = TEXT_SEAT_TEAM_ID_PREFIX + Helper::toString(seatId);
        tmpWin = settingsWin->getChild(name);
        settingsWin->removeChild(tmpWin);
        winMgr.destroyWindow(tmpWin);
    }
}

void GameMode::activate()
{
    // Loads the corresponding Gui sheet.
    Gui& gui = getModeManager().getGui();
    gui.loadGuiSheet(Gui::inGameMenu);

    // We free the menu scene as it is not required anymore
    ODFrameListener::getSingleton().freeMainMenuScene();

    buildPlayerSettingsWindow();

    // Hides the exit pop-up and certain buttons only used by the editor.
    CEGUI::Window* guiSheet = mRootWindow;
    guiSheet->getChild(Gui::EXIT_CONFIRMATION_POPUP)->hide();
    guiSheet->getChild("ObjectivesWindow")->hide();
    guiSheet->getChild("PlayerSettingsWindow")->hide();
    guiSheet->getChild("SkillTreeWindow")->hide();
    guiSheet->getChild("SettingsWindow")->hide();
    guiSheet->getChild("GameOptionsWindow")->hide();
    guiSheet->getChild("GameChatWindow/GameChatEditBox")->hide();
    guiSheet->getChild("GameHelpWindow")->hide();

    giveFocus();

    // Play the game music.
    MusicPlayer::getSingleton().play(mGameMap->getLevelMusicFile()); // in game music

    std::string colorStr = Helper::getImageColoursStringFromColourValue(mGameMap->getLocalPlayer()->getSeat()->getColorValue());
    guiSheet->getChild("HorizontalPipe")->setProperty("ImageColours", colorStr);

    if(mGameMap->getTurnNumber() != -1)
    {
        /* The game has been resumed from another mode (like console).
           Let's refresh the exit popup */
        popupExit(mGameMap->getGamePaused());
    }
    else
    {
        mGameMap->setGamePaused(false);
    }

    // Update available options
    refreshGuiSkill(true);

    syncPlayerSettings();
}

bool GameMode::mouseMoved(const OIS::MouseEvent &arg)
{
    AbstractApplicationMode::mouseMoved(arg);

    auto mouseEvent = toSFMLMouseMove(arg);
    auto mouseDelta = MouseMoveEvent{mPreviousMousePosition.x - mouseEvent.x, mPreviousMousePosition.y - mouseEvent.y};
    mPreviousMousePosition = mouseEvent;

    if (!isConnected())
        return true;

    InputManager& inputManager = mModeManager->getInputManager();
    inputManager.mCommandState = (inputManager.mLMouseDown ? InputCommandState::building : InputCommandState::infoOnly);

    // TODO: Here we should check whether the terminal is active...
    if(inputManager.mMMouseDown)
    {
        ODFrameListener::getSingleton().moveCamera(CameraManager::randomRotateX,mouseDelta.x);
        ODFrameListener::getSingleton().moveCamera(CameraManager::randomRotateY,mouseDelta.y);
    }

    if (!directionKeyPressed && config.getInputValue(Config::AUTOSCROLL, "No", false) == "Yes")
    {
        const bool mouseOverGui = blocksEdgeScrolling(
            CEGUI::System::getSingleton().getDefaultGUIContext().getWindowContainingMouse());
        const double leftIntensity = mouseOverGui ? 0.0 : getAutoscrollIntensity(arg.state.X.abs, arg.state.width, true);
        const double rightIntensity = mouseOverGui ? 0.0 : getAutoscrollIntensity(arg.state.X.abs, arg.state.width, false);
        const double topIntensity = mouseOverGui ? 0.0 : getAutoscrollIntensity(arg.state.Y.abs, arg.state.height, true);
        const double bottomIntensity = mouseOverGui ? 0.0 : getAutoscrollIntensity(arg.state.Y.abs, arg.state.height, false);

        if (leftIntensity > 0.0)
            ODFrameListener::getSingleton().moveCamera(CameraManager::moveLeft, leftIntensity);
        else
            ODFrameListener::getSingleton().moveCamera(CameraManager::stopLeft);

        if (rightIntensity > 0.0)
            ODFrameListener::getSingleton().moveCamera(CameraManager::moveRight, rightIntensity);
        else
            ODFrameListener::getSingleton().moveCamera(CameraManager::stopRight);

        if (topIntensity > 0.0)
            ODFrameListener::getSingleton().moveCamera(CameraManager::moveForward, topIntensity);
        else
            ODFrameListener::getSingleton().moveCamera(CameraManager::stopForward);

        if (bottomIntensity > 0.0)
            ODFrameListener::getSingleton().moveCamera(CameraManager::moveBackward, bottomIntensity);
        else
            ODFrameListener::getSingleton().moveCamera(CameraManager::stopBackward);            
    }
 
    // If we have a room/trap/spell selected, show it
    // TODO: This should be changed, or combined with an icon or something later.
    TextRenderer& textRenderer = TextRenderer::getSingleton();
    textRenderer.moveText(ODApplication::POINTER_INFO_STRING,
                          static_cast<Ogre::Real>(mouseEvent.x + 30), static_cast<Ogre::Real>(mouseEvent.y));

    handleMouseWheel(toSFMLMouseWheel(arg));

    // Since this is a tile selection query we loop over the result set
    // and look for the first object which is actually a tile.
    ODFrameListener::getSingleton().findWorldPositionFromMouse(arg, inputManager.mKeeperHandPos,RenderManager::KEEPER_HAND_WORLD_Z);
    RenderManager::getSingleton().moveWorldCoords(inputManager.mKeeperHandPos.x, inputManager.mKeeperHandPos.y);

    int tileX = Helper::round(inputManager.mKeeperHandPos.x);
    int tileY = Helper::round(inputManager.mKeeperHandPos.y);
    inputManager.mXPos = tileX;
    inputManager.mYPos = tileY;
    Tile* tileClicked = mGameMap->getTile(tileX, tileY);
    if(tileClicked == nullptr)
        return true;

    inputManager.mXPos = tileClicked->getX();
    inputManager.mYPos = tileClicked->getY();
    if (!inputManager.mLMouseDown)
    {
        inputManager.mLStartDragX = inputManager.mXPos;
        inputManager.mLStartDragY = inputManager.mYPos;
    }
    return true;
}

void GameMode::handleMouseWheel(const MouseWheelEvent &arg)
{
    if(isMouseWheelOnCEGUIWindow())
        return;

    ODFrameListener& frameListener = ODFrameListener::getSingleton();

    if (arg.delta > 0)
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
    else if (arg.delta < 0)
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

bool GameMode::isMouseDownOnCEGUIWindow()
{
    CEGUI::Window* currentWindow = CEGUI::System::getSingleton().getDefaultGUIContext().getWindowContainingMouse();

    if (currentWindow == nullptr)
        return false;

    CEGUI::String winName = currentWindow->getName();

    // Passive chat passes through; the opened event surface owns its input.
    if (winName == "Root" || winName == "GameChatWindow" || winName == "GameChatText")
        return false;

    return true;
}

bool GameMode::isMouseWheelOnCEGUIWindow()
{
    CEGUI::Window* currentWindow = CEGUI::System::getSingleton().getDefaultGUIContext().getWindowContainingMouse();

    if (currentWindow == nullptr)
        return false;

    CEGUI::String winName = currentWindow->getName();

    // For the wheel, we do not ignore chat and events windows
    if (winName == "Root")
        return false;

    return true;
}

bool GameMode::mousePressed(const OIS::MouseEvent& arg, OIS::MouseButtonID id)
{
    InputManager& inputManager = mModeManager->getInputManager();

    CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonDown(
        Gui::convertButton(id));

    if (!isConnected())
        return true;

    inputManager.mMouseDownOnCEGUIWindow = isMouseDownOnCEGUIWindow();
    if (inputManager.mMouseDownOnCEGUIWindow)
        return true;

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

    if(mGameMap->getGamePaused())
        return true;

    // Cancelling an action does not require a valid world target.
    if(id == OIS::MB_Right && mPlayerSelection.getCurrentAction() != SelectedAction::none)
    {
        inputManager.mRMouseDown = true;
        inputManager.mLMouseDown = false;
        mPlayerSelection.setCurrentAction(SelectedAction::none);

        unselectAllTiles();
        TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");
        return true;
    }

    if(ODFrameListener::getSingleton().findWorldPositionFromMouse(arg, inputManager.mKeeperHandPos,RenderManager::KEEPER_HAND_WORLD_Z))
    {
        inputManager.mXPos = Helper::round(inputManager.mKeeperHandPos.x);
        inputManager.mYPos = Helper::round(inputManager.mKeeperHandPos.y);
        RenderManager::getSingleton().moveWorldCoords(inputManager.mKeeperHandPos.x, inputManager.mKeeperHandPos.y);
    }
    else
    {
        inputManager.mXPos = -1;
        inputManager.mYPos = -1;
    }

    // The player should be able to move the mouse even if not clicking on a tile. Because of that, we set
    // mMMouseDown before checking which tile is clicked
    if (id == OIS::MB_Middle)
        inputManager.mMMouseDown = true;

    Tile* tileClicked = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    if(tileClicked == nullptr)
    {
        if(id == OIS::MB_Left ||
           (id == OIS::MB_Right && mGameMap->getLocalPlayer()->numObjectsInHand() > 0))
        {
            const InputCommandState previousState = inputManager.mCommandState;
            inputManager.mCommandState = InputCommandState::validated;
            displayText(Ogre::ColourValue::Red, "Point at a tile inside the map.");
            inputManager.mCommandState = previousState;
        }
        return true;
    }

    if (id == OIS::MB_Middle)
    {
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
            double dist = Pathfinding::squaredDistance(entityPos.x, inputManager.mKeeperHandPos.x, entityPos.y, inputManager.mKeeperHandPos.y);
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
        {
            if(showTileDebugWindow)
                tileClicked->createStatsWindow();
        }
        else
            closestEntity->createStatsWindow();
        return true;
    }

    // Right mouse button down
    if (id == OIS::MB_Right)
    {
        inputManager.mRMouseDown = true;
        // Stop creating rooms, traps, etc.
        inputManager.mLStartDragX = inputManager.mXPos;
        inputManager.mLStartDragY = inputManager.mYPos;
        unselectAllTiles();
        TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");
        if(mGameMap->getLocalPlayer()->numObjectsInHand() > 0)
        {
            // If we right clicked with the mouse over a valid map tile, try to drop what we have in hand on the map.
            Tile *curTile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);

            if (curTile == nullptr)
            {
                displayText(Ogre::ColourValue::Red, "Point at a tile inside the map.");
                return true;
            }

            if (mGameMap->getLocalPlayer()->isDropHandPossible(curTile))
            {
                if(ODClient::getSingleton().isConnected())
                {
                    // Send a message to the server telling it we want to drop the creature
                    ClientNotification *clientNotification = new ClientNotification(
                        ClientNotificationType::askHandDrop);
                    mGameMap->tileToPacket(clientNotification->mPacket, curTile);
                    ODClient::getSingleton().queueClientNotification(clientNotification);
                }

                return true;
            }
            const InputCommandState previousState = inputManager.mCommandState;
            inputManager.mCommandState = InputCommandState::validated;
            handlePlayerActionNone();
            inputManager.mCommandState = previousState;
            return true;
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
                double dist = Pathfinding::squaredDistance(entityPos.x, inputManager.mKeeperHandPos.x, entityPos.y, inputManager.mKeeperHandPos.y);
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

    // Left mouse button down
    inputManager.mLMouseDown = true;
    inputManager.mLStartDragX = inputManager.mXPos;
    inputManager.mLStartDragY = inputManager.mYPos;

    // Check whether the player is already placing rooms or traps.
    if (mPlayerSelection.getCurrentAction() == SelectedAction::none)
    {
        // See if the mouse is over any pickup-able entity
        std::vector<GameEntity*> entities;
        tileClicked->fillWithEntities(entities, SelectionEntityWanted::any, mGameMap->getLocalPlayer());
        // We search the closest creature alive
        GameEntity* closestEntity = nullptr;
        double closestDist = 0;
        for(GameEntity* entity : entities)
        {
            if(!entity->tryPickup(mGameMap->getLocalPlayer()->getSeat()))
                continue;

            const Ogre::Vector3& entityPos = entity->getPosition();
            double dist = Pathfinding::squaredDistance(entityPos.x, inputManager.mKeeperHandPos.x, entityPos.y, inputManager.mKeeperHandPos.y);
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
    }


    // If we are doing nothing and we click on a tile, it is a tile selection
    if(mPlayerSelection.getCurrentAction() == SelectedAction::none)
        mPlayerSelection.setCurrentAction(SelectedAction::selectTile);

    // If we are in a game we store the opposite of whether this tile is marked for digging or not, this allows us to mark tiles
    // by dragging out a selection starting from an unmarcked tile, or unmark them by starting the drag from a marked one.
    mDigSetBool = !(tileClicked->getMarkedForDigging(mGameMap->getLocalPlayer()));

    return true;
}

bool GameMode::mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonUp(Gui::convertButton(id));

    InputManager& inputManager = mModeManager->getInputManager();

    // First check for the axis rotation release, as this
    // seems to be the most expected action the user wants to put at end

    if(id == OIS::MB_Middle)
    {
        inputManager.mMMouseDown = false;
        ODFrameListener::getSingleton().moveCamera(CameraManager::zeroRandomRotateX, 0.0);
        ODFrameListener::getSingleton().moveCamera(CameraManager::zeroRandomRotateY, 0.0);
    }

    // Right mouse button up
    if (id == OIS::MB_Right)
    {
        inputManager.mRMouseDown = false;
        return true;
    }

    if (id != OIS::MB_Left)
        return true;

    // Left mouse button up
    const bool wasLeftMouseDown = inputManager.mLMouseDown;
    inputManager.mLMouseDown = false;
    inputManager.mCommandState = InputCommandState::infoOnly;

    // Only finish a world action that began on the map and was not cancelled.
    if(!wasLeftMouseDown || inputManager.mMouseDownOnCEGUIWindow ||
       isMouseDownOnCEGUIWindow() || !isConnected() || mGameMap->getGamePaused())
    {
        if(mPlayerSelection.getCurrentAction() == SelectedAction::selectTile)
            mPlayerSelection.setCurrentAction(SelectedAction::none);
        unselectAllTiles();
        return true;
    }

    if(ODFrameListener::getSingleton().findWorldPositionFromMouse(arg,
        inputManager.mKeeperHandPos, RenderManager::KEEPER_HAND_WORLD_Z))
    {
        inputManager.mXPos = Helper::round(inputManager.mKeeperHandPos.x);
        inputManager.mYPos = Helper::round(inputManager.mKeeperHandPos.y);
    }
    else
    {
        inputManager.mXPos = -1;
        inputManager.mYPos = -1;
    }

    // We notify current selection input
    inputManager.mCommandState = InputCommandState::validated;
    checkInputCommand();
    if(mPlayerSelection.getCurrentAction() == SelectedAction::selectTile)
        mPlayerSelection.setCurrentAction(SelectedAction::none);
    inputManager.mCommandState = InputCommandState::infoOnly;

    return true;
}

bool GameMode::keyPressed(const OIS::KeyEvent& arg)
{
    // Inject key to Gui
    const bool guiHandledKey = CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyDown(
        static_cast<CEGUI::Key::Scan>(arg.key));
    if (arg.text != 0 && !getConsole()->isFreshlyEnabled())
    {
        CEGUI::System::getSingleton().getDefaultGUIContext().injectChar(arg.text);
    }

    switch (mCurrentInputMode)
    {
        case InputModeChat:
            return keyPressedChat(arg);
        case InputModeConsole:
            return getConsole()->keyPressed(arg);
        case InputModeNormal:
        default:
            if(arg.key == OIS::KC_ESCAPE && guiHandledKey)
                return true;
            return keyPressedNormal(arg);
    }
}

bool GameMode::keyPressedNormal(const OIS::KeyEvent &arg)
{
    ODFrameListener& frameListener = ODFrameListener::getSingleton();

    switch (arg.key)
    {
    case OIS::KC_G:
        toggleControlPanel();
        break;

    case OIS::KC_F1:
        toggleHelpWindow();
        break;

    case OIS::KC_F2:
        togglePlayerSettingsWindow();
        break;

    case OIS::KC_F3:
        toggleObjectivesWindow();
        break;

    case OIS::KC_F4:
        toggleSkillWindow();
        break;

    case OIS::KC_F5:
        saveGame();
        break;

    case OIS::KC_F9:
        RenderManager::getSingleton().rrToggleHandSelectorVisibility();
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
        directionKeyPressed = true;
        break;

    case OIS::KC_RIGHT:
    case OIS::KC_D:
        frameListener.moveCamera(CameraManager::Direction::moveRight);
        directionKeyPressed = true;        
        break;

    case OIS::KC_UP:
    case OIS::KC_W:
        frameListener.moveCamera(CameraManager::Direction::moveForward);
        directionKeyPressed = true;
        break;

    case OIS::KC_DOWN:
    case OIS::KC_S:
        frameListener.moveCamera(CameraManager::Direction::moveBackward);
        directionKeyPressed = true;
        break;

    case OIS::KC_Q:
        frameListener.moveCamera(CameraManager::Direction::rotateLeft);
        break;

    case OIS::KC_E:
        frameListener.moveCamera(CameraManager::Direction::rotateRight);
        break;

    case OIS::KC_HOME:
        frameListener.moveCamera(CameraManager::Direction::moveDown);
        break;

    case OIS::KC_END:
        frameListener.moveCamera(CameraManager::Direction::moveUp);
        break;

    case OIS::KC_PGUP:
        frameListener.moveCamera(CameraManager::Direction::rotateUp);
        break;

    case OIS::KC_PGDOWN:
        frameListener.moveCamera(CameraManager::Direction::rotateDown);
        break;

    case OIS::KC_T:
        if(isConnected()) // If we are in a game.
        {
            Seat* tempSeat = mGameMap->getLocalPlayer()->getSeat();
            frameListener.cameraFlyTo(tempSeat->getStartingPosition());
        }
        break;

    case OIS::KC_V:
        ODFrameListener::getSingleton().getCameraManager()->setNextDefaultView();
        break;

    case OIS::KC_LMENU:
        RenderManager::getSingleton().
        RenderManager::getSingleton().rrSetCreaturesTextOverlay(*mGameMap, true);
        break;

    // Zooms to the next event
    case OIS::KC_SPACE:
    {
        Player* player = mGameMap->getLocalPlayer();
        const PlayerEvent* event = player->getNextEvent(mIndexEvent);
        if(event == nullptr)
            break;

        Ogre::Vector3 pos = player->getSeat()->getStartingPosition();
        pos.x = static_cast<Ogre::Real>(event->getTile()->getX());
        pos.y = static_cast<Ogre::Real>(event->getTile()->getY());
        frameListener.cameraFlyTo(pos);
        break;
    }

    // Close one GUI layer before considering a new exit confirmation.
    case OIS::KC_ESCAPE:
        if(closeTopWindow())
            break;
        if(mRootWindow->getChild("GameEventText")->isVisible())
        {
            mRootWindow->getChild("GameEventText")->hide();
            break;
        }
        mExitToDesktop = false;
        popupExit(!mGameMap->getGamePaused());
        break;

    // Print a screenshot
    case OIS::KC_SYSRQ:
        ResourceManager::getSingleton().takeScreenshot(frameListener.getRenderWindow());
        break;

    case OIS::KC_RETURN:
    case OIS::KC_NUMPADENTER: {
        mCurrentInputMode = InputModeChat;
        CEGUI::Window* chatEditBox = mRootWindow->getChild("GameChatWindow/GameChatEditBox");
        chatEditBox->show();
        chatEditBox->activate();
        mChatMessageBoxDisplay |= ChatMessageBoxDisplay::showChatInput;
        refreshChatDisplay();
        break;
    }

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

bool GameMode::keyPressedChat(const OIS::KeyEvent &arg)
{
    // If one presses Escape while in chat mode, let's simply quit it.
    CEGUI::Window* chatEditBox = mRootWindow->getChild("GameChatWindow/GameChatEditBox");
    if (arg.key == OIS::KC_ESCAPE)
    {
        mCurrentInputMode = InputModeNormal;
        chatEditBox->setText("");
        chatEditBox->hide();
        mChatMessageBoxDisplay &= ~ChatMessageBoxDisplay::showChatInput;
        refreshChatDisplay();
        return true;
    }

    if(arg.key != OIS::KC_RETURN && arg.key != OIS::KC_NUMPADENTER)
        return true;

    mCurrentInputMode = InputModeNormal;
    chatEditBox->hide();
    mChatMessageBoxDisplay &= ~ChatMessageBoxDisplay::showChatInput;
    refreshChatDisplay();

    // Check whether something was actually typed.
    if (chatEditBox->getText().empty())
        return true;

    ODClient::getSingleton().queueClientNotification(ClientNotificationType::chat, chatEditBox->getText().c_str());
    chatEditBox->setText("");
    return true;
}

bool GameMode::toggleControlPanel(const CEGUI::EventArgs&)
{
    CEGUI::Window* tabs = mRootWindow->getChild(Gui::MAIN_TABCONTROL);
    CEGUI::Window* content = tabs->getChild("__auto_TabPane__");
    content->setVisible(!content->isVisible());
    tabs->setMousePassThroughEnabled(!content->isVisible());
    return true;
}

void GameMode::refreshMainUI()
{
    Seat* mySeat = mGameMap->getLocalPlayer()->getSeat();
    CEGUI::Window* guiSheet = mRootWindow;

    //! \brief Updates common info on screen.
    CEGUI::Window* widget = guiSheet->getChild(Gui::DISPLAY_TERRITORY);
    std::stringstream tempSS("");
    tempSS << mySeat->getNumClaimedTiles();
    widget->setText(tempSS.str());

    widget = guiSheet->getChild(Gui::DISPLAY_CREATURES);
    tempSS.str("");
    tempSS << mySeat->getNumCreaturesFighters() << "/" << mySeat->getNumCreaturesFightersMax();
    widget->setText(tempSS.str());

    widget = guiSheet->getChild(Gui::DISPLAY_GOLD);
    tempSS.str("");
    tempSS << mySeat->getGold() << "/" << mySeat->getGoldMax();
    widget->setText(tempSS.str());

    widget = guiSheet->getChild(Gui::DISPLAY_MANA);
    tempSS.str("");
    tempSS << mySeat->getMana() << " " << (mySeat->getManaDelta() >= 0 ? "+" : "-")
            << mySeat->getManaDelta();
    widget->setText(tempSS.str());
    unsigned int workers = 0;
    unsigned int fighters = 0;
    for(Creature* creature : mGameMap->getCreaturesBySeat(mySeat))
    {
        if(!creature->tryPickup(mySeat))
            continue;
        if(creature->getDefinition()->isWorker())
            ++workers;
        else
            ++fighters;
    }
    guiSheet->getChild(Gui::BUTTON_CREATURE_WORKER + "/Count")->setText(Helper::toString(workers));
    guiSheet->getChild(Gui::BUTTON_CREATURE_FIGHTER + "/Count")->setText(Helper::toString(fighters));
}

void GameMode::refreshPlayerGoals(const std::string& goalsDisplayString)
{
    CEGUI::Window* widget = mRootWindow->getChild(Gui::OBJECTIVE_TEXT);
    widget->setText(reinterpret_cast<const CEGUI::utf8*>(goalsDisplayString.c_str()));
}

bool GameMode::keyReleased(const OIS::KeyEvent &arg)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyUp(static_cast<CEGUI::Key::Scan>(arg.key));

    if (mCurrentInputMode == InputModeChat || mCurrentInputMode == InputModeConsole)
        return true;

    return keyReleasedNormal(arg);
}

bool GameMode::keyReleasedNormal(const OIS::KeyEvent &arg)
{
    ODFrameListener& frameListener = ODFrameListener::getSingleton();

    switch (arg.key)
    {
    case OIS::KC_LEFT:
    case OIS::KC_A:
        frameListener.moveCamera(CameraManager::Direction::stopLeft);
        directionKeyPressed = false;
        break;

    case OIS::KC_RIGHT:
    case OIS::KC_D:
        frameListener.moveCamera(CameraManager::Direction::stopRight);
        directionKeyPressed = false;
        break;

    case OIS::KC_UP:
    case OIS::KC_W:
        frameListener.moveCamera(CameraManager::Direction::stopForward);
        directionKeyPressed = false;       
        break;

    case OIS::KC_DOWN:
    case OIS::KC_S:
        frameListener.moveCamera(CameraManager::Direction::stopBackward);
        directionKeyPressed = false;
        break;

    case OIS::KC_Q:
        frameListener.moveCamera(CameraManager::Direction::stopRotLeft);
        break;

    case OIS::KC_E:
        frameListener.moveCamera(CameraManager::Direction::stopRotRight);
        break;

    case OIS::KC_HOME:
        frameListener.moveCamera(CameraManager::Direction::stopDown);
        break;

    case OIS::KC_END:
        frameListener.moveCamera(CameraManager::Direction::stopUp);
        break;

    case OIS::KC_PGUP:
        frameListener.moveCamera(CameraManager::Direction::stopRotUp);
        break;

    case OIS::KC_PGDOWN:
        frameListener.moveCamera(CameraManager::Direction::stopRotDown);
        break;

    case OIS::KC_LMENU:
        RenderManager::getSingleton().rrSetCreaturesTextOverlay(*mGameMap, false);
        break;

    default:
        break;
    }

    return true;
}

void GameMode::handleHotkeys(OIS::KeyCode keycode)
{
    ODFrameListener& frameListener = ODFrameListener::getSingleton();
    InputManager& inputManager = mModeManager->getInputManager();

    //keycode minus two because the codes are shifted by two against the actual number
    unsigned int keynumber = keycode - 2;

    if (getKeyboard()->isModifierDown(OIS::Keyboard::Shift))
    {
        inputManager.mHotkeyLocationIsValid[keynumber] = true;
        inputManager.mHotkeyLocation[keynumber].vv  = frameListener.getCameraManager()->getActiveCameraNode()->getPosition();
        inputManager.mHotkeyLocation[keynumber].qq  = frameListener.getCameraManager()->getActiveCameraNode()->getOrientation();
        inputManager.mHotkeyLocation[keynumber].qq2  = frameListener.getCameraManager()->getActiveCameraNode()->getChild(0)->getOrientation();        
    }
    else if (inputManager.mHotkeyLocationIsValid[keynumber])
    {
        frameListener.getCameraManager()->getActiveCameraNode()->setPosition(inputManager.mHotkeyLocation[keynumber].vv);
        frameListener.getCameraManager()->getActiveCameraNode()->setOrientation(  inputManager.mHotkeyLocation[keynumber].qq);
        frameListener.getCameraManager()->getActiveCameraNode()->getChild(0)->setOrientation(  inputManager.mHotkeyLocation[keynumber].qq2);                
    }
}

void GameMode::onFrameStarted(const Ogre::FrameEvent& evt)
{
    GameEditorModeBase::onFrameStarted(evt);
    updateEventMessageIndicator(evt.timeSinceLastFrame);

    refreshGuiSkill();
    refreshSpellButtonCoolDowns();

    Player* player = mGameMap->getLocalPlayer();
    if (player == nullptr)
    {
        OD_LOG_ERR("No local player");
        return;
    }
    player->frameStarted(evt.timeSinceLastFrame);

    // After frameStarted, so that the countdown shown is the one just computed.
    refreshActionFeedback(evt.timeSinceLastFrame);

    if((mSkillCurrentCompletion.mProgressBar != nullptr) &&
       (mSkillCurrentCompletion.mCompletenessDisplayed < mSkillCurrentCompletion.mCompleteness))
    {
        const float completeTime = 3.0f;
        // We update current skill completeness. Because the required amount of skill
        // may be pretty high between the lowest and highest skills, we cannot fill it
        // linearly according to this amount (it would be too fast for cheap skills or
        // too slow for expensive ones).
        // We want to fill it so that a 100% skill is filled in 'completeTime' seconds
        float progress = evt.timeSinceLastFrame / completeTime;
        mSkillCurrentCompletion.mCompletenessDisplayed = std::min(mSkillCurrentCompletion.mCompleteness,
            mSkillCurrentCompletion.mCompletenessDisplayed + progress);

        mSkillCurrentCompletion.mProgressBar->setProgress(mSkillCurrentCompletion.mCompletenessDisplayed);
    }
}

void GameMode::onFrameEnded(const Ogre::FrameEvent& evt)
{
}

void GameMode::popupExit(bool pause)
{
    if(pause)
    {
        mRootWindow->getChild(Gui::EXIT_CONFIRMATION_POPUP)->show();
    }
    else
    {
        mRootWindow->getChild(Gui::EXIT_CONFIRMATION_POPUP)->hide();
    }
    mGameMap->setGamePaused(pause);
}

void GameMode::notifyGuiAction(GuiAction guiAction)
{
    switch(guiAction)
    {
        case GuiAction::ButtonPressedCreatureWorker:
        {
            if(ODClient::getSingleton().isConnected())
            {
                ClientNotification *clientNotification = new ClientNotification(
                    ClientNotificationType::askPickupWorker);
                ODClient::getSingleton().queueClientNotification(clientNotification);
            }
            break;
        }
        case GuiAction::ButtonPressedCreatureFighter:
        {
            if(ODClient::getSingleton().isConnected())
            {
                ClientNotification *clientNotification = new ClientNotification(
                    ClientNotificationType::askPickupFighter);
                ODClient::getSingleton().queueClientNotification(clientNotification);
            }
            break;
        }
        default:
            break;
    }
}

bool GameMode::onClickYesQuitMenu(const CEGUI::EventArgs& /*arg*/)
{
    if(mExitToDesktop)
        ODFrameListener::getSingleton().requestExit();
    else
        mModeManager->requestMode(AbstractModeManager::MENU_MAIN);
    return true;
}

bool GameMode::showObjectivesWindow(const CEGUI::EventArgs&)
{
    mRootWindow->getChild("ObjectivesWindow")->show();
    return true;
}

bool GameMode::hideObjectivesWindow(const CEGUI::EventArgs&)
{
    mRootWindow->getChild("ObjectivesWindow")->hide();
    return true;
}

bool GameMode::toggleObjectivesWindow(const CEGUI::EventArgs& e)
{
    CEGUI::Window* objectives = mRootWindow->getChild("ObjectivesWindow");

    if (objectives->isVisible())
        hideObjectivesWindow(e);
    else
        showObjectivesWindow(e);
    return true;
}

bool GameMode::showPlayerSettingsWindow(const CEGUI::EventArgs&)
{
    mRootWindow->getChild("GameOptionsWindow")->hide();
    // Before showing the player settings, we reset to the values in the seat. That's
    // because only the server can change them and the values in the Seat are the
    // ones that should be shown
    syncPlayerSettings();
    mRootWindow->getChild("PlayerSettingsWindow")->show();
    return true;
}

bool GameMode::togglePlayerSettingsWindow(const CEGUI::EventArgs& e)
{
    CEGUI::Window* playerSettings = mRootWindow->getChild("PlayerSettingsWindow");

    if (playerSettings->isVisible())
        cancelPlayerSettings(e);
    else
        showPlayerSettingsWindow(e);
    return true;
}

bool GameMode::cancelPlayerSettings(const CEGUI::EventArgs&)
{
    mRootWindow->getChild("PlayerSettingsWindow")->hide();
    return true;
}

bool GameMode::applyPlayerSettings(const CEGUI::EventArgs&)
{
    mRootWindow->getChild("PlayerSettingsWindow")->hide();
    ClientNotification* clientNotification = new ClientNotification(
        ClientNotificationType::askSetPlayerSettings);

    CEGUI::ToggleButton* cbKoCreatures = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("PlayerSettingsWindow/KOCreatureCheckbox"));
    bool enabled = cbKoCreatures->isSelected();
    clientNotification->mPacket << enabled;

    ODClient::getSingleton().queueClientNotification(clientNotification);

    return true;
}

void GameMode::syncPlayerSettings()
{
    Seat* localPlayerSeat = mGameMap->getLocalPlayer()->getSeat();
    CEGUI::ToggleButton* cbKoCreatures = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("PlayerSettingsWindow/KOCreatureCheckbox"));
    cbKoCreatures->setSelected(localPlayerSeat->getKoCreatures());
}

bool GameMode::showSkillWindow(const CEGUI::EventArgs&)
{
    resetSkillTree();
    mRootWindow->getChild("SkillTreeWindow")->show();
    return true;
}

bool GameMode::hideSkillWindow(const CEGUI::EventArgs&)
{
    closeSkillWindow(false);
    return true;
}

bool GameMode::unselectAllSkillWindow(const CEGUI::EventArgs&)
{
    mSkillPending.clear();
    mSkillCurrentCompletion.resetValue();
    refreshGuiSkill(true);
    return true;
}

bool GameMode::applySkillWindow(const CEGUI::EventArgs& e)
{
    closeSkillWindow(true);
    return true;
}

void GameMode::closeSkillWindow(bool saveSkill)
{
    endSkillTree(saveSkill);
    mRootWindow->getChild("SkillTreeWindow")->hide();
}

bool GameMode::toggleSkillWindow(const CEGUI::EventArgs& e)
{
    CEGUI::Window* skill = mRootWindow->getChild("SkillTreeWindow");

    if (skill->isVisible())
    {
        closeSkillWindow(false);
    }
    else
    {
        showSkillWindow(e);
    }
    return true;
}

bool GameMode::showOptionsWindow(const CEGUI::EventArgs&)
{
    mRootWindow->getChild("GameOptionsWindow")->show();
    return true;
}

bool GameMode::hideOptionsWindow(const CEGUI::EventArgs& /*e*/)
{
    mRootWindow->getChild("GameOptionsWindow")->hide();
    return true;
}

bool GameMode::toggleOptionsWindow(const CEGUI::EventArgs& e)
{
    CEGUI::Window* options = mRootWindow->getChild("GameOptionsWindow");

    if (options->isVisible())
        hideOptionsWindow(e);
    else
        showOptionsWindow(e);
    return true;
}

bool GameMode::showQuitMenuFromOptions(const CEGUI::EventArgs& /*e*/)
{
    mExitToDesktop = false;
    mRootWindow->getChild("GameOptionsWindow")->hide();
    popupExit(!mGameMap->getGamePaused());
    return true;
}

bool GameMode::showExitApplicationFromOptions(const CEGUI::EventArgs& /*e*/)
{
    mExitToDesktop = true;
    mRootWindow->getChild("GameOptionsWindow")->hide();
    popupExit(!mGameMap->getGamePaused());
    return true;
}

bool GameMode::showObjectivesFromOptions(const CEGUI::EventArgs& /*e*/)
{
    mRootWindow->getChild("GameOptionsWindow")->hide();
    mRootWindow->getChild("ObjectivesWindow")->show();
    return true;
}

bool GameMode::showSkillFromOptions(const CEGUI::EventArgs& /*e*/)
{
    mRootWindow->getChild("GameOptionsWindow")->hide();
    showSkillWindow();
    return true;
}

bool GameMode::saveGame(const CEGUI::EventArgs& /*e*/)
{
    hideOptionsWindow();
    showEventMessages();

    // We can save if launching in server mode only
    if(!ODServer::getSingleton().isConnected())
    {
        std::string msg = "You cannot save the game when not launching in server mode";
        EventMessage* event = new EventMessage(msg, EventShortNoticeType::genericGameInfo);
        receiveEventShortNotice(event);
        return true;
    }

    if(ODClient::getSingleton().isConnected())
    {
        // Send a message to the server telling it we want to drop the creature
        ClientNotification *clientNotification = new ClientNotification(
            ClientNotificationType::askSaveMap);
        ODClient::getSingleton().queueClientNotification(clientNotification);
    }
    return true;
}

void GameMode::receiveEventShortNotice(EventMessage* event)
{
    GameEditorModeBase::receiveEventShortNotice(event);
    if(mRootWindow->getChild("GameEventText")->isVisible())
        showEventMessages();
    else
    {
        mUnreadEventMessages = true;
        mEventMessageFlashTime = 0.0f;
        updateEventMessageIndicator(0.0f);
    }
}

void GameMode::showEventMessages()
{
    CEGUI::Window* events = mRootWindow->getChild("GameEventText");
    events->show();
    events->moveToFront();
    mUnreadEventMessages = false;
    updateEventMessageIndicator(0.0f);
}

bool GameMode::onEventMessagesClicked(const CEGUI::EventArgs& arg)
{
    const CEGUI::MouseEventArgs& mouse = static_cast<const CEGUI::MouseEventArgs&>(arg);
    CEGUI::Window* events = mRootWindow->getChild("GameEventText");
    if(mouse.button == CEGUI::RightButton && !mUnreadEventMessages)
    {
        // Read messages remain available until explicitly dismissed.
        for(EventMessage* message : mEventMessages)
            delete message;
        mEventMessages.clear();
        events->setText("");
        events->hide();
    }
    return true;
}

void GameMode::updateEventMessageIndicator(float elapsed)
{
    CEGUI::Window* button = mRootWindow->getChild("EventsButton");
    if(!mUnreadEventMessages)
    {
        mEventMessageFlashTime = 0.0f;
        button->setAlpha(1.0f);
        return;
    }
    mEventMessageFlashTime = std::fmod(mEventMessageFlashTime + elapsed, 1.0f);
    button->setAlpha(mEventMessageFlashTime < 0.5f ? 0.4f : 1.0f);
}

bool GameMode::showSettingsFromOptions(const CEGUI::EventArgs& /*e*/)
{
    mRootWindow->getChild("GameOptionsWindow")->hide();
    mSettings.show();
    return true;
}

bool GameMode::showHelpWindow(const CEGUI::EventArgs&)
{
    mRootWindow->getChild("GameOptionsWindow")->hide();
    mRootWindow->getChild("GameHelpWindow")->show();
    return true;
}

bool GameMode::hideHelpWindow(const CEGUI::EventArgs& /*e*/)
{
    mRootWindow->getChild("GameHelpWindow")->hide();
    return true;
}

bool GameMode::toggleHelpWindow(const CEGUI::EventArgs& e)
{
    CEGUI::Window* helpWindow = mRootWindow->getChild("GameHelpWindow");
    if (!helpWindow->isVisible())
        showHelpWindow(e);
    else
        hideHelpWindow(e);
    return true;
}

void GameMode::setHelpWindowText()
{
    CEGUI::Window* textWindow = mRootWindow->getChild("GameHelpWindow/TextDisplay");
    const std::string formatTitleOn = "[font='MedievalSharp-12'][colour='CCBBBBFF']";
    const std::string formatTitleOff = "[font='MedievalSharp-10'][colour='FFFFFFFF']";
    std::stringstream txt("");
    txt << "Always skipping classes to play with kobolds in the torture room, were you?" << std::endl
        << "Well, at least you don't seem as dumb as a pit demon, so let's cover the basics again." << std::endl << std::endl;
    txt << formatTitleOn << "Camera controls" << formatTitleOff << std::endl
        << "The camera is your eyes - be watchful! Take the time to learn how to use it efficiently:" << std::endl
        << "  - Camera translation: Arrow keys or WASD." << std::endl
        << "  - Camera rotation: A (left) or E (right)." << std::endl
        << "  - Camera zooming: Mouse wheel, Home (zoom out) or End (zoom in)." << std::endl
        << "  - Camera tilting: Page Up (look up), Page Down (look down)." << std::endl
        << "  - Cycle through camera modes: V." << std::endl << std::endl;
    txt << formatTitleOn << "Keeper hand controls" << formatTitleOff << std::endl
        << "Use your hand wisely to keep all minions under control and let your dungeon thrive!" << std::endl
        << "  - Mouse left click: Select an action, Confirm an action." << std::endl
        << "  - Mouse right click: Unselect an action, Slap a creature (grin)." << std::endl
        << "  - Mouse middle click (on a creature): Show debugging information." << std::endl
        << "You can select multiple tiles for some actions by left clicking and dragging the mouse." << std::endl << std::endl;
    txt << formatTitleOn << "Basic workflow" << formatTitleOff << std::endl
        << "As an overlord of the underworld, your evil plan is to build a strong dungeon and to crush your neighbours."
        << "To do so, you need to deploy your shadow arts to their full extent:" << std::endl
        << "  - Summon workers to do the dirty job: dirt digging and gold mining. Workers will also claim ground "
        << "and wall tiles for your glory when they have nothing better to do. You can order workers around by marking "
        << "dirt and gold tiles for digging and mining." << std::endl
        << "  - Build rooms on claimed tiles. Building rooms costs gold, so make sure to have at least one treasury tile "
        << "in place (thanks to your evil tricks, the first tile is free!) so that your workers can store the gold they mine." << std::endl
        << "Be sure to build a dormitory and a hatchery to fulfill your creatures' lowest needs, and a library to skill "
        << "new buildings, spells and traps. Varied buildings, wealth and great dungeons will attract more powerful creatures." << std::endl
        << "  - Use the Skill Manager (F4) to set the priority for the various skills that can be uncovered at the library."
        << "Make sure to have intelligent creatures always at work at your library - if you can find any among your dumb minions!" << std::endl
        << "  - Once you have a workshop, set traps to protect your dungeon - your creatures will then craft them at the workshop." << std::endl
        << "  - Use spells to macro-manage your creatures more efficiently." << std::endl << std::endl;
    txt << formatTitleOn << "Tips and tricks" << formatTitleOff << std::endl
        << "You can left-click on one of your creatures to pick it up and right click somewhere else to put it back. "
        << "Very useful to help a creature in battle or force a worker to do a specific task..." << std::endl
        << "Note that you can place workers on any of your claimed tiles and unclaimed dirt tiles, "
        << "but you can only place fighters on allied claimed tiles and nothing at all on enemy claimed tiles." << std::endl
        << "Your workers will fortify walls, turning them into your color. Those cannot be broken by enemies until no more "
        << "claimed tiles around are of your color." << std::endl;
    txt << std::endl << std::endl << "Be evil, be cunning, your opponents will do the same... and have fun! ;)" << std::endl;
    textWindow->setText(reinterpret_cast<const CEGUI::utf8*>(txt.str().c_str()));
}

void GameMode::refreshSkillButtonState(const std::string& skillButtonName, const std::string& castButtonName,
        const std::string& skillProgressBarName, SkillType resType)
{
    // Determine the widget name and button accordingly to the SkillType given
    Seat* localPlayerSeat = mGameMap->getLocalPlayer()->getSeat();
    bool isDone = localPlayerSeat->isSkillDone(resType);
    bool isAllowed = true;
    uint32_t queueNumber = 0;
    if(!isDone)
    {
        const std::vector<SkillType>& skillNotAllowed = localPlayerSeat->getSkillNotAllowed();
        if(std::find(skillNotAllowed.begin(), skillNotAllowed.end(), resType) != skillNotAllowed.end())
        {
            // Skill is not allowed
            isAllowed = false;
        }
        else
        {
            // If we are currently changing the skill window, we display the temporary
            // modified pending list
            if(mIsSkillWindowOpen)
            {
                uint32_t cpt = 1;
                for (SkillType pendingRes : mSkillPending)
                {
                    if (pendingRes == resType)
                    {
                        queueNumber = cpt;
                        break;
                    }
                    ++cpt;
                }
            }
            else
            {
                queueNumber = localPlayerSeat->isSkillPending(resType);
            }
        }
    }

    const std::string okIcon = "OpenDungeonsIcons/CheckIcon";
    const std::string pendingIcon = "OpenDungeonsIcons/HourglassIcon";
    const std::string abortIcon = "OpenDungeonsIcons/AbortIcon";
    const std::string workIcon = "OpenDungeonsIcons/CogIcon";

    float curSkillProgress;
    SkillType curResType;
    if(!localPlayerSeat->getCurrentSkillProgress(curResType, curSkillProgress))
    {
        curResType = SkillType::nullSkillType;
    }

    // We show/hide the icons depending on available skills
    CEGUI::Window* guiSheet = mRootWindow;
    CEGUI::Window* skillsWindow = guiSheet->getChild("SkillTreeWindow/Skills");

    CEGUI::Window* skillButton = skillsWindow->getChild(skillButtonName);
    CEGUI::ProgressBar* skillProgressBar =
        static_cast<CEGUI::ProgressBar*>(skillsWindow->getChild(skillProgressBarName));
    if(isDone)
    {
        guiSheet->getChild(castButtonName)->show();
        skillButton->setText("");
        skillButton->setProperty("StateImage", okIcon);
        skillButton->setProperty("StateImageColour", "FF00BB00");
        skillButton->setEnabled(false);
        skillProgressBar->hide();
    }
    else if(!isAllowed)
    {
        guiSheet->getChild(castButtonName)->show();
        skillButton->setText("");
        skillButton->setProperty("StateImage", abortIcon);
        skillButton->setProperty("StateImageColour", "FFBB0000");
        skillButton->setEnabled(false);
        skillProgressBar->hide();
    }
    else if(resType == curResType)
    {
        // The skill is not available but skill is being done
        guiSheet->getChild(castButtonName)->hide();
        if(queueNumber == 0)
            skillButton->setText("");
        else
            skillButton->setText(Helper::toString(queueNumber));

        skillButton->setProperty("StateImage", workIcon);
        skillButton->setProperty("StateImageColour", "FF888800");
        skillButton->setEnabled(true);
        if (curSkillProgress > 0.0f)
        {
            // We reload the values is the current skill changed or if its value changed
            skillProgressBar->show();
            if((mSkillCurrentCompletion.mProgressBar != skillProgressBar) ||
               (mSkillCurrentCompletion.mCompleteness != curSkillProgress))
            {
                mSkillCurrentCompletion.setValue(skillProgressBar, curSkillProgress);
                skillProgressBar->setProgress(0);
            }
        }
        else
        {
            skillProgressBar->hide();
        }
    }
    else if (queueNumber >= 1)
    {
        // The skill is not available but skill is pending
        guiSheet->getChild(castButtonName)->hide();
        skillButton->setText(Helper::toString(queueNumber));
        skillButton->setProperty("StateImage", pendingIcon);
        skillButton->setProperty("StateImageColour", "FFFFFFFF");
        skillButton->setEnabled(true);
        skillProgressBar->hide();
    }
    else
    {
        // The skill is not available and skill is not pending
        guiSheet->getChild(castButtonName)->hide();
        skillButton->setText("");
        skillButton->setProperty("StateImage", "");
        skillButton->setEnabled(true);
        skillProgressBar->hide();
    }
}

void GameMode::refreshGuiSkill(bool forceRefresh)
{
    Seat* localPlayerSeat = mGameMap->getLocalPlayer()->getSeat();

    // If the percentage or the pending skill changed, we force refresh
    float curSkillProgress;
    SkillType curResType;
    if(localPlayerSeat->getCurrentSkillProgress(curResType, curSkillProgress) &&
       ((mCurrentSkillType != curResType) ||
        (mCurrentSkillProgress != curSkillProgress)))
    {
        forceRefresh = true;
    }

    if(!forceRefresh && !localPlayerSeat->getGuiSkillNeedsRefresh())
        return;

    if(mIsSkillWindowOpen && localPlayerSeat->getGuiSkillNeedsRefresh())
    {
        // We check if the temporary current pending list changed.
        for(auto it = mSkillPending.begin(); it != mSkillPending.end();)
        {
            SkillType resType = *it;
            if(!localPlayerSeat->isSkillDone(resType))
            {
                ++it;
                continue;
            }

            it = mSkillPending.erase(it);
        }
    }

    localPlayerSeat->guiSkillRefreshed();

    // We show/hide each icon depending on available skills
    SkillManager::listAllSkills([this](const std::string& skillButtonName, const std::string& castButtonName,
        const std::string& skillProgressBarName, SkillType resType)
    {
        refreshSkillButtonState(skillButtonName, castButtonName, skillProgressBarName, resType);
    });
}

void GameMode::refreshSpellButtonCoolDowns()
{
    Player* player = mGameMap->getLocalPlayer();
    if (player == nullptr)
    {
        OD_LOG_ERR("No local player");
        return;
    }

    // We show/hide each icon depending on available skills
    SkillManager::listAllSpellsProgressBars([this, player](SpellType spellType, const std::string& castProgressBarName)
    {
        CEGUI::ProgressBar* progressBar = static_cast<CEGUI::ProgressBar*>(mRootWindow->getChild(castProgressBarName));
        float progress = player->getSpellCooldownSmooth(spellType);
        if (progress > 0.0)
        {
            progressBar->show();
            progressBar->setProgress(progress);
        }
        else
        {
            progressBar->hide();
        }
    });
}

void GameMode::refreshActionFeedback(float elapsed)
{
    InputManager& inputManager = mModeManager->getInputManager();
    if(isMouseDownOnCEGUIWindow())
    {
        unselectAllTiles();
        mActionTargetText.clear();
        CEGUI::Window* hover = CEGUI::System::getSingleton().getDefaultGUIContext().getWindowContainingMouse();
        if(hover != nullptr)
            mActionTargetText = hover->getTooltipText().c_str();
        if(inputManager.mHighlightedCreature != nullptr)
        {
            inputManager.mHighlightedCreature->normalizeAmbient();
            inputManager.mHighlightedCreature = nullptr;
        }
        TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");
    }
    else
    {
        // Recompute the tile when the camera moves under a stationary pointer, too.
        const CEGUI::Sizef size = CEGUI::System::getSingleton().getDefaultGUIContext().getSurfaceSize();
        OIS::MouseState mouseState;
        mouseState.width = static_cast<int>(size.d_width);
        mouseState.height = static_cast<int>(size.d_height);
        const OIS::MouseEvent mouseEvent(nullptr, mouseState);
        if(ODFrameListener::getSingleton().findWorldPositionFromMouse(mouseEvent,
            inputManager.mKeeperHandPos, RenderManager::KEEPER_HAND_WORLD_Z))
        {
            inputManager.mXPos = Helper::round(inputManager.mKeeperHandPos.x);
            inputManager.mYPos = Helper::round(inputManager.mKeeperHandPos.y);
        }
        else
        {
            inputManager.mXPos = -1;
            inputManager.mYPos = -1;
        }
        if(!inputManager.mLMouseDown)
        {
            inputManager.mLStartDragX = inputManager.mXPos;
            inputManager.mLStartDragY = inputManager.mYPos;
        }
        // Empty-hand hover uses the pickup target selected by handlePlayerActionNone.
        // Other states must also follow camera movement beneath a stationary pointer.
        if(mPlayerSelection.getCurrentAction() != SelectedAction::none ||
           mGameMap->getLocalPlayer()->numObjectsInHand() > 0 || mGameMap->getGamePaused())
        {
            Tile* hoveredTile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);
            Creature* creature = hoveredTile != nullptr ?
                hoveredTile->getClosestCreature(inputManager.mCreatureTypeForOutliner) : nullptr;
            if(inputManager.mHighlightedCreature != creature)
            {
                if(inputManager.mHighlightedCreature != nullptr)
                    inputManager.mHighlightedCreature->normalizeAmbient();
                inputManager.mHighlightedCreature = creature;
                if(creature != nullptr)
                    creature->maxAmbient();
            }
        }
        // A release leaves validated in the input manager: a frame must never repeat it.
        const InputCommandState previousState = inputManager.mCommandState;
        inputManager.mCommandState = inputManager.mLMouseDown ? InputCommandState::building : InputCommandState::infoOnly;
        if(mGameMap->getGamePaused())
        {
            unselectAllTiles();
            displayText(Ogre::ColourValue::Red, "The game is paused.");
        }
        else
            checkInputCommand();
        inputManager.mCommandState = previousState;
    }

    mRootWindow->getChild("ContextInfo")->setText(mActionTargetText);
    CEGUI::Window* icon = mRootWindow->getChild("HandActionIcon");
    const bool overGui = isMouseDownOnCEGUIWindow();
    Player* player = mGameMap->getLocalPlayer();
    const bool active = mPlayerSelection.getCurrentAction() != SelectedAction::none;
    const bool holding = player->numObjectsInHand() > 0;
    const std::string button = SkillManager::getSelectedButton(mPlayerSelection);
    const bool prohibited = !mActionTargetValid && (active || holding);
    icon->setVisible(!overGui && (prohibited || !button.empty()));
    if(icon->isVisible())
    {
        icon->setProperty("Image", prohibited ? "OpenDungeonsIcons/Prohibition" :
            mRootWindow->getChild(button)->getProperty("NormalImage"));
        const CEGUI::Vector2f pointer = CEGUI::System::getSingleton().getDefaultGUIContext().getMouseCursor().getPosition();
        const float scale = icon->getPixelSize().d_width / 50.0f;
        icon->setPosition(CEGUI::UVector2(CEGUI::UDim(0, pointer.d_x + 90.0f * scale),
            CEGUI::UDim(0, pointer.d_y + 8.0f * scale)));
    }
    Tile* tile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    const bool digging = !overGui && !holding && !mGameMap->getGamePaused() && tile != nullptr &&
        (mPlayerSelection.getCurrentAction() == SelectedAction::selectTile ||
         (!active && !mPreviewTiles.empty() && tile->isDiggable(player->getSeat())));
    RenderManager::getSingleton().rrSetHandPose(!overGui && !holding && (active || mActionTargetValid), digging);

}


void GameMode::selectSquaredTiles(int tileX1, int tileY1, int tileX2, int tileY2)
{
    // Collect the eligible region for the outlined world preview.
    std::vector<Tile*> affectedTiles = mGameMap->rectangularRegion(tileX1,
        tileY1, tileX2, tileY2);

    selectTiles(affectedTiles);
}

void GameMode::selectTiles(const std::vector<Tile*> tiles)
{
    mPreviewTiles = tiles;
}

void GameMode::updateSelectedTiles()
{
    const bool building = mPlayerSelection.getCurrentAction() == SelectedAction::buildRoom ||
        mPlayerSelection.getCurrentAction() == SelectedAction::buildTrap;
    if(!mActionTargetValid && !building)
        mPreviewTiles.clear();
    const Ogre::ColourValue colour = mActionTargetValid ? Ogre::ColourValue(0.35f, 0.3f, 1.0f) :
        Ogre::ColourValue(1.0f, 0.15f, 0.1f);
    if(mPreviewTiles == mSelectedTiles)
    {
        RenderManager::getSingleton().rrDrawTilePreview(mSelectedTiles, colour);
        return;
    }
    Player* player = mGameMap->getLocalPlayer();
    for(Tile* tile : mSelectedTiles)
        tile->setSelected(false, player);
    mSelectedTiles = mPreviewTiles;
    RenderManager::getSingleton().rrDrawTilePreview(mSelectedTiles, colour);
}

void GameMode::unselectAllTiles()
{
    Player* player = mGameMap->getLocalPlayer();
    for(Tile* tile : mSelectedTiles)
        tile->setSelected(false, player);
    mSelectedTiles.clear();
    mPreviewTiles.clear();
    RenderManager::getSingleton().rrDrawTilePreview(mSelectedTiles, Ogre::ColourValue::White);
}

void GameMode::displayText(const Ogre::ColourValue& txtColour, const std::string& txt)
{
    // Callers supply both eligibility and context; do not discard the former with the old label.
    mActionTargetValid = txtColour != Ogre::ColourValue::Red;
    mActionTargetText = txt;
    mRootWindow->getChild("ContextInfo")->setText(txt);
    TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");
}

void GameMode::checkInputCommand()
{
    // In the gamemode, by default, we select tiles
    const InputManager& inputManager = mModeManager->getInputManager();

    mPreviewTiles.clear();
    mActionTargetValid = false;
    mActionTargetText.clear();
    if(mPlayerSelection.getCurrentAction() != SelectedAction::none &&
       mGameMap->getTile(inputManager.mXPos, inputManager.mYPos) == nullptr)
    {
        displayText(Ogre::ColourValue::Red, "Point at a tile inside the map.");
        unselectAllTiles();
        return;
    }

    switch(mPlayerSelection.getCurrentAction())
    {
        case SelectedAction::none:
            handlePlayerActionNone();
            break;
        case SelectedAction::selectTile:
            handlePlayerActionSelectTile();
            break;
        case SelectedAction::buildRoom:
            RoomManager::checkBuildRoom(mGameMap, mPlayerSelection.getNewRoomType(), inputManager, *this);
            break;
        case SelectedAction::destroyRoom:
            RoomManager::checkSellRoomTiles(mGameMap, inputManager, *this);
            break;
        case SelectedAction::castSpell:
            SpellManager::checkSpellCast(mGameMap, mPlayerSelection.getNewSpellType(), inputManager, *this);
            break;
        case SelectedAction::buildTrap:
            TrapManager::checkBuildTrap(mGameMap, mPlayerSelection.getNewTrapType(), inputManager, *this);
            break;
        case SelectedAction::destroyTrap:
            TrapManager::checkSellTrapTiles(mGameMap, inputManager, *this);
            break;
        default:
            break;
    }
    if(inputManager.mCommandState != InputCommandState::validated)
        updateSelectedTiles();
    else
        unselectAllTiles();
}

void GameMode::handlePlayerActionNone()
{
    const InputManager& inputManager = mModeManager->getInputManager();
    Player* player = mGameMap->getLocalPlayer();
    if(player->numObjectsInHand() == 0)
    {
        Tile* tile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);
        GameEntity* closest = nullptr;
        double distance = 0.0;
        if(tile != nullptr)
        {
            std::vector<GameEntity*> entities;
            tile->fillWithEntities(entities, SelectionEntityWanted::any, player);
            for(GameEntity* entity : entities)
            {
                if(!entity->tryPickup(player->getSeat()))
                    continue;
                const Ogre::Vector3& pos = entity->getPosition();
                const double candidate = Pathfinding::squaredDistance(pos.x, inputManager.mKeeperHandPos.x,
                    pos.y, inputManager.mKeeperHandPos.y);
                if(closest == nullptr || candidate < distance)
                {
                    closest = entity;
                    distance = candidate;
                }
            }
            if(closest != nullptr)
            {
                Creature* creature = dynamic_cast<Creature*>(closest);
                if(closest->getObjectType() == GameEntityType::chickenEntity)
                    displayText(Ogre::ColourValue::White, "Chicken");
                else if(closest->getObjectType() == GameEntityType::treasuryObject)
                    displayText(Ogre::ColourValue::White, "Gold");
                else
                    displayText(Ogre::ColourValue::White, creature != nullptr ?
                        creature->getDefinition()->getClassName() : closest->getName());
            }
            else if(tile->isDiggable(player->getSeat()))
            {
                displayText(Ogre::ColourValue::White, tile->getMarkedForDigging(player) ?
                    "Marked wall. Click or drag to remove digging marks." : "Wall. Click or drag to mark for digging.");
                selectSquaredTiles(tile->getX(), tile->getY(), tile->getX(), tile->getY());
            }
        }
        InputManager& mutableInput = mModeManager->getInputManager();
        Creature* creature = dynamic_cast<Creature*>(closest);
        if(mutableInput.mHighlightedCreature != creature)
        {
            if(mutableInput.mHighlightedCreature != nullptr)
                mutableInput.mHighlightedCreature->normalizeAmbient();
            mutableInput.mHighlightedCreature = creature;
            if(creature != nullptr)
                creature->maxAmbient();
        }
        TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");
        return;
    }

    Tile* tile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    if(tile == nullptr)
        displayText(Ogre::ColourValue::Red, "Point at a tile inside the map.");
    else if(player->isDropHandPossible(tile))
    {
        displayText(Ogre::ColourValue::White, "Right-click to place the first object in hand.");
        selectTiles({tile});
    }
    else if(tile->isFullTile())
        displayText(Ogre::ColourValue::Red, "Dig out this tile before dropping an object.");
    else if(!player->getSeat()->hasVisionOnTile(tile))
        displayText(Ogre::ColourValue::Red, "You cannot drop objects outside your vision.");
    else if(tile->getCoveringRoom() != nullptr && tile->getCoveringRoom()->getType() == RoomType::arena &&
            player->getObjectsInHand().front()->getObjectType() == GameEntityType::creature)
        displayText(Ogre::ColourValue::Red, "The arena has no available place for this creature.");
    else
        displayText(Ogre::ColourValue::Red, "This object needs ground claimed by you or an ally.");
}

void GameMode::handlePlayerActionSelectTile()
{
    const InputManager& inputManager = mModeManager->getInputManager();
    Player* player = mGameMap->getLocalPlayer();
    std::vector<Tile*> tiles = mGameMap->rectangularRegion(inputManager.mXPos, inputManager.mYPos,
        inputManager.mLStartDragX, inputManager.mLStartDragY);
    std::vector<Tile*> diggableTiles;
    for(Tile* tile : tiles)
    {
        if(mDigSetBool ? tile->isDiggable(player->getSeat()) : tile->getMarkedForDigging(player))
            diggableTiles.push_back(tile);
    }
    if(diggableTiles.empty())
    {
        Tile* tile = mGameMap->getTile(inputManager.mXPos, inputManager.mYPos);
        if(!mDigSetBool)
            displayText(Ogre::ColourValue::Red, "No marked walls in this selection.");
        else if(tile != nullptr && !tile->isFullTile())
            displayText(Ogre::ColourValue::Red, "This ground is already dug out.");
        else if(tile != nullptr && tile->isClaimed() && !tile->isClaimedForSeat(player->getSeat()))
            displayText(Ogre::ColourValue::Red, "You cannot dig an enemy's claimed wall.");
        else
            displayText(Ogre::ColourValue::Red, "This wall cannot be dug out.");
        if(inputManager.mCommandState == InputCommandState::validated)
            mPlayerSelection.setCurrentAction(SelectedAction::none);
        return;
    }
    displayText(Ogre::ColourValue::White, mDigSetBool ? "Release to mark selected walls for digging." :
        "Release to remove the digging marks.");
    if(inputManager.mCommandState != InputCommandState::validated)
    {
        selectTiles(diggableTiles);
        return;
    }

    unselectAllTiles();

    ClientNotification *clientNotification = new ClientNotification(
        ClientNotificationType::askMarkTiles);
    clientNotification->mPacket << inputManager.mXPos << inputManager.mYPos;
    clientNotification->mPacket << inputManager.mLStartDragX << inputManager.mLStartDragY;
    clientNotification->mPacket << mDigSetBool;
    ODClient::getSingleton().queueClientNotification(clientNotification);
    mPlayerSelection.setCurrentAction(SelectedAction::none);
}

void GameMode::resetSkillTree()
{
    mSkillPending = mGameMap->getLocalPlayer()->getSeat()->getSkillPending();
    mSkillCurrentCompletion.resetValue();
    mIsSkillWindowOpen = true;
}

bool GameMode::skillButtonTreeClicked(SkillType type)
{
    // If the skill is already done or not allowed, nothing to do
    const std::vector<SkillType>& skillDone = mGameMap->getLocalPlayer()->getSeat()->getSkillDone();
    if(std::find(skillDone.begin(), skillDone.end(), type) != skillDone.end())
        return false;
    const std::vector<SkillType>& skillNotAllowed = mGameMap->getLocalPlayer()->getSeat()->getSkillNotAllowed();
    if(std::find(skillNotAllowed.begin(), skillNotAllowed.end(), type) != skillNotAllowed.end())
        return false;

    auto it = std::find(mSkillPending.begin(), mSkillPending.end(), type);
    if(it != mSkillPending.end())
    {
        // The skill is pending. We remove it as well as all its dependencies
        mSkillPending.erase(it);

        for(it = mSkillPending.begin(); it != mSkillPending.end();)
        {
            SkillType pendingType = *it;
            const Skill* skill = SkillManager::getSkill(pendingType);
            if(skill == nullptr)
            {
                OD_LOG_ERR("null skill pendingType=" + Helper::toString(static_cast<uint32_t>(pendingType)));
                continue;
            }

            if(!skill->dependsOn(type))
            {
                ++it;
                continue;
            }
            it = mSkillPending.erase(it);
        }
        return true;
    }

    std::vector<SkillType> skillToAdd;

    // The skill is not pending. We need to check availability to all its dependencies and
    // add them at the end of the list if all are available/done
    const Skill* skill = SkillManager::getSkill(type);
    std::vector<SkillType> dependencies;
    skill->buildDependencies(skillDone, dependencies);

    // We check if one of the dependencies is not available. If not, we cannot skill
    for(SkillType skillType : dependencies)
    {
        if(std::find(skillNotAllowed.begin(), skillNotAllowed.end(), skillType) != skillNotAllowed.end())
            return false;

        if(std::find(mSkillPending.begin(), mSkillPending.end(), skillType) != mSkillPending.end())
            continue;

        skillToAdd.push_back(skillType);
    }

    for(SkillType skillType : skillToAdd)
        mSkillPending.push_back(skillType);

    return true;
}

bool GameMode::autoFillSkillWindow(const CEGUI::EventArgs&)
{
    SkillManager::buildRandomPendingSkillsForSeat(mSkillPending,
        mGameMap->getLocalPlayer()->getSeat());
    refreshGuiSkill(true);
    return true;
}

void GameMode::endSkillTree(bool apply)
{
    if(apply)
    {
        uint32_t nbItems = static_cast<uint32_t>(mSkillPending.size());
        ClientNotification *clientNotification = new ClientNotification(
            ClientNotificationType::askSetSkillTree);
        clientNotification->mPacket << nbItems;
        for(const SkillType& type : mSkillPending)
        {
            clientNotification->mPacket << type;
        }
        ODClient::getSingleton().queueClientNotification(clientNotification);
    }

    mSkillPending.clear();
    mSkillCurrentCompletion.resetValue();
    mIsSkillWindowOpen = false;
    refreshGuiSkill(true);
}

void GameMode::buildPlayerSettingsWindow()
{
    if(!mSeatIds.empty())
        return;

    CEGUI::Window* tmpWin = mRootWindow->getChild("PlayerSettingsWindow/Seats/SeatsSP");
    CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
    float offset = 15;
    for(Seat* seat : mGameMap->getSeats())
    {
        if(seat->isRogueSeat())
            continue;

        Player* player = seat->getPlayer();
        if(player == nullptr)
            continue;

        CEGUI::DefaultWindow* tmpElt;
        std::string name;
        Ogre::ColourValue seatColor = seat->getColorValue();
        seatColor.a = 1.0f; // Restore the color opacity

        name = TEXT_SEAT_ID_PREFIX + Helper::toString(seat->getId());
        tmpElt = static_cast<CEGUI::DefaultWindow*>(winMgr.createWindow("OD/StaticText", name));
        tmpWin->addChild(tmpElt);
        tmpElt->setArea(CEGUI::UDim(0,20), CEGUI::UDim(0, offset), CEGUI::UDim(0.2,-20), CEGUI::UDim(0,offset));
        tmpElt->setText("[colour='" + Helper::getCEGUIColorFromOgreColourValue(seatColor) + "']" + Helper::toString(seat->getId()));
        tmpElt->setProperty("FrameEnabled", "False");
        tmpElt->setProperty("BackgroundEnabled", "False");

        name = TEXT_SEAT_PLAYER_NICKNAME_PREFIX + Helper::toString(seat->getId());
        tmpElt = static_cast<CEGUI::DefaultWindow*>(winMgr.createWindow("OD/StaticText", name));
        tmpWin->addChild(tmpElt);
        tmpElt->setArea(CEGUI::UDim(0.2,20), CEGUI::UDim(0, offset), CEGUI::UDim(0.6,-20), CEGUI::UDim(0,offset));
        tmpElt->setText("[colour='" + Helper::getCEGUIColorFromOgreColourValue(seatColor) + "']" + player->getNick());
        tmpElt->setProperty("FrameEnabled", "False");
        tmpElt->setProperty("BackgroundEnabled", "False");

        name = TEXT_SEAT_TEAM_ID_PREFIX + Helper::toString(seat->getId());
        tmpElt = static_cast<CEGUI::DefaultWindow*>(winMgr.createWindow("OD/StaticText", name));
        tmpWin->addChild(tmpElt);
        tmpElt->setArea(CEGUI::UDim(0.8,20), CEGUI::UDim(0, offset), CEGUI::UDim(0.2,-20), CEGUI::UDim(0,offset));
        tmpElt->setText("[colour='" + Helper::getCEGUIColorFromOgreColourValue(seatColor) + "']" + Helper::toString(seat->getTeamId()));
        tmpElt->setProperty("FrameEnabled", "False");
        tmpElt->setProperty("BackgroundEnabled", "False");

        mSeatIds.push_back(seat->getId());
        offset += 15;
    }

    getModeManager().getGui().registerWindowHierarchy(tmpWin);
}
