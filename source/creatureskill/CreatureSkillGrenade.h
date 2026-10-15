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
    //! \brief Creates a grenade skill without a blast radius (it is read from the configuration).
    CreatureSkillGrenade() :
        mBlastRadius(0.0)
    {}

    //! \brief Nothing to release.
    virtual ~CreatureSkillGrenade()
    {}

    //! \brief The name of this skill ("Grenade").
    virtual const std::string& getSkillName() const override;

    //! \brief Returns a copy of this skill.
    virtual CreatureSkillGrenade* clone() const override;

    //! \brief True if the other skill is a grenade with the same parameters.
    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    //! \brief Appends the column names of the missile parameters and of the blast radius.
    virtual void getFormatString(std::string& format) const override;
    //! \brief Writes the missile parameters and the blast radius.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Reads the missile parameters and the blast radius, returns false if one is missing.
    virtual bool importFromStream(std::istream& is) override;

protected:
    //! \brief Creates the exploding missile that hurts every enemy within the blast radius.
    virtual MissileOneHit* createMissile(GameMap& gameMap, Creature* creature, const Ogre::Vector3& direction,
        double phyAtk, double magAtk, double eleAtk, GameEntity* attackedObject, bool ko,
        bool notifyPlayerIfHit) const override;

private:
    //! \brief Radius in tiles around the impact where enemy creatures are hurt
    double mBlastRadius;
};

#endif // CREATURESKILLGRENADE_H
