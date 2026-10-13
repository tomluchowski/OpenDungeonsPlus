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

#include "creatureskill/CreatureSkillGuidedBolt.h"

#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/MissileBlast.h"

const std::string CreatureSkillGuidedBoltName = "GuidedBolt";

namespace
{
class CreatureSkillGuidedBoltFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillGuidedBolt; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillGuidedBoltName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillGuidedBoltFactory);
}

const std::string& CreatureSkillGuidedBolt::getSkillName() const
{
    return CreatureSkillGuidedBoltName;
}

CreatureSkillGuidedBolt* CreatureSkillGuidedBolt::clone() const
{
    return new CreatureSkillGuidedBolt(*this);
}

MissileOneHit* CreatureSkillGuidedBolt::createMissile(GameMap& gameMap, Creature* creature,
        const Ogre::Vector3& direction, double phyAtk, double magAtk, double eleAtk, GameEntity* attackedObject,
        bool ko, bool notifyPlayerIfHit) const
{
    return new MissileBlast(&gameMap, creature->getSeat(), creature->getName(), mMissileMesh, mMissilePartScript,
        direction, mMissileSpeed, phyAtk, magAtk, eleAtk, attackedObject, false, ko, notifyPlayerIfHit, 0.0, true);
}
