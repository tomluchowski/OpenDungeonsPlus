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

#ifndef CREATUREPROFILE_H
#define CREATUREPROFILE_H

#include <stdint.h>
#include <string>

namespace social
{

//! \brief The social profile of a creature. Produced from the creature name only
//! (see SocialGenerator::makeProfile), never stored and never sent over the network.
//! Level, mood and activity are not part of it.
struct CreatureProfile
{
    CreatureProfile() :
        mAge(0)
    {
    }

    //! Name of the creature the profile was made for
    std::string mCreatureName;
    std::string mClassName;
    //! Name group the class belongs to
    std::string mGroupName;

    std::string mFirstName;
    //! Empty if the creature has a title or neither
    std::string mSurname;
    //! Empty if the creature has a surname or neither
    std::string mTitle;
    int32_t mAge;
    //! Number as text, or a joke for some creatures ("older than the bricks")
    std::string mAgeText;
    //! "Female", "Male" or empty (creatures without a gender label)
    std::string mGender;
    std::string mRelationship;
    std::string mHometown;
    std::string mJob;
    //! Two different likes
    std::string mLikes[2];
    //! Two different dislikes
    std::string mDislikes[2];
    std::string mQuirk;
    //! Short text with the slots filled in, at most MAX_BIO_LENGTH characters
    std::string mBio;

    //! \brief First name followed by the surname or the title, if any.
    std::string getFullName() const
    {
        if(!mSurname.empty())
            return mFirstName + " " + mSurname;
        if(!mTitle.empty())
            return mFirstName + " " + mTitle;
        return mFirstName;
    }
};

}

#endif // CREATUREPROFILE_H
