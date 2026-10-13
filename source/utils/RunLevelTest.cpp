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

#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/Random.h"
#include "utils/ResourceManager.h"
#include "ODApplication.h"

#include <OgreRenderTarget.h>

#include <boost/filesystem.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace
{
    //! Real time allowed for the game to start (level load and connection)
    const int32_t LOAD_LIMIT_SECONDS = 90;

    std::atomic<bool> sActive(false);
    std::atomic<bool> sFinished(false);
    std::atomic<bool> sGameStarted(false);
    std::atomic<int> sExitCode(0);
    //! Frames drawn since the game started and their summed time in microseconds
    std::atomic<uint64_t> sFrameCount(0);
    std::atomic<uint64_t> sFrameMicros(0);
    std::mutex sResultMutex;
    //! Render thread only: start of the current one second frame statistics window
    std::chrono::steady_clock::time_point sFrameWindowStart;
    bool sFrameWindowStarted = false;
    std::string sLevelFile;
    std::string sLevelName;
    int32_t sSeconds = 120;
    uint32_t sSeed = 0;
    std::vector<double> sFrameTimes;
    std::vector<double> sTurnTimes;
    std::mutex sTimeMutex;

    //! Average and 95th percentile of the values, scaled by factor
    std::string describeTimes(std::vector<double>& values, double factor)
    {
        std::ostringstream out;
        if(values.empty())
        {
            out << "n=0";
            return out.str();
        }
        std::sort(values.begin(), values.end());
        double sum = 0.0;
        for(double value : values)
            sum += value;
        double p95 = values[static_cast<size_t>(0.95 * static_cast<double>(values.size() - 1))];
        out << std::fixed << std::setprecision(4) << "n=" << values.size()
            << " avg=" << (sum / static_cast<double>(values.size())) * factor << " p95=" << p95 * factor;
        return out.str();
    }

    //! Writes frame and turn time statistics to run-level-timing.txt
    void writeTimings()
    {
        std::lock_guard<std::mutex> lock(sTimeMutex);
        std::ofstream out((ResourceManager::getSingleton().getUserDataPath() + "run-level-timing.txt").c_str());
        out << "frame_ms " << describeTimes(sFrameTimes, 1000.0) << "\n";
        out << "turn_ms " << describeTimes(sTurnTimes, 0.001) << "\n";
    }

    //! Writes seats and creatures (sorted) to run-level-state.txt, so two runs can be compared
    void writeState(GameMap& gameMap)
    {
        std::vector<std::string> lines;
        for(Seat* seat : gameMap.getSeats())
        {
            std::ostringstream line;
            line << "seat " << seat->getId() << " gold=" << seat->getGold() << " claimed=" << seat->getNumClaimedTiles()
                << " fighters=" << seat->getNumCreaturesFighters() << " workers=" << seat->getNumCreaturesWorkers();
            lines.push_back(line.str());
        }
        for(Creature* creature : gameMap.getCreatures())
        {
            std::ostringstream line;
            line << "creature " << creature->getName() << " class=" << creature->getDefinition()->getClassName()
                << " seat=" << creature->getSeat()->getId() << " level=" << creature->getLevel()
                << " hp=" << std::fixed << std::setprecision(2) << creature->getHP() << "/" << creature->getMaxHp()
                << " pos=" << creature->getPosition().x << "," << creature->getPosition().y;
            lines.push_back(line.str());
        }
        std::sort(lines.begin(), lines.end());
        std::ofstream out((ResourceManager::getSingleton().getUserDataPath() + "run-level-state.txt").c_str());
        out << "turn " << gameMap.getTurnNumber() << "\n";
        for(const std::string& line : lines)
            out << line << "\n";
    }

    //! Writes the result line to stdout and to run-level-result.txt. Returns false if a result was already reported.
    bool emitResult(int code, const std::string& line)
    {
        std::lock_guard<std::mutex> lock(sResultMutex);
        if(sFinished.load())
            return false;

        sExitCode = code;
        const uint64_t frames = sFrameCount.load();
        if(frames > 0)
            std::cout << "FRAMETIME frames=" << frames << " avg_ms="
                << (static_cast<double>(sFrameMicros.load()) / 1000.0 / static_cast<double>(frames)) << std::endl;
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

void RunLevelTest::configure(const std::string& levelFile, int32_t seconds, uint32_t seed)
{
    sSeed = seed;
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

void RunLevelTest::recordFrame(float seconds)
{
    if(!sActive.load() || !sGameStarted.load() || sFinished.load() || seconds <= 0.0f)
        return;

    sFrameCount.fetch_add(1);
    sFrameMicros.fetch_add(static_cast<uint64_t>(seconds * 1000000.0f));
}

void RunLevelTest::onServerTurn(GameMap& gameMap)
{
    if(sFinished.load())
        return;

    int64_t turn = gameMap.getTurnNumber();
    if(turn < 1)
        return;

    bool firstTurn = !sGameStarted.exchange(true);
    if(firstTurn && (sSeed != 0))
        Random::setSeed(sSeed);
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
        if(sSeed != 0)
            writeState(gameMap);
        writeTimings();
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

    // Time is over: record the state before the win changes anything
    if(sSeed != 0)
        writeState(gameMap);
    writeTimings();

    // Fire the win of the human seat and check that it is reported
    gameMap.addWinningSeat(humanSeat);
    if(!gameMap.seatIsAWinner(humanSeat))
    {
        fail(codeNoVictory, "no victory: win was triggered after " + played + " but not reported");
        return;
    }
    emitResult(codePass, "PASS " + sLevelName + " : level loaded, ran " + played + ", no errors, triggered win reported");
}

void RunLevelTest::onFrameRendered(Ogre::RenderTarget& target)
{
    if(!sActive.load() || !sGameStarted.load() || sFinished.load())
        return;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if(!sFrameWindowStarted)
    {
        sFrameWindowStarted = true;
        sFrameWindowStart = now;
        target.resetStatistics();
        return;
    }

    if(now - sFrameWindowStart < std::chrono::seconds(1))
        return;

    sFrameWindowStart = now;
    const Ogre::RenderTarget::FrameStats& stats = target.getStatistics();
    std::ofstream out((ResourceManager::getSingleton().getUserDataPath() + "run-level-frames.txt").c_str(),
        std::ios::app);
    out << "frames avgFps " << stats.avgFPS << " worstFrameMs " << stats.worstFrameTime
        << " bestFrameMs " << stats.bestFrameTime << "\n";
    target.resetStatistics();
}

void RunLevelTest::onServerTurnTime(int64_t microseconds)
{
    if(!sActive.load() || !sGameStarted.load() || sFinished.load())
        return;

    std::lock_guard<std::mutex> lock(sTimeMutex);
    sTurnTimes.push_back(static_cast<double>(microseconds));
}

void RunLevelTest::onFrame(double seconds)
{
    if(!sActive.load() || !sGameStarted.load() || sFinished.load())
        return;

    std::lock_guard<std::mutex> lock(sTimeMutex);
    sFrameTimes.push_back(seconds);
}
