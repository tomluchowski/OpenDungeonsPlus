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

#ifndef RUNLEVELTEST_H
#define RUNLEVELTEST_H

#include <cstdint>
#include <string>

class GameMap;

//! \brief Debug mode for automated level load tests (command line: --run-level <file> --seconds <N>).
//! The game starts a local single player game on the level, lets it run for N seconds of game time,
//! then fires the win of the human seat and checks that it is reported. One result line (PASS/FAIL
//! with reason) is printed and written to run-level-result.txt in the user data folder. A watchdog
//! ends runs that hang. The process exit code tells the result (see Code).
class RunLevelTest
{
public:
    enum Code
    {
        codePass = 0,
        codeUsage = 1,
        codeLoadError = 2,
        codeRunError = 3,
        codeNoVictory = 4,
        codeTimeout = 5
    };

    //! \brief Activates the test mode and starts the watchdog.
    static void configure(const std::string& levelFile, int32_t seconds);

    static bool isActive();

    //! \brief True once a result was reported. The main loop then ends the game.
    static bool isFinished();

    static int getExitCode();

    static const std::string& getLevelFile();

    //! \brief Reports a failure. Only the first result (pass or fail) of a run counts.
    static void fail(Code code, const std::string& reason);

    //! \brief Reports an exception that ended the application. Returns the exit code.
    static int reportException(const std::string& what);

    //! \brief Called by the server thread after every turn of the game.
    static void onServerTurn(GameMap& gameMap);
};

#endif // RUNLEVELTEST_H
