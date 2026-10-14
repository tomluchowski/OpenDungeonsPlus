/*!
 * \file   ODFrameListener.cpp
 * \date   09 April 2011
 * \author Ogre team, andrewbuck, oln, StefanP.MUC
 * \brief  Handles the input and rendering request
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

#include "render/ODFrameListener.h"

#include "entities/Creature.h"
#include "eventsystem/ClockTick.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/DraggableTileContainer.h"
#include "modes/AbstractApplicationMode.h"
#include "modes/GameEditorModeConsole.h"
#include "modes/ModeManager.h"
#include "network/ODServer.h"
#include "network/ODClient.h"
#include "render/DebugDrawer.h"
#include "render/CreatureOverlayStatus.h"
#include "render/MovableTextOverlay.h"
#include "render/Gui.h"
#include "render/RenderManager.h"
#include "render/TextRenderer.h"
#include "renderscene/RenderSceneMenu.h"
#include "sound/MusicPlayer.h"
#include "sound/SoundEffectsManager.h"
#include "utils/Helper.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"

#include <OgreCamera.h>
#include <OgreRenderWindow.h>
#include <OgreRenderSystem.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <Overlay/OgreOverlaySystem.h>

#include <CEGUI/EventArgs.h>
#include <CEGUI/Window.h>
#include <CEGUI/Size.h>
#include <CEGUI/System.h>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

#include <signal.h>

template<> ODFrameListener* Ogre::Singleton<ODFrameListener>::msSingleton = nullptr;



namespace
{
    const unsigned int DEFAULT_FRAME_RATE = 60;
}

/*! \brief This constructor is where the OGRE rendering system is initialized and started.
 *
 * The primary function of this routine is to initialize variables, and start
 * up the OGRE system.
 */
ODFrameListener::ODFrameListener(const std::string& mainSceneFileName, Ogre::RenderWindow* renderWindow, Ogre::OverlaySystem* overLaySystem, Gui* gui) :
    lastSecondFrameRenderingQueued(0),
    mInitialized(false),
    mWindow(renderWindow),
    mPrimaryWindow(renderWindow),
    mRenderWindowRecreationPending(false),
    mRenderWindowSequence(0),
    mGui(gui),
    mRenderManager(RenderManager::getSingletonPtr()),
    mGameMap(Utils::make_unique<GameMap>(false)),
    mModeManager(Utils::make_unique<ModeManager>(renderWindow, gui)),
    mMainScene(Utils::make_unique<RenderSceneMenu>()),
    mShowDebugInfo(false),
    mContinue(true),
    mEventMaxTimeDisplay(20.0f),
    mExitRequested(false),
    mCameraManager(mRenderManager->getSceneManager(), mGameMap.get(), renderWindow),
    mFpsLimiter(DEFAULT_FRAME_RATE),
    mIsMainMenuCreated(false),
    currentSeconds(0),
    currentMinutes(0)    
{
    OD_LOG_INF("Creating frame listener...");

    mRenderManager->createScene(mCameraManager.getViewport());

    mRenderManager->getSceneManager()->addRenderQueueListener(this);
    //Set initial mouse clipping size
    windowResized(mWindow);

    //Register as a Window listener
    Ogre::WindowEventUtilities::addWindowEventListener(mWindow, this);

    readMainScene(mainSceneFileName);

    mInitialized = true;
   
}

void ODFrameListener::windowResized(Ogre::RenderWindow* rw)
{
    unsigned int width, height;
    int left, top;
    rw->getMetrics(width, height, left, top);

    if(width == 0 || height == 0)
        return;

    Ogre::Camera* camera = mCameraManager.getActiveCamera();
    camera->setAspectRatio(static_cast<Ogre::Real>(width) / static_cast<Ogre::Real>(height));

    mModeManager->getInputManager().setWidthAndHeight(width, height);
    //Notify CEGUI that the display size has changed.
    CEGUI::System::getSingleton().notifyDisplaySizeChanged(CEGUI::Size<float>(
            static_cast<float> (width), static_cast<float> (height)));
}

