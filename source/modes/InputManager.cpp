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

#include "InputManager.h"

#include "gamemap/SelectionEntityWanted.h"
#include "modes/AbstractApplicationMode.h"
#include "modes/SFMLToOISListener.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"

#include <OISMouse.h>
#include <OISKeyboard.h>

#include <OISInputManager.h>
#include <OgreRenderWindow.h>
#include <CEGUI/System.h>
#include <CEGUI/GUIContext.h>
#include <algorithm>
#include <exception>

#if defined OIS_WIN32_PLATFORM && !defined OD_USE_SFML_WINDOW
#include <windows.h>
#endif

InputManager::InputManager(Ogre::RenderWindow* renderWindow):
    mInputManager(nullptr),
    mKeyboard(nullptr),
    mLMouseDown(false),
    mRMouseDown(false),
    mMMouseDown(false),
    mMouseDownOnCEGUIWindow(false),
    mMouseDownOnDraggableTileContainer(false),
    mKeeperHandPos(Ogre::Vector3::ZERO),
    mKeeperHandPosOverBlock(Ogre::Vector3::ZERO),   
    mXPos(0),
    mYPos(0),
    mLStartDragX(0),
    mLStartDragY(0),
    mRStartDragX(0),
    mRStartDragY(0),
    mSeatIdSelected(0),
    offsetDraggableTileContainer(Ogre::Vector2(Ogre::Vector2::ZERO)),
    mCommandState(InputCommandState::infoOnly),
    mMouse(nullptr),
    mCurrentAMode(nullptr),
    mHighlightedCreature(nullptr),
    mCreatureTypeForOutliner(SelectionEntityWanted::creatureAliveAllied),
    mRenderWindow(renderWindow),
    mMouseGrab(false),
    mKeyboardGrab(false)
#ifdef OD_USE_SFML_WINDOW
    ,
    mListener(Utils::make_unique<SFMLToOISListener>(mCurrentAMode, renderWindow->getWidth(), renderWindow->getHeight()))
#endif
{
    OD_LOG_INF("*** Initializing OIS - Input Manager ***");

    for (int i = 0; i < 10; ++i)
    {
        mHotkeyLocationIsValid[i] = false;
        mHotkeyLocation[i].vv = Ogre::Vector3::ZERO;
        mHotkeyLocation[i].qq = Ogre::Quaternion::IDENTITY;
    }

    ConfigManager& config = ConfigManager::getSingleton();
    createInputDevices(config.getInputValue(Config::MOUSE_GRAB, "No", false) == "Yes",
                       config.getInputValue(Config::KEYBOARD_GRAB, "No", false) == "Yes");
}

void InputManager::createInputDevices(bool mouseGrab, bool keyboardGrab)
{
#ifndef OD_USE_SFML_WINDOW

    // Get the Window attribute for OIS.
    size_t windowHnd = 0;
    mRenderWindow->getCustomAttribute("WINDOW", &windowHnd);
    std::ostringstream windowHndStr;
    windowHndStr << windowHnd;

    //setup parameter list for OIS
    OIS::ParamList paramList;
    paramList.insert(std::make_pair(std::string("WINDOW"), windowHndStr.str()));
#if defined OIS_WIN32_PLATFORM
    paramList.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_FOREGROUND" )));
    paramList.insert(std::make_pair(std::string("w32_mouse"), std::string(mouseGrab ? "DISCL_EXCLUSIVE" : "DISCL_NONEXCLUSIVE")));
    paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_FOREGROUND")));
    paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string(keyboardGrab ? "DISCL_EXCLUSIVE" : "DISCL_NONEXCLUSIVE")));
#elif defined OIS_LINUX_PLATFORM
    paramList.insert(std::make_pair(std::string("x11_mouse_grab"), std::string(mouseGrab ? "true" : "false")));
    // When grabbing, OIS tracks the pointer by accumulating relative motion and warps
    // the real pointer back to the window centre near the edges. Leaving the system
    // cursor visible then shows it drifting away from the one CEGUI draws, so hide it.
    paramList.insert(std::make_pair(std::string("x11_mouse_hide"), std::string(mouseGrab ? "true" : "false")));
    paramList.insert(std::make_pair(std::string("x11_keyboard_grab"), std::string(keyboardGrab ? "true" : "false")));
    paramList.insert(std::make_pair(std::string("XAutoRepeatOn"), std::string("true")));
#endif

    //setup InputManager
    mInputManager = OIS::InputManager::createInputSystem(paramList);

    //setup Keyboard
    OIS::Keyboard* oisKeyboard = static_cast<OIS::Keyboard*>(mInputManager->createInputObject(OIS::OISKeyboard, true));

    oisKeyboard->setTextTranslation(OIS::Keyboard::Unicode);

    mKeyboard.reset(new Keyboard(oisKeyboard));

    //setup Mouse
    mMouse = static_cast<OIS::Mouse*>(mInputManager->createInputObject(OIS::OISMouse, true));
#else
    mKeyboard.reset(new Keyboard());
#endif
    mMouseGrab = mouseGrab;
    mKeyboardGrab = keyboardGrab;
    setWidthAndHeight(mRenderWindow->getWidth(), mRenderWindow->getHeight());
    if(mCurrentAMode != nullptr)
        setCurrentAMode(*mCurrentAMode);
}

