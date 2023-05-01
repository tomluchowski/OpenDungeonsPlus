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

#include "GameEditorModeConsole.h"

#include "modes/ConsoleCommands.h"
#include "render/Gui.h"
#include "utils/LogManager.h"
#include "modes/GameEditorModeBase.h"

#include <CEGUI/widgets/Editbox.h>
#include <CEGUI/widgets/FrameWindow.h>
#include <CEGUI/widgets/Listbox.h>
#include <CEGUI/widgets/ListboxTextItem.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/Scrollbar.h>



#include <pybind11/embed.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>
#include <functional>
#include <cassert>


PYBIND11_EMBEDDED_MODULE(my_sys, m) {
    struct my_stdout {
        my_stdout() = default;
        my_stdout(const my_stdout &) = default;
        my_stdout(my_stdout &&) = default;
    };


    
    pybind11::class_<my_stdout> my_stdout(m, "my_stdout");
    my_stdout.def_static("write", [](pybind11::object buffer) {
        GameEditorModeConsole::getSingleton().printToConsole( buffer.cast<std::string>());
    });
    my_stdout.def_static("flush", []() {
        GameEditorModeConsole::getSingleton().printToConsole( "\n" );
    });

    m.def("hook_stdout", []() {
        auto py_sys = pybind11::module::import("sys");
        auto my_sys = pybind11::module::import("my_sys");
        py_sys.attr("stdout") = my_sys.attr("my_stdout");
    });
}

// std::vector<double> modify(const std::vector<double>& input)
// {
//   std::vector<double> output(input.size());
  
//   for ( size_t i = 0 ; i < input.size() ; ++i )
//     output[i] = 2. * input[i];

//   return output;
// }

// PYBIND11_EMBEDDED_MODULE(example,m)
// {
//   m.doc() = "pybind11 example plugin";

//   m.def("modify", &modify, "Multiply all entries of a list by 2.template");
// }


template<>GameEditorModeConsole* Ogre::Singleton<GameEditorModeConsole>::msSingleton = nullptr;

GameEditorModeConsole::GameEditorModeConsole(ModeManager* modeManager):
    guard{},
    mConsoleInterface(std::bind(&GameEditorModeConsole::printToConsole, this, std::placeholders::_1)),
    mModeManager(modeManager)

{
    ConsoleCommands::addConsoleCommands(mConsoleInterface);

    CEGUI::Window* consoleRootWindow = mModeManager->getGui().getGuiSheet(Gui::guiSheet::console);
    assert(consoleRootWindow != nullptr);
    CEGUI::Window* listbox = consoleRootWindow->getChild("ConsoleHistoryWindow");
    assert(listbox->getType().compare("OD/Listbox") == 0);
    mConsoleHistoryWindow = static_cast<CEGUI::Listbox*>(listbox);
    CEGUI::Window* editbox = consoleRootWindow->getChild("Editbox");
    mEditboxWindow = static_cast<CEGUI::Editbox*>(editbox);
    CEGUI::Window* sendButton = consoleRootWindow->getChild("SendButton");

    addEventConnection(
        sendButton->subscribeEvent(CEGUI::PushButton::EventClicked,
                                   CEGUI::Event::Subscriber(&GameEditorModeConsole::executeCurrentPrompt, this))
    );

    addEventConnection(
        mEditboxWindow->subscribeEvent(CEGUI::Editbox::EventCharacterKey,
                                   CEGUI::Event::Subscriber(&GameEditorModeConsole::characterEntered, this))
    );

    mConsoleHistoryWindow->getVertScrollbar()->setEndLockEnabled(true);

    // Permits closing the console.
    addEventConnection(
        consoleRootWindow->subscribeEvent(CEGUI::FrameWindow::EventCloseClicked,
                                    CEGUI::Event::Subscriber(&GameEditorModeConsole::leaveConsole, this))
    );
    pybind11::module::import("my_sys").attr("hook_stdout")();
    pybind11::object scope = pybind11::module::import("__main__").attr("__dict__");
    
    pybind11::exec("import cheats");

}

