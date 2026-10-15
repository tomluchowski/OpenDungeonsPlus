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

#ifndef CREATURESKILLGUIDEDBOLT_H
#define CREATURESKILLGUIDEDBOLT_H

#include "creatureskill/CreatureSkillMissileLaunch.h"

#include <string>

//! \brief Guided Bolt: a magical missile that follows its target. It has the same parameters as MissileLaunch.
//! Like any missile it is stopped by walls and by the creatures in its way.
class CreatureSkillGuidedBolt : public CreatureSkillMissileLaunch
{
public:
    //! \brief Destroys the skill.
    virtual ~CreatureSkillGuidedBolt()
    {}

    //! \brief Returns the name under which the skill is registered.
    virtual const std::string& getSkillName() const override;

    //! \brief Returns a copy of the skill with the same parameters.
    virtual CreatureSkillGuidedBolt* clone() const override;

protected:
    //! \brief Creates the missile that is launched by this skill.
    virtual MissileOneHit* createMissile(GameMap& gameMap, Creature* creature, const Ogre::Vector3& direction,
        double phyAtk, double magAtk, double eleAtk, GameEntity* attackedObject, bool ko,
        bool notifyPlayerIfHit) const override;
};

#endif // CREATURESKILLGUIDEDBOLT_H
