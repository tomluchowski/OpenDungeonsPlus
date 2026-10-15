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

#ifndef CREATUREPOSTS_H
#define CREATUREPOSTS_H

#include "creaturemood/CreatureMood.h"
#include "entities/CreatureActivity.h"
#include "social/PostLog.h"

#include <stdint.h>
#include <string>

namespace social
{

//! \brief The state of a creature that can make it post. It is copied before and after the
//! creature is updated from the server packet, the difference decides what is posted.
struct CreatureSnapshot
{
    CreatureSnapshot() :
        mLevel(0),
        mHealthStage(0),
        mMoodBits(0),
        mMoodLevel(CreatureMoodLevel::Unknown)
    {
    }

    uint32_t mLevel;
    //! Overlay health stage, 0 is full health, higher is worse
    uint32_t mHealthStage;
    uint32_t mMoodBits;
    CreatureMoodLevel mMoodLevel;
    CreatureActivity mActivity;
};

//! \brief Turns the changes the client already sees (creature updates, arrival, removal, ...)
//! into posts of the PostLog. Nothing here runs per frame and nothing is sent or saved.
class CreaturePosts
{
public:
    //! \brief Compares two snapshots of a creature and adds the posts for what changed.
    static void reportUpdate(int64_t turn, const std::string& creature, const std::string& className,
        bool isWorker, const CreatureSnapshot& before, const CreatureSnapshot& after);

    //! \brief A creature leaves (mood bit "leave dungeon") or dies (bit "KO death"), other
    //! removals do not post.
    static void reportRemoval(int64_t turn, const std::string& creature, const std::string& className,
        bool isWorker, uint32_t lastMoodBits);

    //! \brief Name of a room type as used in the post texts ("training hall", "library", ...).
    static std::string getRoomName(int32_t roomType);
};

}

#endif // CREATUREPOSTS_H