void ODFrameListener::windowClosed(Ogre::RenderWindow*)
{
    // Stop the render loop: we are about to destroy the mode manager, which
    // frameStarted() dereferences unconditionally.
    requestExit();

    // We remove the mode manager to make sure it is destroyed before the window is. That
    // allows to release all taken resources in the mode
    mModeManager = nullptr;
}

ODFrameListener::~ODFrameListener()
{
    OD_LOG_INF("destructor of ODFrameListener");
    if (mInitialized)
        exitApplication();

    mGameMap->clearAll();
}

void ODFrameListener::requestExit()
{
    mExitRequested = true;
}

void ODFrameListener::requestRenderWindowRecreation(
    const std::map<std::string, std::string>& previousRendererOptions,
    const std::map<std::string, std::string>& previousVideoConfig)
{
    mPreviousRendererOptions = previousRendererOptions;
    mPreviousVideoConfig = previousVideoConfig;
    mRenderWindowRecreationPending = true;
}

void ODFrameListener::restorePreviousVideoSettings()
{
    Ogre::RenderSystem* renderer = Ogre::Root::getSingleton().getRenderSystem();
    std::map<std::string, std::string>::const_iterator fullscreen =
        mPreviousRendererOptions.find(Config::FULL_SCREEN);
    if(fullscreen != mPreviousRendererOptions.end())
        renderer->setConfigOption(fullscreen->first, fullscreen->second);
    std::map<std::string, std::string>::const_iterator videoMode =
        mPreviousRendererOptions.find(Config::VIDEO_MODE);
    if(videoMode != mPreviousRendererOptions.end())
        renderer->setConfigOption(videoMode->first, videoMode->second);
    for(const std::pair<const std::string, std::string>& option : mPreviousRendererOptions)
    {
        if(option.first == Config::FULL_SCREEN || option.first == Config::VIDEO_MODE)
            continue;
        renderer->setConfigOption(option.first, option.second);
    }

    ConfigManager& config = ConfigManager::getSingleton();
    for(const std::pair<const std::string, std::string>& option : mPreviousVideoConfig)
        config.setVideoValue(option.first, option.second);
    config.saveUserConfig();
    mPreviousRendererOptions.clear();
    mPreviousVideoConfig.clear();
}

