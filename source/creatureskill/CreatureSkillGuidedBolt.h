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
    virtual ~CreatureSkillGuidedBolt()
    {}

    virtual const std::string& getSkillName() const override;

    virtual CreatureSkillGuidedBolt* clone() const override;

protected:
    virtual MissileOneHit* createMissile(GameMap& gameMap, Creature* creature, const Ogre::Vector3& direction,
        double phyAtk, double magAtk, double eleAtk, GameEntity* attackedObject, bool ko,
        bool notifyPlayerIfHit) const override;
};

#endif // CREATURESKILLGUIDEDBOLT_H
