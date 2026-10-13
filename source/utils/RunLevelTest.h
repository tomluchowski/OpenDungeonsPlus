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

namespace Ogre
{
class RenderTarget;
}

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
    //! If seed is not 0, the random generator is set to it when the first turn runs and the game
    //! state at the end of the run is written to run-level-state.txt (for seeded comparisons).
    static void configure(const std::string& levelFile, int32_t seconds, uint32_t seed = 0);

    static bool isActive();

    //! \brief True once a result was reported. The main loop then ends the game.
    static bool isFinished();

    static int getExitCode();

    static const std::string& getLevelFile();

    //! \brief Reports a failure. Only the first result (pass or fail) of a run counts.
    static void fail(Code code, const std::string& reason);

    //! \brief Reports an exception that ended the application. Returns the exit code.
    static int reportException(const std::string& what);

    //! \brief Called by the render thread every frame. Once the game runs, the frame times are summed and the
    //! average is printed as one line (FRAMETIME frames=... avg_ms=...) before the result line.
    static void recordFrame(float seconds);

    //! \brief Called by the server thread after every turn of the game.
    static void onServerTurn(GameMap& gameMap);

    //! \brief Called by the render loop after every frame. Once the game runs, it appends one line per
    //! second to run-level-frames.txt in the user data folder (average fps, worst and best frame time
    //! in ms of that second), so that frame times can be compared between runs.
    static void onFrameRendered(Ogre::RenderTarget& target);

    //! \brief Called by the server thread with the time (microseconds) it needed to process one turn.
    static void onServerTurnTime(int64_t microseconds);

    //! \brief Called by the client thread for every frame with the time since the last frame (seconds).
    static void onFrame(double seconds);
};

#endif // RUNLEVELTEST_H
