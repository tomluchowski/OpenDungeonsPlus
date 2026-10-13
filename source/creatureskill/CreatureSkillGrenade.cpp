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

#include "creatureskill/CreatureSkillGrenade.h"

#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/MissileBlast.h"

#include <istream>

const std::string CreatureSkillGrenadeName = "Grenade";

namespace
{
class CreatureSkillGrenadeFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillGrenade; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillGrenadeName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillGrenadeFactory);
}

const std::string& CreatureSkillGrenade::getSkillName() const
{
    return CreatureSkillGrenadeName;
}

CreatureSkillGrenade* CreatureSkillGrenade::clone() const
{
    return new CreatureSkillGrenade(*this);
}

MissileOneHit* CreatureSkillGrenade::createMissile(GameMap& gameMap, Creature* creature,
        const Ogre::Vector3& direction, double phyAtk, double magAtk, double eleAtk, GameEntity* attackedObject,
        bool ko, bool notifyPlayerIfHit) const
{
    return new MissileBlast(&gameMap, creature->getSeat(), creature->getName(), mMissileMesh, mMissilePartScript,
        direction, mMissileSpeed, phyAtk, magAtk, eleAtk, attackedObject, false, ko, notifyPlayerIfHit,
        mBlastRadius, false);
}

void CreatureSkillGrenade::getFormatString(std::string& format) const
{
    CreatureSkillMissileLaunch::getFormatString(format);
    format += "\tBlastRadius";
}

void CreatureSkillGrenade::exportToStream(std::ostream& os) const
{
    CreatureSkillMissileLaunch::exportToStream(os);
    os << "\t" << mBlastRadius;
}

bool CreatureSkillGrenade::importFromStream(std::istream& is)
{
    if(!CreatureSkillMissileLaunch::importFromStream(is))
        return false;

    if(!(is >> mBlastRadius))
        return false;

    return true;
}

bool CreatureSkillGrenade::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkillMissileLaunch::isEqual(creatureSkill))
        return false;

    const CreatureSkillGrenade* skill = dynamic_cast<const CreatureSkillGrenade*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mBlastRadius != skill->mBlastRadius)
        return false;

    return true;
}
