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

#ifndef GAMEEDITORMODECONSOLE_H
#define GAMEEDITORMODECONSOLE_H

#include "AbstractApplicationMode.h"
#include "ConsoleInterface.h"
#include "eventsystem/EventHandler.h"

#include <OgreSingleton.h>
#include <pybind11/embed.h>

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <queue>
#include <unordered_map>

namespace CEGUI
{
class Window;
class EventArgs;
class String;
class Listbox;
class MultiLineEditbox;

}




class GameEditorModeConsole : public Ogre::Singleton<GameEditorModeConsole>, public EventHandler
{
    friend struct my_stream;
public:

    GameEditorModeConsole(ModeManager*);

    ~GameEditorModeConsole();

    bool keyPressed(const OIS::KeyEvent &arg);

    //! \brief Called when the game mode is activated
    //! Used to call the corresponding Gui Sheet.
    
    void activate();
    bool isFreshlyEnabled();
    void printToConsole(const std::string& text);
    
    void stopInterpreterThread();
    void startInterpreterThread();

    ConsoleInterface mConsoleInterface;        
    ModeManager* mModeManager;
    void run_line(const std::string& code, pybind11::object scope);
    void run_script(std::string script_code, std::vector<int>);

    static std::unordered_map<std::string, std::multimap<std::vector<int>,std::string>> scriptRegister;

    
private:
    std::unique_ptr<pybind11::gil_scoped_release> mMainThreadGilRelease;
    
    pybind11::scoped_interpreter guard;
    // Thread loop for executing Python commands
    void interpreterLoop();

    // === Python interpreter thread ===
    std::thread mPythonThread;
    std::atomic<bool> mPythonThreadRunning { false };

    // === Command queue (for exec/eval lines typed by user) ===
    std::mutex mCommandMutex;
    std::condition_variable mCommandCond;
    std::queue<std::string> mCommandQueue;

    // === Stdin queue (for input() calls) ===
    std::mutex mStdinMutex;
    std::condition_variable mStdinCond;
    std::queue<std::string> mStdinQueue;
    std::atomic<bool> mStdinWaiting { false };

    // === GUI / CEGUI console state ===
    CEGUI::MultiLineEditbox* mEditboxWindow;




    bool freshlyEnabled;    
    bool executeCurrentPrompt(const CEGUI::EventArgs& e = {});
    bool characterEntered(const CEGUI::EventArgs& e = {});
    bool executePythonPrompt();
    


    CEGUI::Listbox* mConsoleHistoryWindow;
    CEGUI::Window* consoleRootWindow;



    bool leaveConsole(const CEGUI::EventArgs& e = {});

    inline void addEventConnection(CEGUI::Event::Connection conn)
    {
        mEventConnections.emplace_back(conn);
    }

    // Vector of cegui event bindings to be cleared on exiting the mode
    std::vector<CEGUI::Event::Connection> mEventConnections;
};

#endif // GAMEEDITORMODECONSOLE_H