GameEditorModeConsole::~GameEditorModeConsole()
{
    //Disconnect all event connections.
    for(CEGUI::Event::Connection& c : mEventConnections)
    {
        c->disconnect();
    }
}

void GameEditorModeConsole::activate()
{
    // Loads the corresponding Gui sheet.
    mModeManager->getGui().loadGuiSheet(Gui::console);
    mEditboxWindow->activate();
}

bool GameEditorModeConsole::keyPressed(const OIS::KeyEvent &arg)
{
    switch(arg.key)
    {
        case OIS::KC_TAB:
        {
            ConsoleInterface::String_t completed;
            if(mConsoleInterface.tryCompleteCommand(mEditboxWindow->getText().c_str(), completed))
                mEditboxWindow->setText(completed);

            mEditboxWindow->setCaretIndex(mEditboxWindow->getText().length());
            break;
        }
        case OIS::KC_GRAVE:
        case OIS::KC_ESCAPE:
        case OIS::KC_F12:
        {
            leaveConsole();
            break;
        }
        case OIS::KC_UP:
            if(auto completed = mConsoleInterface.scrollCommandHistoryPositionUp(mEditboxWindow->getText().c_str()))
            {
                mEditboxWindow->setText(completed.get());
            }
            mEditboxWindow->setCaretIndex(mEditboxWindow->getText().length());
            break;

        case OIS::KC_DOWN:
        {
            if(auto completed = mConsoleInterface.scrollCommandHistoryPositionDown())
            {
                mEditboxWindow->setText(completed.get());
            }
            mEditboxWindow->setCaretIndex(mEditboxWindow->getText().length());
            break;
        }
        case OIS::KC_RETURN:
        case OIS::KC_NUMPADENTER:
            executePythonPrompt();
            break;
        default:
            break;
    }

    return true;
}

void GameEditorModeConsole::printToConsole(const std::string& text)
{
    CEGUI::ListboxTextItem* lbi = new CEGUI::ListboxTextItem("");
    lbi->setTextParsingEnabled(false);
    lbi->setText(text);
    mConsoleHistoryWindow->addItem(lbi);
}

bool GameEditorModeConsole::executeCurrentPrompt(const CEGUI::EventArgs& e)
{
    mConsoleInterface.tryExecuteClientCommand(mEditboxWindow->getText().c_str(),
                                        mModeManager->getCurrentModeType(),
                                        *mModeManager);
        
    mEditboxWindow->setText("");
    return true;
}

bool GameEditorModeConsole::executePythonPrompt()
{

    pybind11::module::import("my_sys").attr("hook_stdout")();
    pybind11::object scope = pybind11::module::import("__main__").attr("__dict__");
    
    pybind11::exec(mEditboxWindow->getText().c_str(),scope);
    mEditboxWindow->setText("");
    return true;

}
bool GameEditorModeConsole::characterEntered(const CEGUI::EventArgs& e)
{
    // We only accept alphanumeric chars + space
    // const CEGUI::KeyEventArgs& kea = static_cast<const CEGUI::KeyEventArgs&>(e);
    // if((kea.codepoint >= 'a') && (kea.codepoint <= 'z'))
    //     return false;
    // if((kea.codepoint >= 'A') && (kea.codepoint <= 'Z'))
    //     return false;
    // if((kea.codepoint >= '0') && (kea.codepoint <= '9'))
    //     return false;
    // if(kea.codepoint == ' ')
    //     return false;
    // if(kea.codepoint == '.')
    //     return false;
    
    return false;
}

bool GameEditorModeConsole::leaveConsole(const CEGUI::EventArgs& /*e*/)
{
    if (mModeManager->getCurrentModeType() != AbstractModeManager::GAME
        && mModeManager->getCurrentModeType() != AbstractModeManager::EDITOR)
        return true;

    // Warn the mother mode that we can leave the console.
    GameEditorModeBase* mode = static_cast<GameEditorModeBase*>(mModeManager->getCurrentMode());
    mode->leaveConsole();
    return true;
}
