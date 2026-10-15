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

#include "social/PostLog.h"

#include "social/SocialRng.h"

#include <algorithm>
#include <sstream>

namespace social
{

std::string getPostCategoryName(PostCategory category)
{
    switch(category)
    {
        case PostCategory::Eat:
            return "eat";
        case PostCategory::Sleep:
            return "sleep";
        case PostCategory::Train:
            return "train";
        case PostCategory::Work:
            return "work";
        case PostCategory::Fight:
            return "fight";
        case PostCategory::Hurt:
            return "hurt";
        case PostCategory::LevelUp:
            return "levelup";
        case PostCategory::Payday:
            return "payday";
        case PostCategory::Unhappy:
            return "unhappy";
        case PostCategory::Ko:
            return "ko";
        case PostCategory::Jail:
            return "jail";
        case PostCategory::PickedUp:
            return "pickedup";
        case PostCategory::Arrived:
            return "arrived";
        case PostCategory::Left:
            return "left";
        case PostCategory::Died:
            return "died";
        default:
            return "";
    }
}

PostLog::PostLog() :
    mActive(false),
    mEnabledFromTurn(0),
    mTurnsPerSecond(1.0),
    mVersion(0),
    mRosterVersion(0),
    mTextFunction(nullptr)
{
}

PostLog& PostLog::getSingleton()
{
    static PostLog instance;
    return instance;
}

int64_t PostLog::secondsToTurns(uint32_t seconds) const
{
    int64_t turns = static_cast<int64_t>(mTurnsPerSecond * static_cast<double>(seconds) + 0.5);
    return (turns < 1) ? 1 : turns;
}

void PostLog::start(int64_t turnNow, double turnsPerSecond)
{
    stop();
    mActive = true;
    mTurnsPerSecond = (turnsPerSecond > 0.0) ? turnsPerSecond : 1.0;
    mEnabledFromTurn = turnNow + BURST_TURNS;
}

void PostLog::stop()
{
    mActive = false;
    mPosts.clear();
    mLastPostTurns.clear();
    mCreaturePostTurns.clear();
    mGlobalPostTurns.clear();
    mRecentTexts.clear();
    ++mVersion;
    ++mRosterVersion;
}

const Post* PostLog::findLatestPost(const std::string& creature) const
{
    for(std::deque<Post>::const_reverse_iterator it = mPosts.rbegin(); it != mPosts.rend(); ++it)
    {
        if(it->mCreature == creature)
            return &(*it);
    }
    return nullptr;
}

bool PostLog::addPost(int64_t turn, const std::string& creature, const std::string& className,
    bool isWorker, PostCategory category, int32_t argument)
{
    if(!mActive || (turn < mEnabledFromTurn) || (category == PostCategory::Nb))
        return false;

    // Cooldown per creature and category
    std::ostringstream keyStream;
    keyStream << creature << "|" << static_cast<int32_t>(category);
    std::string key = keyStream.str();
    std::map<std::string, int64_t>::iterator lastIt = mLastPostTurns.find(key);
    if((lastIt != mLastPostTurns.end()) && ((turn - lastIt->second) < secondsToTurns(COOLDOWN_SECONDS)))
        return false;

    // Limit per creature
    std::deque<int64_t>& creatureTurns = mCreaturePostTurns[creature];
    int64_t creatureWindow = secondsToTurns(CREATURE_WINDOW_SECONDS);
    while(!creatureTurns.empty() && ((turn - creatureTurns.front()) >= creatureWindow))
        creatureTurns.pop_front();
    if(creatureTurns.size() >= MAX_POSTS_PER_CREATURE)
        return false;

    // Global rate limit, keeps the cost bounded in big dungeons
    int64_t globalWindow = secondsToTurns(1);
    while(!mGlobalPostTurns.empty() && ((turn - mGlobalPostTurns.front()) >= globalWindow))
        mGlobalPostTurns.pop_front();
    if(mGlobalPostTurns.size() >= MAX_POSTS_PER_SECOND)
        return false;

    if(lastIt == mLastPostTurns.end())
        mLastPostTurns.insert(std::make_pair(key, turn));
    else
        lastIt->second = turn;
    creatureTurns.push_back(turn);
    mGlobalPostTurns.push_back(turn);

    std::ostringstream variantStream;
    variantStream << creature << "|" << getPostCategoryName(category) << "|" << (turn / 600);

    Post post;
    post.mTurn = turn;
    post.mCategory = category;
    post.mVariant = static_cast<uint32_t>(fnv1a64(variantStream.str()) & 0xFFFFFFFFULL);
    post.mArgument = argument;
    post.mCreature = creature;
    post.mClassName = className;
    post.mIsWorker = isWorker;
    if(mTextFunction != nullptr)
    {
        // Another variant if the text was posted a moment ago (a few tries, then it is accepted)
        std::string text = mTextFunction(post);
        for(uint32_t tries = 0; (tries < 16) && !text.empty() &&
            (std::find(mRecentTexts.begin(), mRecentTexts.end(), text) != mRecentTexts.end()); ++tries)
        {
            ++post.mVariant;
            text = mTextFunction(post);
        }
        mRecentTexts.push_back(text);
        while(mRecentTexts.size() > RECENT_TEXTS)
            mRecentTexts.pop_front();
    }
    mPosts.push_back(post);
    while(mPosts.size() > MAX_POSTS)
        mPosts.pop_front();
    ++mVersion;
    return true;
}

}
