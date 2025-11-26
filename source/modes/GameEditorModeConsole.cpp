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

#include "eventsystem/CreatureMoved.h"
#include "eventsystem/ClockTick.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "modes/ConsoleCommands.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"
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


std::unordered_map<std::string,std::multimap<std::vector<int>,std::string>> GameEditorModeConsole::scriptRegister = std::unordered_map<std::string,std::multimap<std::vector<int>,std::string>>();


namespace py = pybind11;

struct my_stream
{
    void write(const std::string& text)
    {
        GameEditorModeConsole::getSingleton().printToConsole(text);
    }

    void flush()
    {
        // no-op for CEGUI
    }

    std::string readline()
    {
        GameEditorModeConsole& console = GameEditorModeConsole::getSingleton();
        std::string line;

        {
            py::gil_scoped_release release;
            {
                std::unique_lock<std::mutex> lk(console.mStdinMutex);

                console.mStdinWaiting.store(true);
                console.mStdinCond.wait(lk, [&console]
                {
                    return !console.mStdinQueue.empty() || !console.mPythonThreadRunning.load();
                });
                console.mStdinWaiting.store(false);

                if (!console.mStdinQueue.empty())
                {
                    line = std::move(console.mStdinQueue.front());
                    console.mStdinQueue.pop();
                }
                else
                {
                    line.clear();
                }
            }
        }

        if (!line.empty())
            return line + "\n";
        else
            return std::string();
    }

    std::string read()
    {
        return readline();
    }

    bool isatty() const
    {
        return true;
    }
};

PYBIND11_EMBEDDED_MODULE(my_sys, m)
{
    py::class_<my_stream>(m, "my_stream")
        .def(py::init<>())
        .def("write", &my_stream::write)
        .def("flush", &my_stream::flush)
        .def("readline", &my_stream::readline)
        .def("read", &my_stream::read)
        .def("isatty", &my_stream::isatty);

    m.def("hook_streams", []()
    {
        py::module sys = py::module::import("sys");
        py::module my = py::module::import("my_sys");
        py::object stream_obj = my.attr("my_stream")();

        sys.attr("stdout") = stream_obj;
        sys.attr("stderr") = stream_obj;
        sys.attr("stdin")  = stream_obj;

        sys.attr("displayhook") = py::cpp_function([](py::handle obj)
        {
            if (!obj.is_none())
            {
                py::module builtins = py::module::import("builtins");
                builtins.attr("_") = obj;
                std::string repr = py::repr(obj).cast<std::string>();
                GameEditorModeConsole::getSingleton().printToConsole(repr + "\n");
            }
        }, py::arg("obj"));
    });
}

template<>GameEditorModeConsole* Ogre::Singleton<GameEditorModeConsole>::msSingleton = nullptr;

GameEditorModeConsole::GameEditorModeConsole(ModeManager* modeManager):
    guard{},
    mConsoleInterface(std::bind(&GameEditorModeConsole::printToConsole, this, std::placeholders::_1)),
    mModeManager(modeManager),
    freshlyEnabled(false)

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

    // addEventConnection(
    //     sendButton->subscribeEvent(CEGUI::PushButton::EventClicked,
    //                                CEGUI::Event::Subscriber(&GameEditorModeConsole::executePythonPrompt, this))
    // );

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
    {
        pybind11::gil_scoped_acquire acquire;
        pybind11::module::import("my_sys").attr("hook_streams")();
   
        pybind11::exec("import cheats");

    }
    
    GameEditorModeConsole::getSingleton().printToConsole("The up to now console commands are in the package cheats. \n For example to call command fps with argument 30 type cheats.fps(30) \n For more type help('cheats') ");
    // Creatte a persistent release so main thread does not reacquire the GIL
    mMainThreadGilRelease = Utils::make_unique<pybind11::gil_scoped_release>();
    startInterpreterThread();

    // register an anwser to an Event we want, since GameEditorModeConsole is EventHandler as well
    auto it = scriptRegister.find("CreatureMoved");
    if (it !=scriptRegister.end())
    {
        for ( auto range = it->second.begin(); range != it->second.end(); ++range)
        {
        
            registerEventHandler<CreatureMoved>([=](Subject& ss, Event const& ee, std::vector<int> trigger)
            {
                const Creature& ce = dynamic_cast<const Creature&>(ss);
                if(ce.getPositionTile()->getX() == trigger[0] && ce.getPositionTile()->getY() == trigger[1] && !CreatureMoved::alreadyVisited[trigger[1]* CreatureMoved::GAME_MAP_WIDTH + trigger[0]])
                {
                    GameEditorModeConsole::run_script(range->second, range->first);
                    CreatureMoved::alreadyVisited[trigger[1]* CreatureMoved::GAME_MAP_WIDTH + trigger[0]] = true;
                }
            
            }, range->first);
        }
    }
       
    // register an anwser to an Event we want, since GameEditorModeConsole is EventHandler as well
    it = scriptRegister.find("ClockTick");
    if (it !=scriptRegister.end())
    {
        for ( auto range = it->second.begin(); range != it->second.end(); ++range)
        {

            registerEventHandler<ClockTick>([=](Subject& ss , Event const& ee, std::vector<int> trigger )
            {
                const ODFrameListener& odf = dynamic_cast<const ODFrameListener&>(ss);
                if(odf.getCurrentMinutes() == trigger[0] && odf.getCurrentSeconds() == trigger[1] )
                    GameEditorModeConsole::run_script(range->second, range->first);
            }, range->first);
        }
    }
    

    ODFrameListener::getSingletonPtr()->registerObserver(*this); 
}

