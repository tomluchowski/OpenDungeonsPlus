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

#include "creaturemood/CreatureMoodHatedCompany.h"

#include "creaturemood/CreatureMoodManager.h"
#include "entities/Creature.h"

#include <algorithm>

static const std::string CreatureMoodHatedCompanyName = "HatedCompany";

namespace
{
class CreatureMoodHatedCompanyFactory : public CreatureMoodFactory
{
    CreatureMood* createCreatureMood() const override
    { return new CreatureMoodHatedCompany; }

    const std::string& getCreatureMoodName() const override
    {
        return CreatureMoodHatedCompanyName;
    }
};

// Register the factory
static CreatureMoodRegister reg(new CreatureMoodHatedCompanyFactory);
}

const std::string& CreatureMoodHatedCompany::getModifierName() const
{
    return CreatureMoodHatedCompanyName;
}

int32_t CreatureMoodHatedCompany::computeMood(const Creature& creature) const
{
    int32_t turns = std::min(creature.getNbTurnsHatedCompany(), mTurnsMax);
    return turns * mMoodModifier;
}

CreatureMoodHatedCompany* CreatureMoodHatedCompany::clone() const
{
    return new CreatureMoodHatedCompany(*this);
}

bool CreatureMoodHatedCompany::importFromStream(std::istream& is)
{
    if(!CreatureMood::importFromStream(is))
        return false;

    if(!(is >> mTurnsMax))
        return false;
    if(!(is >> mMoodModifier))
        return false;

    return true;
}

void CreatureMoodHatedCompany::exportToStream(std::ostream& os) const
{
    CreatureMood::exportToStream(os);
    os << "\t" << mTurnsMax;
    os << "\t" << mMoodModifier;
}

void CreatureMoodHatedCompany::getFormatString(std::string& format) const
{
    CreatureMood::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "TurnsMax\tMoodModifier";
}
