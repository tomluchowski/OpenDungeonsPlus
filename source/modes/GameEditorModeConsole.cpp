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

#include <CEGUI/widgets/MultiLineEditbox.h>
#include <CEGUI/widgets/FrameWindow.h>
#include <CEGUI/Font.h>
#include <CEGUI/widgets/Listbox.h>
#include <CEGUI/widgets/ListboxTextItem.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/Scrollbar.h>



#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

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
        // so far we do nothing, the underlying console doesn't have anything similar to "flush"
    });

    m.def("hook_stdout", []() {
        auto py_sys = pybind11::module::import("sys");
        auto my_sys = pybind11::module::import("my_sys");
        py_sys.attr("stdout") = my_sys.attr("my_stdout");
    });
}


template<>GameEditorModeConsole* Ogre::Singleton<GameEditorModeConsole>::msSingleton = nullptr;

GameEditorModeConsole::GameEditorModeConsole(ModeManager* modeManager):
    guard{},
    mConsoleInterface(std::bind(&GameEditorModeConsole::printToConsole, this, std::placeholders::_1)),
    mModeManager(modeManager)

{
    ConsoleCommands::addConsoleCommands(mConsoleInterface);

    consoleRootWindow = mModeManager->getGui().getGuiSheet(Gui::guiSheet::console);
    assert(consoleRootWindow != nullptr);
    consoleRootWindow->setAlpha(0.4);
    
    CEGUI::Window* listbox = consoleRootWindow->getChild("ConsoleHistoryWindow");
    assert(listbox->getType().compare("OD/Listbox") == 0);
    mConsoleHistoryWindow = static_cast<CEGUI::Listbox*>(listbox);
    mConsoleHistoryWindow->setAlpha(0.4);
    CEGUI::Window* editbox = consoleRootWindow->getChild("Editbox");
    mEditboxWindow = static_cast<CEGUI::MultiLineEditbox*>(editbox);
    // mEditboxWindow->setAlpha(0.4);
    // mEditboxWindow->setTextParsingEnabled(true);
    // mEditboxWindow->setText("[colour='FFFF0000']");
    CEGUI::Window* sendButton = consoleRootWindow->getChild("SendButton");

    addEventConnection(
        sendButton->subscribeEvent(CEGUI::PushButton::EventClicked,
                                   CEGUI::Event::Subscriber(&GameEditorModeConsole::executePythonPrompt, this))
    );

    addEventConnection(
        mEditboxWindow->subscribeEvent(CEGUI::MultiLineEditbox::EventCharacterKey,
                                   CEGUI::Event::Subscriber(&GameEditorModeConsole::characterEntered, this))
    );

    // mEditboxWindow->setCaretBlinkEnabled(true);
    // mEditboxWindow->setCaretBlinkTimeout(1.0);
    mConsoleHistoryWindow->getVertScrollbar()->setEndLockEnabled(true);

    // Permits closing the console.
    addEventConnection(
        consoleRootWindow->subscribeEvent(CEGUI::FrameWindow::EventCloseClicked,
                                    CEGUI::Event::Subscriber(&GameEditorModeConsole::leaveConsole, this))
    );
    pybind11::module::import("my_sys").attr("hook_stdout")();
    pybind11::object scope = pybind11::module::import("__main__").attr("__dict__");
    
    pybind11::exec("import cheats");
    GameEditorModeConsole::getSingleton().printToConsole("The up to now console commands are in the package cheats. \n For example to call command fps with argument 30 type cheats.fps(30) \n For more type help('cheats') ");

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
            CEGUI::String line = mEditboxWindow->getText();
            CEGUI::String line2 = line.substr(0,mEditboxWindow->getCaretIndex());
            CEGUI::String line3 = line.substr(mEditboxWindow->getCaretIndex(), line.length() - 1);
            mEditboxWindow->setText(line2 + "    " + line3);
            mEditboxWindow->setCaretIndex(mEditboxWindow->getCaretIndex() + 4);
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
            if(mModeManager->getInputManager().mKeyboard->isModifierDown(OIS::Keyboard::Modifier::Shift))
                executePythonPrompt();
            else
            {
                mEditboxWindow->appendText("\n");
                float fontHeight = mEditboxWindow->getFont('\n')->getFontHeight();
                mEditboxWindow->setHeight(mEditboxWindow->getHeight() + CEGUI::UDim(0.0, fontHeight));
                mEditboxWindow->setYPosition(mEditboxWindow->getYPosition() - CEGUI::UDim(0.0, fontHeight));
                consoleRootWindow->setHeight(consoleRootWindow->getHeight() + CEGUI::UDim(0.0, fontHeight));
            }
            break;
        default:
            break;
    }

    return true;
}

void GameEditorModeConsole::printToConsole(const std::string& text)
{
    CEGUI::ListboxTextItem* lbi = new CEGUI::ListboxTextItem("");
    lbi->setTextParsingEnabled(true);
    std::string ss = text;
    if (ss[ss.length() - 1] == '\n')
             ss.pop_back();
    lbi->setText(ss);
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
    try
    {
        pybind11::exec(mEditboxWindow->getText().c_str(),scope);
    }
    catch(pybind11::error_already_set &error)
    {
        printToConsole(error.what());
    }
    mConsoleInterface.getCommandHistoryBuffer().emplace_back(mEditboxWindow->getText().c_str());
    mEditboxWindow->setText("");
    return true;

}
bool GameEditorModeConsole::characterEntered(const CEGUI::EventArgs& e)
{
    // We only accept alphanumeric chars + space
    const CEGUI::KeyEventArgs& kea = static_cast<const CEGUI::KeyEventArgs&>(e);
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
    if (kea.codepoint == '\t')
        return true;
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