GameEditorModeConsole::~GameEditorModeConsole()
{
    stopInterpreterThread();
    //Disconnect all event connections.
    for(CEGUI::Event::Connection& c : mEventConnections)
    {
        c->disconnect();
    }
    // the order of removing the objects requires to unregister GameEditorModeBase as Observers:
    // luckily we don't need to do something similar for creatures 
    ODFrameListener::getSingletonPtr()->unregisterObserver(*this);
}

void GameEditorModeConsole::activate()
{
    // Loads the corresponding Gui sheet.
    mModeManager->getGui().loadGuiSheet(Gui::console);
    mEditboxWindow->activate();
    freshlyEnabled = true;
}

bool GameEditorModeConsole::keyPressed(const OIS::KeyEvent &arg)
{
    freshlyEnabled = false;
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
            mEditboxWindow->setCaretIndex(mEditboxWindow->getText().length()-1);
            break;

        case OIS::KC_DOWN:
        {
            if(auto completed = mConsoleInterface.scrollCommandHistoryPositionDown())
            {
                mEditboxWindow->setText(completed.get());
            }
            mEditboxWindow->setCaretIndex(mEditboxWindow->getText().length()-1);
            break;
        }
        case OIS::KC_RETURN:
        case OIS::KC_NUMPADENTER:
            if(!mModeManager->getInputManager().mKeyboard->isModifierDown(OIS::Keyboard::Modifier::Shift))
            {
                std::string line(mEditboxWindow->getText().c_str());
                // If an input() is waiting, feed stdin; otherwise feed command queue
                if (mStdinWaiting.load())
                {
                    {
                        std::lock_guard<std::mutex> lk(mStdinMutex);
                        mStdinQueue.push(line);
                    }
                    mStdinCond.notify_one();
                }
                else
                {
                    {
                        std::lock_guard<std::mutex> lk(mCommandMutex);
                        mCommandQueue.push(line);
                    }
                    mCommandCond.notify_one();
                }
                mEditboxWindow->setText("");
            }
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
    lbi->setTextParsingEnabled(false);
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


bool GameEditorModeConsole::isFreshlyEnabled()
{
    return freshlyEnabled;
}


void GameEditorModeConsole::interpreterLoop()
{
    // NOTE: Python interpreter must already be initialized
    // (py::scoped_interpreter or equivalent)

    while (mPythonThreadRunning.load())
    {
        std::string cmd;
        {
            std::unique_lock<std::mutex> lk(mCommandMutex);
            mCommandCond.wait(lk, [this]
            {
                return !mCommandQueue.empty() || !mPythonThreadRunning.load();
            });

            if (!mPythonThreadRunning.load())
                break;

            if (!mCommandQueue.empty())
            {
                cmd = std::move(mCommandQueue.front());
                mCommandQueue.pop();
            }
            else
            {
                continue;
            }
        }

        // Execute the command under GIL
        try
        {
            pybind11::gil_scoped_acquire acquire;
            pybind11::object scope = pybind11::module::import("__main__").attr("__dict__");
            run_line(cmd, scope);
            Command::String_t currentPrompt(cmd);
            //currentPrompt.erase(currentPrompt.end() - 1);
            mConsoleInterface.getCommandHistoryBuffer().emplace_back(currentPrompt);
     
        }
        catch (pybind11::error_already_set& e)
        {
            printToConsole(std::string("[Python exception] ") + e.what() + "\n");
            // e.restore();
            // PyErr_Print();
        }
    }
}


void GameEditorModeConsole::run_line(const std::string& code, pybind11::object scope)
{
    using namespace pybind11;
    object builtins = module::import("builtins");
    object compile = builtins.attr("compile");
    object eval_func = builtins.attr("eval");

    try
    {
        object compiled = compile(code, "<input>", "eval");
        object result = eval_func(compiled, scope);
        if (!result.is_none())
        {
            module sys = module::import("sys");
            sys.attr("displayhook")(result);
        }
    }
    catch (error_already_set& e)
    {
        if (e.matches(PyExc_SyntaxError))
        {
            exec(code, scope);
        }
        else
        {
            throw;
        }
    }
}



void GameEditorModeConsole::run_script(std::string script_code, std::vector<int> mParameters)
{
    pybind11::gil_scoped_acquire acquire;
    pybind11::object scope = pybind11::module::import("__main__").attr("__dict__");
    run_line(script_code, scope);

    
}

void GameEditorModeConsole::startInterpreterThread()
{
    if (mPythonThreadRunning.load())
        return;

    mPythonThreadRunning.store(true);
    mPythonThread = std::thread([this]()
    {
        this->interpreterLoop();
    });
}

void GameEditorModeConsole::stopInterpreterThread()
{
    if (!mPythonThreadRunning.load())
        return;

    // signal thread to exit
    {
        std::lock_guard<std::mutex> lock(mCommandMutex);
        mPythonThreadRunning.store(false);
    }
    mCommandCond.notify_all();

    // also wake any potential waiting stdin
    mStdinCond.notify_all();

    
    if (mMainThreadGilRelease)
    {
        mMainThreadGilRelease.reset();
    }
    
    if (mPythonThread.joinable())
        mPythonThread.join();
}
