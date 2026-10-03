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

#include "utils/RunLevelTest.h"

#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/ResourceManager.h"
#include "ODApplication.h"

#include <boost/filesystem.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

namespace
{
    //! Real time allowed for the game to start (level load and connection)
    const int32_t LOAD_LIMIT_SECONDS = 90;

    std::atomic<bool> sActive(false);
    std::atomic<bool> sFinished(false);
    std::atomic<bool> sGameStarted(false);
    std::atomic<int> sExitCode(0);
    std::mutex sResultMutex;
    std::string sLevelFile;
    std::string sLevelName;
    int32_t sSeconds = 120;

    //! Writes the result line to stdout and to run-level-result.txt. Returns false if a result was already reported.
    bool emitResult(int code, const std::string& line)
    {
        std::lock_guard<std::mutex> lock(sResultMutex);
        if(sFinished.load())
            return false;

        sExitCode = code;
        std::cout << line << std::endl;
        if(ResourceManager::getSingletonPtr() != nullptr)
        {
            std::ofstream out((ResourceManager::getSingleton().getUserDataPath() + "run-level-result.txt").c_str());
            out << line << "\n" << "exit code " << code << "\n";
        }
        sFinished = true;
        return true;
    }

    void watchdog()
    {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        const int64_t totalLimit = LOAD_LIMIT_SECONDS + 2 * static_cast<int64_t>(sSeconds) + 60;
        while(!sFinished.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            int64_t elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            if(!sGameStarted.load() && (elapsed > LOAD_LIMIT_SECONDS))
            {
                RunLevelTest::fail(RunLevelTest::codeLoadError, "load error: the game did not start within "
                    + Helper::toString(LOAD_LIMIT_SECONDS) + " s");
                return;
            }
            if(elapsed > totalLimit)
            {
                if(emitResult(RunLevelTest::codeTimeout, "FAIL " + sLevelName + " : timeout: no result within "
                    + Helper::toString(totalLimit) + " s (watchdog)"))
                    std::_Exit(RunLevelTest::codeTimeout);
                return;
            }
        }
    }
}

void RunLevelTest::configure(const std::string& levelFile, int32_t seconds)
{
    sLevelFile = levelFile;
    sLevelName = boost::filesystem::path(levelFile).filename().string();
    sSeconds = seconds;
    sActive = true;
    std::thread(watchdog).detach();
}

bool RunLevelTest::isActive()
{
    return sActive.load();
}

bool RunLevelTest::isFinished()
{
    return sFinished.load();
}

int RunLevelTest::getExitCode()
{
    return sExitCode.load();
}

const std::string& RunLevelTest::getLevelFile()
{
    return sLevelFile;
}

void RunLevelTest::fail(Code code, const std::string& reason)
{
    std::string detail = reason;
    if((LogManager::getSingletonPtr() != nullptr) && (LogManager::getSingleton().getCriticalCount() > 0))
        detail += " (first error: " + LogManager::getSingleton().getFirstCritical() + ")";
    emitResult(code, "FAIL " + sLevelName + " : " + detail);
}

int RunLevelTest::reportException(const std::string& what)
{
    emitResult(codeRunError, "FAIL " + sLevelName + " : run error: unhandled exception: " + what);
    return sExitCode.load();
}

void RunLevelTest::onServerTurn(GameMap& gameMap)
{
    if(sFinished.load())
        return;

    int64_t turn = gameMap.getTurnNumber();
    if(turn < 1)
        return;

    bool firstTurn = !sGameStarted.exchange(true);
    uint32_t criticalCount = LogManager::getSingleton().getCriticalCount();
    if(criticalCount > 0)
    {
        if(firstTurn)
            fail(codeLoadError, "load error: " + Helper::toString(criticalCount) + " error(s) logged while loading");
        else
            fail(codeRunError, "run error: " + Helper::toString(criticalCount) + " error(s) logged up to turn "
                + Helper::toString(turn));
        return;
    }

    Seat* humanSeat = nullptr;
    for(Player* player : gameMap.getPlayers())
    {
        if(player->getIsHuman())
        {
            humanSeat = player->getSeat();
            break;
        }
    }
    if(humanSeat == nullptr)
    {
        fail(codeRunError, "run error: no human player in the game");
        return;
    }

    std::string played = Helper::toString(static_cast<int64_t>(turn / ODApplication::turnsPerSecond)) + " s game time";
    if(gameMap.seatIsAWinner(humanSeat))
    {
        emitResult(codePass, "PASS " + sLevelName + " : level loaded, victory reported by the level goals after " + played);
        return;
    }

    if(humanSeat->numFailedGoals() > 0)
    {
        fail(codeNoVictory, "no victory: the player seat failed a goal (defeated) after " + played);
        return;
    }

    if(static_cast<double>(turn) < sSeconds * ODApplication::turnsPerSecond)
        return;

    // Time is over: fire the win of the human seat and check that it is reported
    gameMap.addWinningSeat(humanSeat);
    if(!gameMap.seatIsAWinner(humanSeat))
    {
        fail(codeNoVictory, "no victory: win was triggered after " + played + " but not reported");
        return;
    }
    emitResult(codePass, "PASS " + sLevelName + " : level loaded, ran " + played + ", no errors, triggered win reported");
}