template<> InputManager* Ogre::Singleton<InputManager>::msSingleton = nullptr;


InputManager::~InputManager()
{
    OD_LOG_INF("*** Destroying Input Manager ***");
    destroyInputDevices();
}

void InputManager::destroyInputDevices()
{
#ifndef OD_USE_SFML_WINDOW
    if(mInputManager != nullptr)
    {
        if(mMouse != nullptr)
            mInputManager->destroyInputObject(mMouse);
        if(mKeyboard != nullptr)
            mInputManager->destroyInputObject(mKeyboard->getKeyboard());
        OIS::InputManager::destroyInputSystem(mInputManager);
    }
    mInputManager = nullptr;
    mMouse = nullptr;
#endif
    mKeyboard.reset();
}

void InputManager::refreshSettings()
{
    // Called before capturing input, never from inside an OIS callback.
    ConfigManager& config = ConfigManager::getSingleton();
    const bool mouseGrab = config.getInputValue(Config::MOUSE_GRAB, "No", false) == "Yes";
    const bool keyboardGrab = config.getInputValue(Config::KEYBOARD_GRAB, "No", false) == "Yes";
    if(mouseGrab == mMouseGrab && keyboardGrab == mKeyboardGrab)
        return;

    const bool previousMouseGrab = mMouseGrab;
    const bool previousKeyboardGrab = mKeyboardGrab;
    destroyInputDevices();
    try
    {
        createInputDevices(mouseGrab, keyboardGrab);
    }
    catch(const std::exception& error)
    {
        OD_LOG_ERR("Could not apply input capture settings: " + std::string(error.what()));
        destroyInputDevices();
        config.setInputValue(Config::MOUSE_GRAB, previousMouseGrab ? "Yes" : "No");
        config.setInputValue(Config::KEYBOARD_GRAB, previousKeyboardGrab ? "Yes" : "No");
        config.saveUserConfig();
        createInputDevices(previousMouseGrab, previousKeyboardGrab);
    }
    mLMouseDown = mRMouseDown = mMMouseDown = false;
}

bool InputManager::setRenderWindow(Ogre::RenderWindow* renderWindow)
{
    if(renderWindow == mRenderWindow)
        return true;

#ifdef OD_USE_SFML_WINDOW
    return false;
#else
    Ogre::RenderWindow* previousWindow = mRenderWindow;
    destroyInputDevices();
    mRenderWindow = renderWindow;
    try
    {
        createInputDevices(mMouseGrab, mKeyboardGrab);
    }
    catch(const std::exception& error)
    {
        OD_LOG_ERR("Could not move input to the new render window: " + std::string(error.what()));
        destroyInputDevices();
        mRenderWindow = previousWindow;
        createInputDevices(mMouseGrab, mKeyboardGrab);
        return false;
    }
    mLMouseDown = mRMouseDown = mMMouseDown = false;
    return true;
#endif
}

void InputManager::setWidthAndHeight(int width, int height)
{
#ifndef OD_USE_SFML_WINDOW
    const OIS::MouseState& ms = mMouse->getMouseState();
    ms.width = width;
    ms.height = height;
#endif
}

void InputManager::setMousePosition(int x, int y)
{
#ifndef OD_USE_SFML_WINDOW
    OIS::MouseState& state = const_cast<OIS::MouseState&>(mMouse->getMouseState());
    x = (std::max)(0, (std::min)(x, state.width - 1));
    y = (std::max)(0, (std::min)(y, state.height - 1));
#if defined OIS_WIN32_PLATFORM
    size_t windowHandle = 0;
    mRenderWindow->getCustomAttribute("WINDOW", &windowHandle);
    const HWND window = reinterpret_cast<HWND>(windowHandle);
    POINT point = {x, y};
    if(GetForegroundWindow() == window && ClientToScreen(window, &point))
        SetCursorPos(point.x, point.y);
#else
    if(!mMouseGrab)
        return;
#endif
    // Discard motion accumulated during loading without dispatching input.
    OIS::MouseListener* listener = mMouse->getEventCallback();
    mMouse->setEventCallback(nullptr);
    mMouse->capture();
    mMouse->setEventCallback(listener);
    state.X.abs = x;
    state.Y.abs = y;
    state.X.rel = state.Y.rel = state.Z.rel = 0;
    CEGUI::System::getSingleton().getDefaultGUIContext().injectMousePosition(
        static_cast<float>(x), static_cast<float>(y));
#endif
}

void InputManager::setCurrentAMode(AbstractApplicationMode& mode)
{
    mCurrentAMode = &mode;
#ifndef OD_USE_SFML_WINDOW
    mMouse->setEventCallback(&mode);
    mKeyboard->getKeyboard()->setEventCallback(&mode);
#else
    mListener->setReceiver(&mode);
#endif
}

void InputManager::handleSFMLEvent(const sf::Event& evt)
{
#ifdef OD_USE_SFML_WINDOW
    mListener->handleEvent(evt);
#else
    OD_LOG_ERR("TRIED TO USE SFML EVENTS BUT THEY ARE DISABLED!");
#endif
}
