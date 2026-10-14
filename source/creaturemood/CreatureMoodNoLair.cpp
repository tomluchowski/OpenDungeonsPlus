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

#include "creaturemood/CreatureMoodNoLair.h"

#include "creaturemood/CreatureMoodManager.h"
#include "entities/Creature.h"

#include <algorithm>

static const std::string CreatureMoodNoLairName = "NoLair";

namespace
{
//! \brief Factory that creates CreatureMoodNoLair objects from the mood configuration.
class CreatureMoodNoLairFactory : public CreatureMoodFactory
{
    //! \brief Creates a new CreatureMoodNoLair with default values.
    CreatureMood* createCreatureMood() const override
    { return new CreatureMoodNoLair; }

    //! \brief Returns the configuration name of this modifier.
    const std::string& getCreatureMoodName() const override
    {
        return CreatureMoodNoLairName;
    }
};

// Register the factory
static CreatureMoodRegister reg(new CreatureMoodNoLairFactory);
}

//! \brief Returns the name under which this modifier is listed in the mood configuration.
const std::string& CreatureMoodNoLair::getModifierName() const
{
    return CreatureMoodNoLairName;
}

//! \brief Returns the mood change while the creature has no home tile, otherwise 0.
int32_t CreatureMoodNoLair::computeMood(const Creature& creature) const
{
    if(creature.getHomeTile() != nullptr)
        return 0;

    return mMoodModifier;
}

//! \brief Returns a copy of this modifier.
CreatureMoodNoLair* CreatureMoodNoLair::clone() const
{
    return new CreatureMoodNoLair(*this);
}

//! \brief Reads the modifier parameters from the configuration stream; returns false if a value is missing.
bool CreatureMoodNoLair::importFromStream(std::istream& is)
{
    if(!CreatureMood::importFromStream(is))
        return false;

    if(!(is >> mMoodModifier))
        return false;

    return true;
}

//! \brief Writes the modifier parameters to the stream.
void CreatureMoodNoLair::exportToStream(std::ostream& os) const
{
    CreatureMood::exportToStream(os);
    os << "\t" << mMoodModifier;
}

//! \brief Appends the names of the modifier parameters to the format string.
void CreatureMoodNoLair::getFormatString(std::string& format) const
{
    CreatureMood::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "MoodModifier";
}
