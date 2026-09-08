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

#include "modes/AbstractApplicationMode.h"

#include "network/ODClient.h"
#include "network/ODServer.h"
#include "render/Gui.h"

#include <CEGUI/System.h>
#include <CEGUI/GUIContext.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/FrameWindow.h>
#include <CEGUI/widgets/Combobox.h>
#include <CEGUI/widgets/PopupMenu.h>

#include <algorithm>

namespace
{
void collectEscapeWindows(CEGUI::Window* window, std::vector<CEGUI::Window*>& windows)
{
    if(window == nullptr || !window->isVisible() || window->isDisabled())
        return;

    CEGUI::Combobox* combo = dynamic_cast<CEGUI::Combobox*>(window);
    if(dynamic_cast<CEGUI::FrameWindow*>(window) != nullptr ||
       dynamic_cast<CEGUI::PopupMenu*>(window) != nullptr ||
       (combo != nullptr && combo->isDropDownListVisible()))
        windows.push_back(window);

    for(size_t i = 0; i < window->getChildCount(); ++i)
        collectEscapeWindows(window->getChildAtIdx(i), windows);
}
}

AbstractApplicationMode::~AbstractApplicationMode()
{
    //Disconnect all event connections.
    for(CEGUI::Event::Connection& c : mEventConnections)
    {
        c->disconnect();
    }
}

bool AbstractApplicationMode::isConnected()
{
    return (ODServer::getSingleton().isConnected() || ODClient::getSingleton().isConnected());
}

bool AbstractApplicationMode::mouseMoved(const OIS::MouseEvent& arg)
{

    if(arg.state.Z.rel != 0)
    {
        AbstractApplicationMode::wheelMovedN(toSFMLMouseWheel(arg));
    }
    return mouseMovedN(toSFMLMouseMove(arg));
}

bool AbstractApplicationMode::mouseMovedN(const MouseMoveEvent& arg)
{
    return CEGUI::System::getSingleton().getDefaultGUIContext().injectMousePosition(
              static_cast<float>(arg.x), static_cast<float>(arg.y));
}

bool AbstractApplicationMode::wheelMovedN(const MouseWheelEvent& arg)
{
    // TODO: Handle multiple mouse wheels
    return CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseWheelChange(
              static_cast<float>(arg.delta) / 100.0f);
}

bool AbstractApplicationMode::mousePressed(const OIS::MouseEvent& arg, OIS::MouseButtonID id)
{
    return CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonDown(
        Gui::convertButton(id));
}

bool AbstractApplicationMode::mouseReleased(const OIS::MouseEvent& arg, OIS::MouseButtonID id)
{
    return CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonUp(
        Gui::convertButton(id));
}

bool AbstractApplicationMode::keyPressed(const OIS::KeyEvent& arg)
{
    switch (arg.key)
    {
    case OIS::KC_ESCAPE:
        if(!CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyDown(CEGUI::Key::Escape) &&
           !closeTopWindow() && mModeType != ModeManager::ADVERTISMENT)
            goBack();
        break;
    default:
        CEGUI::System::getSingleton().getDefaultGUIContext().injectChar(
            static_cast<CEGUI::String::value_type>(arg.text));
        CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyDown(
            static_cast<CEGUI::Key::Scan>(arg.key));
        break;
    }
    return true;
}

bool AbstractApplicationMode::keyReleased(const OIS::KeyEvent& arg)
{
    return CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyUp(
      static_cast<CEGUI::Key::Scan>(arg.key));
}

void AbstractApplicationMode::giveFocus()
{
    mModeManager->getInputManager().setCurrentAMode(*this);
}

bool AbstractApplicationMode::goBack(const CEGUI::EventArgs&)
{
    mModeManager->requestPreviousMode();
    return true;
}

bool AbstractApplicationMode::closeTopWindow()
{
    CEGUI::GUIContext& context = CEGUI::System::getSingleton().getDefaultGUIContext();
    std::vector<CEGUI::Window*> windows;
    collectEscapeWindows(context.getModalWindow() != nullptr ?
        context.getModalWindow() : context.getRootWindow(), windows);
    std::sort(windows.begin(), windows.end(), [](CEGUI::Window* left, CEGUI::Window* right)
    {
        return left->isInFront(*right);
    });

    for(CEGUI::Window* window : windows)
    {
        if(CEGUI::Combobox* combo = dynamic_cast<CEGUI::Combobox*>(window))
        {
            combo->hideDropList();
            return true;
        }
        if(CEGUI::PopupMenu* popup = dynamic_cast<CEGUI::PopupMenu*>(window))
        {
            popup->closePopupMenu();
            return true;
        }
        CEGUI::WindowEventArgs args(window);
        window->fireEvent(CEGUI::FrameWindow::EventCloseClicked, args);
        if(args.handled != 0)
            return true;
    }
    return false;
}
