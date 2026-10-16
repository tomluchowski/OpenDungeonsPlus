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

#include "creaturemood/CreatureMoodOutOfWork.h"

#include "creaturemood/CreatureMoodManager.h"
#include "entities/Creature.h"

#include <algorithm>

static const std::string CreatureMoodOutOfWorkName = "OutOfWork";

namespace
{
//! \brief Factory that creates CreatureMoodOutOfWork objects from the mood configuration.
class CreatureMoodOutOfWorkFactory : public CreatureMoodFactory
{
    //! \brief Creates a new CreatureMoodOutOfWork with default values.
    CreatureMood* createCreatureMood() const override
    { return new CreatureMoodOutOfWork; }

    //! \brief Returns the configuration name of this modifier.
    const std::string& getCreatureMoodName() const override
    {
        return CreatureMoodOutOfWorkName;
    }
};

// Register the factory
static CreatureMoodRegister reg(new CreatureMoodOutOfWorkFactory);
}

//! \brief Returns the name under which this modifier is listed in the mood configuration.
const std::string& CreatureMoodOutOfWork::getModifierName() const
{
    return CreatureMoodOutOfWorkName;
}

//! \brief Returns the mood change for the turns the creature found no job beyond TurnsMin (counted up to TurnsMax turns).
int32_t CreatureMoodOutOfWork::computeMood(const Creature& creature) const
{
    int32_t turns = creature.getNbTurnsOutOfWork();
    if(turns < mTurnsMin)
        return 0;

    turns = std::min(turns - mTurnsMin, mTurnsMax);
    return turns * mMoodModifier;
}

//! \brief Returns a copy of this modifier.
CreatureMoodOutOfWork* CreatureMoodOutOfWork::clone() const
{
    return new CreatureMoodOutOfWork(*this);
}

//! \brief Reads the modifier parameters from the configuration stream; returns false if a value is missing.
bool CreatureMoodOutOfWork::importFromStream(std::istream& is)
{
    if(!CreatureMood::importFromStream(is))
        return false;

    if(!(is >> mTurnsMin))
        return false;
    if(!(is >> mTurnsMax))
        return false;
    if(!(is >> mMoodModifier))
        return false;

    return true;
}

//! \brief Writes the modifier parameters to the stream.
void CreatureMoodOutOfWork::exportToStream(std::ostream& os) const
{
    CreatureMood::exportToStream(os);
    os << "\t" << mTurnsMin;
    os << "\t" << mTurnsMax;
    os << "\t" << mMoodModifier;
}

//! \brief Appends the names of the modifier parameters to the format string.
void CreatureMoodOutOfWork::getFormatString(std::string& format) const
{
    CreatureMood::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "TurnsMin\tTurnsMax\tMoodModifier";
}
