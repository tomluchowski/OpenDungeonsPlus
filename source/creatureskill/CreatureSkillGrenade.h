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

#ifndef CREATURESKILLGRENADE_H
#define CREATURESKILLGRENADE_H

#include "creatureskill/CreatureSkillMissileLaunch.h"

#include <iosfwd>
#include <string>

//! \brief Grenade: a thrown explosive that explodes when it hits a creature or a wall and hurts every
//! enemy creature within BlastRadius tiles. It has the parameters of MissileLaunch plus the blast radius.
class CreatureSkillGrenade : public CreatureSkillMissileLaunch
{
public:
    // Constructors
    CreatureSkillGrenade() :
        mBlastRadius(0.0)
    {}

    virtual ~CreatureSkillGrenade()
    {}

    virtual const std::string& getSkillName() const override;

    virtual CreatureSkillGrenade* clone() const override;

    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    virtual void getFormatString(std::string& format) const override;
    virtual void exportToStream(std::ostream& os) const override;
    virtual bool importFromStream(std::istream& is) override;

protected:
    virtual MissileOneHit* createMissile(GameMap& gameMap, Creature* creature, const Ogre::Vector3& direction,
        double phyAtk, double magAtk, double eleAtk, GameEntity* attackedObject, bool ko,
        bool notifyPlayerIfHit) const override;

private:
    double mBlastRadius;
};

#endif // CREATURESKILLGRENADE_H
