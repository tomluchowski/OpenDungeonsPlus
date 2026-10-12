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

#include "social/CreaturePosts.h"

#include "entities/CreatureMoodValues.h"
#include "rooms/RoomType.h"

namespace social
{

namespace
{
//! \brief What the creature does according to its activity, Nb if nothing worth a post.
//! The room type is returned for the categories that mention a room.
PostCategory getActivityCategory(const CreatureActivity& activity, int32_t& roomType)
{
    roomType = 0;
    if(!activity.known)
        return PostCategory::Nb;

    if(activity.action == CreatureActionType::fight)
        return PostCategory::Fight;

    if((activity.task == CreatureActionType::eatChicken) || (activity.task == CreatureActionType::searchFood))
        return PostCategory::Eat;

    if(activity.task == CreatureActionType::getFee)
        return PostCategory::Payday;

    if((activity.task == CreatureActionType::sleep) ||
       ((activity.assignedRoom == RoomType::dormitory) && activity.inAssignedRoom))
        return PostCategory::Sleep;

    if(activity.task != CreatureActionType::useRoom)
        return PostCategory::Nb;

    roomType = static_cast<int32_t>(activity.assignedRoom);
    switch(activity.assignedRoom)
    {
        case RoomType::hatchery:
            return PostCategory::Eat;
        case RoomType::dormitory:
            return PostCategory::Sleep;
        case RoomType::trainingHall:
            return PostCategory::Train;
        case RoomType::nullRoomType:
        case RoomType::treasury:
        case RoomType::dungeonTemple:
            return PostCategory::Nb;
        default:
            return PostCategory::Work;
    }
}

int32_t getMoodRank(CreatureMoodLevel level)
{
    return static_cast<int32_t>(level);
}
}

std::string CreaturePosts::getRoomName(int32_t roomType)
{
    switch(static_cast<RoomType>(roomType))
    {
        case RoomType::dungeonTemple:
            return "temple";
        case RoomType::dormitory:
            return "dormitory";
        case RoomType::treasury:
            return "treasury";
        case RoomType::portal:
        case RoomType::portalWave:
            return "portal";
        case RoomType::workshop:
            return "workshop";
        case RoomType::trainingHall:
            return "training hall";
        case RoomType::library:
            return "library";
        case RoomType::hatchery:
            return "hatchery";
        case RoomType::crypt:
            return "crypt";
        case RoomType::prison:
            return "prison";
        case RoomType::bridgeWooden:
        case RoomType::bridgeStone:
            return "bridge";
        case RoomType::arena:
            return "arena";
        case RoomType::casino:
            return "casino";
        case RoomType::torture:
            return "torture chamber";
        default:
            return "dungeon";
    }
}

void CreaturePosts::reportUpdate(int64_t turn, const std::string& creature, const std::string& className,
    bool isWorker, const CreatureSnapshot& before, const CreatureSnapshot& after)
{
    PostLog& log = PostLog::getSingleton();
    int32_t level = static_cast<int32_t>(after.mLevel);

    if(after.mLevel > before.mLevel)
        log.addPost(turn, creature, className, isWorker, PostCategory::LevelUp, level);

    if(after.mHealthStage > before.mHealthStage)
        log.addPost(turn, creature, className, isWorker, PostCategory::Hurt, 0);

    uint32_t newBits = after.mMoodBits & ~before.mMoodBits;
    bool worseMood = (before.mMoodLevel != CreatureMoodLevel::Unknown) &&
        (getMoodRank(after.mMoodLevel) >= getMoodRank(CreatureMoodLevel::Upset)) &&
        (getMoodRank(after.mMoodLevel) > getMoodRank(before.mMoodLevel));
    if(worseMood || ((newBits & (CreatureMoodValues::Hungry | CreatureMoodValues::Tired)) != 0))
        log.addPost(turn, creature, className, isWorker, PostCategory::Unhappy, 0);
    if((newBits & CreatureMoodValues::GetFee) != 0)
        log.addPost(turn, creature, className, isWorker, PostCategory::Payday, level);
    if((newBits & CreatureMoodValues::KoTemp) != 0)
        log.addPost(turn, creature, className, isWorker, PostCategory::Ko, 0);
    if((newBits & CreatureMoodValues::InJail) != 0)
        log.addPost(turn, creature, className, isWorker, PostCategory::Jail, 0);

    // The activity only counts when the creature starts doing something different
    if(!before.mActivity.known || !after.mActivity.known)
        return;

    int32_t oldRoom = 0;
    int32_t newRoom = 0;
    PostCategory oldCategory = getActivityCategory(before.mActivity, oldRoom);
    PostCategory newCategory = getActivityCategory(after.mActivity, newRoom);
    if((newCategory == PostCategory::Nb) || ((newCategory == oldCategory) && (newRoom == oldRoom)))
        return;

    int32_t argument = newRoom;
    if(newCategory == PostCategory::Payday)
        argument = level;
    log.addPost(turn, creature, className, isWorker, newCategory, argument);
}

void CreaturePosts::reportRemoval(int64_t turn, const std::string& creature, const std::string& className,
    bool isWorker, uint32_t lastMoodBits)
{
    PostLog& log = PostLog::getSingleton();
    if((lastMoodBits & CreatureMoodValues::LeaveDungeon) != 0)
        log.addPost(turn, creature, className, isWorker, PostCategory::Left, 0);
    else if((lastMoodBits & CreatureMoodValues::KoDeath) != 0)
        log.addPost(turn, creature, className, isWorker, PostCategory::Died, 0);
}

}
