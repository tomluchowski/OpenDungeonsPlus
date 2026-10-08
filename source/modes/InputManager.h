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

#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <OgreVector.h>
#include <OgreSingleton.h>

#include <memory>


#include "modes/Keyboard.h"

namespace OIS
{
    class InputManager;
    class Keyboard;
    class Mouse;
}

namespace Ogre
{
    class RenderWindow;
}

namespace sf
{
    class Event;
}

class AbstractApplicationMode;
class SFMLToOISListener;

enum class InputCommandState
{
    infoOnly, // When the player is only moving mouse (but not building)
    building, // When the player is building (but has not validated he build)
    validated, // When the player is happy with the build and wants to build
};

enum class SelectionEntityWanted;

class Creature;

class InputManager: public Ogre::Singleton<InputManager>
{
public:

    struct PosWithOrient
    {
        Ogre::Vector3 vv;
        Ogre::Quaternion qq;
        Ogre::Quaternion qq2;
    };

    
    InputManager(Ogre::RenderWindow* renderWindow);
    ~InputManager();

    void setWidthAndHeight(int width, int height);
    void setCurrentAMode(AbstractApplicationMode& mode);
    void handleSFMLEvent(const sf::Event& evt);
    //! \brief Recreates the input devices if the mouse or keyboard capture setting changed.
    //! Must not be called from an input callback.
    void refreshSettings();
    //! \brief Moves the input devices to another render window. Returns false (and keeps the old window)
    //! if the devices could not be created for the new one.
    bool setRenderWindow(Ogre::RenderWindow* renderWindow);

    OIS::InputManager*  mInputManager;


    bool                mHotkeyLocationIsValid[10];
    PosWithOrient       mHotkeyLocation[10];
    std::unique_ptr<Keyboard>           mKeyboard;

    //! \brief mouse handling related member
    bool                mLMouseDown, mRMouseDown, mMMouseDown;
    bool                mMouseDownOnCEGUIWindow;

    bool                mMouseDownOnDraggableTileContainer;
    Ogre::Vector3       mKeeperHandPos;
    Ogre::Vector3       mKeeperHandPosOverBlock;
    int                 mXPos, mYPos;
    int                 mLStartDragX, mLStartDragY;
    int                 mRStartDragX, mRStartDragY;
    //! \brief In editor mode, it contains the selected seat Id. In gamemode, it is not used
    int                 mSeatIdSelected;
    Ogre::Vector2       offsetDraggableTileContainer;
    InputCommandState   mCommandState;
    OIS::Mouse*         mMouse;
    Creature*           mHighlightedCreature;
    SelectionEntityWanted mCreatureTypeForOutliner;

    
    private:
    AbstractApplicationMode* mCurrentAMode;
    //! \brief The render window the input devices are bound to
    Ogre::RenderWindow* mRenderWindow;
    //! \brief Whether the input devices were created with mouse capture
    bool mMouseGrab;
    //! \brief Whether the input devices were created with keyboard capture
    bool mKeyboardGrab;
    //! \brief Creates the mouse and keyboard for mRenderWindow
    void createInputDevices(bool mouseGrab, bool keyboardGrab);
    //! \brief Destroys the mouse and keyboard; safe to call when they do not exist
    void destroyInputDevices();
#ifdef OD_USE_SFML_WINDOW
    std::unique_ptr<SFMLToOISListener> mListener;
#endif
};

#endif // INPUTMANAGER_H
