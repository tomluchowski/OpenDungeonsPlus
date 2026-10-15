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

#include "creaturemood/CreatureMoodHeldInHand.h"

#include "creaturemood/CreatureMoodManager.h"
#include "entities/Creature.h"

#include <algorithm>

static const std::string CreatureMoodHeldInHandName = "HeldInHand";

namespace
{
class CreatureMoodHeldInHandFactory : public CreatureMoodFactory
{
    CreatureMood* createCreatureMood() const override
    { return new CreatureMoodHeldInHand; }

    const std::string& getCreatureMoodName() const override
    {
        return CreatureMoodHeldInHandName;
    }
};

// Register the factory
static CreatureMoodRegister reg(new CreatureMoodHeldInHandFactory);
}

const std::string& CreatureMoodHeldInHand::getModifierName() const
{
    return CreatureMoodHeldInHandName;
}

int32_t CreatureMoodHeldInHand::computeMood(const Creature& creature) const
{
    int32_t turns = creature.getNbTurnsInHand();
    if(turns < mTurnsMin)
        return 0;

    turns = std::min(turns - mTurnsMin, mTurnsMax);
    return turns * mMoodModifier;
}

CreatureMoodHeldInHand* CreatureMoodHeldInHand::clone() const
{
    return new CreatureMoodHeldInHand(*this);
}

bool CreatureMoodHeldInHand::importFromStream(std::istream& is)
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

void CreatureMoodHeldInHand::exportToStream(std::ostream& os) const
{
    CreatureMood::exportToStream(os);
    os << "\t" << mTurnsMin;
    os << "\t" << mTurnsMax;
    os << "\t" << mMoodModifier;
}

void CreatureMoodHeldInHand::getFormatString(std::string& format) const
{
    CreatureMood::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "TurnsMin\tTurnsMax\tMoodModifier";
}