void ODFrameListener::applyPendingRenderWindowRecreation()
{
    if(!mRenderWindowRecreationPending)
        return;
    mRenderWindowRecreationPending = false;

    Ogre::Root& root = Ogre::Root::getSingleton();
    Ogre::RenderSystem* renderer = root.getRenderSystem();
    Ogre::RenderWindow* previousWindow = mWindow;
    Ogre::RenderWindow* replacementWindow = nullptr;
    unsigned int previousWidth = 0;
    unsigned int previousHeight = 0;
    int previousLeft = 0;
    int previousTop = 0;
    previousWindow->getMetrics(previousWidth, previousHeight, previousLeft, previousTop);
    const bool previousWasFullScreen = previousWindow->isFullScreen();
    bool guiMoved = false;
    bool cameraMoved = false;
    bool inputMoved = false;
    bool listenerMoved = false;
    bool windowRegistered = false;

    try
    {
        Ogre::RenderWindowDescription description = renderer->getRenderWindowDescription();
        description.name = "OpenDungeonsLiveSettings" + Helper::toString(++mRenderWindowSequence);
        description.miscParams["title"] = mPrimaryWindow->getName();
        description.miscParams["hidden"] = "true";
        if(!description.useFullScreen && !previousWasFullScreen)
        {
            description.miscParams["left"] = Helper::toString(previousLeft);
            description.miscParams["top"] = Helper::toString(previousTop);
        }

        replacementWindow = root.createRenderWindow(description);
        replacementWindow->setAutoUpdated(false);
        replacementWindow->setActive(false);
        Ogre::WindowEventUtilities::_addRenderWindow(replacementWindow);
        windowRegistered = true;

        renderer->_setRenderTarget(replacementWindow);
        mCameraManager.setRenderWindow(replacementWindow);
        cameraMoved = true;
        mRenderManager->setViewport(mCameraManager.getViewport());
        mGui->setRenderTarget(*replacementWindow);
        guiMoved = true;
        if(!mModeManager->getInputManager().setRenderWindow(replacementWindow))
            throw std::runtime_error("input initialization failed for the replacement window");
        mModeManager->getInputManager().refreshSettings();
        inputMoved = true;

        Ogre::WindowEventUtilities::removeWindowEventListener(previousWindow, this);
        Ogre::WindowEventUtilities::addWindowEventListener(replacementWindow, this);
        listenerMoved = true;
        mWindow = replacementWindow;

        previousWindow->setAutoUpdated(false);
        previousWindow->setActive(false);
        previousWindow->setVisible(false);
        if(previousWasFullScreen)
        {
            previousWindow->setFullscreen(false, previousWidth, previousHeight);
            if(description.useFullScreen)
            {
                replacementWindow->setFullscreen(false, description.width, description.height);
                replacementWindow->setFullscreen(true, description.width, description.height);
            }
        }
        replacementWindow->setVisible(true);
        replacementWindow->setActive(true);
        replacementWindow->setAutoUpdated(true);
        replacementWindow->windowMovedOrResized();
        windowResized(replacementWindow);

        if(previousWindow != mPrimaryWindow)
        {
            Ogre::WindowEventUtilities::_removeRenderWindow(previousWindow);
            root.destroyRenderTarget(previousWindow);
        }
        mPreviousRendererOptions.clear();
        mPreviousVideoConfig.clear();
        OD_LOG_INF("Applied video settings without restarting the game");
    }
    catch(const std::exception& error)
    {
        OD_LOG_ERR("Could not apply video settings: " + std::string(error.what()));
        renderer->_setRenderTarget(previousWindow);
        if(listenerMoved)
        {
            Ogre::WindowEventUtilities::removeWindowEventListener(replacementWindow, this);
            Ogre::WindowEventUtilities::addWindowEventListener(previousWindow, this);
            mWindow = previousWindow;
        }
        if(inputMoved)
            mModeManager->getInputManager().setRenderWindow(previousWindow);
        if(guiMoved)
            mGui->setRenderTarget(*previousWindow);
        if(cameraMoved)
        {
            mCameraManager.setRenderWindow(previousWindow);
            mRenderManager->setViewport(mCameraManager.getViewport());
        }
        if(replacementWindow != nullptr)
        {
            if(windowRegistered)
                Ogre::WindowEventUtilities::_removeRenderWindow(replacementWindow);
            root.destroyRenderTarget(replacementWindow);
        }
        if(previousWasFullScreen && !previousWindow->isFullScreen())
            previousWindow->setFullscreen(true, previousWidth, previousHeight);
        restorePreviousVideoSettings();
        previousWindow->setVisible(true);
        previousWindow->setActive(true);
        previousWindow->setAutoUpdated(true);
        windowResized(previousWindow);
    }
}

void ODFrameListener::prepareRenderWindowShutdown()
{
    if(mInitialized)
        exitApplication();
    mModeManager.reset();

    if(mWindow != mPrimaryWindow)
    {
        Ogre::WindowEventUtilities::_removeRenderWindow(mWindow);
        Ogre::Root& root = Ogre::Root::getSingleton();
        root.getRenderSystem()->_setRenderTarget(mPrimaryWindow);
        mGui->setRenderTarget(*mPrimaryWindow);
        root.destroyRenderTarget(mWindow);
        mWindow = mPrimaryWindow;
    }
    Ogre::WindowEventUtilities::_removeRenderWindow(mPrimaryWindow);
}

