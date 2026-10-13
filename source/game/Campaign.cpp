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

#include "game/Campaign.h"

#include "ai/KeeperAIType.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>

namespace
{
std::string trim(const std::string& text)
{
    const std::string whitespace = " \t\r\n";
    size_t first = text.find_first_not_of(whitespace);
    if(first == std::string::npos)
        return std::string();

    size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}
} // namespace

Campaign::Campaign():
    mActive(false),
    mPlayedLevel(0),
    mPlayedLevelWon(false),
    mDifficulty(getDefaultDifficulty()),
    mKeptPendingLevel(0),
    mKeptLevel(0)
{
}

Campaign& Campaign::getSingleton()
{
    static Campaign campaign;
    return campaign;
}

std::string Campaign::unescapeText(const std::string& text)
{
    std::string result;
    for(size_t i = 0; i < text.size(); ++i)
    {
        if((text[i] == '\\') && (i + 1 < text.size()) && (text[i + 1] == 'n'))
        {
            result += '\n';
            ++i;
            continue;
        }
        result += text[i];
    }
    return result;
}

bool Campaign::importDefinition(std::istream& is)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mLevels.clear();
    mCompleted.clear();
    mDiscovered.clear();

    CampaignLevel level;
    bool inLevel = false;
    std::string line;
    while(std::getline(is, line))
    {
        line = trim(line);
        if(line.empty() || (line[0] == '#'))
            continue;

        if(line == "[Level]")
        {
            if(inLevel && !level.mFile.empty())
                mLevels.push_back(level);

            level = CampaignLevel();
            inLevel = true;
            continue;
        }

        size_t pos = line.find('=');
        if(!inLevel || (pos == std::string::npos))
            continue;

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));
        if(key == "File")
            level.mFile = value;
        else if(key == "Title")
            level.mTitle = value;
        else if(key == "Briefing")
            level.mBriefing = unescapeText(value);
        else if(key == "Debriefing")
            level.mDebriefing = unescapeText(value);
        else if(key == "Bonus")
            level.mBonus = (value == "1");
        else if(key == "Heartstone")
            level.mHeartstone = (value != "0");
        else if(key == "Province")
            level.mProvince = value;
        else if(key == "Branch")
            level.mBranch = value;
        else if(key == "Warden")
            level.mWarden = value;
        else if(key == "Difficulty")
            level.mDifficulty = static_cast<uint32_t>(std::max(1, std::min(5, std::atoi(value.c_str()))));
    }

    if(inLevel && !level.mFile.empty())
        mLevels.push_back(level);

    mCompleted.assign(mLevels.size(), false);
    mDiscovered.assign(mLevels.size(), false);
    return !mLevels.empty();
}

bool Campaign::importProgress(std::istream& is)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mCompleted.assign(mLevels.size(), false);
    mDiscovered.assign(mLevels.size(), false);
    mDifficulty = getDefaultDifficulty();
    mKeptClass.clear();
    mKeptLevel = 0;

    std::string line;
    while(std::getline(is, line))
    {
        std::istringstream ss(trim(line));
        std::string keyword;
        ss >> keyword;
        if(keyword == "Difficulty")
        {
            uint32_t difficulty;
            if((ss >> difficulty) && (difficulty < static_cast<uint32_t>(KeeperAIType::nbAI)))
                mDifficulty = difficulty;
            continue;
        }
        if(keyword == "KeptMinion")
        {
            std::string className;
            uint32_t level;
            if((ss >> className >> level) && !className.empty())
            {
                mKeptClass = className;
                mKeptLevel = level;
            }
            continue;
        }
        if((keyword != "Completed") && (keyword != "Discovered"))
            continue;

        std::vector<bool>& target = (keyword == "Completed") ? mCompleted : mDiscovered;
        size_t index;
        while(ss >> index)
        {
            if(index < target.size())
                target[index] = true;
        }
    }
    return true;
}

void Campaign::exportProgress(std::ostream& os) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    os << "# Campaign progress. Do not edit while the game is running.\n";
    os << "Completed";
    for(size_t i = 0; i < mCompleted.size(); ++i)
    {
        if(mCompleted[i])
            os << " " << i;
    }
    os << "\n";
    os << "Discovered";
    for(size_t i = 0; i < mDiscovered.size(); ++i)
    {
        if(mDiscovered[i])
            os << " " << i;
    }
    os << "\n";
    os << "Difficulty " << mDifficulty << "\n";
    if(!mKeptClass.empty())
        os << "KeptMinion " << mKeptClass << " " << mKeptLevel << "\n";
}

void Campaign::setProgressPath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mProgressPath = path;
}

bool Campaign::saveProgress() const
{
    std::string path;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        path = mProgressPath;
    }
    if(path.empty())
        return false;

    std::ofstream file(path.c_str());
    if(!file.is_open())
        return false;

    exportProgress(file);
    return file.good();
}

size_t Campaign::getNumLevels() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mLevels.size();
}

const CampaignLevel& Campaign::getLevel(size_t index) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mLevels.at(index);
}

size_t Campaign::findLevelByProvince(const std::string& province) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    for(size_t i = 0; i < mLevels.size(); ++i)
    {
        if(!province.empty() && (mLevels[i].mProvince == province))
            return i;
    }
    return mLevels.size();
}

bool Campaign::isCompleted(size_t index) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return (index < mCompleted.size()) && mCompleted[index];
}

size_t Campaign::findBranchSisterNoLock(size_t index) const
{
    const std::string& branch = mLevels[index].mBranch;
    if(branch.empty())
        return mLevels.size();
    for(size_t i = 0; i < mLevels.size(); ++i)
    {
        if((i != index) && (mLevels[i].mProvince == branch))
            return i;
    }
    return mLevels.size();
}

