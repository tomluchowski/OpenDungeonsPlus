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

#include "social/SocialProfileCache.h"

#include "social/SocialGenerator.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

namespace social
{

SocialProfileCache::SocialProfileCache() :
    mDataLoaded(false)
{
}

SocialProfileCache& SocialProfileCache::getSingleton()
{
    static SocialProfileCache instance;
    return instance;
}

void SocialProfileCache::loadData()
{
    if(mDataLoaded)
        return;

    mDataLoaded = true;
    mData.loadFromDirectory(ConfigManager::getSingleton().getConfigPath());
    const std::vector<std::string>& errors = mData.getErrors();
    for(std::vector<std::string>::const_iterator it = errors.begin(); it != errors.end(); ++it)
    {
        OD_LOG_ERR("Creature profile data: " + *it);
    }
}

const SocialData& SocialProfileCache::getData()
{
    loadData();
    return mData;
}

const CreatureProfile& SocialProfileCache::getProfile(const std::string& creatureName,
    const std::string& className, bool isWorker)
{
    loadData();
    std::map<std::string, CreatureProfile>::iterator it = mProfiles.find(creatureName);
    if(it != mProfiles.end())
        return it->second;

    if(!mData.hasGroupForClass(className) && mLoggedUnmappedClasses.insert(className).second)
    {
        OD_LOG_WRN("Creature profile data: class " + className + " has no name group, using the default group");
    }

    // Two creatures of a dungeon must not share a full name: the first one asking keeps the name the
    // generator draws, a later one with the same name gets the next name variant (always the same one
    // for the same sequence of requests, and the last variant is unique through the creature name)
    CreatureProfile profile;
    for(uint32_t variant = 0; variant <= SocialGenerator::MAX_NAME_VARIANT + 1; ++variant)
    {
        profile = SocialGenerator::makeProfile(mData, creatureName, className, isWorker, variant);
        if(mUsedNames.insert(profile.getFullName()).second)
            break;
    }
    std::pair<std::map<std::string, CreatureProfile>::iterator, bool> inserted =
        mProfiles.insert(std::make_pair(creatureName, profile));
    return inserted.first->second;
}

const SocialProfileCache::FriendsAndFoe* SocialProfileCache::findFriendsAndFoe(const std::string& creatureName,
    uint32_t rosterVersion) const
{
    std::map<std::string, FriendsAndFoe>::const_iterator it = mFriends.find(creatureName);
    if((it == mFriends.end()) || (it->second.mRosterVersion != rosterVersion))
        return nullptr;

    return &it->second;
}

void SocialProfileCache::storeFriendsAndFoe(const std::string& creatureName, const FriendsAndFoe& friendsAndFoe)
{
    mFriends[creatureName] = friendsAndFoe;
}

void SocialProfileCache::clear()
{
    mProfiles.clear();
    mUsedNames.clear();
    mFriends.clear();
}

}