void ODFrameListener::exitApplication()
{
    OD_LOG_INF("Closing down.");

    ODClient::getSingleton().notifyExit();
    ODServer::getSingleton().notifyExit();
    mGameMap->clearAll();

    OD_LOG_INF("Remove listener registration");
    //Remove ourself as a Window listener
    Ogre::WindowEventUtilities::removeWindowEventListener(mWindow, this);

    OD_LOG_INF("Frame listener uninitialization done.");
    mInitialized = false;
}

void ODFrameListener::updateAnimations(Ogre::Real timeSinceLastFrame)
{
    updateMenuScene(timeSinceLastFrame);
    MusicPlayer::getSingleton().update(static_cast<float>(timeSinceLastFrame));
    mRenderManager->updateRenderAnimations(timeSinceLastFrame);
    mGameMap->processDeletionQueues();

    mGameMap->updateAnimations(timeSinceLastFrame);
}

bool ODFrameListener::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
    CEGUI::MouseCursor& mouseCursor = CEGUI::System::getSingleton().getDefaultGUIContext().getMouseCursor();
    CEGUI::Vector2<float> mousePos = mouseCursor.getDisplayIndependantPosition();
    RenderManager::getSingleton().moveCursor(mousePos.d_x, mousePos.d_y);

    if (mWindow->isClosed())
        return false;

    // Sleep to limit the framerate to the max value
    mFpsLimiter.sleepIfEarly();
    

    CEGUI::System::getSingleton().injectTimePulse(evt.timeSinceLastFrame);
    CEGUI::System::getSingleton().getDefaultGUIContext().injectTimePulse(evt.timeSinceLastFrame);

    mModeManager->update(evt);
    applyPendingRenderWindowRecreation();

    int64_t currentTurn = mGameMap->getTurnNumber();

    if(!mExitRequested)
    {
        // Updates animations independent from the server new turn event
        updateAnimations(evt.timeSinceLastFrame);
    }

    mCameraManager.updateCameraFrameTime(evt.timeSinceLastFrame);
    mCameraManager.onFrameStarted();

    SoundEffectsManager::getSingleton().updateListener(
        evt.timeSinceLastFrame,
        mCameraManager.getActiveCameraPosition(),
        mCameraManager.getActiveCameraOrientation());

    
    currentMinutes = ODClient::getSingleton().getGameTimeMillis() / 60000;
    currentSeconds = (ODClient::getSingleton().getGameTimeMillis() % 60000)/1000;
    std::stringstream ss ;
    // ss << currentMinutes<< " " << lastMinuteFrameRenderingQueued;
    // OD_LOG_INF(ss.str());
    if( currentSeconds != lastSecondFrameRenderingQueued  )
        // trigger observers dependent on time
    {
        // OD_LOG_INF("Another second of gameplay has passed, triggering dependent events");
        notifyObservers(ClockTick(currentMinutes,currentSeconds));
    }
    lastSecondFrameRenderingQueued = currentSeconds;
    if((currentTurn != -1) && (mGameMap->getGamePaused()) && (!mExitRequested))
        return true;

    //If an exit has been requested, start cleaning up.
    if(mExitRequested == true || mContinue == false)
    {
        exitApplication();
        mContinue = false;
        return mContinue;
    }

    printDebugInfo();

    mGameMap.get()->processDeletionQueues();
    ODClient::getSingleton().processClientSocketMessages();
    ODClient::getSingleton().processClientNotifications();


    
    return mContinue;
}

bool ODFrameListener::frameEnded(const Ogre::FrameEvent& evt)
{

    AbstractApplicationMode* currentMode = mModeManager->getCurrentMode();
    if(currentMode)
        currentMode->onFrameEnded(evt);

    mCameraManager.onFrameEnded();

    return true;
}