size_t Campaign::getCurrentLevelNoLock() const
{
    for(size_t i = 0; i < mCompleted.size(); ++i)
    {
        if(!mCompleted[i] && !mLevels[i].mBonus)
        {
            // The open sister of a finished branch is optional
            size_t sister = findBranchSisterNoLock(i);
            if((sister < mCompleted.size()) && mCompleted[sister])
                continue;
            return i;
        }
    }
    return mCompleted.size();
}

size_t Campaign::getCurrentLevel() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return getCurrentLevelNoLock();
}

bool Campaign::isUnlockedNoLock(size_t index) const
{
    if(index >= mLevels.size())
        return false;

    if(mLevels[index].mBonus)
        return mDiscovered[index] || mCompleted[index];

    // All main levels before the level must be completed. Of two branch
    // sisters one is enough, and a sister never blocks the other one.
    size_t ownSister = findBranchSisterNoLock(index);
    for(size_t i = 0; i < index; ++i)
    {
        if(mCompleted[i] || mLevels[i].mBonus || (i == ownSister))
            continue;
        size_t sister = findBranchSisterNoLock(i);
        if((sister < mCompleted.size()) && mCompleted[sister])
            continue;
        return false;
    }
    return true;
}

bool Campaign::isUnlocked(size_t index) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return isUnlockedNoLock(index);
}

bool Campaign::discoverBonusLevel(const std::string& file)
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if(!mActive)
            return false;

        size_t index = 0;
        while((index < mLevels.size()) && (mLevels[index].mFile != file))
            ++index;

        if((index >= mLevels.size()) || !mLevels[index].mBonus || mDiscovered[index])
            return false;

        mDiscovered[index] = true;
    }
    saveProgress();
    return true;
}

bool Campaign::isDiscovered(size_t index) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return (index < mDiscovered.size()) && mDiscovered[index];
}

size_t Campaign::getHeartstonePieces() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    size_t count = 0;
    for(size_t i = 0; i < mLevels.size(); ++i)
    {
        if(mLevels[i].mBonus && mLevels[i].mHeartstone && mDiscovered[i])
            ++count;
    }
    return count;
}

size_t Campaign::getHeartstoneTotal() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    size_t count = 0;
    for(size_t i = 0; i < mLevels.size(); ++i)
    {
        if(mLevels[i].mBonus && mLevels[i].mHeartstone)
            ++count;
    }
    return count;
}

bool Campaign::isHeartstoneComplete() const
{
    size_t total = getHeartstoneTotal();
    return (total > 0) && (getHeartstonePieces() == total);
}

bool Campaign::isFinished() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return !mLevels.empty() && (getCurrentLevelNoLock() == mLevels.size());
}

bool Campaign::hasProgress() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    for(size_t i = 0; i < mCompleted.size(); ++i)
    {
        if(mCompleted[i])
            return true;
    }
    return false;
}

void Campaign::setKeptMinion(const std::string& className, uint32_t level)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mKeptPendingClass = className;
    mKeptPendingLevel = level;
}

bool Campaign::takeKeptMinion(std::string& className, uint32_t& level)
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if(mKeptClass.empty())
            return false;

        className = mKeptClass;
        level = mKeptLevel;
        mKeptClass.clear();
        mKeptLevel = 0;
    }
    saveProgress();
    return true;
}

void Campaign::resetProgress()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCompleted.assign(mLevels.size(), false);
        mDiscovered.assign(mLevels.size(), false);
        mPlayedLevel = mLevels.size();
        mPlayedLevelWon = false;
        mDifficulty = getDefaultDifficulty();
        mKeptClass.clear();
        mKeptLevel = 0;
    }
    saveProgress();
}

uint32_t Campaign::getDifficulty() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mDifficulty;
}

void Campaign::setDifficulty(uint32_t difficulty)
{
    if(difficulty >= static_cast<uint32_t>(KeeperAIType::nbAI))
        return;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mDifficulty = difficulty;
    }
    saveProgress();
}

uint32_t Campaign::getDefaultDifficulty()
{
    return static_cast<uint32_t>(KeeperAIType::normal);
}

void Campaign::startLevel(size_t index)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mActive = true;
    mPlayedLevel = index;
    mPlayedLevelWon = false;
    mLevelSummary.clear();
    mKeptPendingClass.clear();
    mKeptPendingLevel = 0;
}

void Campaign::stopCampaign()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mActive = false;
    mPlayedLevel = mLevels.size();
    mPlayedLevelWon = false;
    mLevelSummary.clear();
}

bool Campaign::isActive() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mActive;
}

size_t Campaign::getPlayedLevel() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mPlayedLevel;
}

bool Campaign::getPlayedLevelWon() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mPlayedLevelWon;
}

void Campaign::clearPlayedLevel()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mPlayedLevel = mLevels.size();
    mPlayedLevelWon = false;
    mLevelSummary.clear();
}

void Campaign::setLevelSummary(const std::string& summary)
{
    std::lock_guard<std::mutex> lock(mMutex);
    // Only a campaign level that is played keeps the numbers
    if(mActive)
        mLevelSummary = summary;
}

std::string Campaign::getLevelSummary() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mLevelSummary;
}

bool Campaign::onLevelWon()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if(!mActive || (mPlayedLevel >= mLevels.size()))
            return false;

        mCompleted[mPlayedLevel] = true;
        mPlayedLevelWon = true;
        if(!mKeptPendingClass.empty())
        {
            mKeptClass = mKeptPendingClass;
            mKeptLevel = mKeptPendingLevel;
            mKeptPendingClass.clear();
            mKeptPendingLevel = 0;
        }
    }
    saveProgress();
    return true;
}
