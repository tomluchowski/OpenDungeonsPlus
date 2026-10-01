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

#include "creaturemood/CreatureMoodSlapped.h"

#include "creaturemood/CreatureMoodManager.h"
#include "entities/Creature.h"

#include <algorithm>

static const std::string CreatureMoodSlappedName = "Slapped";

namespace
{
class CreatureMoodSlappedFactory : public CreatureMoodFactory
{
    CreatureMood* createCreatureMood() const override
    { return new CreatureMoodSlapped; }

    const std::string& getCreatureMoodName() const override
    {
        return CreatureMoodSlappedName;
    }
};

// Register the factory
static CreatureMoodRegister reg(new CreatureMoodSlappedFactory);
}

const std::string& CreatureMoodSlapped::getModifierName() const
{
    return CreatureMoodSlappedName;
}

int32_t CreatureMoodSlapped::computeMood(const Creature& creature) const
{
    int32_t nbSlaps = creature.getNbRecentSlaps(mDurationTurns);
    nbSlaps = std::min(nbSlaps, mMaxSlaps);
    return nbSlaps * mMoodModifier;
}

CreatureMoodSlapped* CreatureMoodSlapped::clone() const
{
    return new CreatureMoodSlapped(*this);
}

bool CreatureMoodSlapped::importFromStream(std::istream& is)
{
    if(!CreatureMood::importFromStream(is))
        return false;

    if(!(is >> mDurationTurns))
        return false;
    if(!(is >> mMaxSlaps))
        return false;
    if(!(is >> mMoodModifier))
        return false;

    return true;
}

void CreatureMoodSlapped::exportToStream(std::ostream& os) const
{
    CreatureMood::exportToStream(os);
    os << "\t" << mDurationTurns;
    os << "\t" << mMaxSlaps;
    os << "\t" << mMoodModifier;
}

void CreatureMoodSlapped::getFormatString(std::string& format) const
{
    CreatureMood::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "DurationTurns\tMaxSlaps\tMoodModifier";
}