bool ODFrameListener::frameStarted(const Ogre::FrameEvent& evt)
{
    AbstractApplicationMode* currentMode = mModeManager->getCurrentMode();
    if(currentMode)
        currentMode->onFrameStarted(evt);
    if(mRenderManager  && mRenderManager->mRenderTarget != nullptr)
    {
        // preRenderTargetUpdate:
        // getOverlayStatus() is null for every creature whose mesh is not currently
        // created, so it has to be checked here the same way Creature::update() does.
        // Remember the overlays we actually hid rather than walking the creature list a
        // second time: that kept the two loops in lockstep only as long as no creature
        // gained or lost its overlay in between.
        for (Creature* creature : mGameMap->getCreatures())
        {
            CreatureOverlayStatus* tmp = creature->getOverlayStatus();
            if(tmp == nullptr)
                continue;

            MovableTextOverlay* overlay = tmp->getMovableTextOverlay();
            if(overlay == nullptr || !overlay->isVisible())
                continue;

            overlay->setVisible(false);
            mTemporaryHiddenOverlays.push_back(overlay);
        }
        mRenderManager->mRenderTarget->update();
        // postRenderTargetUpdate:
        for (MovableTextOverlay* overlay : mTemporaryHiddenOverlays)
            overlay->setVisible(true);

        mTemporaryHiddenOverlays.clear();
    }
   
    return true;
}

void ODFrameListener::renderQueueStarted(Ogre::uint8 queueGroupId, const Ogre::String& invocation,
    bool&)
{
    if(queueGroupId == RenderManager::OD_RENDER_QUEUE_ID_GUI && invocation.empty())
    {
        Ogre::Root::getSingleton().getRenderSystem()->clearFrameBuffer(Ogre::FBT_DEPTH);
        CEGUI::System::getSingleton().renderAllGUIContexts();
        // The static menu background must not inherit the last widget's clip.
        Ogre::Root::getSingleton().getRenderSystem()->setScissorTest(false);
    }
    else if(queueGroupId == Ogre::RenderQueueGroupID::RENDER_QUEUE_MAIN )
    {
        Ogre::Root::getSingleton().getRenderSystem()->clearFrameBuffer(Ogre::FBT_DEPTH);
    }
    
}

bool ODFrameListener::quit(const CEGUI::EventArgs &)
{
    requestExit();
    return true;
}

bool ODFrameListener::findWorldPositionFromMouse(const OIS::MouseEvent &arg, Ogre::Vector3& keeperHand3DPos, Ogre::Real height)
{
    // Setup the ray scene query, use CEGUI's mouse position
    CEGUI::Vector2<float> mousePos = CEGUI::System::getSingleton().getDefaultGUIContext().getMouseCursor().getPosition();// * mMouseScale;
    Ogre::Ray mouseRay = mCameraManager.getActiveCamera()->getCameraToViewportRay(mousePos.d_x / float(
            arg.state.width), mousePos.d_y / float(arg.state.height));


    Ogre::Plane groundPlane(Ogre::Vector3::UNIT_Z, height);


    std::pair<bool, Ogre::Real> p = mouseRay.intersects(groundPlane);
    if(p.first)
    {
        keeperHand3DPos = mouseRay.getPoint(p.second);
        return true;
    }

    return false;

}


bool ODFrameListener::rayIntersectionGameMap(const OIS::MouseEvent &arg, Ogre::Vector3& keeperHand3DPos, DraggableTileContainer* draggableTileContainer)
{
    CEGUI::Vector2<float> mousePos = CEGUI::System::getSingleton().getDefaultGUIContext().getMouseCursor().getPosition();// * mMouseScale;
    Ogre::Ray mouseRay = mCameraManager.getActiveCamera()->getCameraToViewportRay(mousePos.d_x / float(
            arg.state.width), mousePos.d_y / float(arg.state.height));
    Ogre::AxisAlignedBox aab = draggableTileContainer->getAABB();
    
    std::pair<bool, Ogre::Real> p = mouseRay.intersects(aab);
    if(p.first)
    {
        keeperHand3DPos = mouseRay.getPoint(p.second);
        return true;
    }
    return false;
}

