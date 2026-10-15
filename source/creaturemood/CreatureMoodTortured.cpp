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

#include "creaturemood/CreatureMoodTortured.h"

#include "creaturemood/CreatureMoodManager.h"
#include "entities/Creature.h"

#include <algorithm>

static const std::string CreatureMoodTorturedName = "Tortured";

namespace
{
class CreatureMoodTorturedFactory : public CreatureMoodFactory
{
    CreatureMood* createCreatureMood() const override
    { return new CreatureMoodTortured; }

    const std::string& getCreatureMoodName() const override
    {
        return CreatureMoodTorturedName;
    }
};

// Register the factory
static CreatureMoodRegister reg(new CreatureMoodTorturedFactory);
}

const std::string& CreatureMoodTortured::getModifierName() const
{
    return CreatureMoodTorturedName;
}

int32_t CreatureMoodTortured::computeMood(const Creature& creature) const
{
    int32_t turns = std::min(creature.getNbTurnsTortureMood(), mTurnsMax);
    return turns * mMoodModifier;
}

CreatureMoodTortured* CreatureMoodTortured::clone() const
{
    return new CreatureMoodTortured(*this);
}

bool CreatureMoodTortured::importFromStream(std::istream& is)
{
    if(!CreatureMood::importFromStream(is))
        return false;

    if(!(is >> mTurnsMax))
        return false;
    if(!(is >> mMoodModifier))
        return false;

    return true;
}

void CreatureMoodTortured::exportToStream(std::ostream& os) const
{
    CreatureMood::exportToStream(os);
    os << "\t" << mTurnsMax;
    os << "\t" << mMoodModifier;
}

void CreatureMoodTortured::getFormatString(std::string& format) const
{
    CreatureMood::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "TurnsMax\tMoodModifier";
}