void ODFrameListener::printDebugInfo()
{
    std::stringstream infoSS;
    if (getModeManager()->getCurrentModeType() == ModeManager::GAME && mGameMap->getTurnNumber() == -1)
    {
        infoSS << "Waiting for players...\n";
    }
    if (mShowDebugInfo)
    {
        infoSS << "last FPS: " << mWindow->getStatistics().lastFPS;
        infoSS << "\naverage FPS: " <<  mWindow->getStatistics().avgFPS;
        infoSS << "\nbest FPS: " << mWindow->getStatistics().bestFPS;
        infoSS << "\nworse FPS: " << mWindow->getStatistics().worstFPS;       
        infoSS << "\ntriangleCount: " << mWindow->getStatistics().triangleCount;
        infoSS << "\nBatches: " << mWindow->getStatistics().batchCount;
        infoSS << "\nTurn number:  " << mGameMap->getTurnNumber();
        infoSS << "\nCursor:  " << mModeManager->getInputManager().mXPos << ", " << mModeManager->getInputManager().mYPos;
        if(ODClient::getSingleton().isConnected())
        {
            int32_t gameTime = ODClient::getSingleton().getGameTimeMillis() / 1000;
            int32_t seconds = gameTime % 60;
            gameTime /= 60;
            int32_t minutes = gameTime % 60;
            gameTime /= 60;
            infoSS << "\nElapsed time:  " << gameTime << ":" << minutes << ":" << seconds;
        }
    }

    TextRenderer::getSingleton().setText("DebugMessages", infoSS.str());
}

void ODFrameListener::initGameRenderer()
{
    mRenderManager->initGameRenderer(mGameMap.get());
}

void ODFrameListener::stopGameRenderer()
{
    mRenderManager->stopGameRenderer(mGameMap.get());
}

void ODFrameListener::createMainMenuScene()
{
    if(mIsMainMenuCreated)
        return;

    mIsMainMenuCreated = true;
    mMainScene->resetMenu(mCameraManager, *mRenderManager);
}

void ODFrameListener::freeMainMenuScene()
{
    if(!mIsMainMenuCreated)
        return;

    mIsMainMenuCreated = false;
    mMainScene->freeMenu(mCameraManager, *mRenderManager);
}

void ODFrameListener::updateMenuScene(Ogre::Real timeSinceLastFrame)
{
    if(!mIsMainMenuCreated)
        return;

    mMainScene->updateMenu(mCameraManager, *mRenderManager, timeSinceLastFrame);
}

void ODFrameListener::resetCamera(const Ogre::Vector3& position)
{
    mCameraManager.resetCamera(position);
}

void ODFrameListener::moveCamera(CameraManager::Direction direction, double aux)
{
    mCameraManager.move(direction, aux);
}

void ODFrameListener::setActiveCameraNearClipDistance(Ogre::Real value)
{
    mCameraManager.getActiveCamera()->setNearClipDistance(value);
}

Ogre::Real ODFrameListener::getActiveCameraNearClipDistance()
{
    return mCameraManager.getActiveCamera()->getNearClipDistance();
}

void ODFrameListener::setActiveCameraFarClipDistance(Ogre::Real value)
{
    mCameraManager.getActiveCamera()->setFarClipDistance(value);
}

Ogre::Real ODFrameListener::getActiveCameraFarClipDistance()
{
    return mCameraManager.getActiveCamera()->getFarClipDistance();
}

Ogre::Vector3 ODFrameListener::getCameraViewTarget()
{
    return mCameraManager.getCameraViewTarget();
}

void ODFrameListener::cameraFlyTo(const Ogre::Vector3& destination)
{
    mCameraManager.flyTo(destination);
}

void ODFrameListener::readMainScene(const std::string& fileName)
{
    OD_LOG_INF("Load main scene file: " + fileName);
    mMainScene->readSceneMenu(fileName);
}
